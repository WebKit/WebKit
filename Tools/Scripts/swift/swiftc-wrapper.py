#!/usr/bin/env python3
# Thin wrapper around swiftc that:
#   - filters one benign diagnostic out of stderr
#   - relays the command line through a response file on Windows
#   - forwards the MSVC-style linker switches that CMake's own defaults inject
#
# Flags that swiftc cannot accept are kept off the Swift command line by the
# CMake configuration, using $<COMPILE_LANGUAGE:Swift> / $<LINK_LANGUAGE:Swift>
# genexes and the LINKER: prefix (which CMake expands to -Xlinker for swiftc
# and -Wl, for clang). See WEBKIT_SCOPE_OPTIONS_TO_NON_SWIFT and
# webkit_target_compile_definitions in Source/cmake/WebKitMacros.cmake.
#
# Invoked with a --original-swift-compiler= flag mixed in that refers to the
# real compiler. CMake does not appear to call this script when set as a
# CMAKE_Swift_COMPILER_LAUNCHER.

import os
import re
import subprocess
import sys
import tempfile

# Swift's C++ interop changes which imported members are @unsafe between
# toolchain versions, so an `unsafe` that is required on one toolchain emits
# "no unsafe operations occur within 'unsafe' expression" on another. The
# diagnostic is in the UnnecessaryUnsafe group, but Swift's group flags only
# escalate severity (-Werror/-Wwarning <group>) -- there is no per-group
# suppression, and -suppress-warnings would hide everything -- so filter this
# one diagnostic (and its multi-line source snippet) out of stderr instead.
#
# FIXME: This is the only reason the wrapper exists on non-Windows hosts. Once
# the toolchain range WebKit builds against agrees on which imported members are
# @unsafe, or Swift gains per-group warning suppression, drop this.
_BENIGN_WARNING = re.compile(r": warning: no unsafe operations occur within .unsafe. expression")
_SOURCE_SNIPPET = re.compile(r"^[ \t]*[0-9]*[ \t]*\|")


def filter_benign_warnings(lines):
    skip = False
    for line in lines:
        if _BENIGN_WARNING.search(line):
            skip = True
            continue
        if skip and _SOURCE_SNIPPET.match(line):
            continue
        if skip and line.strip() == "":
            skip = False
            continue
        skip = False
        yield line


def quote_response_file_token(arg):
    if not re.search(r"\s", arg):
        return arg
    # Windows argv-unescaping rules (the platform this is used on):
    # a backslash is literal unless it precedes a quote, and N backslashes
    # before a quote collapse to N/2.
    escaped = arg.replace("\\", "\\\\").replace('"', '\\"')
    return '"' + escaped + '"'


def write_response_file(args):
    fd, path = tempfile.mkstemp(prefix="swiftc-wrapper-", suffix=".rsp")
    with os.fdopen(fd, "w") as f:
        for arg in args:
            f.write(quote_response_file_token(arg))
            f.write("\n")
    return path


def main(argv):
    real_swiftc = "swiftc"
    args = []
    for arg in argv:
        if arg.startswith("--original-swift-compiler="):
            real_swiftc = arg[len("--original-swift-compiler="):]
        elif os.name == "nt" and arg.startswith("/") and len(arg) > 1:
            # Work around a bug in CMake: Its MSVC defaults
            # (Platform/Windows-MSVC.cmake) put raw linker switches like
            # /machine:x64 and /INCREMENTAL:NO into CMAKE_*_LINKER_FLAGS.
            args.extend(["-Xlinker", arg])
        else:
            args.append(arg)

    flat_command = [real_swiftc] + args

    if os.name == "nt" and "-explicit-module-build" not in args:
        # WebKit's swiftc invocations run tens of thousands of characters long
        # (hundreds of -I flags); Windows' CreateProcess caps a command line at
        # ~32767 characters
        response_file = write_response_file(args)
        command = [real_swiftc, "@" + response_file]
    else:
        if os.name == "nt":
            cmdline = subprocess.list2cmdline(flat_command)
            if len(cmdline) >= 32767:
                sys.stderr.write(
                    f"swiftc-wrapper: command line is {len(cmdline)} characters, "
                    "over the Windows 32767 limit, and -explicit-module-build "
                    "prevents relaying it through a response file\n")
                return 1
        command = flat_command
        response_file = None

    try:
        # Relay stderr line by line while the compiler runs.
        with subprocess.Popen(
            command,
            stdout=sys.stdout,
            stderr=subprocess.PIPE,
            encoding="utf-8",
            errors="replace",
        ) as process:
            for line in filter_benign_warnings(process.stderr):
                sys.stderr.write(line)
                sys.stderr.flush()
            return process.wait()
    finally:
        if response_file:
            os.remove(response_file)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
