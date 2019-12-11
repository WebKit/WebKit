#!/usr/bin/env python

import logging
import tempfile
import os
import urllib
import shutil
import subprocess
import tarfile

from webkitpy.benchmark_runner.utils import get_path_from_project_root, force_remove
from zipfile import ZipFile


_log = logging.getLogger(__name__)


class BenchmarkBuilder(object):
    def __init__(self, name, plan, driver):
        self._name = name
        self._plan = plan
        self._driver = driver

    def __enter__(self):
        self._web_root = tempfile.mkdtemp()
        self._dest = os.path.join(self._web_root, self._name)
        if 'local_copy' in self._plan:
            self._copy_benchmark_to_temp_dir(self._plan['local_copy'])
        elif 'remote_archive' in self._plan:
            self._fetch_remote_archive(self._plan['remote_archive'])
        elif 'svn_source' in self._plan:
            self._checkout_with_subversion(self._plan['svn_source'])
        else:
            raise Exception('The benchmark location was not specified')

        _log.info('Copied the benchmark into: %s' % self._dest)
        try:
            if 'create_script' in self._plan:
                self._run_create_script(self._plan['create_script'])
            patch_file_key = "{driver}_benchmark_patch".format(driver=self._driver)
            if patch_file_key in self._plan:
                self._apply_patch(self._plan[patch_file_key])
            return self._web_root
        except Exception:
            self._clean()
            raise

    def __exit__(self, exc_type, exc_value, traceback):
        self._clean()

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
        if archive_url.endswith('.zip'):
            archive_type = 'zip'
        elif archive_url.endswith('tar.gz'):
            archive_type = 'tar.gz'
        else:
            raise Exception('Could not infer the file extention from URL: %s' % archive_url)

        archive_path = os.path.join(self._web_root, 'archive.' + archive_type)
        _log.info('Downloading %s to %s' % (archive_url, archive_path))
        urllib.urlretrieve(archive_url, archive_path)

        if archive_type == 'zip':
            with ZipFile(archive_path, 'r') as archive:
                archive.extractall(self._dest)
        elif archive_type == 'tar.gz':
            with tarfile.open(archive_path, 'r:gz') as archive:
                archive.extractall(self._dest)

        unarchived_files = filter(lambda name: not name.startswith('.'), os.listdir(self._dest))
        if len(unarchived_files) == 1:
            first_file = os.path.join(self._dest, unarchived_files[0])
            if os.path.isdir(first_file):
                shutil.move(first_file, self._web_root)
                os.rename(os.path.join(self._web_root, unarchived_files[0]), self._dest)

    def _checkout_with_subversion(self, subversion_url):
        _log.info('Checking out %s to %s' % (subversion_url, self._dest))
        error_code = subprocess.call(['svn', 'checkout', '--trust-server-cert', '--non-interactive', subversion_url, self._dest])
        if error_code:
            raise Exception('Cannot checkout the benchmark - Error: %s' % error_code)

    def _apply_patch(self, patch):
        old_working_directory = os.getcwd()
        os.chdir(self._dest)
        error_code = subprocess.call(['patch', '-p1', '-f', '-i', get_path_from_project_root(patch)])
        os.chdir(old_working_directory)
        if error_code:
            raise Exception('Cannot apply patch, will skip current benchmark_path - Error: %s' % error_code)

    def _clean(self):
        _log.info('Cleaning Benchmark')
        if self._web_root:
            force_remove(self._web_root)
