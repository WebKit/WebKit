#!/usr/bin/env python

import logging
import tempfile
import os
import urllib
import shutil
import subprocess

from zipfile import ZipFile
from webkitpy.benchmark_runner.utils import get_path_from_project_root, force_remove


_log = logging.getLogger(__name__)


class GenericBenchmarkBuilder(object):

    builder_name = 'GenericBenchmarkBuilder'

    def prepare(self, name, plan):
        self._name = name
        self._web_root = tempfile.mkdtemp()
        self._dest = os.path.join(self._web_root, self._name)
        if 'local_copy' in plan:
            self._copy_benchmark_to_temp_dir(plan['local_copy'])
        elif 'remote_archive' in plan:
            self._fetch_remote_archive(plan['remote_archive'])
        elif 'svn_source' in plan:
            self._checkout_with_subversion(plan['svn_source'])
        else:
            raise Exception('The benchmark location was not specified')

        _log.info('Copied the benchmark into: %s' % self._dest)
        try:
            if 'create_script' in plan:
                self._run_create_script(plan['create_script'])
            if 'benchmark_patch' in plan:
                self._apply_patch(plan['benchmark_patch'])
            return self._web_root
        except Exception:
            self.clean()
            raise

    def _run_create_script(self, create_script):
        old_working_directory = os.getcwd()
        os.chdir(self._dest)
        _log.debug('Running %s in %s' % (create_script, self._dest))
        error_code = subprocess.call(create_script)
        os.chdir(old_working_directory)
        if error_code:
            raise Exception('Cannot create the benchmark - Error: %s' % error_code)

    def _copy_benchmark_to_temp_dir(self, benchmark_path):
        shutil.copytree(get_path_from_project_root(benchmark_path), self._dest)

    def _fetch_remote_archive(self, archive_url):
        archive_path = os.path.join(self._web_root, 'archive.zip')
        _log.info('Downloading %s to %s' % (archive_url, archive_path))
        urllib.urlretrieve(archive_url, archive_path)

        with ZipFile(archive_path, 'r') as archive:
            archive.extractall(self._dest)

        unarchived_files = filter(lambda name: not name.startswith('.'), os.listdir(self._dest))
        if len(unarchived_files) == 1:
            first_file = os.path.join(self._dest, unarchived_files[0])
            if os.path.isdir(first_file):
                shutil.move(first_file, self._web_root)
                os.rename(os.path.join(self._web_root, unarchived_files[0]), self._dest)

    def _checkout_with_subversion(self, subversion_url):
        _log.info('Checking out %s to %s' % (subversion_url, self._dest))
        error_code = subprocess.call(['svn', 'checkout', subversion_url, self._dest])
        if error_code:
            raise Exception('Cannot checkout the benchmark - Error: %s' % error_code)

    def _apply_patch(self, patch):
        old_working_directory = os.getcwd()
        os.chdir(self._dest)
        error_code = subprocess.call(['patch', '-p1', '-f', '-i', get_path_from_project_root(patch)])
        os.chdir(old_working_directory)
        if error_code:
            raise Exception('Cannot apply patch, will skip current benchmark_path - Error: %s' % error_code)

    def clean(self):
        _log.info('Cleaning Benchmark')
        if self._web_root:
            force_remove(self._web_root)
