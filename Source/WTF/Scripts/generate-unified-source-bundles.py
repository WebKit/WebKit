# Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import fnmatch
import hashlib
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

COMMENT_REGEXP = re.compile(r'//')
MAX_DENSE_BUNDLE_SIZE = 64


def log(args, text):
    if args.verbose:
        print(text, file=sys.stderr)


def bundle_prefix_and_size_for_path(path: Path, args) -> tuple[str, int]:
    top_level_directory = path.parent
    # Walk up until the parent's parent equals the parent (i.e., we're at a single-component path)
    while top_level_directory.parent.parent != top_level_directory.parent:
        top_level_directory = top_level_directory.parent
    for filt in args.dense_bundle_filter:
        if fnmatch.fnmatch(path, filt):
            return filt, MAX_DENSE_BUNDLE_SIZE
    return str(top_level_directory), args.max_bundle_size


@dataclass(init=False)
class SourceFile:
    path: Path
    file_index: int
    unifiable: bool
    bundle_manager_key: str

    def __init__(self, file_line: str, file_index: int, args):
        self.unifiable = True
        self.file_index = file_index
        self._non_arc = False
        self._header_group: Optional[str] = None
        self._derived: Optional[bool] = None
        self._args = args

        attribute_start = file_line.find('@')
        if attribute_start != -1:
            attributes_text = file_line[attribute_start + 1:]
            for attribute in re.split(r'\s*@', attributes_text):
                attribute = attribute.strip()
                if attribute == 'no-unify':
                    self.unifiable = False
                elif attribute == 'nonARC':
                    self._non_arc = True
                elif attribute.startswith('header:'):
                    self._header_group = attribute[7:]
                else:
                    raise RuntimeError("unknown attribute: " + attribute)
            file_line = file_line[:attribute_start]

        self.path = Path(file_line.strip())
        if self.path.is_absolute():
            raise RuntimeError(f'path parsed from "{file_line}" is absolute; use a relative path')
        self.bundle_manager_key = self.path.suffix
        if self._non_arc:
            if self.bundle_manager_key == '.mm':
                self.bundle_manager_key = '.nonARC-mm'
            else:
                raise RuntimeError("used @nonARC with source file that does not have a .mm extension")
        if self._header_group:
            ext = self.path.suffix.lstrip('.')
            self.bundle_manager_key = f'.header-{self._header_group}-{ext}'

    def sort_key(self):
        return self.path.parent.parts, self.file_index, self.path.name

    @property
    def derived(self) -> bool:
        if self._derived is None:
            self._derived = not (self._args.source_tree_path / self.path).exists()
        return self._derived

    def __str__(self):
        if self._args.mode == 'GenerateXCFilelists':
            if self.derived:
                return str(self._args.derived_sources_path / self.path)
            else:
                return f'$(SRCROOT)/{self.path}'
        elif self._args.mode == 'GenerateBundles' or not self.derived:
            return str(self.path)
        else:
            return str(self._args.derived_sources_path / self.path)


class BundleManager:
    def __init__(self, extension: str, suffix: str, max_count: int, args, generated_sources: list[str], output_sources: list[str]):
        self.extension = extension
        self.suffix = suffix
        self.file_count = 0
        self.bundle_count = 0
        self.current_bundle_text = ""
        self.max_count = max_count
        self.extra_files: list[str] = []
        self._current_directory: Optional[Path] = None
        self._last_bundling_prefix: Optional[str] = None
        self._args = args
        self._generated_sources = generated_sources
        self._output_sources = output_sources

    def write_file(self, file_name) -> None:
        bundle_file = self._args.unified_source_output_path / file_name
        if self._args.mode == 'GenerateXCFilelists':
            self._output_sources.append(str(bundle_file))
            return
        if not bundle_file.exists() or bundle_file.read_text() != self.current_bundle_text:
            log(self._args, "Writing bundle {} with: \n{}".format(bundle_file, self.current_bundle_text))
            bundle_file.write_text(self.current_bundle_text)

    def bundle_file_name(self) -> str:
        if self.max_count is not None:
            id_str = str(self.bundle_count)
        else:
            assert self._current_directory is not None, 'flush() without adding any files'
            hash_val = hashlib.sha1(bytes(self._current_directory)).hexdigest()[:8]
            id_str = "-{}-{}".format(hash_val, self.bundle_count)
        return "{}UnifiedSource{}{}".format(self._args.bundle_filename_prefix, id_str, self.suffix)

    def flush(self) -> None:
        self.bundle_count += 1
        bundle_file = self.bundle_file_name()
        self._generated_sources.append(str(self._args.unified_source_output_path / bundle_file))
        if self.max_count is not None and self.bundle_count > self.max_count:
            self.extra_files.append(bundle_file)

        self.write_file(bundle_file)
        self.current_bundle_text = ""
        self.file_count = 0

    def flush_to_max(self) -> None:
        assert self.max_count is not None
        while self.bundle_count < self.max_count:
            self.flush()

    def add_file(self, source_file: SourceFile) -> None:
        path = source_file.path
        bundle_prefix, bundle_size = bundle_prefix_and_size_for_path(path, self._args)
        if self._last_bundling_prefix != bundle_prefix:
            if self.file_count != 0:
                log(self._args, "Flushing because new top level directory; old: {}, new: {}".format(self._current_directory, path.parent))
                self.flush()
            self._last_bundling_prefix = bundle_prefix
            self._current_directory = path.parent
            if self.max_count is None:
                self.bundle_count = 0
        if self.file_count >= bundle_size:
            log(self._args, "Flushing because new bundle is full ({} sources)".format(self.file_count))
            self.flush()
        self.current_bundle_text += '#include "{}"\n'.format(source_file)
        self.file_count += 1


def process_file_for_unified_source_generation(source_file: SourceFile, args, bundle_managers: dict[str, BundleManager], generated_sources: list[str], input_sources: list[str]) -> None:
    input_sources.append(str(source_file))

    bundle = bundle_managers.get(source_file.bundle_manager_key)
    if not bundle:
        log(args, "No bundle for {} files, building {} standalone".format(source_file.bundle_manager_key, source_file.path))
        generated_sources.append(str(source_file))
    elif not source_file.unifiable:
        log(args, "Not allowed to unify {}, building standalone".format(source_file.path))
        generated_sources.append(str(source_file))
    else:
        bundle.add_file(source_file)


def parse_args():
    parser = argparse.ArgumentParser(
        description='Generate unified source bundles for WebKit builds.',
        epilog='<sources-list-file> may be separate arguments or one semicolon separated string')

    parser.add_argument('--verbose', '-v', action='store_true', default=False,
                        help='Adds extra logging to stderr.')
    parser.add_argument('--derived-sources-path', '-d', required=True, type=Path,
                        help='Path to the directory where the unified source files should be placed.')
    parser.add_argument('--source-tree-path', '-s', required=True, type=Path,
                        help='Path to the root of the source directory.')

    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument('--print-bundled-sources', action='store_true', default=False,
                            help='Print bundled sources rather than generating sources.')
    mode_group.add_argument('--print-all-sources', action='store_true', default=False,
                            help='Print all sources rather than generating sources.')
    mode_group.add_argument('--generate-xcfilelists', action='store_true', default=False,
                            help='Generate .xcfilelist files.')

    parser.add_argument('--input-xcfilelist-path', type=Path,
                        help='Path of the generated input .xcfilelist file.')
    parser.add_argument('--output-xcfilelist-path', type=Path,
                        help='Path of the generated output .xcfilelist file.')
    parser.add_argument('--max-cpp-bundle-count', type=int, default=None,
                        help='Use global sequential numbers for cpp bundle filenames and set the limit on the number.')
    parser.add_argument('--max-c-bundle-count', type=int, default=None,
                        help='Use global sequential numbers for c bundle filenames and set the limit on the number.')
    parser.add_argument('--max-obj-c-bundle-count', type=int, default=None,
                        help='Use global sequential numbers for Obj-C bundle filenames and set the limit on the number.')
    parser.add_argument('--max-non-arc-obj-c-bundle-count', type=int, default=None,
                        help='Use global sequential numbers for non-ARC Obj-C bundle filenames and set the limit on the number.')
    parser.add_argument('--max-header-bundle-count', type=int, default=None,
                        help='Use global sequential numbers for header-grouped bundle filenames and set the limit on the number.')
    parser.add_argument('--max-bundle-size', type=int, default=8,
                        help='The number of files to merge into a single bundle (default: 8).')
    parser.add_argument('--dense-bundle-filter', action='append', default=[],
                        help='Densely bundle files matching the given path glob (repeatable).')
    parser.add_argument('--bundle-filename-prefix', default='',
                        help='Prefix for generated bundle filenames.')
    parser.add_argument('source_files', nargs='+', metavar='sources-list-file',
                        help='Source list files to process.')

    args = parser.parse_args()

    if not args.source_tree_path.exists():
        parser.error("Source tree {} does not exist.".format(args.source_tree_path))

    # Determine mode from flags
    if args.print_bundled_sources:
        args.mode = 'PrintBundledSources'
    elif args.print_all_sources:
        args.mode = 'PrintAllSources'
    elif args.generate_xcfilelists:
        args.mode = 'GenerateXCFilelists'
    else:
        args.mode = 'GenerateBundles'

    # Compute unified source output path
    args.unified_source_output_path = args.derived_sources_path / "unified-sources"
    if not args.unified_source_output_path.exists() and args.mode != 'GenerateXCFilelists':
        args.unified_source_output_path.mkdir(parents=True, exist_ok=True)

    return args


def main() -> None:
    args = parse_args()

    log(args, "Putting unified sources in {}".format(args.unified_source_output_path))

    # Even though CMake will only pass us a single semicolon separated argument, we separate all the arguments for simplicity.
    source_list_files: list[Path] = []
    for arg in args.source_files:
        source_list_files.extend(Path(p) for p in arg.split(';'))
    log(args, "Source files: {}".format(source_list_files))

    generated_sources: list[str] = []
    input_sources: list[str] = []
    output_sources: list[str] = []

    bundle_managers = {
        '.cpp': BundleManager('cpp', '.cpp', args.max_cpp_bundle_count, args, generated_sources, output_sources),
        '.c': BundleManager('c', '-c.c', args.max_c_bundle_count, args, generated_sources, output_sources),
        '.mm': BundleManager('mm', '-ARC.mm', args.max_obj_c_bundle_count, args, generated_sources, output_sources),
        '.nonARC-mm': BundleManager('mm', '-nonARC.mm', args.max_non_arc_obj_c_bundle_count, args, generated_sources, output_sources),
    }

    seen = set()
    source_files = []

    for source_file_index, path in enumerate(source_list_files):
        log(args, "Reading {}".format(path))
        result = []
        with open(path, 'r') as f:
            for line in f:
                log(args, "Before: {}".format(line.rstrip('\n')))
                comment_match = COMMENT_REGEXP.search(line)
                if comment_match is not None:
                    line = line[:comment_match.start()]
                    log(args, "After: {}".format(line))
                line = line.strip()

                if not line:
                    continue

                if line in seen:
                    if args.mode == 'GenerateXCFilelists':
                        continue
                    raise RuntimeError("duplicate line: {} in {}".format(line, path))
                seen.add(line)
                result.append(SourceFile(line, source_file_index, args))

        log(args, "Found {} source files in {}".format(len(result), path))
        source_files.extend(result)

    # Create BundleManagers for any @header: groups discovered in source files.
    header_keys_seen = set()
    for sf in source_files:
        if sf._header_group:
            header_keys_seen.add(sf.bundle_manager_key)
    for key in sorted(header_keys_seen):
        # Extract extension from key like '.header-RenderStyleGetters-cpp'
        parts = key.split('-')
        ext = parts[-1]  # 'cpp' or 'mm'
        header_group = '-'.join(parts[1:-1])  # 'RenderStyleGetters'
        suffix = f'-header-{header_group}.{ext}'
        bundle_managers[key] = BundleManager(ext, suffix, args.max_header_bundle_count, args, generated_sources, output_sources)

    log(args, "Found sources: {}".format(sorted(source_files, key=SourceFile.sort_key)))

    for source_file in sorted(source_files, key=SourceFile.sort_key):
        if args.mode in ('GenerateBundles', 'GenerateXCFilelists'):
            process_file_for_unified_source_generation(source_file, args, bundle_managers, generated_sources, input_sources)
        elif args.mode == 'PrintAllSources':
            generated_sources.append(str(source_file))
        elif args.mode == 'PrintBundledSources':
            if bundle_managers.get(source_file.bundle_manager_key) and source_file.unifiable:
                generated_sources.append(str(source_file))

    if args.mode != 'PrintAllSources':
        for manager in bundle_managers.values():
            if manager.file_count != 0:
                manager.flush()

            if manager.max_count is None:
                continue

            manager.flush_to_max()

            if manager.extra_files:
                extension = manager.extension
                bundle_count = manager.bundle_count
                files_to_add = ", ".join(manager.extra_files)
                raise RuntimeError(
                    "number of bundles for {} sources, {}, exceeded limit, {}. Please add {} to Xcode then update UnifiedSource{}FileCount".format(
                        extension, bundle_count, manager.max_count, files_to_add, extension.capitalize()))

    if args.mode == 'GenerateXCFilelists':
        if args.input_xcfilelist_path:
            with open(args.input_xcfilelist_path, 'w') as f:
                f.write("\n".join(sorted(input_sources)) + "\n")
        if args.output_xcfilelist_path:
            with open(args.output_xcfilelist_path, 'w') as f:
                f.write("\n".join(sorted(output_sources)) + "\n")

    # We use stdout to report our unified source list to CMake.
    # Add trailing semicolon and avoid a trailing newline for CMake's sake.
    output = ";".join(generated_sources) + ";"
    log(args, output)
    sys.stdout.write(output)


if __name__ == '__main__':
    main()
