#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r'''Finds which build target(s) contain a particular Java class.

This is a utility script for finding out which build target dependency needs to
be added to import a given Java class.

It is a best-effort script.

Example:

Find build target with class FooUtil:
   tools/android/modularization/convenience/lookup_dep.py FooUtil
'''

import argparse
import collections
import dataclasses
import json
import logging
import os
import pathlib
import subprocess
import sys
import zipfile
from typing import Dict, List, Set

_SRC_DIR = pathlib.Path(__file__).parents[4].resolve()

sys.path.append(str(_SRC_DIR / 'build' / 'android'))
from pylib import constants

# Import list_java_targets so that the dependency is found by print_python_deps.
import list_java_targets


def main():
    arg_parser = argparse.ArgumentParser(
        description='Finds which build target contains a particular Java class.')

    arg_parser.add_argument('-C', '--output-directory', help='Build output directory.')
    arg_parser.add_argument('--build', action='store_true', help='Build all .build_config files.')
    arg_parser.add_argument('classes', nargs='+', help='Java classes to search for')
    arg_parser.add_argument('-v', '--verbose', action='store_true', help='Verbose logging.')

    arguments = arg_parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if arguments.verbose else logging.WARNING,
        format='%(asctime)s.%(msecs)03d %(levelname).1s %(message)s',
        datefmt='%H:%M:%S')

    if arguments.output_directory:
        constants.SetOutputDirectory(arguments.output_directory)
    constants.CheckOutputDirectory()
    abs_out_dir: pathlib.Path = pathlib.Path(constants.GetOutDirectory()).resolve()

    index = ClassLookupIndex(abs_out_dir, arguments.build)
    matches = {c: index.match(c) for c in arguments.classes}

    if not arguments.build:
        # Try finding match without building because it is faster.
        for class_name, match_list in matches.items():
            if len(match_list) == 0:
                arguments.build = True
                break
        if arguments.build:
            index = ClassLookupIndex(abs_out_dir, True)
            matches = {c: index.match(c) for c in arguments.classes}

    if not arguments.build:
        print('Showing potentially stale results. Run lookup.dep.py with --build '
              '(slower) to build any unbuilt GN targets and get full results.')
        print()

    for (class_name, class_entries) in matches.items():
        if not class_entries:
            print(f'Could not find build target for class "{class_name}"')
        elif len(class_entries) == 1:
            class_entry = class_entries[0]
            print(f'Class {class_entry.full_class_name} found:')
            print(f'    "{class_entry.target}"')
        else:
            print(f'Multiple targets with classes that match "{class_name}":')
            print()
            for class_entry in class_entries:
                print(f'    "{class_entry.target}"')
                print(f'        contains {class_entry.full_class_name}')
                print()


@dataclasses.dataclass(frozen=True)
class ClassEntry:
    """An assignment of a Java class to a build target."""
    full_class_name: str
    target: str


class ClassLookupIndex:
    """A map from full Java class to its build targets.

  A class might be in multiple targets if it's bytecode rewritten."""

    def __init__(self, abs_build_output_dir: pathlib.Path, should_build: bool):
        self._abs_build_output_dir = abs_build_output_dir
        self._should_build = should_build
        self._class_index = self._index_root()

    def match(self, search_string: str) -> List[ClassEntry]:
        """Get class/target entries where the class matches search_string"""
        # Priority 1: Exact full matches
        if search_string in self._class_index:
            return self._entries_for(search_string)

        # Priority 2: Match full class name (any case), if it's a class name
        matches = []
        lower_search_string = search_string.lower()
        if '.' not in lower_search_string:
            for full_class_name in self._class_index:
                package_and_class = full_class_name.rsplit('.', 1)
                if len(package_and_class) < 2:
                    continue
                class_name = package_and_class[1]
                class_lower = class_name.lower()
                if class_lower == lower_search_string:
                    matches.extend(self._entries_for(full_class_name))
            if matches:
                return matches

        # Priority 3: Match anything
        for full_class_name in self._class_index:
            if lower_search_string in full_class_name.lower():
                matches.extend(self._entries_for(full_class_name))

        return matches

    def _entries_for(self, class_name) -> List[ClassEntry]:
        return [ClassEntry(class_name, target) for target in self._class_index.get(class_name)]

    def _index_root(self) -> Dict[str, List[str]]:
        """Create the class to target index."""
        logging.debug('Running list_java_targets.py...')
        list_java_targets_command = [
            'build/android/list_java_targets.py', '--gn-labels', '--print-build-config-paths',
            f'--output-directory={self._abs_build_output_dir}'
        ]
        if self._should_build:
            list_java_targets_command += ['--build']

        list_java_targets_run = subprocess.run(
            list_java_targets_command, cwd=_SRC_DIR, capture_output=True, text=True, check=True)
        logging.debug('... done.')

        # Parse output of list_java_targets.py with mapping of build_target to
        # build_config
        root_build_targets = list_java_targets_run.stdout.split('\n')
        class_index = collections.defaultdict(list)
        for target_line in root_build_targets:
            # Skip empty lines
            if not target_line:
                continue

            target_line_parts = target_line.split(': ')
            assert len(target_line_parts) == 2, target_line_parts
            target, build_config_path = target_line_parts

            if not os.path.exists(build_config_path):
                assert not self._should_build
                continue

            with open(build_config_path) as build_config_contents:
                build_config: Dict = json.load(build_config_contents)
            deps_info = build_config['deps_info']
            # Checking the library type here instead of in list_java_targets.py avoids
            # reading each .build_config file twice.
            if deps_info['type'] != 'java_library':
                continue

            target = self._compute_toplevel_target(target)
            full_class_names = self._compute_full_class_names_for_build_config(deps_info)
            for full_class_name in full_class_names:
                class_index[full_class_name].append(target)

        return class_index

    @staticmethod
    def _compute_toplevel_target(target: str) -> str:
        """Computes top level target from the passed-in sub-target."""
        if target.endswith('_java'):
            return target

        # Handle android_aar_prebuilt() sub targets.
        index = target.find('_java__subjar')
        if index >= 0:
            return target[0:index + 5]
        index = target.find('_java__classes')
        if index >= 0:
            return target[0:index + 5]

        return target

    def _compute_full_class_names_for_build_config(self, deps_info: Dict) -> Set[str]:
        """Returns set of fully qualified class names for build config."""

        full_class_names = set()

        # Read the location of the java_sources_file from the build_config
        sources_path = deps_info.get('java_sources_file')
        if sources_path:
            # Read the java_sources_file, indexing the classes found
            with open(self._abs_build_output_dir / sources_path) as sources_contents:
                for source_line in sources_contents:
                    source_path = pathlib.Path(source_line.strip())
                    java_class = self._parse_full_java_class(source_path)
                    if java_class:
                        full_class_names.add(java_class)

        # |unprocessed_jar_path| is set for prebuilt targets. (ex:
        # android_aar_prebuilt())
        # |unprocessed_jar_path| might be set but not exist if not all targets have
        # been built.
        unprocessed_jar_path = deps_info.get('unprocessed_jar_path')
        if unprocessed_jar_path:
            abs_unprocessed_jar_path = (self._abs_build_output_dir / unprocessed_jar_path)
            if abs_unprocessed_jar_path.exists():
                # Normalize path but do not follow symlink if .jar is symlink.
                abs_unprocessed_jar_path = (
                    abs_unprocessed_jar_path.parent.resolve() / abs_unprocessed_jar_path.name)

                full_class_names.update(
                    self._extract_full_class_names_from_jar(self._abs_build_output_dir,
                                                            abs_unprocessed_jar_path))

        return full_class_names

    @staticmethod
    def _extract_full_class_names_from_jar(abs_build_output_dir: pathlib.Path,
                                           abs_jar_path: pathlib.Path) -> Set[str]:
        """Returns set of fully qualified class names in passed-in jar."""
        out = set()
        jar_namelist = ClassLookupIndex._read_jar_namelist(abs_build_output_dir, abs_jar_path)
        for zip_entry_name in jar_namelist:
            if not zip_entry_name.endswith('.class'):
                continue
            # Remove .class suffix
            full_java_class = zip_entry_name[:-6]

            full_java_class = full_java_class.replace('/', '.')
            dollar_index = full_java_class.find('$')
            if dollar_index >= 0:
                full_java_class[0:dollar_index]

            out.add(full_java_class)
        return out

    @staticmethod
    def _read_jar_namelist(abs_build_output_dir: pathlib.Path,
                           abs_jar_path: pathlib.Path) -> List[str]:
        """Returns list of jar members by name."""

        # Caching namelist speeds up lookup_dep.py runtime by 1.5s.
        cache_path = abs_jar_path.with_suffix(abs_jar_path.suffix + '.namelist_cache')
        if (not ClassLookupIndex._is_path_relative_to(abs_jar_path, abs_build_output_dir)):
            cache_path = (abs_build_output_dir / 'gen' / cache_path.relative_to(_SRC_DIR))
        if (cache_path.exists() and os.path.getmtime(cache_path) > os.path.getmtime(abs_jar_path)):
            with open(cache_path) as f:
                return [s.strip() for s in f.readlines()]

        with zipfile.ZipFile(abs_jar_path) as z:
            namelist = z.namelist()

        cache_path.parent.mkdir(parents=True, exist_ok=True)
        with open(cache_path, 'w') as f:
            f.write('\n'.join(namelist))

        return namelist

    @staticmethod
    def _is_path_relative_to(path: pathlib.Path, other: pathlib.Path) -> bool:
        # PurePath.is_relative_to() was introduced in Python 3.9
        resolved_path = path.resolve()
        resolved_other = other.resolve()
        return str(resolved_path).startswith(str(resolved_other))

    @staticmethod
    def _parse_full_java_class(source_path: pathlib.Path) -> str:
        """Guess the fully qualified class name from the path to the source file."""
        if source_path.suffix != '.java':
            logging.warning(f'"{source_path}" does not have the .java suffix')
            return None

        directory_path: pathlib.Path = source_path.parent
        package_list_reversed = []
        for part in reversed(directory_path.parts):
            if part == 'java':
                break
            package_list_reversed.append(part)
            if part in ('com', 'org'):
                break
        else:
            logging.debug(f'File {source_path} not in a subdir of "org" or "com", '
                          'cannot detect package heuristically.')
            return None

        package = '.'.join(reversed(package_list_reversed))
        class_name = source_path.stem
        return f'{package}.{class_name}'


if __name__ == '__main__':
    main()
