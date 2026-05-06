# Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
import requests

from django.utils import timezone

from ews.common.buildbot import Buildbot
from ews.models.patch import Change
from ews.views.bubble.compute import compute_bubble
from ews.views.bubble.config import BUILDER_ICON, BubbleConfig
from ews.views.bubble.fetcher import BubbleDataFetcher
from ews.views.bubble.model import BubbleInputs
from ews.views.bubble.render_markdown import GithubIcons, render_markdown_bubble
import ews.common.util as util

_log = logging.getLogger(__name__)

GITHUB_URL = 'https://github.com/'
GITHUB_PROJECTS = ['WebKit/WebKit', 'WebKit/WebKit-security']

is_test_mode_enabled = util.load_password('EWS_PRODUCTION') is None
is_dev_instance = (util.get_custom_suffix() != '')


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

    @classmethod
    def fetch_data_from_url_with_authentication_github(cls, url):
        response = None
        try:
            _, access_token = GitHub.credentials()
            response = requests.get(
                url, timeout=60,
                headers=dict(
                    Accept='application/vnd.github.v3+json',
                    Authorization=f'Bearer {access_token}',
                ),
            )
            if response.status_code // 100 != 2:
                _log.error(f'Accessed {url} with unexpected status code {response.status_code}.\n')
                return None
        except Exception as e:
            # Catching all exceptions here to safeguard access token.
            _log.error(f'Failed to access {url}.\n')
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
            _, access_token = GitHub.credentials()
            response = requests.request(
                'POST', comment_url, timeout=60,
                headers=dict(
                    Accept='application/vnd.github.v3+json',
                    Authorization=f'Bearer {access_token}',
                ), json=dict(body=ews_comment),
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
            _, access_token = GitHub.credentials()

            response = requests.request(
                'GET', description_url, timeout=60,
                headers=dict(
                    Accept='application/vnd.github.v3+json',
                    Authorization=f'Bearer {access_token}',
                ),
            )
            if response.status_code // 100 != 2:
                _log.error('Failed to get PR {} description. Unexpected response code from GitHub: {}\n'.format(pr_number, response.status_code))
                return -1

            description = response.json().get('body')
            description = GitHubEWS.generate_updated_pr_description(description, ews_comment)

            response = requests.request(
                'POST', description_url, timeout=60,
                headers=dict(
                    Accept='application/vnd.github.v3+json',
                    Authorization=f'Bearer {access_token}',
                ),
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
    APPLE_INTERNAL_QUEUES = ['ios-apple', 'mac-apple', 'vision-apple']
    APPLE_INTERNAL_BUILDS_TITLE = 'Apple Internal'
    ICON_BUILD_PASS = u'\U00002705'
    ICON_BUILD_FAIL = u'\U0000274C'
    ICON_BUILD_WAITING = u'\U000023F3'
    ICON_BUILD_ONGOING = u'![loading](https://user-images.githubusercontent.com/3098702/171232313-daa606f1-8fd6-4b0f-a20b-2cb93c43d19b.png)'
    ICON_BUILD_ONGOING_WITH_FAILURES = u'![loading-orange](https://github-production-user-asset-6210df.s3.amazonaws.com/3098702/291015173-08c448be-ac0a-4fd6-92a3-8165057445b7.png)'
    # FIXME: Make ICON_BUILD_ONGOING_WITH_FAILURES more accessible
    ICON_BUILD_ERROR = u'\U0001F4A5'
    ICON_EMPTY_SPACE = u'\U00002003'
    STATUS_BUBBLE_START = u'<!--EWS-Status-Bubble-Start-->'
    STATUS_BUBBLE_END = u'<!--EWS-Status-Bubble-End-->'
    PR_COLUMN_DISPLAY_ORDER = ['misc', 'embedded', 'macos', 'linux', 'windows']
    approved_user_list_for_apple_internal_builds = []

    @classmethod
    def _compute_status_bubble_rows(cls):
        columns = {key: [] for key in cls.PR_COLUMN_DISPLAY_ORDER}
        columns['misc'].extend(Buildbot.pr_comment_misc_handlers)
        for shortname in Buildbot.all_queues:
            column = Buildbot.pr_column_by_shortname.get(shortname)
            if column in columns:
                columns[column].append(shortname)
        ordered_columns = [columns[key] for key in cls.PR_COLUMN_DISPLAY_ORDER]
        height = max((len(c) for c in ordered_columns), default=0)
        rows = []
        for i in range(height):
            rows.append([col[i] if i < len(col) else '' for col in ordered_columns])
        return rows

    @classmethod
    def update_approved_user_list_for_apple_internal_builds(cls):
        PER_PAGE_LIMIT = 100
        NUM_PAGE_LIMIT = 10
        URL_FOR_TRUSTED_GITHUB_LIST = f'https://api.github.com/orgs/apple/teams/webkit/members'
        members = []
        for page in range(1, NUM_PAGE_LIMIT + 1):
            url = f'{URL_FOR_TRUSTED_GITHUB_LIST}?per_page={PER_PAGE_LIMIT}&page={page}'
            content = cls.fetch_data_from_url_with_authentication_github(url)
            if not content:
                _log.error(f'ERROR: Unable to fetch list of trusted users, this might impact Internal EWS.')
                break
            response_content = content.json() or []
            if not isinstance(response_content, list):
                _log.info(f"Malformed response when listing users with '{url}'\n")
                return members
            members.extend(user['login'] for user in response_content)
            if len(response_content) < PER_PAGE_LIMIT:
                break

        _log.info(f'{len(members)} Members: \n{members}')
        if members:
            GitHubEWS.approved_user_list_for_apple_internal_builds = members

    @classmethod
    def generate_updated_pr_description(self, description, ews_comment):
        description = "" if description is None else description.split(self.STATUS_BUBBLE_START)[0]
        return u'{}{}\n{}\n{}'.format(description, self.STATUS_BUBBLE_START, ews_comment, self.STATUS_BUBBLE_END)

    def generate_comment_text_for_change(self, change, include_apple_internal_builds=False):
        repository_url = 'https://github.com/{}'.format(change.pr_project)
        hash_url = '{}/commit/{}'.format(repository_url, change.change_id)

        comment = '\n\n| Misc | iOS, visionOS, tvOS & watchOS  | macOS  | Linux |  Windows |'
        comment += '\n| ----- | ---------------------- | ------- |  ----- |  --------- |'

        status_bubble_rows = self._compute_status_bubble_rows()
        if include_apple_internal_builds:
            comment = comment.replace('Windows', f'Windows | {self.APPLE_INTERNAL_BUILDS_TITLE}')
            comment += ' ------ |'
            for row in status_bubble_rows:
                row.append('')
            for i, queue in enumerate(self.APPLE_INTERNAL_QUEUES):
                status_bubble_rows[i][-1] = queue

        for row in status_bubble_rows:
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
            pr_url = GitHub.pr_url(change.pr_number, repository_url=repository_url)
            folded_comment = u'Starting EWS tests for {}. Live statuses available at the PR page, {}'.format(hash_url, pr_url)

        return (regular_comment, folded_comment)

    @classmethod
    def escape_github_markdown(cls, string):
        return string.replace('\n', ' ').replace('"', ' ').replace('|', '\\|')

    def should_include_apple_internal_builds(self, pr_author, pr_project):
        return (pr_author in self.approved_user_list_for_apple_internal_builds) and (pr_project in GITHUB_PROJECTS)

    def github_status_for_queue(self, change, queue):
        if queue in self.APPLE_INTERNAL_QUEUES:
            return self.github_status_for_cibuild(change, queue)
        return self.github_status_for_buildbot_queue(change, queue)

    def github_status_for_cibuild(self, change, queue):
        name = f'{BUILDER_ICON} {queue}'
        builds = util.get_cibuilds_for_queue(change, queue)
        build = None
        if builds:
            build = builds[0]
        icon = GitHubEWS.ICON_BUILD_WAITING
        url = build.url if build else None
        if url == '':
            url = None
        if not build:
            return f'| {icon} {name} '
        if build.result is None:
            icon = GitHubEWS.ICON_BUILD_ONGOING
        elif build.result == Buildbot.SUCCESS:
            icon = GitHubEWS.ICON_BUILD_PASS
        elif build.result == Buildbot.FAILURE:
            icon = GitHubEWS.ICON_BUILD_FAIL
        elif build.result == Buildbot.CANCELLED:
            icon = GitHubEWS.ICON_EMPTY_SPACE
        else:
            icon = GitHubEWS.ICON_BUILD_ERROR
        return f'| [{icon} {name}]({url}) '

    def github_status_for_buildbot_queue(self, change, queue):
        cfg = BubbleConfig.from_buildbot_globals()
        fetcher = BubbleDataFetcher(cfg)
        parent_queue = cfg.get_parent_queue(queue)
        builds, is_parent_build = fetcher.fetch_builds_for_queue(change, queue, parent_queue)

        if not builds and queue in cfg.pr_comment_misc_handlers:
            return u'| '

        # Force a "waiting" bubble for non-misc-handler queues with no build by
        # supplying a placeholder queue position; the markdown renderer ignores
        # the queue_position field.
        queue_position = None if builds else cfg.unknown_queue_position

        inputs = BubbleInputs(
            change=fetcher.fetch_change_snapshot(change),
            queue=queue,
            builds=builds,
            is_parent_build=is_parent_build,
            queue_position=queue_position,
            hide_icons=False,
            sent_to_buildbot=True,
        )
        data = compute_bubble(inputs, cfg, now=timezone.now())
        if data is None:
            return u'| '

        is_in_progress = bool(builds and builds[0].result is None)
        return render_markdown_bubble(
            data,
            icons=self._github_icons(),
            is_in_progress=is_in_progress,
            escape=self.escape_github_markdown,
        )

    @classmethod
    def _github_icons(cls) -> GithubIcons:
        return GithubIcons(
            pass_=cls.ICON_BUILD_PASS,
            fail=cls.ICON_BUILD_FAIL,
            waiting=cls.ICON_BUILD_WAITING,
            ongoing=cls.ICON_BUILD_ONGOING,
            ongoing_with_failures=cls.ICON_BUILD_ONGOING_WITH_FAILURES,
            error=cls.ICON_BUILD_ERROR,
            empty=cls.ICON_EMPTY_SPACE,
        )

    @classmethod
    def add_or_update_comment_for_change_id(self, sha, pr_number, pr_project=None, pr_author='', allow_new_comment=False):
        if not pr_number or pr_number == -1:
            _log.error(f'Invalid pr_number: {pr_number}')
            return -1

        if is_test_mode_enabled or is_dev_instance:
            _log.info('Skipped updating GitHub PR since this is not production instance.')
            return -1

        repository_url = GITHUB_URL + pr_project if pr_project else None

        change = Change.get_change(sha)
        if not change:
            _log.error('Change not found for hash: {}. Unable to generate github comment.'.format(sha))
            return -1
        gh = GitHubEWS()
        include_apple_internal_builds = gh.should_include_apple_internal_builds(pr_author, pr_project)
        comment_text, folded_comment = gh.generate_comment_text_for_change(change, include_apple_internal_builds)
        if not change.obsolete:
            gh.update_pr_description_with_status_bubble(pr_number, comment_text, repository_url)

        comment_id = change.comment_id
        if comment_id == -1:
            if not allow_new_comment:
                # FIXME: improve this logic to use locking instead
                return -1
            _log.info(f'Adding comment for hash: {sha}, PR: {pr_number}')
            new_comment_id = gh.update_or_leave_comment_on_pr(pr_number, folded_comment, repository_url=repository_url, change=change)
            obsolete_changes = Change.mark_old_changes_as_obsolete(pr_number, sha)
            for obsolete_change in obsolete_changes:
                obsolete_comment_text, _ = gh.generate_comment_text_for_change(obsolete_change, include_apple_internal_builds)
                gh.update_or_leave_comment_on_pr(pr_number, obsolete_comment_text, repository_url=repository_url, comment_id=obsolete_change.comment_id, change=obsolete_change)
                _log.info('Updated obsolete status-bubble on pr {} for hash: {}'.format(pr_number, obsolete_change.change_id))

        else:
            _log.info(f'Updating comment for hash: {sha}, pr_number: {pr_number}, pr_number from db: {change.pr_number}.')
            new_comment_id = gh.update_or_leave_comment_on_pr(pr_number, folded_comment, repository_url=repository_url, comment_id=comment_id)

        return comment_id
