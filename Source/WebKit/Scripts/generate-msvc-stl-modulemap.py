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

import argparse
import json
import os
import re
import sys

# Generates an amended copy of the Swift Windows SDK's vcruntime.modulemap, plus a
# clang VFS overlay that mounts it over the MSVC include directory as
# module.modulemap.
#
# The additions have to go inside `std._Private` rather than into a separate module
# of our own: these headers include other STL headers, and STL headers include them,
# so a separate top-level module would form a cyclic dependency with `std`.
#
# Writes <output-dir>/vcruntime.modulemap and <output-dir>/vfs-overlay.yaml, and
# prints the path of the overlay to stdout for cmake to put on the importer's
# command line.

# An aggregate that pulls in the whole STL; giving it a module of its own is not useful.
EXCLUDED_HEADERS = frozenset(['__msvc_all_public_headers.hpp'])

# `module _Private [system] {` inside the `std` module, plus any `requires` lines that
# open it, so that the additions land after them. Anchoring on the containing submodule
# rather than on one of its entries keeps this working if the SDK reshuffles which
# __msvc_*.hpp headers it happens to name.
PRIVATE_MODULE_RE = re.compile(
    r'^(?P<indent>[ \t]*)module _Private\b[^\n]*\{[ \t]*\n'
    r'(?:[ \t]*(?:requires[^\n]*)?\n)*', re.MULTILINE)


def declared_headers(modulemap_text):
    return frozenset(re.findall(r'header\s+"([^"]+)"', modulemap_text))


def amend_modulemap(modulemap_text, headers):
    already_declared = declared_headers(modulemap_text)
    missing = sorted(h for h in headers if h not in already_declared and h not in EXCLUDED_HEADERS)
    if not missing:
        return modulemap_text, []

    match = PRIVATE_MODULE_RE.search(modulemap_text)
    if not match:
        raise ValueError("could not find the `module _Private` block to extend")

    indent = match.group('indent') + '  '
    additions = ''
    for header in missing:
        module_name = os.path.splitext(header)[0]
        additions += '%sexplicit module %s {\n' % (indent, module_name)
        additions += '%s  header "%s"\n' % (indent, header)
        additions += '%s  export *\n' % indent
        additions += '%s}\n\n' % indent

    insertion_point = match.end()
    return modulemap_text[:insertion_point] + additions + modulemap_text[insertion_point:], missing


def write_if_changed(path, contents):
    try:
        with open(path, 'r') as f:
            if f.read() == contents:
                return False
    except OSError:
        pass
    with open(path, 'w') as f:
        f.write(contents)
    return True


def vfs_overlay(msvc_include_dir, modulemap_path):
    return {
        'version': 0,
        'case-sensitive': 'false',
        'roots': [
            {
                'name': msvc_include_dir.replace('\\', '/'),
                'type': 'directory',
                'contents': [
                    {
                        'name': 'module.modulemap',
                        'type': 'file',
                        'external-contents': modulemap_path.replace('\\', '/'),
                    },
                ],
            },
        ],
    }


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-modulemap', required=True,
                        help='the Swift SDK\'s usr/share/vcruntime.modulemap')
    parser.add_argument('--msvc-include-dir', required=True,
                        help='the MSVC include directory to mount the amended map over')
    parser.add_argument('--output-dir', required=True,
                        help='where to write vcruntime.modulemap and vfs-overlay.yaml')
    parser.add_argument('--headers', nargs='*', default=[], metavar='HEADER',
                        help='__msvc_*.hpp header names, relative to --msvc-include-dir')
    args = parser.parse_args(argv[1:])

    with open(args.sdk_modulemap, 'r') as f:
        sdk_modulemap = f.read()

    if not args.headers:
        raise ValueError('no __msvc_*.hpp headers found in %s' % args.msvc_include_dir)

    amended, added = amend_modulemap(sdk_modulemap, args.headers)
    if not added:
        print('%s already declares every internal STL header' % args.sdk_modulemap, file=sys.stderr)
        return 0

    os.makedirs(args.output_dir, exist_ok=True)
    modulemap_path = os.path.join(args.output_dir, 'vcruntime.modulemap')
    overlay_path = os.path.join(args.output_dir, 'vfs-overlay.yaml')

    overlay = json.dumps(vfs_overlay(args.msvc_include_dir, modulemap_path), indent=2) + '\n'
    wrote = write_if_changed(modulemap_path, amended) | write_if_changed(overlay_path, overlay)

    if wrote:
        print('declared %d internal STL header(s): %s' % (len(added), ' '.join(added)), file=sys.stderr)
    print(overlay_path.replace('\\', '/'))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (OSError, ValueError) as error:
        print('%s: %s' % (os.path.basename(sys.argv[0]), error), file=sys.stderr)
        sys.exit(1)
