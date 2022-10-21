# Copyright (C) 2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import logging
import re
import requests
from requests.auth import HTTPBasicAuth

from ews.common.buildbot import Buildbot
from ews.models.patch import Change
from ews.views.statusbubble import StatusBubble
import ews.config as config

_log = logging.getLogger(__name__)

GITHUB_URL = 'https://github.com/'
GITHUB_PROJECTS = ['WebKit/WebKit', 'apple/WebKit', 'WebKit/WebKit-security']


class GitHub(object):
    _cache = {}

    @classmethod
    def repository_urls(cls):
        return [GITHUB_URL + project for project in GITHUB_PROJECTS]

    @classmethod
    def pr_url(cls, pr_number, repository_url=None):
        if not repository_url:
            repository_url = '{}{}'.format(GITHUB_URL, GITHUB_PROJECTS[0])

        if repository_url not in GitHub.repository_urls():
            return ''
        if not pr_number or not isinstance(pr_number, int):
            return ''
        return '{}/pull/{}'.format(repository_url, pr_number)

    @classmethod
    def commit_url(cls, sha, repository_url=None):
        if not repository_url:
            repository_url = '{}{}'.format(GITHUB_URL, GITHUB_PROJECTS[0])
        if repository_url not in GitHub.repository_urls():
            return ''
        if not sha:
            return ''
        return '{}/commit/{}'.format(repository_url, sha)

    @classmethod
    def api_url(cls, repository_url=None):
        if not repository_url:
            repository_url = '{}{}'.format(GITHUB_URL, GITHUB_PROJECTS[0])

        if repository_url not in GitHub.repository_urls():
            return ''
        _, url_base = repository_url.split('://', 1)
        host, path = url_base.split('/', 1)
        return 'https://api.{}/repos/{}'.format(host, path)

    @classmethod
    def commit_status_url(cls, sha, repository_url=None):
        api_url = cls.api_url(repository_url)
        if not sha or not api_url:
            return ''
        return '{}/statuses/{}'.format(api_url, sha)

    @classmethod
    def credentials(cls):
        prefix = 'GITHUB_COM_'

        if prefix in cls._cache:
            return cls._cache[prefix]

        try:
            passwords = json.load(open('passwords.json'))
            cls._cache[prefix] = passwords.get('GITHUB_COM_USERNAME', None), passwords.get('GITHUB_COM_ACCESS_TOKEN', None)
        except Exception as e:
            _log.error('Error reading GitHub credentials')
            cls._cache[prefix] = None, None

        return cls._cache[prefix]

    def fetch_data_from_url_with_authentication_github(self, url):
        response = None
        try:
            username, access_token = GitHub.credentials()
            auth = HTTPBasicAuth(username, access_token) if username and access_token else None
            response = requests.get(
                url, timeout=60, auth=auth,
                headers=dict(Accept='application/vnd.github.v3+json'),
            )
            if response.status_code // 100 != 2:
                _log.error('Accessed {url} with unexpected status code {status_code}.\n'.format(url=url, status_code=response.status_code))
                return None
        except Exception as e:
            # Catching all exceptions here to safeguard access token.
            _log.error('Failed to access {}.\n'.format(url=url))
            return None
        return response

    def update_or_leave_comment_on_pr(self, pr_number, ews_comment, repository_url=None, comment_id=-1, change=None):
        api_url = GitHub.api_url(repository_url)
        if not api_url:
            return False
        if not ews_comment:
            _log.error('Unable to comment on GitHub for PR {} since comment is None.'.format(pr_number))
            return False

        if comment_id != -1:
            comment_url = '{api_url}/issues/comments/{comment_id}'.format(api_url=api_url, comment_id=comment_id)
        else:
            comment_url = '{api_url}/issues/{pr_number}/comments'.format(api_url=api_url, pr_number=pr_number)
        try:
            username, access_token = GitHub.credentials()
            auth = HTTPBasicAuth(username, access_token) if username and access_token else None
            response = requests.request(
                'POST', comment_url, timeout=60, auth=auth,
                headers=dict(Accept='application/vnd.github.v3+json'),
                json=dict(body=ews_comment),
            )
            if response.status_code // 100 != 2:
                _log.error('Failed to post comment to PR {}. Unexpected response code from GitHub: {}, url: {}\n'.format(pr_number, response.status_code, comment_url))
                return -1
            new_comment_id = response.json().get('id')
            comment_url = '{}/pull/{}#issuecomment-{}'.format(repository_url, pr_number, new_comment_id)
            _log.info('Commented on PR {}, link: {}'.format(pr_number, comment_url))

            if comment_id == -1 and new_comment_id != -1 and change:
                change.set_comment_id(new_comment_id)
                _log.info('PR {}: set new comment id as {} for hash: {}.'.format(pr_number, new_comment_id, change.change_id))
            return new_comment_id
        except Exception as e:
            _log.error('Error in posting comment to PR {}: {}\n'.format(pr_number, e))
        return -1

    def update_pr_description_with_status_bubble(self, pr_number, ews_comment, repository_url=None):
        api_url = GitHub.api_url(repository_url)
        if not api_url:
            return -1

        description_url = '{api_url}/issues/{pr_number}'.format(api_url=api_url, pr_number=pr_number)
        try:
            username, access_token = GitHub.credentials()
            auth = HTTPBasicAuth(username, access_token) if username and access_token else None

            response = requests.request(
                'GET', description_url, timeout=60, auth=auth,
                headers=dict(Accept='application/vnd.github.v3+json'),
            )
            if response.status_code // 100 != 2:
                _log.error('Failed to get PR {} description. Unexpected response code from GitHub: {}\n'.format(pr_number, response.status_code))
                return -1

            description = response.json().get('body')
            description = GitHubEWS.generate_updated_pr_description(description, ews_comment)

            response = requests.request(
                'POST', description_url, timeout=60, auth=auth,
                headers=dict(Accept='application/vnd.github.v3+json'),
                json=dict(body=description),
            )

            if response.status_code // 100 != 2:
                _log.error('Failed to update PR {} description. Unexpected response code from GitHub: {}\n'.format(pr_number, response.status_code))
                return -1

            _log.info('Updated description for PR {}'.format(pr_number))
            return 0
        except Exception as e:
            _log.error('Error in updating PR description for PR {}: {}\n'.format(pr_number, e))
        return -1


class GitHubEWS(GitHub):
    ICON_BUILD_PASS = u'\U00002705'
    ICON_BUILD_FAIL = u'\U0000274C'
    ICON_BUILD_WAITING = u'\U000023F3'
    ICON_BUILD_ONGOING = u'![loading](https://user-images.githubusercontent.com/3098702/171232313-daa606f1-8fd6-4b0f-a20b-2cb93c43d19b.png)'
    ICON_BUILD_ONGOING_WITH_FAILURES = ICON_BUILD_ONGOING  # FIXME: Update icon for this case
    ICON_BUILD_ERROR = u'\U0001F4A5'
    ICON_EMPTY_SPACE = u'\U00002003'
    STATUS_BUBBLE_START = u'<!--EWS-Status-Bubble-Start-->'
    STATUS_BUBBLE_END = u'<!--EWS-Status-Bubble-End-->'
    STATUS_BUBBLE_ROWS = [['style', 'ios', 'mac', 'wpe', 'win'],  # FIXME: generate this list dynamically to have merge queue show up on top
                          ['bindings', 'ios-sim', 'mac-debug', 'gtk', 'wincairo'],
                          ['webkitperl', 'ios-wk2', 'mac-AS-debug', 'gtk-wk2', ''],
                          ['webkitpy', 'api-ios', 'api-mac', 'api-gtk', ''],
                          ['jsc', 'tv', 'mac-wk1', 'jsc-armv7', ''],
                          ['services', 'tv-sim', 'mac-wk2', 'jsc-armv7-tests', ''],
                          ['merge', 'watch', 'mac-AS-debug-wk2', 'jsc-mips', ''],
                          ['unsafe-merge', 'watch-sim', 'mac-wk2-stress', 'jsc-mips-tests', '']]

    @classmethod
    def generate_updated_pr_description(self, description, ews_comment):
        description = description.split(self.STATUS_BUBBLE_START)[0]
        return u'{}{}\n{}\n{}'.format(description, self.STATUS_BUBBLE_START, ews_comment, self.STATUS_BUBBLE_END)

    def generate_comment_text_for_change(self, change):
        hash_url = 'https://github.com/{}/commit/{}'.format(change.pr_project, change.change_id)

        comment = '\n\n| Misc | iOS, tvOS & watchOS  | macOS  | Linux |  Windows |'
        comment += '\n| ----- | ---------------------- | ------- |  ----- |  --------- |'

        for row in self.STATUS_BUBBLE_ROWS:
            comment_for_row = '\n'
            for queue in row:
                if queue == '':
                    comment_for_row += '| '
                    continue
                comment_for_row += self.github_status_for_queue(change, queue)
            comment += comment_for_row

        if change.obsolete:
            comment = u'EWS run on previous version of this PR (hash {})<details>{}</details>'.format(hash_url, comment)
            return (comment, comment)

        regular_comment = u'{}{}'.format(hash_url, comment)
        folded_comment = u'EWS run on current version of this PR (hash {})<details>{}</details>'.format(hash_url, comment)
        if change.comment_id == -1:
            pr_url = GitHub.pr_url(change.pr_id)
            folded_comment = u'Starting EWS tests for {}. Live statuses available at the PR page, {}'.format(hash_url, pr_url)

        return (regular_comment, folded_comment)

    def github_status_for_queue(self, change, queue):
        name = queue
        is_tester_queue = Buildbot.is_tester_queue(queue)
        is_builder_queue = Buildbot.is_builder_queue(queue)
        if is_tester_queue:
            name = StatusBubble.TESTER_ICON + ' ' + name
        if is_builder_queue:
            name = StatusBubble.BUILDER_ICON + ' ' + name

        builds, is_parent_build = StatusBubble().get_all_builds_for_queue(change, queue, Buildbot.get_parent_queue(queue))
        # FIXME: Handle parent build case
        build = None
        if builds:
            build = builds[0]
            builds = builds[:10]  # Limit number of builds to display in status-bubble hover over message

        hover_over_text = ''
        icon = GitHubEWS.ICON_BUILD_WAITING
        if not build:
            if queue in ['merge', 'unsafe-merge']:
                return u'| '
            if Buildbot.get_parent_queue(queue):
                queue = Buildbot.get_parent_queue(queue)
            queue_full_name = Buildbot.queue_name_by_shortname_mapping.get(queue)
            if queue_full_name:
                url = 'https://{}/#/builders/{}'.format(config.BUILDBOT_SERVER_HOST, queue_full_name)
            hover_over_text = 'Waiting in queue, processing has not started yet'
            return u'| [{icon} {name} ]({url} "{hover_over_text}") '.format(icon=icon, name=name, url=url, hover_over_text=hover_over_text)

        url = 'https://{}/#/builders/{}/builds/{}'.format(config.BUILDBOT_SERVER_HOST, build.builder_id, build.number)

        if build.result is None:
            if self._does_build_contains_any_failed_step(build):
                icon = GitHubEWS.ICON_BUILD_ONGOING_WITH_FAILURES
            else:
                icon = GitHubEWS.ICON_BUILD_ONGOING
            hover_over_text = 'Build is in progress. Recent messages:' + self._steps_messages(build)
        elif build.result == Buildbot.SUCCESS:
            if is_parent_build:
                icon = GitHubEWS.ICON_BUILD_WAITING
                hover_over_text = 'Waiting to run tests'
                queue_full_name = Buildbot.queue_name_by_shortname_mapping.get(queue)
                if queue_full_name:
                    url = 'https://{}/#/builders/{}'.format(config.BUILDBOT_SERVER_HOST, queue_full_name)
            else:
                icon = GitHubEWS.ICON_BUILD_PASS
                if is_builder_queue and is_tester_queue:
                    hover_over_text = 'Built successfully and passed tests'
                elif is_builder_queue:
                    hover_over_text = 'Built successfully'
                elif is_tester_queue:
                    if queue == 'style':
                        hover_over_text = 'Passed style check'
                    else:
                        hover_over_text = 'Passed tests'
                else:
                    hover_over_text = 'Pass'
        elif build.result == Buildbot.WARNINGS:
            icon = GitHubEWS.ICON_BUILD_PASS
        elif build.result == Buildbot.FAILURE:
            icon = GitHubEWS.ICON_BUILD_FAIL
            hover_over_text = self._most_recent_failure_message(build)
        elif build.result == Buildbot.CANCELLED:
            icon = GitHubEWS.ICON_EMPTY_SPACE
            name = u'~~{}~~'.format(name)
            hover_over_text = 'Build was cancelled. Recent messages:' + self._steps_messages(build)
        elif build.result == Buildbot.SKIPPED:
            icon = GitHubEWS.ICON_EMPTY_SPACE
            if re.search(r'Pull request .* doesn\'t have relevant changes', build.state_string):
                return u'| '
            name = u'~~{}~~'.format(name)
            hover_over_text = 'The change is no longer eligible for processing.'
            if re.search(r'Pull request .* is already closed', build.state_string):
                hover_over_text += ' Pull Request was already closed when EWS attempted to process it.'
            elif re.search(r'Hash .* on PR .* is outdated', build.state_string):
                hover_over_text += ' Commit was outdated when EWS attempted to process it.'
        elif build.result == Buildbot.RETRY:
            hover_over_text = 'Build is being retried. Recent messages:' + self._steps_messages(build)
            icon = GitHubEWS.ICON_BUILD_ONGOING
        elif build.result == Buildbot.EXCEPTION:
            hover_over_text = 'An unexpected error occured. Recent messages:' + self._steps_messages(build)
            icon = GitHubEWS.ICON_BUILD_ERROR
        else:
            icon = GitHubEWS.ICON_BUILD_ERROR
            hover_over_text = 'An unexpected error occured. Recent messages:' + self._steps_messages(build)

        return u'| [{icon} {name}]({url} "{hover_over_text}") '.format(icon=icon, name=name, url=url, hover_over_text=hover_over_text)

    @classmethod
    def add_or_update_comment_for_change_id(self, sha, pr_id, pr_project=None, allow_new_comment=False):
        if not pr_id or pr_id == -1:
            _log.error('Invalid pr_id: {}'.format(pr_id))
            return -1

        repository_url = GITHUB_URL + pr_project if pr_project else None

        change = Change.get_change(sha)
        if not change:
            _log.error('Change not found for hash: {}. Unable to generate github comment.'.format(sha))
            return -1
        gh = GitHubEWS()
        comment_text, folded_comment = gh.generate_comment_text_for_change(change)
        if not change.obsolete:
            gh.update_pr_description_with_status_bubble(pr_id, comment_text, repository_url)

        comment_id = change.comment_id
        if comment_id == -1:
            if not allow_new_comment:
                # FIXME: improve this logic to use locking instead
                return -1
            _log.info('Adding comment for hash: {}, PR: {}'.format(sha, pr_id))
            new_comment_id = gh.update_or_leave_comment_on_pr(pr_id, folded_comment, repository_url=repository_url, change=change)
            obsolete_changes = Change.mark_old_changes_as_obsolete(pr_id, sha)
            for obsolete_change in obsolete_changes:
                obsolete_comment_text, _ = gh.generate_comment_text_for_change(obsolete_change)
                gh.update_or_leave_comment_on_pr(pr_id, obsolete_comment_text, repository_url=repository_url, comment_id=obsolete_change.comment_id, change=obsolete_change)
                _log.info('Updated obsolete status-bubble on pr {} for hash: {}'.format(pr_id, obsolete_change.change_id))

        else:
            _log.info('Updating comment for hash: {}, pr_id: {}, pr_id from db: {}.'.format(sha, pr_id, change.pr_id))
            new_comment_id = gh.update_or_leave_comment_on_pr(pr_id, folded_comment, repository_url=repository_url, comment_id=comment_id)

        return comment_id

    def _steps_messages(self, build):
        # FIXME: figure out if it is possible to have multi-line hover-over messages in GitHub UI.
        return '; '.join([step.state_string for step in build.step_set.all().order_by('uid') if self._should_display_step(step)])

    def _should_display_step(self, step):
        return not [step_to_hide for step_to_hide in StatusBubble.STEPS_TO_HIDE if re.search(step_to_hide, step.state_string)]

    def _does_build_contains_any_failed_step(self, build):
        for step in build.step_set.all():
            if step.result and step.result != Buildbot.SUCCESS and step.result != Buildbot.WARNINGS and step.result != Buildbot.SKIPPED:
                return True
        return False

    def _most_recent_failure_message(self, build):
        for step in build.step_set.all().order_by('-uid'):
            if step.result == Buildbot.SUCCESS and 'retrying build' in step.state_string:
                return step.state_string
            if step.result == Buildbot.FAILURE:
                return step.state_string
        return ''
