#include "modernizer/modernizer.h"

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Edit/Commit.h"
#include "clang/Edit/EditedSource.h"
#include "clang/Edit/EditsReceiver.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/AllTUsExecution.h"
#include "clang/Tooling/JSONCompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Refactoring/AtomicChange.h"
#include "clang/Tooling/StandaloneExecution.h"
#include "modernizer/diff.h"
#include "modernizer/filesystem.h"
#include "modernizer/mutex_lock.h"
#include "modernizer/refactoring.h"
#include "re2/re2.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace modernizer {
namespace {

constexpr std::string_view kModernizeMacroRegex =
    "(RTC_DISALLOW_COPY_AND_ASSIGN\\(\\s*(\\S(|.*\\S))\\s*\\);)";

class LOCKABLE ReplacementsContext {
 public:
  void lock() EXCLUSIVE_LOCK_FUNCTION() { mutex_.Lock(); }

  void unlock() UNLOCK_FUNCTION() { mutex_.Unlock(); }

  std::map<std::string, Replacements>& GetReplacements()
      EXCLUSIVE_LOCKS_REQUIRED(this) {
    return impl_;
  }

 private:
  mutable absl::Mutex mutex_;
  std::map<std::string, Replacements> impl_ GUARDED_BY(mutex_);
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

  const CXXRecordDecl* record_decl_;
  int depth_ = 0;
  std::vector<Decl*> inner_decls_;
};

class ModernizerCallback : public MatchFinder::MatchCallback {
 public:
  explicit ModernizerCallback(const std::filesystem::path& root_path,
                              const std::filesystem::path& build_path,
                              ReplacementsContext* replacements_context)
      : root_path_(root_path),
        build_path_(build_path),
        replacements_context_(replacements_context) {
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
    auto rel_file_path = Relative(file_path, build_path_);
    if (!rel_file_path) {
      llvm::errs() << "filesystem::relative failed: "
                   << rel_file_path.takeError() << "\n";
      return;
    }
    const CXXRecordDecl* class_decl = decl->getParent();
    assert(class_decl);
    AccessSpecifier class_access_specifier =
        (class_decl->getTagKind() == clang::TTK_Class)
            ? AccessSpecifier::AS_private
            : AccessSpecifier::AS_public;
    ClassMemberFunctionVisitor visitor(class_decl);
    visitor.Visit();
    const auto& inner_decls = visitor.GetInnerDecls();

    CXXMethodDecl* next_decl = nullptr;
    for (int i = 0; i < static_cast<int>(inner_decls.size()); ++i) {
      if (inner_decls[i] == decl && i < static_cast<int>(inner_decls.size())) {
        const Decl* inner_next_decl = inner_decls[i + 1];
        if (llvm::isa<CXXMethodDecl>(inner_next_decl)) {
          next_decl = static_cast<CXXMethodDecl*>(inner_decls[i + 1]);
          break;
        }
      }
    }
    if (next_decl == nullptr) {
      return;
    }
    std::optional<std::pair<SourceRange, std::string>> macro_source_range_name =
        FindMacro(decl, kModernizeMacroRegex, sm, lang_opts);
    if (!macro_source_range_name) {
      return;
    }
    auto [macro_source_range, class_name] = macro_source_range_name.value();

    std::optional<SourceLocation> insertable_loc = FindInsertableLocation(
        class_access_specifier, inner_decls, sm, lang_opts);
    if (!insertable_loc) {
      return;
    }

    std::vector<AtomicChange> changes;
    {
      AtomicChange change(sm, macro_source_range.getBegin());
      llvm::Error result =
          change.replace(sm, CharSourceRange(macro_source_range, true), "");
      assert(!result);
      changes.push_back(std::move(change));
    }
    {
      SourceLocation insert_offset_loc = insertable_loc->getLocWithOffset(1);
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
      changes.push_back(std::move(change));
    }

    std::string rel_file_path_str = rel_file_path->string();
    MutexLock guard(*replacements_context_);
    std::map<std::string, Replacements>& replacements =
        replacements_context_->GetReplacements();
    for (const auto& change : changes) {
      auto iter = replacements.find(rel_file_path_str);
      if (iter == replacements.end()) {
        replacements.insert(
            std::make_pair(rel_file_path_str, change.getReplacements()));
      } else {
        Replacements merged = iter->second.merge(change.getReplacements());
        replacements[rel_file_path_str] = merged;
      }
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
    for (int i = 0; i < 6; ++i) {
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
        return std::make_pair(source_range, match1.as_string());
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

  static std::optional<SourceLocation> FindInsertableLocation(
      AccessSpecifier class_access_specifier,
      const std::vector<Decl*>& all_decls,
      const SourceManager& sm,
      const LangOptions& lang_opts) {
    AccessSpecifier as = class_access_specifier;
    Decl* candidate_decl = nullptr;
    for (Decl* decl : all_decls) {
      if (llvm::isa<AccessSpecDecl>(decl)) {
        as = static_cast<AccessSpecDecl*>(decl)->getAccess();
        assert(as != clang::AS_none);
        continue;
      }
      if (llvm::isa<CXXDestructorDecl>(decl)) {
        if (as == clang::AS_public && !decl->isImplicit()) {
          candidate_decl = decl;
          break;
        }
      }
    }

    if (candidate_decl == nullptr) {
      return std::nullopt;
    }

    SourceLocation candidate_loc = candidate_decl->getLocation();
    FileID candidate_loc_fileid = sm.getFileID(candidate_loc);
    if (candidate_loc_fileid.isInvalid()) {
      return std::nullopt;
    }
    std::optional<SourceLocation> next_semi_loc =
        FindNextSemi(candidate_loc, sm, lang_opts);
    if (!next_semi_loc) {
      return std::nullopt;
    }

    if (candidate_loc_fileid != sm.getFileID(*next_semi_loc)) {
      return std::nullopt;
    }
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

  const std::filesystem::path root_path_;
  const std::filesystem::path build_path_;
  ReplacementsContext* replacements_context_;
};

class NullDiagnosticConsumer : public DiagnosticConsumer {
 public:
  ~NullDiagnosticConsumer() override = default;

  bool IncludeInDiagnosticCounts() const override { return false; }
  void HandleDiagnostic(DiagnosticsEngine::Level diag_level,
                        const Diagnostic& info) override {
    if (diag_level >= DiagnosticsEngine::Error) {
      num_hidden_errors_++;
    }
  }

  int GetNumHiddenErrors() const { return num_hidden_errors_; }

 private:
  int num_hidden_errors_ = 0;
};

}  // namespace

int RunModernizer(const RunModernizerOptions& options) {
  const auto& project_root = options.project_root;
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
  if (!in_place && !out_stream) {
    llvm::errs() << "Output stream is not set.\n";
    return 1;
  }

  std::string error_message;
  auto compilation_database = JSONCompilationDatabase::loadFromFile(
      compile_commands.string(), error_message, JSONCommandLineSyntax::Gnu);
  if (!compilation_database) {
    llvm::errs() << "Parsing compile_commands.json failed: " << error_message
                 << "\n";
    return 1;
  }

  std::filesystem::path build_root;
  std::vector<std::string> source_paths;
  for (const auto& compile_command :
       compilation_database->getAllCompileCommands()) {
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
      auto file_path_result = Canonical(std::filesystem::path(
          compile_command.Directory /
          std::filesystem::path(compile_command.Filename)));
      if (!file_path_result) {
        llvm::errs() << "filesystem::canonical returned error: "
                     << file_path_result.takeError() << "\n";
        return 1;
      }
      file_path = *file_path_result;
    }
    source_paths.push_back(file_path.string());
  }

  ReplacementsContext replacements_context;
  // TODO(bc-lee): Apply AllTUsToolExecutor
  StandaloneToolExecutor executor(*compilation_database.get(), source_paths);

  NullDiagnosticConsumer null_diagnostic_consumer;
  executor.setDiagnosticConsumer(&null_diagnostic_consumer);

  ArgumentsAdjuster arguments_adjuster =
      combineAdjusters(getClangStripDependencyFileAdjuster(),
                       combineAdjusters(getClangSyntaxOnlyAdjuster(),
                                        getClangStripOutputAdjuster()));

  MatchFinder finder;
  ModernizerCallback callback(project_root, build_root, &replacements_context);

  finder.addMatcher(
      namedDecl(cxxConstructorDecl(), isExpandedFromMacro(kModernizeMacro))
          .bind("decl"),
      &callback);

  llvm::Error error = executor.execute(newFrontendActionFactory(&finder));
  llvm::errs() << "Number of errors in diagnostic: "
               << null_diagnostic_consumer.GetNumHiddenErrors() << "\n";
  if (error) {
    llvm::errs() << "Execute error: " << toString(std::move(error)) << "\n";
    return 1;
  }

  // boilerplate
  LangOptions default_lang_options;
  llvm::IntrusiveRefCntPtr<DiagnosticOptions> diag_opts =
      new DiagnosticOptions();
  DiagnosticsEngine diagnostics(
      IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()), &*diag_opts,
      &null_diagnostic_consumer, false);
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> file_system(
      llvm::vfs::createPhysicalFileSystem().release());
  file_system->setCurrentWorkingDirectory(build_root.string());
  llvm::IntrusiveRefCntPtr<FileManager> files =
      llvm::makeIntrusiveRefCnt<FileManager>(FileSystemOptions(), file_system);

  SourceManager sm(diagnostics, *files);
  Rewriter rewrite(sm, default_lang_options);
  {
    MutexLock guard(replacements_context);
    if (!FormatAndApplyAllReplacements(replacements_context.GetReplacements(),
                                       rewrite)) {
      llvm::errs() << "Apply Replacements failed\n";
      return 1;
    }
  }
  if (in_place) {
    if (!rewrite.overwriteChangedFiles()) {
      llvm::errs() << "write to file failed\n";
      return 1;
    }
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
