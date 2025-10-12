# Copyright (C) 2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import annotations

import argparse
import os
import pkgutil
import shlex
import sys
import time
from pathlib import Path
from typing import Optional
from types import ModuleType

import webkitapipy
from webkitapipy.sdkdb import SDKDB
from webkitapipy.macho import APIReport
from webkitapipy.reporter import configure_reporter

# Some symbols, namely ones that are low-level parts of system libraries and
# runtimes, are implicitly available.
ALLOWED_SYMBOLS = {
    '_OBJC_METACLASS_$_NSObject',
    '_OBJC_EHTYPE_$_NSException',
    # Foundation APIs
    '_OBJC_CLASS_$_NSConstantArray',
    '_OBJC_CLASS_$_NSConstantDictionary',
    '_OBJC_CLASS_$_NSConstantDoubleNumber',
    '_OBJC_CLASS_$_NSConstantIntegerNumber',
    '___CFConstantStringClassReference',
    '___NSArray0__',
    '___NSDictionary0__',
    # rdar://79462292
    '___kCFBooleanFalse',
    '___kCFBooleanTrue',
    '___NSArray0__struct',
    '___NSDictionary0__struct',
    # C++ std
    '__ZdlPv',
    '__Znwm',
}

ALLOWED_SYMBOL_GLOBS = (
    # C++ std
    '__ZS*',
    '__ZT*',
    '__ZNS*',
    '__ZNK*',
    '__ZN9__gnu_cxx*',
    # We remove SwiftUI from SDK DBs due to rdar://143449950.
    '_$s*7SwiftUI*',
    # rdar://79109142
    '__swift_FORCE_LOAD_$_*',
)

# TBDs from the active SDK whose symbols are treated as implicitly available.
# Pattern strings on the right-hand side select individual libraries from the
# TBD.
SDK_ALLOWLIST = {
    'usr/lib/libobjc*.tbd': (),
    'usr/lib/swift/lib*.tbd': (),
    'usr/lib/libc++*.tbd': (),
    'usr/lib/libSystem.B.tbd': ('/usr/lib/system/libsystem_*',
                                '/usr/lib/system/libcompiler_rt*',
                                '/usr/lib/system/libunwind*'),
    'usr/lib/libicucore.A.tbd': (),
    # rdar://149428625
    'usr/lib/libxslt.1.tbd': (),
}

# In addition to the main directory of partial SDKDBs passed via `--sdkdb-dir`,
# this path will be appended to framework search paths to find partial SDKDBs
# that correspond to frameworks added via `-framework`.
FRAMEWORK_SDKDB_DIR = 'SDKDB'

def get_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='''\
    Using API availability information from a directory of SDKDB records,
    scans Mach-O binaries for use of unknown symbols or Objective-C selectors.
    ''', fromfile_prefix_chars='@', epilog='''\
    Additional arguments can be loaded from a file by prefixing it with the @
    sign.
    ''')
    parser.add_argument('input_files', nargs='+', type=Path,
                        help='files to analyze')
    parser.add_argument('-a', '--arch-name', required=True,
                        help='which architecture to analyze binary with')
    parser.add_argument('--allowlist', action='append', type=Path,
                        dest='allowlists',
                        help='config files listing additional allowed SPI')
    parser.add_argument('-D', action='append', dest='defines',
                        help='use this compiler flag for purposes of '
                        'evaluating `requires` blocks in allowlists')

    binaries = parser.add_argument_group('framework and library dependencies',
                                         description='''ld-style arguments to
                                         support finding and using declarations
                                         from arbitrary binaries, on top of the
                                         SDKDB_DIR.''')

    binaries.add_argument('-framework', metavar='FRAMEWORK', type=str,
                          action='append', dest='frameworks',
                          help='allow arbitrary use of this framework')
    binaries.add_argument('-l', metavar='LIBRARY', type=Path, action='append',
                          dest='libraries',
                          help='allow arbitrary use of this dynamic library')
    binaries.add_argument('-F', metavar='PATH', type=Path, action='append',
                          dest='framework_search_paths',
                          help='add to the frameworks search path')
    binaries.add_argument('-L', metavar='PATH', type=Path, action='append',
                          dest='library_search_paths',
                          help='add to the libraries search path')

    parser.add_argument('--sdkdb-dir', type=Path, required=True,
                        help='directory of partial SDKDB records for an SDK')
    parser.add_argument('--sdkdb-cache', type=Path, required=True,
                        help='database file to store SDKDB availabilities')
    parser.add_argument('--sdk-dir', type=Path, required=True,
                        help='Xcode SDK the binary is built against')
    parser.add_argument('--depfile', type=Path,
                        help='write inputs used for incremental rebuilds')

    output = parser.add_argument_group('output formatting')
    output.add_argument('--format', choices=('tsv', 'build-tool'), default='build-tool',
                        help='how to style output messages (default: build-tool)')
    output.add_argument('--details', action='store_true',
                        help=argparse.SUPPRESS)
    output.add_argument('--errors',
                        action=argparse.BooleanOptionalAction, default=True,
                        help='whether to report SPI use as an error')
    return parser


class Options(argparse.Namespace):
    input_files: list[Path]
    arch_name: str
    allowlists: Optional[list[Path]]
    defines: Optional[list[str]]

    frameworks: list[str]
    libraries: list[str]
    framework_search_paths: list[Path]
    library_search_paths: list[Path]

    sdkdb_dir: Path
    sdkdb_cache: Path
    sdk_dir: Path
    depfile: Optional[Path]

    format: str
    details: bool
    errors: bool


def main(argv: Optional[list[str]] = None):
    webkitapipy_additions: Optional[ModuleType]
    program_additions: Optional[ModuleType]
    try:
        import webkitapipy_additions
        from webkitapipy_additions import program as program_additions
    except ImportError:
        webkitapipy_additions = None
        program_additions = None

    if program_additions:
        parser = program_additions.get_parser()
        args = parser.parse_args(argv, namespace=program_additions.Options)
    else:
        parser = get_parser()
        args = parser.parse_args(argv, namespace=Options)

    inputs = []

    # Response files loaded via ArgumentParser are transparently available in
    # options, but should be tracked as input dependencies.
    for arg in (argv if argv is not None else sys.argv):
        if arg.startswith('@'):
            inputs.append(arg.removeprefix('@'))

    # For the depfile, start with the paths of all the modules in webkitapipy
    # since this library is part of WebKit source code.
    for package in (webkitapipy, webkitapipy_additions):
        if not package:
            continue
        for info in pkgutil.walk_packages(package.__path__):
            spec = info.module_finder.find_spec(info.name, None)
            if spec and spec.origin:
                inputs.append(spec.origin)

    def use_input(path):
        inputs.append(path)
        return path

    # Do not `use_input` on the sdkdb cache, because we may write to it, and
    # that would invalidate audit-spi invocations in other projects.
    db = SDKDB(args.sdkdb_cache)

    # Initializing the SDKDB cache from scratch takes some time (~ 15-20 sec).
    # Print progress updates and measure execution time to indicate how much
    # build time the cache will save.
    n_changes = 0

    def increment_changes():
        nonlocal n_changes
        if n_changes == 0:
            print(f'Building SDKDB cache from {args.sdkdb_dir}...')
        n_changes += 1
        if n_changes % 10 == 0:
            print(f'{n_changes} projects...')

    db_initialization_start = time.monotonic()
    with db:
        for file in args.sdkdb_dir.iterdir():
            if file.suffix != '.sdkdb':
                continue
            if db.add_partial_sdkdb(use_input(file)):
                increment_changes()
        for file_pattern, library_patterns in SDK_ALLOWLIST.items():
            for tbd_path in args.sdk_dir.glob(file_pattern):
                if tbd_path.is_symlink():
                    # Ignore symlinks (for example, libswift*.dylib libraries
                    # that have been merged into other frameworks).
                    continue
                if db.add_tbd(use_input(tbd_path),
                              only_including=library_patterns):
                    increment_changes()

    if n_changes:
        symbols, classes, selectors = db.stats()
        db_initialization_duration = time.monotonic() - db_initialization_start
        print(f'Done. Took {db_initialization_duration:.2f} sec.',
              f'{symbols=} {classes=} {selectors=}')

    def add_corresponding_sdkdb(binary: Path) -> None:
        # There is no platform convention for where to put partial
        # SDKDBs in build products, so match what WebKit.xcconfig
        # does and look for a "SDKDB" directory.
        for search_path in args.framework_search_paths or ():
            sdkdb_path = (search_path / FRAMEWORK_SDKDB_DIR /
                          f'{binary.name}.partial.sdkdb')
            if sdkdb_path.exists():
                # Work around rdar://153937150: Instead of adding a dependency
                # on the sdkdb path, emit a dependency on the framework's .tbd
                # if it exists.
                db.add_partial_sdkdb(sdkdb_path, spi=True,
                                     abi=True)
                tbd_path = binary.with_suffix('.tbd')
                if tbd_path.exists():
                    use_input(tbd_path)
                break

    for name in args.frameworks or ():
        with db:
            for search_path in args.framework_search_paths or ():
                binary_path = search_path / f'{name}.framework/{name}'
                if binary_path.exists():
                    db.add_binary(use_input(binary_path), arch=args.arch_name)
                    add_corresponding_sdkdb(binary_path)
                    break
            else:
                sys.exit(f'error: Could not find "{name}.framework/{name}" '
                         'in search paths')

    for name in args.libraries or ():
        with db:
            for search_path in args.library_search_paths or ():
                path = search_path / f'lib{name}.dylib'
                if path.exists():
                    db.add_binary(use_input(path), arch=args.arch_name)
                    break
            else:
                sys.exit(f'error: Could not find "lib{name}.dylib" in search '
                         'paths')

    for path in args.allowlists or ():
        with db:
            db.add_allowlist(use_input(path))
    if args.defines:
        db.add_defines(args.defines)

    if program_additions:
        reporter = program_additions.configure_reporter(args, db)
    else:
        reporter = configure_reporter(args, db)

    for binary_path in args.input_files:
        add_corresponding_sdkdb(binary_path)
        report = APIReport.from_binary(use_input(binary_path), arch=args.arch_name)
        db.add_for_auditing(report)
    for diagnostic in db.audit():
        reporter.emit_diagnostic(diagnostic)

    reporter.finished()

    if args.depfile:
        with open(args.depfile, 'w') as fd:
            fd.write('dependencies: ')
            fd.write(' \\\n  '.join(shlex.quote(os.path.abspath(path))
                                    for path in inputs))
            fd.write('\n')

    if args.errors and reporter.has_errors():
        sys.exit(1)
