#!/usr/bin/env python3

import argparse
import difflib
import os
import shlex
import shutil
import sys
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).parent.absolute()

THIRD_PARTY_LIBS = [ROOT / "third_party" / "depot_tools"]

for lib_dir in THIRD_PARTY_LIBS:
  if f"{lib_dir}" not in sys.path:
    sys.path.append(f"{lib_dir}")

import subprocess2

TEST_ROOT = ROOT / "test" / "data"
COMPILE_COMMANDS_JSON = TEST_ROOT / "build" / "compile_commands.json"

TEST_FILES = ["input.cc"]


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      "--program", type=str, required=True, help="Modernizer Program")
  parser.add_argument(
      "--keep_temp", action="store_true", help="Keep Temporary files")
  args = parser.parse_args(argv)

  program = Path(args.program).absolute()
  keep_temp = args.keep_temp
  cmd = [
      f"{program}", f"--project_root={TEST_ROOT}",
      f"--compile_commands={COMPILE_COMMANDS_JSON}", "--in_place=false"
  ]

  r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=sys.stderr, check=True)
  patch = r.stdout.decode("UTF-8")
  print(f"patch:\n{patch}")

  td = tempfile.TemporaryDirectory()
  try:
    for test_file in TEST_FILES:
      src = TEST_ROOT.joinpath(test_file)
      dest = Path(td.name).joinpath(test_file)
      dest.parent.mkdir(exist_ok=True)
      shutil.copy(src, dest)

    cmd = ["patch", "-p1", "--verbose"]
    ((stdout, stderr), retcode) = subprocess2.communicate(
        cmd,
        cwd=td.name,
        stdin=patch,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True)
    if retcode != 0:
      raise RuntimeError(
          f"Command '{shlex.join(cmd)}' returned non-zero exit status {retcode}."
          f" stdout:\n{stdout}\nstderr:\n{stderr}")

    for test_file in TEST_FILES:
      expected_name, ext = os.path.splitext(test_file)
      expected = TEST_ROOT.joinpath(expected_name + "-expected" + ext)
      actual = Path(td.name).joinpath(test_file)

      with open(expected, "r") as f:
        expected_contents = f.read()
      with open(actual, "r") as f:
        actual_contents = f.read()

      diff = "".join(
          list(
              x for x in difflib.unified_diff(
                  expected_contents.splitlines(
                      keepends=True), actual_contents.splitlines(
                          keepends=True)))[2:])
      if diff:
        raise RuntimeError(f"Diff found in {test_file}: \n{diff}")

  finally:
    if not keep_temp:
      td.cleanup()


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
