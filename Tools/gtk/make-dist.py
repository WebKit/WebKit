#!/usr/bin/env python
# Copyright (C) 2014 Igalia S.L.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

from __future__ import print_function
from contextlib import closing

import argparse
import os
import re
import sys
import tarfile


def enum(**enums):
    return type('Enum', (), enums)


class Rule(object):
    Result = enum(INCLUDE=1, EXCLUDE=2, NO_MATCH=3)

    def __init__(self, type, pattern):
        self.type = type
        self.original_pattern = pattern
        self.pattern = re.compile(pattern)

    def test(self, file):
        if not(self.pattern.search(file)):
            return Rule.Result.NO_MATCH
        return self.type


class Ruleset(object):
    _global_rules = None

    def __init__(self):
        # By default, accept all files.
        self.rules = [Rule(Rule.Result.INCLUDE, '.*')]

    @classmethod
    def global_rules(cls):
        if not cls._global_rules:
            cls._global_rules = Ruleset()
        return cls._global_rules

    @classmethod
    def add_global_rule(cls, rule):
        cls.global_rules().add_rule(rule)

    def add_rule(self, rule):
        self.rules.append(rule)

    def passes(self, file):
        allowed = False
        for rule in self.rules:
            result = rule.test(file)
            if result == Rule.Result.NO_MATCH:
                continue
            allowed = Rule.Result.INCLUDE == result
        return allowed


class File(object):
    def __init__(self, source_root, tarball_root):
        self.source_root = source_root
        self.tarball_root = tarball_root

    def get_files(self):
        yield (self.source_root, self.tarball_root)


class Directory(object):
    def __init__(self, source_root, tarball_root):
        self.source_root = source_root
        self.tarball_root = tarball_root
        self.rules = Ruleset()

    def add_rule(self, rule):
        self.rules.add_rule(rule)

    def get_tarball_path(self, filename):
        return filename.replace(self.source_root, self.tarball_root, 1)

    def get_files(self):
        for root, dirs, files in os.walk(self.source_root):

            def passes_all_rules(entry):
                return Ruleset.global_rules().passes(entry) and self.rules.passes(entry)

            to_keep = filter(passes_all_rules, dirs)
            del dirs[:]
            dirs.extend(to_keep)

            for file in files:
                file = os.path.join(root, file)
                if not passes_all_rules(file):
                    continue
                yield (file, self.get_tarball_path(file))


class Manifest(object):
    def __init__(self, manifest_filename, source_root, build_root, tarball_root='/'):
        self.current_directory = None
        self.directories = []
        self.tarball_root = tarball_root
        self.source_root = os.path.abspath(source_root)
        self.build_root = os.path.abspath(build_root)

        # Normalize the tarball root so that it starts and ends with a slash.
        if self.tarball_root.endswith('/'):
            self.tarball_root = self.tarball_root + '/'
        if self.tarball_root.startswith('/'):
            self.tarball_root = '/' + self.tarball_root

        with open(manifest_filename, 'r') as file:
            for line in file.readlines():
                self.process_line(line)

    def add_rule(self, rule):
        if self.current_directory is not None:
            self.current_directory.add_rule(rule)
        else:
            Ruleset.add_global_rule(rule)

    def add_directory(self, directory):
        self.current_directory = directory
        self.directories.append(directory)

    def resolve_variables(self, string, strip=False):
        if strip:
            return string.replace('$source', '').replace('$build', '')

        string = string.replace('$source', self.source_root)
        if self.build_root:
            string = string.replace('$build', self.build_root)
        elif string.find('$build') != -1:
            raise Exception('Manifest has $build but build root not given.')
        return string

    def get_full_source_path(self, source_path):
        full_source_path = self.resolve_variables(source_path)
        if not os.path.exists(full_source_path):
            full_source_path = os.path.join(self.source_root, source_path)
        if not os.path.exists(full_source_path):
            raise Exception('Could not find directory %s' % full_source_path)
        return full_source_path

    def get_full_tarball_path(self, path):
        path = self.resolve_variables(path, strip=True)
        return self.tarball_root + path

    def get_source_and_tarball_paths_from_parts(self, parts):
        full_source_path = self.get_full_source_path(parts[1])
        if len(parts) > 2:
            full_tarball_path = self.get_full_tarball_path(parts[2])
        else:
            full_tarball_path = self.get_full_tarball_path(parts[1])
        return (full_source_path, full_tarball_path)

    def process_line(self, line):
        parts = line.split()
        if not parts:
            return
        if parts[0].startswith("#"):
            return

        if parts[0] == "directory" and len(parts) > 1:
            self.add_directory(Directory(*self.get_source_and_tarball_paths_from_parts(parts)))
        elif parts[0] == "file" and len(parts) > 1:
            self.add_directory(File(*self.get_source_and_tarball_paths_from_parts(parts)))
        elif parts[0] == "exclude" and len(parts) > 1:
            self.add_rule(Rule(Rule.Result.EXCLUDE, self.resolve_variables(parts[1])))
        elif parts[0] == "include" and len(parts) > 1:
            self.add_rule(Rule(Rule.Result.INCLUDE, self.resolve_variables(parts[1])))

    def get_files(self):
        for directory in self.directories:
            for file_tuple in directory.get_files():
                yield file_tuple

    def create_tarfile(self, output):
        count = 0
        for file_tuple in self.get_files():
            count = count + 1

        with closing(tarfile.open(output, 'w')) as tarball:
            for i, (file_path, tarball_path) in enumerate(self.get_files(), start=1):
                print('Tarring file {0} of {1}'.format(i, count).ljust(40), end='\r')
                tarball.add(file_path, tarball_path)
        print("Wrote {0}".format(output).ljust(40))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Build a distribution bundle.')
    parser.add_argument('-s', '--source-directory', type=str, default=os.getcwd(),
                        help='The top-level directory of the source distribution. ' + \
                              'Directory for relative paths. Defaults to current directory.')
    parser.add_argument('--tarball-root', type=str, default='/',
                        help='The top-level path of the tarball. By default files are added to the root of the tarball.')
    parser.add_argument('-b', '--build-directory', type=str, default=None,
                        help='The top-level path of directory of the build root. ' + \
                              'By default there is no build root.')
    parser.add_argument('-o', type=str, default='out.tar', dest="output_filename",
                        help='The tarfile to produce. By default this is "out.tar"')
    parser.add_argument('manifest_filename', metavar="manifest", type=str, help='The path to the manifest file.')

    arguments = parser.parse_args()

    manifest = Manifest(arguments.manifest_filename, arguments.source_directory, arguments.build_directory, arguments.tarball_root)
    manifest.create_tarfile(arguments.output_filename)
