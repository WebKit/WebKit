import logging
import os.path
import re
import subprocess
import time
from multiprocessing import Pool
from functools import wraps

import requests


_log = logging.getLogger(__name__)


class GithubDownloadTask(object):
    URL_RE = re.compile(r'\Ahttps?://github.(?P<domain>\S+)/(?P<owner>\S+)/(?P<repository>\S+)/tree/(?P<commit>[0-9a-zA-Z]+)/(?P<relpath>\S+)\Z')

    def __init__(self, source_url, subtree=None):
        match = self.URL_RE.match(source_url)
        assert match, 'Unrecognized source URL: {}'.format(source_url)
        owner = match.group('owner')
        repository = match.group('repository')
        api_url = 'https://api.github.{}'.format(match.group('domain'))
        self._downloader = GithubDownloader(api_url, owner, repository)
        self._commit = match.group('commit')
        self._relpath = match.group('relpath')
        self._subtree = subtree

    def execute(self, dest):
        self._downloader.download_content(self._relpath, self._commit, dest, subtree=self.subtree)

    @property
    def subtree(self):
        if self._subtree is None:
            self._subtree = self._downloader.subtree_for_path_with_commit(self._relpath, self._commit)
        return self._subtree


def retry_on_exception(count):
    def decorator(function):
        @wraps(function)
        def wrapped(*args, **kwargs):
            remaining_retry = count + 1
            while remaining_retry:
                remaining_retry -= 1
                try:
                    return function(*args, **kwargs)
                except Exception as error:
                    _log.warning('Encountered {} in {}, remaining retry: {}.'.format(error, function.__name__, remaining_retry))
                    if not remaining_retry:
                        raise
        return wrapped
    return decorator


class GithubDownloader(object):
    def __init__(self, url, owner, repository):
        self._url = url
        self._owner = owner
        self._repository = repository
        self._session = None

    @classmethod
    def is_directory(cls, entry):
        return entry['type'] == 'tree'

    @classmethod
    def is_file(cls, entry):
        return entry['type'] == 'blob' and not cls.is_symlink(entry)

    @classmethod
    def is_symlink(cls, entry):
        return entry['mode'].startswith('120')

    @property
    def session(self):
        if self._session is None:
            self._session = requests.Session()
        return self._session

    def download_content(self, relpath_in_repo, commit_hash, destination, subtree=None):
        relpath_in_repo = os.path.normpath(relpath_in_repo)
        # "subtree" can be None, a subtree url or a cached and compressed http response from subtree API.
        # Latter two are intended to reduce Github api call.
        # The second will issue one API request, and the last will NOT issue any API request.
        # Unauthenticated user will be limited to 60 requests per hour.
        # https://docs.github.com/en/rest/rate-limit#about-the-rate-limit-api
        if subtree is None:
            subtree = self.subtree_for_path_with_commit(relpath_in_repo, commit_hash)
        elif not isinstance(subtree, dict):
            subtree = self._subtree(subtree, recursive=True)
        return self._download_subtree_multiprocessing(relpath_in_repo, commit_hash, subtree, destination)

    def subtree_for_path_with_commit(self, relpath_in_repo, commit_hash):
        commit = self._commit(commit_hash)
        subtree = self._subtree(commit['tree']['url'])
        parts = relpath_in_repo.split(os.path.sep)
        last_index = len(parts) - 1
        visited_path = []
        for index, part in enumerate(parts):
            for tree in subtree['tree']:
                if tree['path'] == part:
                    subtree = self._subtree(tree['url'], recursive=(index == last_index))
                    visited_path.append(part)
                    break
            else:
                raise Exception('Path {} does not exists under {}.'.format(part, os.path.join(*visited_path)))
        return subtree

    def _download_subtree_multiprocessing(self, base_relpath_in_repo, commit_hash, subtree, output_directory):
        _log.info('Checking out content under {}:'.format(base_relpath_in_repo))
        result_by_path = {}

        with Pool(10) as pool:
            for entry in self.walk(subtree):
                rel_path_in_repo = os.path.join(base_relpath_in_repo, entry['path'])
                destination_path = os.path.join(output_directory, entry['path'])
                mode = entry['mode']
                if self.is_directory(entry):
                    os.makedirs(destination_path, exist_ok=True)
                    _log.info(str(rel_path_in_repo))
                elif self.is_symlink(entry):
                    result_by_path[rel_path_in_repo] = pool.apply_async(self._resolve_symlink,
                                                                        (commit_hash, rel_path_in_repo, destination_path))
                elif self.is_file(entry):
                    result_by_path[rel_path_in_repo] = pool.apply_async(self._download_raw_content,
                                                                        (commit_hash, rel_path_in_repo, destination_path))
                else:
                    raise Exception('Unrecognized file: {} with mode {}'.format(rel_path_in_repo, mode))
            for path, result in result_by_path.items():
                result.wait()
                assert result.successful(), 'Failed to download {}'.format(path)
                _log.info(str(path))

        for entry in self.walk(subtree):
            mode = entry['mode']
            if mode.endswith('000'):
                continue
            os.chmod(destination_path, int(mode[-3:], 8))

    def walk(self, subtree, is_full_subtree=True):
        if is_full_subtree and not subtree['truncated']:
            for entry in subtree['tree']:
                yield entry
            return
        subtree = self._subtree(subtree['url'])
        for entry in self._walk(subtree):
            yield entry

    def _walk(self, subtree):
        for entry in subtree['tree']:
            yield entry
            if not self.is_directory(entry['mode']):
                continue
            for sub_entry in self.walk(self._subtree(entry['url'])):
                path = os.path.join(entry['path'], sub_entry['path'])
                sub_entry['path'] = path
                yield sub_entry

    def _subtree(self, url, **kwargs):
        return self._request(url, kwargs)

    def _commit(self, commit_hash):
        url = '{}/repos/{}/{}/git/commits/{}'.format(self._url, self._owner, self._repository, commit_hash)
        return self._request(url)

    def _raw_content_url(self, commit_hash, rel_path_in_repo):
        return 'https://raw.githubusercontent.com/{}/{}/{}/{}'.format(self._owner, self._repository, commit_hash, rel_path_in_repo)

    @retry_on_exception(3)
    def _download_raw_content(self, commit_hash, rel_path_in_repo, destination_path):
        subprocess.check_call(['/usr/bin/curl', self._raw_content_url(commit_hash, rel_path_in_repo), '-o',
                               destination_path], stderr=subprocess.DEVNULL)

    @retry_on_exception(3)
    def _resolve_symlink(self, commit_hash, rel_path_in_repo, destination_path):
        target = subprocess.check_output(['/usr/bin/curl', self._raw_content_url(commit_hash, rel_path_in_repo)],
                                         stderr=subprocess.DEVNULL, encoding='utf-8')
        os.symlink(target, destination_path)

    def _request(self, url, params=None):
        params = params or dict()
        _log.info('Accessing {} with {}'.format(url, params))
        response = self.session.get(url, headers={'Accept': 'application/vnd.github+json'}, params=params).json()
        if 'API rate limit exceeded' in response.get('message', ''):
            reset_time = int(response.headers.get('X-RateLimit-Reset', '0'))
            if not reset_time:
                rate_limit = self.session.get('{}/rate_limit'.format(self._url)).json()
                reset_time = rate_limit['resources']['core']['reset']
            wait_time = reset_time - int(time.time())
            _log.info('Hit rate limit, will wait for {} seconds'.format(wait_time))
            time.sleep(wait_time)
            return self._request(url, params)
        return response
