#include "modernizer/modernizer.h"

#include "absl/algorithm/container.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Edit/Commit.h"
#include "clang/Edit/EditedSource.h"
#include "clang/Edit/EditsReceiver.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/AllTUsExecution.h"
#include "clang/Tooling/JSONCompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Refactoring/AtomicChange.h"
#include "clang/Tooling/StandaloneExecution.h"
#include "modernizer/diff.h"
#include "modernizer/filesystem.h"
#include "modernizer/mutex_lock.h"
#include "modernizer/path_pattern.h"
#include "re2/re2.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace modernizer {
namespace {

constexpr std::string_view kModernizeMacroRegex =
    "(RTC_DISALLOW_COPY_AND_ASSIGN\\(\\s*(\\S(|.*\\S))\\s*\\);)";

constexpr std::string_view kModernizeHeader = "rtc_base/constructor_magic.h";

struct SimpleSourceLocation {
  int line;
  int column;

  constexpr bool operator<(const SimpleSourceLocation& other) const {
    return std::tie(line, column) < std::tie(other.line, other.column);
  }
};

class LOCKABLE ReplacementsContext {
 public:
  void lock() EXCLUSIVE_LOCK_FUNCTION() { mutex_.Lock(); }

  void unlock() UNLOCK_FUNCTION() { mutex_.Unlock(); }

  std::map<std::string, std::map<SimpleSourceLocation, Replacements>>&
  GetReplacements() EXCLUSIVE_LOCKS_REQUIRED(this) {
    return impl_;
  }

 private:
  mutable absl::Mutex mutex_;
  std::map<std::string, std::map<SimpleSourceLocation, Replacements>> impl_
      GUARDED_BY(mutex_);
};

class ClassMemberFunctionVisitor
    : public RecursiveASTVisitor<ClassMemberFunctionVisitor> {
 public:
  using Base = RecursiveASTVisitor<ClassMemberFunctionVisitor>;
  friend class RecursiveASTVisitor<ClassMemberFunctionVisitor>;

  explicit ClassMemberFunctionVisitor(const CXXRecordDecl* record_decl)
      : record_decl_(record_decl) {}

  bool shouldVisitImplicitCode() const { return true; }

  void Visit() {
    inner_decls_.clear();
    depth_ = 0;
    Base::TraverseDecl(const_cast<CXXRecordDecl*>(record_decl_));
  }

  const std::vector<Decl*>& GetInnerDecls() const { return inner_decls_; }

 private:
  bool TraverseDecl(Decl* node) {
    if (!node) {
      return Base::TraverseDecl(node);
    }

    ++depth_;
    bool should_continue = true;
    if (depth_ <= 1) {
      should_continue = Base::TraverseDecl(node);
    }
    --depth_;
    return should_continue;
  }

  bool VisitCXXMethodDecl(CXXMethodDecl* decl) {
    if (depth_ == 1) {
      inner_decls_.emplace_back(decl);
    }
    return true;
  }

  bool VisitAccessSpecDecl(AccessSpecDecl* decl) {
    if (depth_ == 1) {
      inner_decls_.emplace_back(decl);
    }
    return true;
  }

  bool VisitFieldDecl(FieldDecl* decl) {
    if (depth_ == 1) {
      inner_decls_.emplace_back(decl);
    }
    return true;
  }

  bool VisitVarDecl(VarDecl* decl) {
    if (depth_ == 1) {
      inner_decls_.emplace_back(decl);
    }
    return true;
  }

  const CXXRecordDecl* record_decl_;
  int depth_ = 0;
  std::vector<Decl*> inner_decls_;
};

class ModernizerCallback : public MatchFinder::MatchCallback {
 public:
  explicit ModernizerCallback(const std::filesystem::path& root_path,
                              const std::filesystem::path& build_path,
                              ReplacementsContext* replacements_context,
                              const PathPattern* path_pattern)
      : root_path_(root_path),
        build_path_(build_path),
        replacements_context_(replacements_context),
        path_pattern_(path_pattern) {
    assert(replacements_context_);
  }

  ~ModernizerCallback() override = default;

  void run(const MatchFinder::MatchResult& result) override {
    assert(result.SourceManager);
    assert(result.Context);
    const SourceManager& sm = *result.SourceManager;
    LangOptions lang_opts = result.Context->getLangOpts();
    const CXXConstructorDecl* decl =
        result.Nodes.getNodeAs<CXXConstructorDecl>("decl");
    assert(decl);
    if (!decl->getBeginLoc().isValid()) {
      assert(false);
      return;
    }
    FullSourceLoc source_loc(sm.getExpansionLoc(decl->getLocation()), sm);
    assert(source_loc.isValid());
    const FileEntry* file_entry = source_loc.getFileEntry();
    assert(file_entry);
    std::filesystem::path file_path(
        std::string_view(file_entry->tryGetRealPathName()));
    auto pair = std::mismatch(root_path_.begin(), root_path_.end(),
                              file_path.begin(), file_path.end());
    assert(pair.first == root_path_.end());
    auto rel_file_path_over_buildroot = Relative(file_path, build_path_);
    if (!rel_file_path_over_buildroot) {
      llvm::errs() << "filesystem::relative failed: "
                   << rel_file_path_over_buildroot.takeError() << "\n";
      return;
    }
    std::string rel_file_path_str = rel_file_path_over_buildroot->string();

    if (path_pattern_) {
      auto rel_file_path = Relative(file_path, root_path_);
      if (!rel_file_path) {
        llvm::errs() << "filesystem::relative failed: "
                     << rel_file_path.takeError() << "\n";
        return;
      }
      if (!path_pattern_->Match(rel_file_path->string())) {
        llvm::errs() << "Skip " << rel_file_path->string()
                     << " because it does not match the source file pattern\n";
        return;
      }
    }

    auto simple_source_loc = GetSimpleSourceLocation(source_loc);
    assert(simple_source_loc);
    llvm::errs() << "candidate <file:" << *rel_file_path_over_buildroot
                 << ",line:" << simple_source_loc->line
                 << ",column:" << simple_source_loc->column << ">\n";

    const CXXRecordDecl* class_decl = decl->getParent();
    assert(class_decl);
    AccessSpecifier class_access_specifier =
        (class_decl->getTagKind() == clang::TTK_Class)
            ? AccessSpecifier::AS_private
            : AccessSpecifier::AS_public;
    ClassMemberFunctionVisitor visitor(class_decl);
    visitor.Visit();
    const auto& inner_decls = visitor.GetInnerDecls();

    std::optional<SourceLocation> remove_decl_source_location =
        CheckIfRemovablePrivateDeclLocation(class_access_specifier, decl,
                                            inner_decls, sm, lang_opts);
    std::optional<std::pair<SourceRange, std::string>> macro_source_range_name =
        FindMacro(decl, kModernizeMacroRegex, sm, lang_opts);
    if (!macro_source_range_name) {
      return;
    }
    auto [macro_source_range, class_name] = macro_source_range_name.value();
    if (remove_decl_source_location && remove_decl_source_location->isValid()) {
      macro_source_range = SourceRange(*remove_decl_source_location,
                                       macro_source_range.getEnd());
    }

    std::optional<SourceLocation> insertable_loc = FindInsertableLocation(
        class_access_specifier, inner_decls, sm, lang_opts);
    if (!insertable_loc) {
      return;
    }

    std::map<SimpleSourceLocation, Replacements> loc_replacements;
    {
      AtomicChange change(sm, macro_source_range.getBegin());
      llvm::Error result =
          change.replace(sm, CharSourceRange(macro_source_range, false), "");
      assert(!result);
      std::optional<SimpleSourceLocation> simple_source_loc =
          GetSimpleSourceLocation(
              FullSourceLoc(macro_source_range.getBegin(), sm));
      assert(simple_source_loc);
      loc_replacements[*simple_source_loc] = change.getReplacements();
    }
    {
      SourceLocation insert_offset_loc = insertable_loc->getLocWithOffset(1);
      assert(insert_offset_loc.isValid());
      AtomicChange change(sm, insert_offset_loc);
      std::string deleted_decl;
      llvm::raw_string_ostream deleted_decl_stream(deleted_decl);
      deleted_decl_stream << llvm::format(
          "\n\n%s(const %s&) = delete;\n%s& operator=(const %s&) = delete;\n",
          class_name.c_str(), class_name.c_str(), class_name.c_str(),
          class_name.c_str());
      llvm::Error result =
          change.insert(sm, insert_offset_loc, deleted_decl_stream.str(), true);
      assert(!result);
      std::optional<SimpleSourceLocation> simple_source_loc =
          GetSimpleSourceLocation(FullSourceLoc(insert_offset_loc, sm));
      assert(simple_source_loc);
      loc_replacements[*simple_source_loc] = change.getReplacements();
    }

    MutexLock guard(*replacements_context_);
    auto& replacements = replacements_context_->GetReplacements();
    auto replacements_iter = replacements.find(rel_file_path_str);
    if (replacements_iter == replacements.end()) {
      auto result2 = replacements.insert(
          std::remove_reference<decltype(replacements)>::type::value_type(
              rel_file_path_str, {}));
      assert(result2.second);
      replacements_iter = result2.first;
    }
    for (const auto& loc_replacement : loc_replacements) {
      replacements_iter->second.insert(loc_replacement);
    }
  }

 private:
  static std::optional<std::pair<SourceRange, std::string>> FindMacro(
      const Decl* decl,
      std::string_view regex_text,
      const SourceManager& sm,
      const LangOptions& lang_opts) {
    re2::RE2 regex{re2::StringPiece(regex_text)};
    assert(regex.ok());
    SourceLocation source_location_begin =
        sm.getExpansionLoc(decl->getLocation());
    SourceLocation source_location_end = source_location_begin;
    for (int i = 0; i < 10; ++i) {
      SourceRange source_range(source_location_begin, source_location_end);
      if (source_range.isInvalid()) {
        return std::nullopt;
      }
      CharSourceRange char_source_range =
          Lexer::getAsCharRange(source_range, sm, lang_opts);
      if (char_source_range.isInvalid()) {
        return std::nullopt;
      }
      std::string source_text =
          Lexer::getSourceText(char_source_range, sm, lang_opts).str();

      re2::StringPiece match0;
      re2::StringPiece match1;
      if (re2::RE2::FullMatch(source_text, regex, &match0, &match1)) {
        return std::make_pair(
            SourceRange(source_location_begin,
                        source_location_end.getLocWithOffset(1)),
            match1.as_string());
      }
      llvm::Optional<Token> tok =
          Lexer::findNextToken(source_location_end, sm, lang_opts);
      if (!tok) {
        return std::nullopt;
      }
      source_location_end = tok->getLocation();
    }
    return std::nullopt;
  }

  static std::optional<SourceLocation> CheckIfRemovablePrivateDeclLocation(
      AccessSpecifier class_access_specifier,
      const CXXConstructorDecl* macro_ctor_decl,
      const std::vector<Decl*>& all_decls,
      const SourceManager& sm,
      const LangOptions& lang_opts) {
    AccessSpecifier as = class_access_specifier;
    AccessSpecDecl* candidate_decl = nullptr;
    bool maybe_remove = false;
    for (auto iter = all_decls.begin(); iter != all_decls.end(); ++iter) {
      Decl* decl = *iter;
      if (llvm::isa<AccessSpecDecl>(decl)) {
        auto access_decl = static_cast<AccessSpecDecl*>(decl);
        as = access_decl->getAccess();
        assert(as != clang::AS_none);
        if (maybe_remove && as != clang::AS_private) {
          candidate_decl = nullptr;
          maybe_remove = false;
          break;
        } else if (!maybe_remove && as == clang::AS_private) {
          candidate_decl = access_decl;
          maybe_remove = true;
        }
        continue;
      }
      if (decl == macro_ctor_decl) {
        if (as != clang::AS_private) {
          return std::nullopt;
        }
        maybe_remove = true;
        ++iter;
        assert(llvm::isa<CXXMethodDecl>(*iter));
        continue;
      }
      if (!decl->isImplicit() && maybe_remove) {
        candidate_decl = nullptr;
        maybe_remove = false;
        break;
      }
    }
    if (maybe_remove) {
      return candidate_decl->getLocation();
    }
    return std::nullopt;
  }

  static std::optional<SourceLocation> FindInsertableLocation(
      AccessSpecifier class_access_specifier,
      const std::vector<Decl*>& all_decls,
      const SourceManager& sm,
      const LangOptions& lang_opts) {
    AccessSpecifier as = class_access_specifier;
    Decl* selected_decl = nullptr;
    for (Decl* decl : all_decls) {
      if (llvm::isa<AccessSpecDecl>(decl)) {
        as = static_cast<AccessSpecDecl*>(decl)->getAccess();
        assert(as != clang::AS_none);
        continue;
      }
      if (llvm::isa<CXXDestructorDecl>(decl)) {
        if (as == clang::AS_public && !decl->isImplicit()) {
          selected_decl = static_cast<CXXDestructorDecl*>(decl);
          break;
        }
      }
    }

    if (selected_decl == nullptr) {
      as = class_access_specifier;
      for (Decl* decl : all_decls) {
        if (llvm::isa<AccessSpecDecl>(decl)) {
          as = static_cast<AccessSpecDecl*>(decl)->getAccess();
          assert(as != clang::AS_none);
          continue;
        }
        if (llvm::isa<CXXConstructorDecl>(decl)) {
          if (as == clang::AS_public && !decl->isImplicit()) {
            selected_decl = static_cast<CXXConstructorDecl*>(decl);
          }
        }
      }
    }

    if (selected_decl == nullptr) {
      as = class_access_specifier;
      for (Decl* decl : all_decls) {
        if (llvm::isa<AccessSpecDecl>(decl)) {
          as = static_cast<AccessSpecDecl*>(decl)->getAccess();
          assert(as != clang::AS_none);
          continue;
        }
        if (llvm::isa<CXXDestructorDecl>(decl)) {
          if (!decl->isImplicit()) {
            selected_decl = static_cast<CXXDestructorDecl*>(decl);
            break;
          }
        }
      }
    }

    if (selected_decl == nullptr) {
      as = class_access_specifier;
      Decl* candidate_decl = nullptr;
      for (Decl* decl : all_decls) {
        if (llvm::isa<AccessSpecDecl>(decl)) {
          auto previous_as = as;
          as = static_cast<AccessSpecDecl*>(decl)->getAccess();
          assert(as != clang::AS_none);
          if (previous_as == clang::AS_public && as != previous_as &&
              candidate_decl) {
            selected_decl = candidate_decl;
            break;
          }
          continue;
        }
        if (as == clang::AS_public) {
          candidate_decl = decl;
        }
      }
    }

    if (selected_decl == nullptr) {
      return std::nullopt;
    }

    SourceLocation candidate_loc = selected_decl->getEndLoc();
    bool need_next_semi;
    if (llvm::isa<FunctionDecl>(selected_decl)) {
      FunctionDecl* selected_function_decl =
          static_cast<FunctionDecl*>(selected_decl);
      if (selected_function_decl->isDefaulted()) {
        need_next_semi = true;
      } else if (selected_function_decl->isInlined()) {
        need_next_semi = false;
      } else {
        need_next_semi = true;
      }
    } else {
      need_next_semi = true;
    }

    if (!need_next_semi) {
      return candidate_loc;
    }

    std::optional<SourceLocation> next_semi_loc =
        FindNextSemi(candidate_loc, sm, lang_opts);
    return next_semi_loc;
  }

  static std::optional<SourceLocation> FindNextSemi(
      SourceLocation loc,
      const SourceManager& sm,
      const LangOptions& lang_opts) {
    while (true) {
      llvm::Optional<Token> token = Lexer::findNextToken(loc, sm, lang_opts);
      if (!token) {
        return std::nullopt;
      }
      loc = token->getLocation();
      if (token->getKind() == clang::tok::semi) {
        return loc;
      }
    }
  }

  static std::optional<SimpleSourceLocation> GetSimpleSourceLocation(
      const FullSourceLoc& source_loc) {
    SimpleSourceLocation simple_source_loc;
    bool invalid = false;
    simple_source_loc.line = source_loc.getLineNumber(&invalid);
    if (invalid) {
      return std::nullopt;
    }
    simple_source_loc.column = source_loc.getColumnNumber(&invalid);
    if (invalid) {
      return std::nullopt;
    }
    return simple_source_loc;
  }

  const std::filesystem::path root_path_;
  const std::filesystem::path build_path_;
  ReplacementsContext* replacements_context_;
  const PathPattern* path_pattern_;
};

class StoredCompilationDatabase : public CompilationDatabase {
 public:
  ~StoredCompilationDatabase() override = default;

  std::vector<CompileCommand> getCompileCommands(
      llvm::StringRef file_path) const override {
    std::vector<CompileCommand> result;
    auto iter = impl_.lower_bound(file_path.str());
    const auto iter_end = impl_.upper_bound(file_path.str());
    for (; iter != impl_.end() && iter != iter_end; ++iter) {
      result.emplace_back(iter->second.directory, iter->first,
                          iter->second.command_line, iter->second.output);
    }
    return result;
  }
  std::vector<std::string> getAllFiles() const override {
    std::vector<std::string> result;
    for (const auto& iter : impl_) {
      result.push_back(iter.first);
    }
    return result;
  }
  std::vector<CompileCommand> getAllCompileCommands() const override {
    std::vector<CompileCommand> result;
    for (const auto& iter : impl_) {
      result.emplace_back(iter.second.directory, iter.first,
                          iter.second.command_line, iter.second.output);
    }
    return result;
  }

  void Add(std::string&& file_name,
           std::string&& directory,
           std::vector<std::string>&& command_line,
           std::string&& output);

 private:
  struct CompilationData {
    std::string directory;
    std::vector<std::string> command_line;
    std::string output;
  };

  std::multimap<std::string, CompilationData> impl_;
};

void StoredCompilationDatabase::Add(std::string&& file_name,
                                    std::string&& directory,
                                    std::vector<std::string>&& command_line,
                                    std::string&& output) {
  impl_.insert(
      std::make_pair(file_name, CompilationData{.directory = directory,
                                                .command_line = command_line,
                                                .output = output}));
}

}  // namespace

int RunModernizer(const RunModernizerOptions& options) {
  auto project_root_or_error = Canonical(options.project_root);
  if (!project_root_or_error) {
    llvm::errs() << "Invalid project root: " << options.project_root
                 << " error: "
                 << llvm::toString(project_root_or_error.takeError()) << "\n";
    return 1;
  }

  const auto& project_root = *project_root_or_error;
  const auto& compile_commands = options.compile_commands;
  bool in_place = options.in_place;
  llvm::raw_ostream* out_stream = options.out_stream;
  std::error_code ec;
  if (!std::filesystem::exists(project_root, ec) ||
      !std::filesystem::is_directory(project_root, ec)) {
    llvm::errs() << "Project Root does not exist or not a directory: "
                 << ec.message() << "\n";
    return 1;
  }
  if (!std::filesystem::exists(compile_commands, ec)) {
    llvm::errs() << "compile_commands.json does not exist: " << ec.message()
                 << "\n";
    return 1;
  }

  std::optional<PathPattern> source_file_pattern;
  if (!options.source_file_pattern.empty()) {
    source_file_pattern = PathPattern::Create(options.source_file_pattern);
    if (!source_file_pattern) {
      llvm::errs() << "Bad source file pattern: " << options.source_file_pattern
                   << "\n";
      return 1;
    }
  }

  if (!in_place && !out_stream) {
    llvm::errs() << "Output stream is not set.\n";
    return 1;
  }

  StoredCompilationDatabase stored_compilation_database;
  std::filesystem::path build_root;
  std::vector<std::string> source_paths;
  {
    std::string error_message;
    auto compilation_database = JSONCompilationDatabase::loadFromFile(
        compile_commands.string(), error_message, JSONCommandLineSyntax::Gnu);
    if (!compilation_database) {
      llvm::errs() << "Parsing compile_commands.json failed: " << error_message
                   << "\n";
      return 1;
    }
    for (auto compile_command : compilation_database->getAllCompileCommands()) {
      if (build_root.empty()) {
        build_root = compile_command.Directory;
      } else if (build_root != compile_command.Directory) {
        llvm::errs() << "Multiple directory not supported: first: "
                     << build_root.string()
                     << ", second: " << compile_command.Directory << "\n";
        return 1;
      }
      std::filesystem::path file_path(compile_command.Filename);
      if (file_path.is_relative()) {
        auto new_file_path = std::filesystem::path(
            compile_command.Directory /
            std::filesystem::path(compile_command.Filename));
        auto file_path_result = Canonical(new_file_path);
        if (!file_path_result) {
          llvm::errs() << "filesystem::canonical for " << new_file_path
                       << " returned error: "
                       << llvm::toString(file_path_result.takeError()) << "\n";
          continue;
        }
        file_path = *file_path_result;
      }
      if (source_file_pattern) {
        auto relative_file_path = Relative(file_path, project_root);
        if (!relative_file_path) {
          llvm::errs() << "filesystem::relative for " << file_path
                       << " returned error: "
                       << llvm::toString(relative_file_path.takeError())
                       << "\n";
          continue;
        }
        if (!source_file_pattern->Match(relative_file_path->string())) {
          llvm::errs()
              << "Skip " << *relative_file_path
              << " because it does not match the source file pattern\n";
          continue;
        }
      }
      source_paths.push_back(file_path.string());
      stored_compilation_database.Add(file_path.string(),
                                      std::move(compile_command.Directory),
                                      std::move(compile_command.CommandLine),
                                      std::move(compile_command.Output));
    }
  }

  ReplacementsContext replacements_context;
  std::unique_ptr<ToolExecutor> executor;
  if (options.num_jobs > 1) {
    executor = std::make_unique<AllTUsToolExecutor>(stored_compilation_database,
                                                    options.num_jobs);
  } else {
    executor = std::make_unique<StandaloneToolExecutor>(
        stored_compilation_database, source_paths);
  }

  ArgumentsAdjuster arguments_adjuster = combineAdjusters(
      getClangStripDependencyFileAdjuster(),
      combineAdjusters(getClangSyntaxOnlyAdjuster(),
                       combineAdjusters(getStripPluginsAdjuster(),
                                        getClangStripOutputAdjuster())));

  MatchFinder finder;
  ModernizerCallback callback(
      project_root, build_root, &replacements_context,
      (source_file_pattern ? &(*source_file_pattern) : nullptr));

  finder.addMatcher(
      namedDecl(cxxConstructorDecl(), isExpandedFromMacro(kModernizeMacro))
          .bind("decl"),
      &callback);

  llvm::Error error =
      executor->execute(newFrontendActionFactory(&finder), arguments_adjuster);
  if (error) {
    llvm::errs() << "Execute error: " << toString(std::move(error)) << "\n";
    return 1;
  }

  // boilerplate
  LangOptions default_lang_options;
  llvm::IntrusiveRefCntPtr<DiagnosticOptions> diag_opts =
      llvm::makeIntrusiveRefCnt<DiagnosticOptions>();
  TextDiagnosticPrinter diagnostic_printer(llvm::errs(), &*diag_opts);
  DiagnosticsEngine diagnostics(
      IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()), &*diag_opts,
      &diagnostic_printer, false);
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> file_system(
      llvm::vfs::createPhysicalFileSystem().release());
  file_system->setCurrentWorkingDirectory(build_root.string());
  llvm::IntrusiveRefCntPtr<FileManager> files =
      llvm::makeIntrusiveRefCnt<FileManager>(FileSystemOptions(), file_system);

  SourceManager sm(diagnostics, *files);
  Rewriter rewrite(sm, default_lang_options);
  {
    FileManager& file_manager = sm.getFileManager();
    MutexLock guard(replacements_context);
    const std::map<std::string, std::map<SimpleSourceLocation, Replacements>>&
        replacements = replacements_context.GetReplacements();

    for (const auto& file_replacements : replacements) {
      const std::string& file_path = file_replacements.first;
      const FileEntry* entry = nullptr;
      if (auto file = file_manager.getFile(file_path)) {
        entry = *file;
      } else {
        llvm::errs() << "getFile failed for " << file_path
                     << " with error: " << file.getError().message() << "\n";
        continue;
      }

      FileID id = sm.getOrCreateFileID(entry, SrcMgr::C_User);
      llvm::StringRef buffer = sm.getBufferData(id);

      auto style = format::getStyle("file", file_path, "LLVM", "",
                                    &file_manager.getVirtualFileSystem());
      if (!style) {
        llvm::errs() << llvm::toString(style.takeError()) << "\n";
        continue;
      }

      Replacements merged_replacements;
      for (auto iter = file_replacements.second.rbegin();
           iter != file_replacements.second.rend(); ++iter) {
        merged_replacements = merged_replacements.merge(iter->second);
      }

      do {
        if (!id.isValid()) {
          break;
        }
        Replacements current_replacements;

        llvm::Error err = current_replacements.add(
            Replacement(file_path, UINT_MAX, 1, kModernizeHeader));
        if (err) {
          llvm::errs() << llvm::toString(std::move(err)) << "\n";
          break;
        }
        llvm::Expected<Replacements> header_replacements =
            clang::format::cleanupAroundReplacements(
                buffer, current_replacements, *style);
        if (!header_replacements) {
          llvm::errs() << llvm::toString(header_replacements.takeError())
                       << "\n";
          break;
        }
        merged_replacements = merged_replacements.merge(*header_replacements);
      } while (0);

      auto formatted_replacements =
          format::formatReplacements(buffer, merged_replacements, *style);
      if (!formatted_replacements) {
        llvm::errs() << llvm::toString(formatted_replacements.takeError())
                     << "\n";
        continue;
      }

      if (formatted_replacements &&
          !applyAllReplacements(*formatted_replacements, rewrite)) {
        llvm::errs() << "Apply Replacements failed\n";
        return 1;
      }
    }
  }

  if (in_place) {
    // TODO(bc-lee): Remove chdir
    errno = 0;
    chdir(build_root.c_str());
    assert(errno == 0);
    if (rewrite.overwriteChangedFiles()) {
      llvm::errs() << "write to file failed\n";
      return 1;
    }
    return 0;
  }
  for (auto iter = rewrite.buffer_begin(); iter != rewrite.buffer_end();
       ++iter) {
    llvm::Optional<llvm::MemoryBufferRef> buffer_before =
        sm.getBufferOrNone(iter->first);
    assert(buffer_before);
    std::string_view file_name =
        std::string_view(buffer_before->getBufferIdentifier());
    std::filesystem::path file_path(build_root);
    file_path /= file_name;

    std::string buffer_after;
    llvm::raw_string_ostream buffer_after_stream(buffer_after);
    iter->second.write(buffer_after_stream);
    buffer_after_stream.flush();

    auto file_name_result = Relative(file_path, project_root);
    if (!file_name_result) {
      llvm::errs() << "filesystem::relative failed: "
                   << file_name_result.takeError() << "\n";
      return 1;
    }
    CreateDiff(file_name_result->string(),
               std::string_view(buffer_before->getBuffer()), buffer_after,
               *out_stream);
  }

  return 0;
}

}  // namespace modernizer
