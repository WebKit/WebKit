# Copyright (C) 2026 Apple Inc. All rights reserved.
# Copyright (C) 2026 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Shared pieces of the SaferCPP analysis tools: the analyzer toolchain lookup,
and how a compile_commands.json entry becomes a clang --analyze invocation."""

import glob
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

from webkitpy.common.checkout.scm.git import Git
from webkitpy.common.system.executive import ScriptError
from webkitpy.safer_cpp.checkers import ANALYZER_DISABLED_CATEGORIES, Checker, PROJECTS

SCRIPTS_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))


def find_webkit_root():
    try:
        return os.path.realpath(Git(cwd=SCRIPTS_DIR, patch_directories=None).checkout_root)
    except ScriptError:
        return os.path.dirname(os.path.dirname(SCRIPTS_DIR))


WEBKIT_ROOT = find_webkit_root()
WEBKIT_CHECKERS = Checker.analyzer_names()
DISABLE_CATEGORIES = ','.join(ANALYZER_DISABLED_CATEGORIES)

SOURCE_EXT = {'.cpp', '.cc', '.cxx', '.c', '.mm', '.m'}
HEADER_EXT = {'.h', '.hpp', '.hxx'}

DEFAULT_BUILD_DIR = os.path.join(WEBKIT_ROOT, 'WebKitBuild', 'cmake-mac', 'Debug')
TOOLCHAIN_DIRS = [
    os.path.expanduser('~/Library/Developer/Toolchains'),
    '/Library/Developer/Toolchains',
]
SWIFT_LATEST_URL = 'https://download.swift.org/development/xcode/latest-build.yml'
SWIFT_DOWNLOAD_BASE = 'https://download.swift.org/development/xcode'
LINUX_CLANG_NAMES = ['clang++'] + ['clang++-{}'.format(v) for v in range(30, 17, -1)]
WEBKIT_BUILD_DIRECTORY = os.path.join(SCRIPTS_DIR, 'webkit-build-directory')

# Ordered so that PAL (which lives under WebCore) is matched before WebCore.
PROJECT_ROOTS = [('PAL', os.path.join(WEBKIT_ROOT, 'Source', 'WebCore', 'PAL'))] + [(p, os.path.join(WEBKIT_ROOT, 'Source', p)) for p in PROJECTS if p != 'PAL']

CHECKER_HELP_RE = re.compile(r'^ {2}(\S+)')
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')


def project_for_path(absPath):
    for name, root in PROJECT_ROOTS:
        if absPath.startswith(root + os.sep):
            return name, root
    return None, None


class CompileDB:
    def __init__(self, compileCommandsPath):
        with open(compileCommandsPath) as f:
            entries = json.load(f)
        self.byFile = {os.path.realpath(e['file']): e for e in entries}
        self._bundleIndex = None

    def _buildBundleIndex(self):
        index = {}
        for path, entry in self.byFile.items():
            if '/unified-sources/UnifiedSource' not in path:
                continue
            try:
                with open(path) as f:
                    for line in f:
                        m = INCLUDE_RE.match(line)
                        if m:
                            index.setdefault(m.group(1), entry)
            except OSError:
                pass
        self._bundleIndex = index

    def lookup(self, absSrc):
        absSrc = os.path.realpath(absSrc)
        entry = self.byFile.get(absSrc)
        if entry:
            return entry
        if self._bundleIndex is None:
            self._buildBundleIndex()
        _, root = project_for_path(absSrc)
        if not root:
            return None
        rel = os.path.relpath(absSrc, root).replace(os.sep, '/')
        return self._bundleIndex.get(rel)


def resolve_preset_build_dir(presetName):
    presets = {}
    for name in ('CMakePresets.json', 'CMakeUserPresets.json'):
        path = os.path.join(WEBKIT_ROOT, name)
        if not os.path.isfile(path):
            continue
        with open(path) as f:
            for p in json.load(f).get('configurePresets', []):
                presets[p['name']] = p

    def findBinaryDir(name, seen):
        if name in seen or name not in presets:
            return None
        seen.add(name)
        p = presets[name]
        if 'binaryDir' in p:
            return p['binaryDir']
        parents = p.get('inherits', [])
        if isinstance(parents, str):
            parents = [parents]
        for parent in parents:
            result = findBinaryDir(parent, seen)
            if result:
                return result
        return None

    if presetName not in presets:
        sys.exit("error: unknown preset '{}'".format(presetName))
    binaryDir = findBinaryDir(presetName, set())
    if not binaryDir:
        sys.exit("error: preset '{}' does not define binaryDir".format(presetName))
    binaryDir = binaryDir.replace('${sourceDir}', WEBKIT_ROOT)
    if not os.path.isabs(binaryDir):
        binaryDir = os.path.join(WEBKIT_ROOT, binaryDir)
    return binaryDir


def is_cmake_pch(path):
    return 'cmake_pch' in os.path.basename(path)


# We include prefix header #includes directly instead of including the prefix
# header itself because clang substitutes the precompiled .pcm for a prefix .h
# automatically, which triggers version mismatch when you use a new toolchain
# to run analysis on an existing build.
def prefix_header_includes(pchHeaderPath):
    headers = []
    try:
        with open(pchHeaderPath) as f:
            for line in f:
                m = INCLUDE_RE.match(line)
                if m:
                    headers.append(m.group(1))
    except OSError:
        pass
    return headers


def rewrite_argv(entry, absSrc, clang, checkers, analyzerOutput='text', outputPath=os.devnull):
    tokens = shlex.split(entry['command'])
    out = [clang]
    prefixHeaderIncludes = []
    i = 1
    n = len(tokens)
    entryFile = entry['file']
    entryFileReal = os.path.realpath(entryFile)
    while i < n:
        t = tokens[i]
        if t == '-o' or t == '-MT' or t == '-MF' or t == '-MQ':
            i += 2
            continue
        if t == '-include' and i + 1 < n:
            if is_cmake_pch(tokens[i + 1]):
                prefixHeaderIncludes += prefix_header_includes(tokens[i + 1])
            else:
                out += [t, tokens[i + 1]]
            i += 2
            continue
        if t == '-x' and i + 1 < n:
            i += 2
            continue
        if t in ('-c', '-MD', '-MMD', '-Winvalid-pch', '-fpch-instantiate-templates',
                 '-Werror', '-fcolor-diagnostics'):
            i += 1
            continue
        if t.startswith('-fdiagnostics-color') or t.startswith('-Werror='):
            i += 1
            continue
        if t == '-Xclang' and i + 1 < n:
            nxt = tokens[i + 1]
            if nxt in ('-include-pch', '-include'):
                if nxt == '-include' and i + 3 < n and is_cmake_pch(tokens[i + 3]):
                    prefixHeaderIncludes += prefix_header_includes(tokens[i + 3])
                i += 4
                continue
            if nxt == '-fno-pch-timestamp':
                i += 2
                continue
        if t == entryFile or os.path.realpath(t) == entryFileReal:
            i += 1
            continue
        out.append(t)
        i += 1

    for prefixHeaderInclude in prefixHeaderIncludes:
        out += ['-include', prefixHeaderInclude]
    out += [
        absSrc,
        '--analyze',
        '-fno-color-diagnostics',
        '-Wno-error',
        # The SaferCPP EWS analyzes a Release build. Disable assertions so a
        # Debug compile_commands.json produces the same checker results.
        '-UASSERT_ENABLED', '-DASSERT_ENABLED=0', '-DNDEBUG=1',
        '-DRELEASE_WITHOUT_OPTIMIZATIONS=1',
        '-DENABLE_IPC_TESTING_API=1',
        # Define these for compatibility when using open source clang.
        '-D__ptrauth_swift_value_witness_function_pointer(x)=',
        '-D__ptrauth_swift_class_method_pointer(x)=',
        '-Xclang', '-analyzer-output=' + analyzerOutput,
        '-Xclang', '-analyzer-disable-checker', '-Xclang', DISABLE_CATEGORIES,
        '-Xclang', '-analyzer-checker', '-Xclang', ','.join(checkers),
        '-Xclang', '-analyzer-config', '-Xclang', 'max-nodes=10000000',
        '-o', outputPath,
    ]
    return out, entry['directory']


def available_checkers(clang):
    proc = subprocess.run([clang, '-cc1', '-analyzer-checker-help', '-analyzer-checker-help-alpha'], capture_output=True, text=True)
    if proc.returncode != 0:
        return None
    names = set()
    for line in proc.stdout.splitlines():
        m = CHECKER_HELP_RE.match(line)
        if m:
            names.add(m.group(1))
    return names or None


def drop_unavailable_checkers(checkers, clang, explicit):
    available = available_checkers(clang)
    if available is None:
        return checkers
    missing = [c for c in checkers if c not in available]
    if not missing:
        return checkers
    if explicit:
        sys.exit('error: this analyzer does not implement {}'.format(', '.join(missing)))
    remaining = [c for c in checkers if c in available]
    if not remaining:
        sys.exit('error: this analyzer implements none of the SaferCPP checkers; '
                 'pass --clang PATH or --download-toolchain for a newer one.')
    print('Skipping checkers this analyzer does not implement: {}'.format(
        ', '.join(c.rsplit('.', 1)[-1] for c in missing)), file=sys.stderr)
    return remaining


def find_toolchain_clangs():
    def read_plist_key(plist, key):
        proc = subprocess.run(['plutil', '-extract', key, 'raw', '-o', '-', plist],
                              capture_output=True, text=True)
        return proc.stdout.strip() if proc.returncode == 0 else ''

    candidates = []
    for d in TOOLCHAIN_DIRS:
        for tc in glob.glob(os.path.join(d, '*.xctoolchain')):
            clang = os.path.join(tc, 'usr', 'bin', 'clang++')
            if not os.path.isfile(clang):
                continue
            plist = os.path.join(tc, 'Info.plist')
            created = read_plist_key(plist, 'CreatedDate')
            display = read_plist_key(plist, 'DisplayName') or os.path.basename(tc)
            candidates.append((created, clang, display))
    candidates.sort(reverse=True)
    return candidates


# FIXME: Download the snapshot named in Tools/CISupport/safer-cpp-swift-version, the one the bots run,
# instead of the latest one.
def download_swift_toolchain():
    print('Fetching latest swift.org development snapshot manifest...', file=sys.stderr)
    proc = subprocess.run(['curl', '-fsSL', SWIFT_LATEST_URL], capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit('error: failed to fetch {}: {}'.format(SWIFT_LATEST_URL, proc.stderr.strip()))
    fields = dict(re.findall(r'^(\w+):\s*(\S+)', proc.stdout, re.M))
    snapshot = fields.get('dir')
    pkgName = fields.get('download')
    if not snapshot or not pkgName:
        sys.exit('error: could not parse snapshot manifest:\n' + proc.stdout)
    url = '{}/{}/{}'.format(SWIFT_DOWNLOAD_BASE, snapshot, pkgName)
    with tempfile.NamedTemporaryFile(suffix='.pkg', delete=False) as f:
        pkgPath = f.name
    # FIXME: Don't hardcode the download size.
    print('Downloading {} (~1.5 GB)...'.format(url), file=sys.stderr)
    rc = subprocess.call(['curl', '-fL', '-#', '-o', pkgPath, url])
    if rc != 0:
        sys.exit('error: download failed')
    print('Installing to ~/Library/Developer/Toolchains/ ...', file=sys.stderr)
    rc = subprocess.call(['installer', '-pkg', pkgPath, '-target', 'CurrentUserHomeDirectory'])
    os.unlink(pkgPath)
    if rc != 0:
        sys.exit('error: installer failed (exit {})'.format(rc))
    expected = os.path.join(TOOLCHAIN_DIRS[0], snapshot + '.xctoolchain', 'usr', 'bin', 'clang++')
    if os.path.isfile(expected):
        return expected, snapshot
    for _, clang, display in find_toolchain_clangs():
        if snapshot in clang:
            return clang, display
    sys.exit('error: installed toolchain not found at {}'.format(expected))


def find_linux_clang(db):
    """Return the clang++ that implements the most SaferCPP checkers, or None.

    Ties go to the first candidate: $CXX, then the compiler that produced the
    compile database (so the recorded flags are known to be accepted), then
    clang++ and the versioned clang++-N names in PATH.
    """
    names = [os.environ.get('CXX', '')]
    if db.byFile:
        names.append(shlex.split(next(iter(db.byFile.values()))['command'])[0])
    names += LINUX_CLANG_NAMES

    candidates = []
    seen = set()
    for name in names:
        if 'clang' not in os.path.basename(name):
            continue
        path = shutil.which(name)
        if not path or os.path.realpath(path) in seen:
            continue
        seen.add(os.path.realpath(path))
        available = available_checkers(path)
        if available:
            candidates.append((len(available.intersection(WEBKIT_CHECKERS)), path))
    candidates = [c for c in candidates if c[0]]
    return max(candidates, key=lambda c: c[0])[1] if candidates else None


def resolve_analyzer_clang(args, db):
    if args.clang:
        if not os.path.isfile(args.clang):
            sys.exit("error: --clang path '{}' does not exist".format(args.clang))
        print('Using analyzer: {}'.format(args.clang), file=sys.stderr)
        return args.clang

    if sys.platform != 'darwin':
        if args.download_toolchain:
            sys.exit('error: --download-toolchain installs an Xcode toolchain and only works on macOS; pass --clang PATH')
        clang = find_linux_clang(db)
        if not clang:
            sys.exit('error: no clang++ implementing the SaferCPP checkers found in $CXX or PATH; pass --clang PATH')
        print('Using analyzer: {}'.format(clang), file=sys.stderr)
        return clang

    if args.download_toolchain:
        clang, display = download_swift_toolchain()
        print('Using analyzer: {} ({})'.format(clang, display), file=sys.stderr)
        return clang

    candidates = find_toolchain_clangs()
    if candidates:
        _, clang, display = candidates[0]
        print('Using analyzer: {} ({})'.format(clang, display), file=sys.stderr)
        return clang

    print(file=sys.stderr)
    print('No Swift toolchain found in {}.'.format(' or '.join(TOOLCHAIN_DIRS)), file=sys.stderr)
    print(file=sys.stderr)
    if not sys.stdin.isatty():
        sys.exit('Pass --clang PATH, or --download-toolchain to fetch a swift.org snapshot.')
    try:
        answer = input('`analyze-safer-cpp` requires an up-to-date toolchain. Download the latest swift.org development snapshot (~1.5 GB)? [Y/n] ')
    except (EOFError, KeyboardInterrupt):
        answer = ''
    if answer.strip().lower() in ('y', 'yes', ''):
        clang, display = download_swift_toolchain()
        print('Using analyzer: {} ({})'.format(clang, display), file=sys.stderr)
        return clang
    sys.exit('No analyzer clang available. Pass --clang PATH or re-run with --download-toolchain.')


def resolve_port_build_dir(port):
    proc = subprocess.run(['perl', WEBKIT_BUILD_DIRECTORY, '--configuration', '--' + port],
                          capture_output=True, text=True)
    if proc.returncode != 0 or not proc.stdout.strip():
        sys.exit('error: webkit-build-directory --{} failed:\n{}'.format(port, proc.stderr.strip()))
    return proc.stdout.strip().splitlines()[-1]


def add_compile_commands_arguments(parser):
    where = parser.add_mutually_exclusive_group()
    where.add_argument('--cmake-preset', help='CMake configure preset name')
    where.add_argument('--compile-commands', metavar='PATH',
                       help='path to a compile_commands.json file')
    for port in ('gtk', 'wpe'):
        where.add_argument('--' + port, action='store_const', const=port, dest='port',
                           help='use the {} build-webkit tree (honors set-webkit-configuration and WEBKIT_OUTPUTDIR)'.format(port.upper()))


def resolve_compile_commands_path(args):
    if args.compile_commands:
        compileCommandsPath = os.path.abspath(args.compile_commands)
    elif args.cmake_preset:
        compileCommandsPath = os.path.join(resolve_preset_build_dir(args.cmake_preset), 'compile_commands.json')
    elif args.port:
        compileCommandsPath = os.path.join(resolve_port_build_dir(args.port), 'compile_commands.json')
    elif sys.platform != 'darwin':
        sys.exit('error: specify --gtk, --wpe, --cmake-preset or --compile-commands')
    else:
        compileCommandsPath = os.path.join(DEFAULT_BUILD_DIR, 'compile_commands.json')
    if not os.path.isfile(compileCommandsPath):
        sys.exit("error: compile_commands.json not found at '{}' (run cmake --preset ... or build-webkit first)".format(compileCommandsPath))
    return compileCommandsPath
