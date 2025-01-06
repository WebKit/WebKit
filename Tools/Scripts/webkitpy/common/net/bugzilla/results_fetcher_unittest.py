# Copyright (C) 2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import json
import unittest

from mock import mock
from webkitcorepy.testing import PathTestCase
from webkitscmpy import PullRequest, local, mocks, remote

from webkitpy.common.net.bugzilla import results_fetcher
from webkitpy.common.net.bugzilla.attachment import Attachment


class MockAttachment(Attachment):
    def __init__(self, attachment_dictionary, contents, is_patch, is_obsolete):
        Attachment.__init__(self, attachment_dictionary, self)
        self._contents = contents
        self._is_patch = is_patch
        self._is_obsolete = is_obsolete

    def contents(self):
        return self._contents

    def is_patch(self):
        return self._is_patch

    def is_obsolete(self):
        return self._is_obsolete


class MockBugzilla:
    def fetch_bug(self, _):
        return self

    def attachments(self, include_obsolete=False):
        attachments = []
        attachments.append(
            MockAttachment(
                {"id": 1, "name": "Test case"},
                "How to reproduce the issue",
                False,
                False,
            )
        )

        if include_obsolete:
            attachments.append(
                MockAttachment({"id": 2, "name": "Patch"}, "Patch v1", True, True)
            )
            attachments.append(
                MockAttachment({"id": 3, "name": "Patch"}, "Patch v2", True, True)
            )

        attachments.append(
            MockAttachment({"id": 4, "name": "Patch"}, "Patch v3", True, False)
        )

        return attachments


class MockRequestsGet:
    def __init__(self, url):
        self._urls_data = {
            "https://ews.webkit.org/status/4/": b'{"mac-wk1": {"url": "https://ews-build.webkit.org/#/builders/1/builds/99"}, "mac-wk2": {"url": "https://ews-build.webkit.org/#/builders/2/builds/99"}, "mac-debug-wk1": {"url": "https://ews-build.webkit.org/#/builders/3/builds/99"}, "ios": {"url": "https://ews-build.webkit.org/#/builders/4/builds/99"}, "win": {"url": "https://ews-build.webkit.org/#/builders/5/builds/99"}}',
            "https://ews-build.webkit.org/api/v2/builders": b'{"builders": [{"builderid": 1, "description": "mac-wk1"}, {"builderid": 2, "description": "mac-wk2"}, {"builderid": 3, "description": "mac-debug-wk1"}, {"builderid": 4, "description": "ios"}, {"builderid": 5, "description": "win"}]}',
            "https://ews-build.webkit.org/api/v2/builders/1/builds/99/steps": b'{ "steps": [ { "complete": true, "name": "layout-tests", "results": 2, "state_string": "Ran layout tests",  "urls": [ { "name": "download layout test results", "url": "/results/mac-wk1/r12345.zip" } ] } ] }',
            "https://ews-build.webkit.org/api/v2/builders/2/builds/99/steps": b'{ "steps": [ { "complete": true, "name": "layout-tests", "results": 2, "state_string": "Ran layout tests",  "urls": [ { "name": "download layout test results", "url": "/results/mac-wk2/r12345.zip" } ] } ] }',
            "https://ews-build.webkit.org/api/v2/builders/3/builds/99/steps": b'{ "steps": [ { "complete": true, "name": "layout-tests", "results": 2, "state_string": "Ran layout tests",  "urls": [ { "name": "download layout test results", "url": "/results/mac-debug-wk1/r12345.zip" } ] } ] }',
            "https://ews-build.webkit.org/api/v2/builders/4/builds/99/steps": b'{ "steps": [ { "complete": true, "name": "layout-tests", "results": 2, "state_string": "Ran layout tests",  "urls": [ { "name": "download layout test results", "url": "/results/ios/r12345.zip" }  ] } ] }',
            "https://ews-build.webkit.org/api/v2/builders/5/builds/99/steps": b'{ "steps": [ { "complete": true, "name": "layout-tests", "results": 2, "state_string": "Ran layout tests",  "urls": [ { "name": "download layout test results", "url": "/results/win/r12345.zip" }  ] } ] }',
        }
        self._url = url
        if url not in self._urls_data:
            msg = "url {} not mocked".format(url)
            raise ValueError(msg)

    @property
    def content(self):
        c = self._urls_data[self._url]
        assert isinstance(c, bytes)
        return c

    @property
    def text(self):
        return self.content.decode("utf-8")

    def raise_for_status(self):
        pass

    def json(self):
        return json.loads(self.content)


class ResultsFetcherTest(unittest.TestCase):
    def test_is_relevant_results(self):
        # Incomplete.
        self.assertFalse(
            results_fetcher._is_relevant_results(
                "foo",
                {
                    "buildid": 588706,
                    "complete": False,
                    "complete_at": None,
                    "hidden": False,
                    "name": "layout-tests",
                    "number": 17,
                    "results": None,
                    "started_at": 1693871103,
                    "state_string": "layout-tests running",
                    "stepid": 8989299,
                    "urls": [],
                },
            )
        )

        # Completed, failures.
        self.assertTrue(
            results_fetcher._is_relevant_results(
                "foo",
                {
                    "buildid": 588183,
                    "complete": True,
                    "complete_at": 1693846670,
                    "hidden": False,
                    "name": "layout-tests",
                    "number": 17,
                    "results": 2,
                    "started_at": 1693844047,
                    "state_string": "Exiting early after 60 failures. 47470 tests run. 60 failures",
                    "stepid": 8980814,
                    "urls": [
                        {
                            "name": "view layout test results",
                            "url": "https://ews-build.s3-us-west-2.amazonaws.com/macOS-AppleSilicon-Ventura-Debug-WK2-Tests-EWS/8d9c2057-13881/results.html",
                        },
                        {
                            "name": "download layout test results",
                            "url": "https://ews-build.s3-us-west-2.amazonaws.com/macOS-AppleSilicon-Ventura-Debug-WK2-Tests-EWS/8d9c2057-13881.zip",
                        },
                    ],
                },
            )
        )

        # Completed with failures, but re-run.
        self.assertFalse(
            results_fetcher._is_relevant_results(
                "foo",
                {
                    "buildid": 588183,
                    "complete": True,
                    "complete_at": 1693849318,
                    "hidden": False,
                    "name": "re-run-layout-tests",
                    "number": 23,
                    "results": 2,
                    "started_at": 1693846711,
                    "state_string": "Exiting early after 60 failures. 47458 tests run. 60 failures",
                    "stepid": 8982249,
                    "urls": [
                        {
                            "name": "view layout test results",
                            "url": "https://ews-build.s3-us-west-2.amazonaws.com/macOS-AppleSilicon-Ventura-Debug-WK2-Tests-EWS/8d9c2057-13881-rerun/results.html",
                        },
                        {
                            "name": "download layout test results",
                            "url": "https://ews-build.s3-us-west-2.amazonaws.com/macOS-AppleSilicon-Ventura-Debug-WK2-Tests-EWS/8d9c2057-13881-rerun.zip",
                        },
                    ],
                },
            )
        )

        # Complete with warnings.
        self.assertTrue(
            results_fetcher._is_relevant_results(
                "foo",
                {
                    "buildid": 588085,
                    "complete": True,
                    "complete_at": 1693846848,
                    "hidden": False,
                    "name": "layout-tests",
                    "number": 17,
                    "results": 1,
                    "started_at": 1693842245,
                    "state_string": "Ignored 1 pre-existing failure based on results-db",
                    "stepid": 8979256,
                    "urls": [
                        {
                            "name": "view layout test results",
                            "url": "https://ews-build.s3-us-west-2.amazonaws.com/macOS-AppleSilicon-Ventura-Debug-WK2-Tests-EWS/13436a69-13880/results.html",
                        },
                        {
                            "name": "download layout test results",
                            "url": "https://ews-build.s3-us-west-2.amazonaws.com/macOS-AppleSilicon-Ventura-Debug-WK2-Tests-EWS/13436a69-13880.zip",
                        },
                    ],
                },
            )
        )

    def test_lookup_ews_results_from_bugzilla(self):
        with mock.patch("requests.Session.get", MockRequestsGet), mock.patch("requests.get", MockRequestsGet):
            actual = results_fetcher.lookup_ews_results_from_bugzilla("123456", True, MockBugzilla())
            expected = {
                "mac-wk1": [
                    "https://ews-build.webkit.org/results/mac-wk1/r12345.zip",
                    "https://ews-build.webkit.org/results/mac-debug-wk1/r12345.zip",
                ],
                "mac-wk2": ["https://ews-build.webkit.org/results/mac-wk2/r12345.zip"],
                "ios": ["https://ews-build.webkit.org/results/ios/r12345.zip"],
                "win": ["https://ews-build.webkit.org/results/win/r12345.zip"],
            }
            self.assertEqual(expected, actual)


class ResultsFetcherGitHubTest(PathTestCase):
    basepath = "dir"

    @classmethod
    def webserver(cls):
        result = mocks.remote.GitHub()
        result.users.create("Eager Reviewer", "ereviewer", ["ereviewer@webkit.org"])
        result.users.create("Reluctant Reviewer", "rreviewer", ["rreviewer@webkit.org"])
        result.users.create(
            "Suspicious Reviewer", "sreviewer", ["sreviewer@webkit.org"]
        )
        result.users.create(
            "Tim Contributor", "tcontributor", ["tcontributor@webkit.org"]
        )
        result.issues = {
            1: {
                "number": 1,
                "opened": True,
                "title": "Example Change",
                "description": "?",
                "creator": result.users.create(
                    name="Tim Contributor", username="tcontributor"
                ),
                "timestamp": 1639536160,
                "assignee": None,
                "comments": [],
            },
        }
        result.pull_requests = [
            {
                "number": 1,
                "state": "open",
                "title": "Example Change",
                "user": {"login": "tcontributor"},
                "body": """#### 95507e3a1a4a919d1a156abbc279fdf6d24b13f5
<pre>
Example Change
<a href="https://bugs.webkit.org/show_bug.cgi?id=1234">https://bugs.webkit.org/show_bug.cgi?id=1234</a>

Reviewed by NOBODY (OOPS!).

* Source/file.cpp:
</pre>
""",
                "head": {
                    "ref": "branch-a",
                    "sha": "95507e3a1a4a919d1a156abbc279fdf6d24b13f5",
                },
                "base": {"ref": "main"},
                "requested_reviews": [{"login": "rreviewer"}],
                "reviews": [
                    {"user": {"login": "ereviewer"}, "state": "APPROVED"},
                    {"user": {"login": "sreviewer"}, "state": "CHANGES_REQUESTED"},
                ],
                "_links": {
                    "issue": {"href": "https://{}/issues/1".format(result.api_remote)},
                },
                "draft": False,
            }
        ]

        result.statuses["95507e3a1a4a"] = PullRequest.Status.Encoder().default(
            [
                PullRequest.Status(
                    name="test-webkitpy",
                    status="pending",
                    description="Running...",
                    url="http://example.com/1",
                ),
                PullRequest.Status(
                    name="test-webkitcorepy",
                    status="success",
                    description="Finished!",
                    url="http://example.com/2",
                ),
                PullRequest.Status(
                    name="test-webkitscmpy",
                    status="failure",
                    description="Failed webkitscmpy.test.pull_request_unittest.TestNetworkPullRequestGitHub.test_status",
                    url="http://example.com/3",
                ),
            ]
        )

        return result

    def test_lookup_ews_results_from_pr(self):
        with self.webserver():
            repo = remote.GitHub("https://github.example.com/WebKit/WebKit")
            pr = repo.pull_requests.get(1)

            with mock.patch(
                "webkitpy.common.net.bugzilla.results_fetcher.lookup_ews_results"
            ) as m:
                results_fetcher.lookup_ews_results_from_pr(pr)
                m.assert_called_once_with(
                    [
                        "http://example.com/1",
                        "http://example.com/2",
                        "http://example.com/3",
                    ]
                )

    def test_lookup_ews_results_from_pr_no_status(self):
        remote_mock = self.webserver()
        remote_mock.statuses = {}
        with remote_mock:
            repo = remote.GitHub("https://github.example.com/WebKit/WebKit")
            pr = repo.pull_requests.get(1)

            with mock.patch(
                "webkitpy.common.net.bugzilla.results_fetcher.lookup_ews_results"
            ) as m:
                results_fetcher.lookup_ews_results_from_pr(pr)
                m.assert_called_once_with([])

    def test_lookup_ews_results_from_repo_via_pr(self):
        with self.webserver() as remote_mock_repo, mocks.local.Git(
            self.path,
            remote="https://{}".format(remote_mock_repo.remote),
            remotes={
                "fork": "https://{}/Contributor/WebKit".format(
                    remote_mock_repo.hosts[0]
                )
            },
            default_branch="branch-a",
        ), mock.patch(
            "webkitpy.common.net.bugzilla.results_fetcher.lookup_ews_results_from_pr"
        ) as m:
            remote_repo = remote.GitHub("https://github.example.com/WebKit/WebKit")
            pr = remote_repo.pull_requests.get(1)
            local_repo = local.Git(self.path)
            results_fetcher.lookup_ews_results_from_repo_via_pr(local_repo)
            m.assert_called_once()
            args, kwargs = m.call_args
            self.assertEqual(len(args), 1)
            self.assertEqual(kwargs, {})
            called_pr = args[0]
            # PullRequest objects don't implement ==, so use the URL as a substitute to
            # check we have the same PR.
            self.assertEqual(pr.url, called_pr.url)

    def test_lookup_ews_results_from_repo_via_pr_missing_pr(self):
        with self.webserver() as remote_mock_repo, mocks.local.Git(
            self.path,
            remote="https://{}".format(remote_mock_repo.remote),
            remotes={
                "fork": "https://{}/Contributor/WebKit".format(
                    remote_mock_repo.hosts[0]
                )
            },
            default_branch="main",
        ), mock.patch(
            "webkitpy.common.net.bugzilla.results_fetcher.lookup_ews_results_from_pr"
        ) as m:
            remote_repo = remote.GitHub("https://github.example.com/WebKit/WebKit")
            remote_repo.pull_requests.get(1)
            local_repo = local.Git(self.path)
            with self.assertRaises(ValueError, msg="No PR found for current branch"):
                results_fetcher.lookup_ews_results_from_repo_via_pr(local_repo)
            m.assert_not_called()
