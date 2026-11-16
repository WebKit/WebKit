# Copyright 2016 The Chromium Authors. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import base64
import json
import logging
import re

from collections import namedtuple
from webkitcorepy import string_utils

from webkitpy.common.memoized import memoized
from webkitpy.w3c.common import WPT_GH_ORG, WPT_GH_REPO_NAME

from urllib.parse import quote

_log = logging.getLogger(__name__)
API_BASE = 'https://api.github.com'


class WPTGitHub(object):
    """An interface to GitHub for interacting with the web-platform-tests repo.

    This class contains methods for sending requests to the GitHub API.
    Unless mentioned otherwise, API calls are expected to succeed, and
    GitHubError will be raised if an API call fails.
    """

    def __init__(self, host, user=None, token=None):
        self.host = host
        self.user = user
        self.token = token

    def has_credentials(self):
        return self.user and self.token

    def auth_token(self):
        assert self.has_credentials()
        return string_utils.decode(base64.b64encode(string_utils.encode('{}:{}'.format(self.user, self.token))), target_type=str)

    def request(self, path, method, body=None):
        """Sends a request to GitHub API and deserializes the response.

        Args:
            path: API endpoint without base URL (starting with '/').
            method: HTTP method to be used for this request.
            body: Optional payload in the request body (default=None).

        Returns:
            A JSONResponse instance.
        """
        assert path.startswith('/')

        if body:
            body = json.dumps(body).encode('utf-8')

        headers = {'Accept': 'application/vnd.github.v3+json'}

        if self.has_credentials():
            headers['Authorization'] = 'Basic {}'.format(self.auth_token())

        response = self.host.web.request(
            method=method,
            url=API_BASE + path,
            data=body,
            headers=headers
        )
        return JSONResponse(response)

    def create_pr(self, remote_branch_name, desc_title, body):
        """Creates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#create-a-pull-request

        Returns:
            The issue number of the created PR.
        """
        assert remote_branch_name
        assert desc_title
        assert body

        path = '/repos/%s/%s/pulls' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
        body = {
            'title': desc_title,
            'body': body,
            'head': remote_branch_name,
            'base': 'master',
        }
        response = self.request(path, method='POST', body=body)

        if response.status_code != 201:
            raise GitHubError(201, response.status_code, 'create PR')

        return response.data['number']

    def add_label(self, number, label):
        """Adds a label to a GitHub issue (or PR).

        API doc: https://developer.github.com/v3/issues/labels/#add-labels-to-an-issue
        """
        path = '/repos/%s/%s/issues/%d/labels' % (
            WPT_GH_ORG,
            WPT_GH_REPO_NAME,
            number
        )
        body = [label]
        response = self.request(path, method='POST', body=body)

        if response.status_code != 200:
            raise GitHubError(200, response.status_code, 'add label %s to issue %d' % (label, number))

    @staticmethod
    def extract_metadata(tag, commit_body, all_matches=False):
        values = []
        for line in commit_body.splitlines():
            if not line.startswith(tag):
                continue
            value = line[len(tag):]
            if all_matches:
                values.append(value)
            else:
                return value
        return values if all_matches else None


class JSONResponse(object):
    """An HTTP response containing JSON data."""

    def __init__(self, raw_response):
        """Initializes a JSONResponse instance.

        Args:
            raw_response: a response object returned by open methods in urllib2/urllib.
        """
        self._raw_response = raw_response
        self.status_code = raw_response.getcode()
        try:
            self.data = json.load(raw_response)
        except ValueError:
            self.data = None

    def getheader(self, header):
        """Gets the value of the header with the given name.

        Delegates to HTTPMessage.getheader(), which is case-insensitive."""
        return self._raw_response.info().getheader(header)


class GitHubError(Exception):
    """Raised when an GitHub returns a non-OK response status for a request."""

    def __init__(self, expected, received, action, extra_data=None):
        message = 'Expected {}, but received {} from GitHub when attempting to {}'.format(
            expected, received, action
        )
        if extra_data:
            message += '\n' + str(extra_data)
        super(GitHubError, self).__init__(message)


class MergeError(GitHubError):
    """An error specifically for when a PR cannot be merged.

    This should only be thrown when GitHub returns status code 405,
    indicating that the PR could not be merged.
    """

    def __init__(self, pr_number):
        super(MergeError, self).__init__(200, 405, 'merge PR %d' % pr_number)
