# Copyright (C) 2026 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import contextlib
import importlib.machinery
import io
import importlib.util
import os
import tempfile
import unittest
from unittest.mock import patch


SCRIPT_PATH = os.path.join(os.path.dirname(__file__), os.pardir, 'ensure-free-disk-space')
SPEC = importlib.util.spec_from_file_location(
    'ensure_free_disk_space',
    SCRIPT_PATH,
    loader=importlib.machinery.SourceFileLoader('ensure_free_disk_space', SCRIPT_PATH),
)
ensure_free_disk_space = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ensure_free_disk_space)


class FakeVolumes:
    def __init__(self, directories, volumes, free_gb, sizes_gb):
        self.directories = directories
        self.volumes = volumes
        self.base_free_gb = free_gb
        self.sizes_gb = sizes_gb

    def volume_id(self, path):
        return next(self.volumes[name] for name, directory in self.directories.items() if path == directory)

    def free_gb(self, path):
        volume = self.volume_id(path)
        freed = sum(self.sizes_gb[name] for name, directory in self.directories.items() if self.volumes[name] == volume and not os.path.exists(directory))
        return self.base_free_gb[volume] + freed


class EnsureFreeDiskSpaceTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.directories = {
            'compilation-cache': os.path.join(self.temporary_directory.name, 'CompilationCache.noindex'),
            'build': os.path.join(self.temporary_directory.name, 'WebKitBuild'),
        }
        for directory in self.directories.values():
            os.makedirs(os.path.join(directory, 'nested'))

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_with_volumes(self, volumes, free_gb, sizes_gb, minimum_free_gb=30):
        fake = FakeVolumes(self.directories, volumes, free_gb, sizes_gb)
        with patch.object(ensure_free_disk_space, 'volume_id', fake.volume_id), patch.object(ensure_free_disk_space, 'free_gb', fake.free_gb), contextlib.redirect_stdout(io.StringIO()):
            return ensure_free_disk_space.ensure_free_disk_space(self.directories, minimum_free_gb)

    def test_enough_free_space_removes_nothing(self):
        report = self.run_with_volumes({'compilation-cache': 1, 'build': 2}, {1: 380, 2: 56}, {'compilation-cache': 20, 'build': 100})
        self.assertEqual(report['removed'], [])
        self.assertTrue(report['sufficient'])
        self.assertTrue(all(os.path.isdir(directory) for directory in self.directories.values()))

    def test_shared_volume_removes_compilation_cache_before_build_directory(self):
        report = self.run_with_volumes({'compilation-cache': 1, 'build': 1}, {1: 21}, {'compilation-cache': 40, 'build': 100})
        self.assertEqual(report['removed'], ['compilation-cache'])
        self.assertEqual(report['free_gb'], {'compilation-cache': 61, 'build': 61})
        self.assertTrue(report['sufficient'])
        self.assertTrue(os.path.isdir(self.directories['build']))

    def test_shared_volume_removes_build_directory_when_cache_is_not_enough(self):
        report = self.run_with_volumes({'compilation-cache': 1, 'build': 1}, {1: 5}, {'compilation-cache': 10, 'build': 100})
        self.assertEqual(report['removed'], ['compilation-cache', 'build'])
        self.assertEqual(report['initial_free_gb'], {'compilation-cache': 5, 'build': 5})
        self.assertTrue(report['sufficient'])

    def test_separate_volumes_only_cleans_the_low_volume(self):
        report = self.run_with_volumes({'compilation-cache': 1, 'build': 2}, {1: 380, 2: 3}, {'compilation-cache': 20, 'build': 100})
        self.assertEqual(report['removed'], ['build'])
        self.assertTrue(report['sufficient'])
        self.assertTrue(os.path.isdir(self.directories['compilation-cache']))
        self.assertFalse(os.path.exists(self.directories['build']))

    def test_reports_insufficient_space_after_cleanup(self):
        report = self.run_with_volumes({'compilation-cache': 1, 'build': 2}, {1: 380, 2: 3}, {'compilation-cache': 20, 'build': 10})
        self.assertEqual(report['removed'], ['build'])
        self.assertEqual(report['free_gb'], {'compilation-cache': 380, 'build': 13})
        self.assertFalse(report['sufficient'])

    def test_missing_directories_are_not_reported_as_removed(self):
        for directory in self.directories.values():
            os.rmdir(os.path.join(directory, 'nested'))
            os.rmdir(directory)
        report = self.run_with_volumes({'compilation-cache': 1, 'build': 1}, {1: 3}, {'compilation-cache': 0, 'build': 0})
        self.assertEqual(report['removed'], [])
        self.assertFalse(report['sufficient'])

    def test_reports_directories_that_could_not_be_removed(self):
        with patch.object(ensure_free_disk_space.shutil, 'rmtree', side_effect=PermissionError('Operation not permitted')):
            report = self.run_with_volumes({'compilation-cache': 1, 'build': 2}, {1: 380, 2: 3}, {'compilation-cache': 20, 'build': 100})
        self.assertEqual(report['removed'], [])
        self.assertEqual(report['failed_to_remove'], ['build'])
        self.assertFalse(report['sufficient'])

    def test_free_gb_and_volume_id_handle_missing_paths(self):
        missing = os.path.join(self.temporary_directory.name, 'missing', 'path')
        self.assertEqual(ensure_free_disk_space.volume_id(missing), os.stat(self.temporary_directory.name).st_dev)
        self.assertGreater(ensure_free_disk_space.free_gb(missing), 0)


if __name__ == '__main__':
    unittest.main()
