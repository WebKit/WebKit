#!/usr/bin/env python3
# cmake accumulates CFLAGS from pkg-config, and then passes them to swiftc.
# This script filters out the arguments that swiftc cannot accommodate.
#
# CMAKE_Swift_COMPILER is set to this script directly (on Windows, to
# python.exe with this script as CMAKE_Swift_COMPILER_ARG1), and the real
# compiler arrives via a --original-swift-compiler= flag mixed in with the
# rest of the args -- see CMakeLists.txt for why (not
# CMAKE_Swift_COMPILER_LAUNCHER, which testing showed CMake/Ninja can
# silently fail to apply to some Swift rules).

import re
import subprocess
import sys

# Swift's C++ interop changes which imported members are @unsafe between
# toolchain versions, so an `unsafe` that is required on one toolchain emits
# "no unsafe operations occur within 'unsafe' expression" on another. The
# diagnostic has no group, so it can't be suppressed with -Wwarning; filter it
# (and its multi-line source snippet) from stderr instead.
_BENIGN_WARNING = re.compile(r": warning: no unsafe operations occur within .unsafe. expression")
_SOURCE_SNIPPET = re.compile(r"^[ \t]*[0-9]*[ \t]*\|")


def filter_benign_warnings(lines):
    skip = False
    out = []
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
        out.append(line)
    return out


def expand_response_file(path):
    lines = []
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            lines.append(line)
    return lines


def process_args(argv):
    real_swiftc = "swiftc"
    args = []

    linking = any(arg in ("-emit-library", "-emit-executable") for arg in argv)

    pass_next_verbatim = False
    skip_next = False
    skip_next_as_xlinker = False

    for arg in argv:
        if arg.startswith("@") and arg.endswith(".platform-swift-args.resp"):
            resp_path = arg[1:]
            try:
                # Expand our resp in-process: the swift driver doesn't expand
                # @-files under -explicit-module-build, and emitting tokens
                # directly bypasses the case-statement's -D doubling, which
                # would otherwise leak Platform.h-derived defines into the
                # clang importer. Other @-files (CMake's link/compile rsp)
                # pass through; swiftc expands them.
                args.extend(expand_response_file(resp_path))
                continue
            except OSError:
                pass

        if pass_next_verbatim:
            args.append(arg)
            pass_next_verbatim = False
            continue

        if arg in ("-Xcc", "-Xlinker", "-Xfrontend"):
            args.append(arg)
            pass_next_verbatim = True
        elif arg in ("-mfpmath=sse", "-msse", "-msse2", "-pthread"):
            pass
        elif arg.startswith("-fsanitize="):
            args.append("-sanitize=" + arg[len("-fsanitize="):])
        elif arg == "-g":
            if not linking:
                args.append(arg)
        elif arg == "-include":
            skip_next = True
        elif arg == "-flto" or arg.startswith("-flto="):
            args.extend(["-Xcc", arg])
        elif arg.startswith("-fuse-ld="):
            args.extend(["-Xcc", arg])
        elif arg in ("-isystem", "-iquote", "-idirafter", "-isysroot"):
            # swiftc does not understand clang-specific include flags like
            # -isystem / -iquote / -idirafter; wrap them (and their following
            # path argument) as -Xcc so they reach the Clang importer instead
            # of being rejected at parse time.
            args.extend(["-Xcc", arg, "-Xcc"])
            pass_next_verbatim = True
        elif arg in ("-compatibility_version", "-current_version"):
            # CMake leaks clang linker flags into swiftc; translate them.
            args.extend(["-Xlinker", arg])
            skip_next_as_xlinker = True
        elif arg == "-weak_framework":
            args.extend(["-Xlinker", "-weak_framework"])
            skip_next_as_xlinker = True
        elif arg.startswith("-Wl,"):
            # Split -Wl,arg1,arg2 into -Xlinker arg1 -Xlinker arg2
            for wl_arg in arg[len("-Wl,"):].split(","):
                args.extend(["-Xlinker", wl_arg])
        elif arg.startswith("--original-swift-compiler="):
            real_swiftc = arg[len("--original-swift-compiler="):]
        elif arg.startswith("-D") and "=" in arg:
            # Swift conditional-compilation flags are valueless; the
            # importer's -D set comes from
            # _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS, so valued
            # target_compile_definitions are dropped here.
            pass
        else:
            if skip_next:
                skip_next = False
            elif skip_next_as_xlinker:
                args.extend(["-Xlinker", arg])
                skip_next_as_xlinker = False
            else:
                args.append(arg)

    return real_swiftc, args


def main(argv):
    real_swiftc, args = process_args(argv)

    process = subprocess.run(
        [real_swiftc] + args,
        stdout=sys.stdout,
        stderr=subprocess.PIPE,
    )

    stderr_text = process.stderr.decode(errors="replace")
    filtered_lines = filter_benign_warnings(stderr_text.splitlines(keepends=True))
    sys.stderr.write("".join(filtered_lines))
    sys.stderr.flush()

    return process.returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
