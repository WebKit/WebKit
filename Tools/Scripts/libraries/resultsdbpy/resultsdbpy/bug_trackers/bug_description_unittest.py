import unittest
import bisect
from .bug_description import translate_selected_dots_to_bug_title_and_description
from .bugzilla import WebKitBugzilla

MOCK_COMMITS = {
    'webkit': {
        'main': [
            {
                'author': {
                    'emails': ['simon.fraser@apple.com'],
                    'name': 'simon.fraser@apple.com'
                },
                'branch': 'main',
                'bugUrls': ['https://bugs.webkit.org/show_bug.cgi?id=236671'],
                'hash': None,
                'identifier': '247341@main',
                'message': 'some msg',
                'order': 0,
                'radarUrls': None,
                'repository_id': 'webkit',
                'revision': 289911,
                'timestamp': 1645036552,
                'uuid': 164880465000
            }, {
                'author': {
                    'emails': ['pvollan@apple.com'],
                    'name': 'pvollan@apple.com'
                },
                'branch': 'main',
                'bugUrls': ['https://bugs.webkit.org/show_bug.cgi?id=236653'],
                'hash': None,
                'identifier': '247345@main',
                'message': 'some msg',
                'order': 0,
                'radarUrls': ['rdar://88787266'],
                'repository_id': "webkit",
                'revision': 289926,
                'timestamp': 1645041304,
                'uuid': 164881488600
            }, {
                'author': {
                    'emails': ['pvollan@apple.com'],
                    'name': 'pvollan@apple.com'
                },
                'branch': 'main',
                'bugUrls': ['https://bugs.webkit.org/show_bug.cgi?id=236653'],
                'hash': None,
                'identifier': '247348@main',
                'message': 'some msg',
                'order': 0,
                'radarUrls': ['rdar://88787266'],
                'repository_id': "webkit",
                'revision': 289926,
                'timestamp': 1645041304,
                'uuid': 164882229900
            }
        ]
    }
}


class MockCommit(object):

    def __init__(self, commit_data):
        for k, v in commit_data.items():
            setattr(self, k, v)


class MockCommitContext(object):

    def __init__(self, mock_commits) -> None:
        self._mock_commits = mock_commits

    def find_commits_by_uuid(self, repository_id, branch, uuid):
        commits = self._mock_commits[repository_id][branch]
        commit_data_index = bisect.bisect_left(list(map(lambda commit_data: commit_data['uuid'], commits)), uuid)
        if commit_data_index == len(commits):
            commit_data_index -= 1
        commit_data = commits[commit_data_index]
        return [MockCommit(commit_data)]

    def url(self, commit):
        if commit.repository_id == 'webkit':
            return 'https://trac.webkit.org/changeset/{}/webkit'.format(commit.revision)
        elif commit.repository_id == 'safari':
            return 'https://stashweb.sd.apple.com/projects/SAFARI/repos/safari/commits/{}'.format(commit.hash)


class TestBugDescription(unittest.TestCase):

    def setUp(self):
        self.bugzilla = WebKitBugzilla()
        self.bugzilla.set_commit_context(MockCommitContext(MOCK_COMMITS))
        return super().setUp()

    def test_single_row_and_single_test_failure(self):

        mock_data = {
            "selectedRows": [{
                "config": {
                    "platform": "mac",
                    "version": 12003000,
                    "version_name": "Monterey E",
                    "sdk": None,
                    "is_simulator": False,
                    "style": "debug",
                    "flavor": "wk2",
                    "model": "Macmini8,1",
                    "architecture": "x86_64"
                },
                "results": [
                    {
                        "actual": "TEXT",
                        "expected": "PASS",
                        "start_time": 1648825539,
                        "time": 307,
                        "uuid": 164882229900,
                        "_dotCenter": {"x": 195, "y": 9},
                        "_dotRadius": 9,
                        "_cachedScrollLeft": 0,
                        "_index": 1,
                        "tipPoints": [{"x": 195, "y": -4.5}, {"x": 195, "y": 13.5}],
                        "_selected": True
                    },
                    {
                        "actual": "TEXT",
                        "expected": "PASS",
                        "start_time": 1648818741,
                        "time": 285,
                        "uuid": 164881488600,
                        "_dotCenter": {"x": 255, "y": 9},
                        "_dotRadius": 9,
                        "_cachedScrollLeft": 0,
                        "tipPoints": [{"x": 255, "y": -4.5}, {"x": 255, "y": 13.5}],
                        "_index": 2,
                        "_selected": True
                    },
                    {
                        "actual": "TEXT",
                        "expected": "PASS",
                        "start_time": 1648811531,
                        "time": 355,
                        "uuid": 164880465000,
                        "_dotCenter": {"x": 375, "y": 9},
                        "_dotRadius": 9,
                        "_cachedScrollLeft": 0,
                        "_index": 3,
                        "tipPoints": [{"x": 375, "y": -4.5}, {"x": 375, "y": 13.5}],
                        "_selected": True
                    }
                ]
            }],
            "willFilterExpected": False,
            "repositories": ["webkit"],
            "suite": "layout-tests",
            "test": "http/tests/paymentrequest/updateWith-shippingOptions.https.html"
        }

        res = self.bugzilla.create_bug(mock_data)
        self.assertEqual(res['url'], 'https://bugs.webkit.org/enter_bug.cgi?product=WebKit&component=New%20Bugs&version=WebKit%20Nightly%20Build&short_desc=http/tests/paymentrequest/updateWith-shippingOptions.https.html ERROR%20on%20Monterey%20E%28Macmini8%2C1%29&comment=Hardware%3A%20%20%20%20%20%09Macmini8%2C1%0AArchitecture%3A%20%09x86_64%0AOS%3A%20%20%20%20%20%20%20%20%20%20%20%09Monterey%20E%0AStyle%3A%20%20%20%20%20%20%20%20%09debug%0AFlavor%20%20%20%20%20%20%20%20%09wk2%0ASDK%3A%20%20%20%20%20%20%20%20%20%20%09None%0A-----------------------------%0AMost%20recent%20failures%3A%0A247348%40main%3A%20https%3A//trac.webkit.org/changeset/289926/webkit%0A-----------------------------')
