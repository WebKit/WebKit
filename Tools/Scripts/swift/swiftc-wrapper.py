#!/usr/bin/env python3
# Thin wrapper around swiftc that:
#   - filters one benign diagnostic out of stderr
#   - relays the command line through a response file on Windows
#   - forwards the MSVC-style linker switches that CMake's own defaults inject
#   - merges per-frontend depfiles into one depfile for the whole module
#   - records when the module last compiled, for rebuild_trigger.py
#   - optionally runs another wrapper in the compiler's place, so a tool that
#     needs to spawn swiftc itself can be nested below this one
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

import json
import os
import re
import subprocess
import sys
import tempfile
from collections import namedtuple
from pathlib import Path

import depfile

# Swift's C++ interop changes which imported members are @unsafe between
# toolchain versions, so an `unsafe` that is required on one toolchain emits
# "no unsafe operations occur within 'unsafe' expression" on another. The
# diagnostic is in the UnnecessaryUnsafe group, but Swift's group flags only
# escalate severity (-Werror/-Wwarning <group>) -- there is no per-group
# suppression, and -suppress-warnings would hide everything -- so filter this
# one diagnostic (and its multi-line source snippet) out of stderr instead.
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


# Where to write the merged depfile, which output to key it on, and which
# dependencies to drop. Populated from the --*ninja-depfile* flags that
# WebKitMacros.cmake passes; absent when the caller didn't ask for a depfile.
DepfileRequest = namedtuple("DepfileRequest", "path target excludes")


def _canonical(path):
    """A spelling-insensitive key for paths that name the same file."""
    return os.path.normcase(os.path.normpath(path)).replace("\\", "/")


def excluded_paths(request):
    """Canonical keys of everything that must not appear in the depfile."""
    excludes = {_canonical(exclude): exclude for exclude in request.excludes}
    # A depfile may never list its own target or itself, whatever the caller
    # asked to exclude.
    excludes.setdefault(_canonical(request.target), request.target)
    excludes.setdefault(_canonical(request.path), request.path)
    return excludes


def dependency_files(output_file_map):
    """The depfiles swiftc wrote for this compile, per its output-file-map."""
    try:
        with open(output_file_map) as f:
            entries = json.load(f)
    except (OSError, ValueError):
        return []

    paths = [entry["dependencies"] for entry in entries.values() if "dependencies" in entry]
    # The driver derives the emit-module job's depfile name instead of taking it
    # from the output-file-map, and that one has the fullest view of the
    # module's imports, so pick it up from the map's own directory.
    paths += (str(path) for path in Path(output_file_map).parent.glob("*.emit-module.d"))
    return [path for path in paths if os.path.exists(path)]


def write_ninja_depfile(request, output_file_map):
    sources = dependency_files(output_file_map) if output_file_map else []
    if not sources:
        sys.exit(
            f"{Path(__file__).name}: error: no dependency files named by "
            f"{output_file_map or '-output-file-map'}; "
            "can't track Swift header dependencies"
        )

    excludes = excluded_paths(request)
    # Sorted: swiftc reports a dependency once per frontend job that saw it, so
    # discovery order follows job scheduling and would rewrite this file, and
    # cost a build, for a dependency set that did not change.
    deps = sorted({
        dep
        for source in sources
        for dep in depfile.parse(source)
        if _canonical(dep) not in excludes
    })

    lines = [f"{depfile.escape(request.target)}:"]
    lines += (f"  {depfile.escape(dep)}" for dep in deps)
    content = " \\\n".join(lines) + "\n"

    try:
        # Only rewrite the depfile when it changes, otherwise the rebuild
        # trigger causes the module to rebuild forever.
        if Path(request.path).read_text() == content:
            return
    except OSError:
        pass

    scratch = request.path + ".tmp"
    Path(scratch).write_text(content)
    os.replace(scratch, request.path)


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
    linking = any(arg in ("-emit-library", "-emit-executable") for arg in argv)

    depfile_path = None
    depfile_target = None
    depfile_excludes = []
    stamp_path = None
    inner_wrapper = None
    for arg in argv:
        if arg.startswith("--original-swift-compiler="):
            real_swiftc = arg[len("--original-swift-compiler="):]
        elif arg.startswith("--swift-wrapper="):
            inner_wrapper = arg[len("--swift-wrapper="):]
        elif arg.startswith("--emit-ninja-depfile="):
            depfile_path = arg[len("--emit-ninja-depfile="):]
        elif arg.startswith("--ninja-depfile-target="):
            depfile_target = arg[len("--ninja-depfile-target="):]
        elif arg.startswith("--ninja-depfile-exclude="):
            depfile_excludes.append(arg[len("--ninja-depfile-exclude="):])
        elif arg.startswith("--emit-compile-stamp="):
            stamp_path = arg[len("--emit-compile-stamp="):]
        elif os.name == "nt" and arg.startswith("/") and len(arg) > 1:
            # Work around a bug in CMake: Its MSVC defaults
            # (Platform/Windows-MSVC.cmake) put raw linker switches like
            # /machine:x64 and /INCREMENTAL:NO into CMAKE_*_LINKER_FLAGS.
            args.extend(["-Xlinker", arg])
        else:
            args.append(arg)

    depfile_request = None
    if depfile_path and depfile_target and not linking:
        depfile_request = DepfileRequest(depfile_path, depfile_target, frozenset(depfile_excludes))

    if inner_wrapper:
        flat_command = [inner_wrapper, f"--original-swift-compiler={real_swiftc}"] + args
    else:
        flat_command = [real_swiftc] + args

    if os.name == "nt" and "-explicit-module-build" not in args and not inner_wrapper:
        # WebKit's swiftc invocations run tens of thousands of characters long
        # (hundreds of -I flags); Windows' CreateProcess caps a command line at
        # ~32767 characters. An inner wrapper is excluded because it would have
        # to expand the response file itself.
        response_file = write_response_file(args)
        command = [real_swiftc, "@" + response_file]
    else:
        if os.name == "nt":
            cmdline = subprocess.list2cmdline(flat_command)
            if len(cmdline) >= 32767:
                sys.stderr.write(
                    f"swiftc-wrapper: command line is {len(cmdline)} characters, "
                    "over the Windows 32767 limit, and a response file cannot be "
                    "used here\n")
                return 1
        command = flat_command
        response_file = None

    rc = None
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
            rc = process.wait()
            return rc
    finally:
        if response_file:
            os.remove(response_file)
        if depfile_request and rc == 0:
            write_ninja_depfile(depfile_request, args[args.index('-output-file-map') + 1])
        if stamp_path and rc == 0 and not linking:
            Path(stamp_path).touch()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
