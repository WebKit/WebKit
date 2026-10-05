#!/usr/bin/env python3
#
# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Writes the WebCore_Private module map that the CMake build's Swift Clang importer sees.
#
# The in-tree WebCore_Private.modulemap umbrellas all of PrivateHeaders. Every header of a module Swift
# imports is a dependency of the Swift module that imports it, so with the umbrella an edit to any WebCore
# private header recompiles all of the importer's Swift. This map instead lists the headers that declare
# the WebCore names the Swift sources use (WebCore.Name), and every WebCore header those include. A WebCore
# header that is parsed while building this module but not listed in it is only visible inside it, so a
# header that includes it later, such as one in WebKit_Internal, could not use its declarations.
#
# The output is only rewritten when it changes, and the depfile lists every file that was read.

import argparse
import os
import re
import sys

EXPORT_MACROS_HEADER = 'PlatformExportMacros.h'

SWIFT_NAME = re.compile(r'\bWebCore\.([A-Za-z_][A-Za-z0-9_]*)')
INCLUDE = re.compile(r'^\s*#\s*(?:include|import)\s*[<"](?:WebCore/)?([^>"/]+\.h)[>"]', re.MULTILINE)


def declaration(name):
    # Matches a definition of the name, not a forward declaration.
    return re.compile(r'^\s*(?:class|struct|enum(?:\s+class)?|using|typedef)\s+(?:WEBCORE_EXPORT\s+)?(?:[^;{]*\s)?' + re.escape(name) + r'\b(?!\s*;)', re.MULTILINE)


def read(path):
    with open(path, encoding='utf-8', errors='replace') as file:
        return file.read()


def swift_names(sources):
    names = {}
    for source in sources:
        for line in read(source).splitlines():
            if line.lstrip().startswith('//'):
                continue
            for name in SWIFT_NAME.findall(line):
                names.setdefault(name, source)
    return names


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--headers', required=True, help='WebCore.framework PrivateHeaders directory')
    parser.add_argument('--sources', required=True, help='File listing the Swift sources, one per line')
    parser.add_argument('--source-dir', required=True, help='Directory that relative source paths are relative to')
    parser.add_argument('--build-dir', required=True, help='Generated sources under this directory are skipped')
    parser.add_argument('--output', required=True)
    parser.add_argument('--depfile', required=True)
    args = parser.parse_args()

    # Generated Swift reaches WebCore through WebKit_Internal, and depending on it here could make a cycle.
    build_dir = os.path.join(os.path.realpath(args.build_dir), '')
    with open(args.sources) as file:
        sources = [os.path.join(args.source_dir, line.strip()) for line in file if line.strip()]
    sources = [source for source in sources if not os.path.realpath(source).startswith(build_dir)]
    headers = set(os.listdir(args.headers))
    read_files = list(sources)

    roots = set()
    unresolved = []
    for name, source in sorted(swift_names(sources).items()):
        if name + '.h' in headers:
            roots.add(name + '.h')
            continue
        pattern = declaration(name)
        declaring = [header for header in sorted(headers) if header.endswith('.h') and pattern.search(read(os.path.join(args.headers, header)))]
        if not declaring:
            unresolved.append((name, source))
            continue
        roots.update(declaring)

    if unresolved:
        for name, source in unresolved:
            print(f'{source}: error: no WebCore private header declares WebCore.{name}', file=sys.stderr)
        return 1

    listed = set()
    pending = sorted(roots)
    while pending:
        header = pending.pop()
        if header in listed:
            continue
        listed.add(header)
        for included in INCLUDE.findall(read(os.path.join(args.headers, header))):
            if included in headers and included not in listed:
                pending.append(included)
    read_files.extend(os.path.join(args.headers, header) for header in sorted(listed))
    listed.discard(EXPORT_MACROS_HEADER)

    lines = [
        '// Generated by generate-swift-private-module-map.py.',
        'framework module WebCore_Private [system] {',
        '    module PlatformExportMacros {',
        f'        header "{EXPORT_MACROS_HEADER}"',
        '        export *',
        '    }',
        '',
        '    module Core {',
        '        requires cplusplus',
        '',
    ]
    lines += [f'        header "{header}"' for header in sorted(listed)]
    lines += [
        '',
        '        export *',
        '    }',
        '}',
    ]
    contents = '\n'.join(lines) + '\n'

    if not os.path.exists(args.output) or read(args.output) != contents:
        # The output directory may be a framework symlink whose target does not exist yet.
        os.makedirs(os.path.realpath(os.path.dirname(args.output)), exist_ok=True)
        with open(args.output + '.tmp', 'w') as file:
            file.write(contents)
        os.replace(args.output + '.tmp', args.output)

    def escape(path):
        return path.replace('\\', '\\\\').replace(' ', '\\ ')

    with open(args.depfile, 'w') as file:
        file.write(escape(args.output) + ': ' + ' \\\n  '.join(escape(path) for path in read_files) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
