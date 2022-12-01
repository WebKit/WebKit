import logging
import tempfile
import os
import re
import shutil
import subprocess
import sys
import tarfile

from webkitpy.benchmark_runner.utils import get_path_from_project_root, force_remove
from webkitpy.benchmark_runner.github_downloader import GithubDownloadTask
from zipfile import ZipFile

if sys.version_info > (3, 0):
    from urllib.request import urlretrieve
else:
    from urllib import urlretrieve


_log = logging.getLogger(__name__)


class BenchmarkBuilder(object):
    LOCAL_GIT_ARCHIVE_SCHEMA = re.compile(r'\A(?P<path>.+)@(?P<reference>[0-9a-zA-z.\-]+)\Z')

    def __init__(self, name, plan, driver, enable_signposts=False):
        self._name = name
        self._plan = plan
        self._driver = driver
        self._enable_signposts = enable_signposts

    def __enter__(self):
        self._web_root = tempfile.mkdtemp(dir="/tmp")
        self._dest = os.path.join(self._web_root, self._name)
        if 'local_copy' in self._plan:
            self._copy_benchmark_to_temp_dir(self._plan['local_copy'])
        elif 'remote_archive' in self._plan:
            self._fetch_remote_archive(self._plan['remote_archive'])
        elif 'svn_source' in self._plan:
            self._checkout_with_subversion(self._plan['svn_source'])
        elif self._local_git_archive_eligible():
            self._prepare_content_from_local_git_archive(self._plan['local_git_archive'])
        elif 'github_source' in self._plan:
            self._download_from_github(self._plan['github_source'], self._plan.get('github_subtree'))
        else:
            raise Exception('The benchmark location was not specified')

        _log.info('Copied the benchmark into: %s' % self._dest)
        try:
            if 'create_script' in self._plan:
                self._run_create_script(self._plan['create_script'])
            patch_file_key = "{driver}_benchmark_patch".format(driver=self._driver)
            if patch_file_key in self._plan:
                self._apply_patch(self._plan[patch_file_key])
            if self._enable_signposts:
                if 'signpost_patch' in self._plan:
                    self._apply_patch(self._plan['signpost_patch'])
                else:
                    _log.warning('Signposts are enabled but a signpost patch was not found in the test plan. Skipping.')
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
        urlretrieve(archive_url, archive_path)

        if archive_type == 'zip':
            with ZipFile(archive_path, 'r') as archive:
                archive.extractall(self._dest)
        elif archive_type == 'tar.gz':
            with tarfile.open(archive_path, 'r:gz') as archive:
                archive.extractall(self._dest)

        unarchived_files = [name for name in os.listdir(self._dest) if not name.startswith('.')]
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

    def _download_from_github(self, github_source, github_subtree):
        _log.info('Downloading content from {}'.format(github_source))
        GithubDownloadTask(github_source, github_subtree).execute(self._dest)

    def _local_git_archive_eligible(self):
        if 'local_git_archive' not in self._plan:
            return False

        match = self.LOCAL_GIT_ARCHIVE_SCHEMA.match(self._plan['local_git_archive'])
        if not match:
            return False

        return not subprocess.call(['git', '-C', os.path.dirname(__file__), 'cat-file', '-e', match.group('reference')],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def _prepare_content_from_local_git_archive(self, local_git_archive):
        match = self.LOCAL_GIT_ARCHIVE_SCHEMA.match(local_git_archive)
        relpath_in_repo = match.group('path').lstrip('/')
        reference = match.group('reference')
        with tempfile.TemporaryDirectory() as temp_dir:
            output = os.path.join(temp_dir, 'temp.tar.gz')
            subprocess.check_call(['git', 'archive', '--format=tar.gz', reference, relpath_in_repo, '-o', output],
                                  cwd=get_path_from_project_root('../../../../'))
            temp_extract_path = os.path.join(temp_dir, 'extract')
            os.makedirs(temp_extract_path)
            subprocess.check_call(['tar', 'zxvf', output, '-C', temp_extract_path])
            shutil.copytree(os.path.join(temp_extract_path, relpath_in_repo), self._dest)

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
