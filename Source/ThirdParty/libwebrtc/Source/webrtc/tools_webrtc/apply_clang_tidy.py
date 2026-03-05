#!/usr/bin/env vpython3

# Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
#
# Run clang-tidy in the webrtc source directory.
# The list of checks that is getting applied is in the toplevel
# .clang-tidy file. To add a new check, add it to that file and run
# the script.
#
# clang-tidy needs to be added to the .gclient file:
# Example .gclient file:
# solutions = [
#  {
#    "name": "src",
#    "url": "https://webrtc.googlesource.com/src.git",
#    "deps_file": "DEPS",
#    "managed": False,
#    "custom_deps": {},
#    "custom_vars" : {
#      "checkout_clangd": True,
#      "checkout_clang_tidy": True,
#    }
#  },
# ]
#
# See also
# https://chromium.googlesource.com/chromium/src/+/main/docs/clang_tidy.md

import argparse
import time
import pathlib
import subprocess

_DEFAULT_WORKDIR = pathlib.Path("out/Default")

# This is relative to src dir.
_TIDY_BUILD = "tools/clang/scripts/build_clang_tools_extra.py"
# These are relative to the work dir so the path needs to be constructed later.
_LLVM = "tools/clang/third_party/llvm/"
_TIDY_RUNNER = _LLVM + "clang-tools-extra/clang-tidy/tool/run-clang-tidy.py"
_TIDY_BINARY = _LLVM + "build/bin/clang-tidy"
_REPLACEMENTS_BINARY = _LLVM + "build/bin/clang-apply-replacements"

def _valid_dir(path: str) -> pathlib.Path:
    """Checks if the given path is an existing dir
    relative to the current working directory.

    Args:
        path: Relative dir path to the current working directory

    Returns:
        pathlib.Path object wrapping the dir path

    Raises:
        ValueError: If the dir doesn't exist
    """
    pathlib_handle = pathlib.Path(path)
    if not pathlib_handle.is_dir():
        raise ValueError(f"Dir path {pathlib_handle} does not exist!")
    return pathlib_handle


def _build_clang_tools(work_dir: pathlib.Path) -> None:
    if pathlib.Path(work_dir, _TIDY_RUNNER).exists() and pathlib.Path(
            work_dir, _TIDY_BINARY).exists() and pathlib.Path(
                work_dir, _REPLACEMENTS_BINARY).exists():
        # Assume that tidy updates at least once every 30 days, and
        # recompile it if it's more than 30 days old.
        tidy_binary_path = pathlib.Path(work_dir, _TIDY_BINARY)
        age_in_seconds = time.time() - tidy_binary_path.stat().st_mtime
        age_in_days = age_in_seconds / (24 * 60 * 60)
        if age_in_days < 30:
            return
        print("Binary is %d days old - recompiling" % age_in_days)
    print("Fetching and building clang-tidy")
    build_clang_tools_cmd = (_TIDY_BUILD, "--fetch", work_dir, "clang-tidy",
                             "clang-apply-replacements")
    subprocess.run(build_clang_tools_cmd,
                   capture_output=False,
                   text=True,
                   check=True)


def _generate_compile_commands(work_dir: pathlib.Path) -> None:
    """Automatically generates the compile_commands.json file to be used
    by the include cleaner binary.

    Args:
        work_dir: gn out dir where the compile_commands json file exists
    """
    compile_commands_path = work_dir / "compile_commands.json"
    print("Generating compile commands file...")
    subprocess.run(
        ["tools/clang/scripts/generate_compdb.py", "-p", work_dir],
        stdout=compile_commands_path.open(mode="w+"),
        check=True,
    )


def _run_clang_tidy(work_dir: pathlib.Path) -> None:
    clang_tidy_cmd = (work_dir / _TIDY_RUNNER, "-p", work_dir,
                      "-allow-no-checks", "-clang-tidy-binary", work_dir /
                      _TIDY_BINARY, "-clang-apply-replacements-binary",
                      work_dir / _REPLACEMENTS_BINARY, "-fix")
    subprocess.run(clang_tidy_cmd,
                   capture_output=False,
                   text=True,
                   check=False)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Runs clang-tidy with a set of rules",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-w",
        "--work-dir",
        type=_valid_dir,
        default=str(_DEFAULT_WORKDIR),
        help="Specify the gn workdir",
    )

    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    _build_clang_tools(args.work_dir)
    _generate_compile_commands(args.work_dir)
    _run_clang_tidy(args.work_dir)


if __name__ == "__main__":
    main()
