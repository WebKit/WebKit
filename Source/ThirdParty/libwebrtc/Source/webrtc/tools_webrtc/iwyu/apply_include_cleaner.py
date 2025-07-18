#!/usr/bin/env vpython3

# Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
#
# Run the include-cleaner tool (iwyu replacement) on a file in the webrtc
# source directory.
#
#
# In order to handle include paths correctly, you need to provide
# a compile DB (aka compile_commands.json).
# You can create it in one of the following ways:
# - "gn gen --export-compile-commands path/to/out"
# - "tools/clang/scripts/generate_compdb.py -p path/to/out
#       > compile_commands.json"
# If "out/Default" exists, the script will attempt to generate it for you.
#
# clang-include-cleaner is built as part of the "clangd" package in our
# LLVM build.
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
#      "download_remoteexec_cfg" : True,
#    }
#  },
# ]

import argparse
import re
import pathlib
import subprocess
import sys
from typing import Tuple

_CLEANER_BINARY_PATH = pathlib.Path(
    "third_party/llvm-build/Release+Asserts/bin/clang-include-cleaner")
_DEFAULT_WORKDIR = pathlib.Path("out/Default")
_EXTRA_ARGS = [
    "-I../../third_party/googletest/src/googlemock/include/",
    "-I../../third_party/googletest/src/googletest/include/",
]
_IWYU_MAPPING = {
    '"gmock/gmock.h"': '"test/gmock.h"',
    '"gtest/gtest.h"': '"test/gtest.h"',
    "<sys/socket.h>": '"rtc_base/net_helpers.h"',
}


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Runs the include-cleaner tool on a list of files",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("files",
                        nargs="+",
                        type=_valid_file,
                        help="List of files to process")
    parser.add_argument(
        "-p",
        "--print",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Don't modify the files, just print the changes",
    )
    parser.add_argument(
        "-c",
        "--check-for-changes",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="""Checks whether include-cleaner generated changes and exit with
1 in case it did. Used for bot validation that the current commit did not
introduce an include regression.""")
    parser.add_argument(
        "-w",
        "--work-dir",
        type=_valid_dir,
        default=str(_DEFAULT_WORKDIR),
        help="Specify the gn workdir",
    )

    return parser.parse_args()


def _valid_file(path: str) -> pathlib.Path:
    """Checks if the given path is an existing file
    relative to the current working directory.

    Args:
        path: Relative file path to the current working directory

    Returns:
        pathlib.Path object wrapping the file path

    Raises:
        ValueError: If the file doesn't exist
    """
    pathlib_handle = pathlib.Path(path)
    if not pathlib_handle.is_file():
        raise ValueError(f"File path {pathlib_handle} does not exist!")
    return pathlib_handle


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


def _generate_compile_commands(work_dir: pathlib.Path) -> None:
    """Automatically generates the compile_commands.json file to be used
    by the include cleaner binary.

    Args:
        work_dir: gn out dir where the compile_commands json file exists
    """
    compile_commands_path = work_dir / "compile_commands.json"
    if not compile_commands_path.is_file():
        print("Generating compile commands file...")
        subprocess.run(
            ["tools/clang/scripts/generate_compdb.py", "-p", work_dir],
            stdout=compile_commands_path.open(mode="w+"),
            check=True,
        )


# Transitioning the cmd type to tuple to prevent modification of
# the original command from the callsite in main...
def _apply_include_cleaner_to_file(file_path: pathlib.Path,
                                   should_modify: bool,
                                   cmd: Tuple[str, ...]) -> bool:
    """Applies the include cleaner binary to a given file.
    Other than that, make sure to do include substitutions following the
    _IWYU_MAPPING variable and clear the tool output from redundant additions
    (those that came from _IWYU_MAPPING and weren't necessary)

    Args:
        file_path: The path to the file to execute include cleaner on
        should_print: whether we'd like to apply the include cleaner changes to
                      the file
        cmd: pre defined include cleaner command with all the relevant
             arguments but the file path

    Returns:
        True if include cleaner provided a substitution that was actually
        required in code (wasn't removed by the _IWYU_MAPPING)
    """
    cmd += (str(file_path), )
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        print(f"Failed to run include cleaner on {file_path}, stderr:",
              f"{result.stderr.strip()}")
    output = result.stdout.strip()

    content = file_path.read_text()
    modified_content = content
    for key, value in _IWYU_MAPPING.items():
        if value in modified_content:
            # If the required include is already in the file, clear it from the
            # cleaner output and remove what the cleaner added to the file
            output = output.replace(f'+ {key.replace("#include ", "")}', "")
            if should_modify:
                modified_content = re.sub(rf"^#include {re.escape(key)}.*\n?",
                                          "",
                                          modified_content,
                                          flags=re.MULTILINE)

        elif should_modify:
            # Otherwise, change what the cleaner added to the correct include
            # from _IWYU_MAPPING
            modified_content = re.sub(rf"^#include {re.escape(key)}",
                                      f"#include {value}",
                                      modified_content,
                                      flags=re.MULTILINE)
    if should_modify and content != modified_content:
        file_path.write_text(modified_content)

    if output:
        print(output)
    else:
        print(f"Successfuly ran include cleaner on {file_path}")
    return bool(output)


def main() -> None:
    if not _CLEANER_BINARY_PATH.exists():
        print(f"clang-include-cleaner not found in {_CLEANER_BINARY_PATH}")
        print(
            "Add '\"checkout_clangd\": True' to 'custom_vars' in your ",
            ".gclient file and run 'gclient sync'.",
        )

    args = _parse_args()

    _generate_compile_commands(args.work_dir)

    # Build the execution command
    cmd = [str(_CLEANER_BINARY_PATH), "-p", str(args.work_dir)]
    for extra_arg in _EXTRA_ARGS:
        cmd.append(f"--extra-arg={extra_arg}")
    if args.print or args.check_for_changes:
        cmd.append("--print=changes")
        should_modify = False
    else:
        cmd.append("--edit")
        should_modify = True

    changes_generated = False
    # TODO(dorhen@meta): Ideally don't iterate on the files
    # and execute cleaner on each, but instead execute the
    # cleaner binary once - passing in all files.
    # e.g instead of `cleaner foo.cc && cleaner bar.cc`
    # do `cleaner foo.cc bar.cc`
    for file in args.files:
        changes_generated = (_apply_include_cleaner_to_file(
            file, should_modify, tuple(cmd)) or changes_generated)

    print("Finished. Check diff, compile, gn gen --check",
          "(tools_webrtc/gn_check_autofix.py can fix most of the issues)")
    print("and git cl format before uploading.")

    if changes_generated and args.check_for_changes:
        sys.exit(1)


if __name__ == "__main__":
    main()
