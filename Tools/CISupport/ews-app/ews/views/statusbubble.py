# Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

from __future__ import unicode_literals

import datetime
import logging
import re

from django.http import HttpResponse
from django.shortcuts import render
from django.utils import timezone
from django.views import View
from django.views.decorators.clickjacking import xframe_options_exempt
from ews.common.buildbot import Buildbot
from ews.models.build import Build
from ews.models.patch import Change
import ews.config as config

_log = logging.getLogger(__name__)


class StatusBubble(View):
    # These queue names are from shortname in https://trac.webkit.org/browser/webkit/trunk/Tools/CISupport/ews-build/config.json
    # FIXME: Auto-generate this list https://bugs.webkit.org/show_bug.cgi?id=195640
    # Note: This list is sorted in the order of which bubbles appear in bugzilla.
    ALL_QUEUES = ['style', 'ios', 'ios-sim', 'mac', 'mac-AS-debug', 'tv', 'tv-sim', 'watch', 'watch-sim', 'gtk', 'wpe', 'wincairo', 'win',
                  'ios-wk2', 'mac-wk1', 'mac-wk2', 'mac-wk2-stress', 'mac-AS-debug-wk2', 'gtk-wk2', 'api-ios', 'api-mac', 'api-gtk',
                  'bindings', 'jsc', 'jsc-arm64', 'jsc-armv7', 'jsc-armv7-tests', 'jsc-mips', 'jsc-mips-tests', 'jsc-i386', 'webkitperl', 'webkitpy', 'services']

    STEPS_TO_HIDE = ['^Archived built product$', '^Uploaded built product$', '^Transferred archive to S3$',
                     '^Archived test results$', '^Uploaded test results$', '^Extracted test results$',
                     '^Downloaded built product$', '^Extracted built product$',
                     '^Crash collection has quiesced$', '^Triggered crash log submission$',
                     '^Cleaned and updated working directory$', '^Checked out required revision$', '^Updated working directory$',
                     '^Validated change$', '^Killed old processes$', '^Configured build$', '^OS:.*Xcode:', '(skipped)',
                     '^Printed configuration$', '^Patch contains relevant changes$', '^Deleted .git/index.lock$',
                     '^triggered.*$', '^Found modified ChangeLogs$', '^Created local git commit$', '^Set build summary$',
                     '^Validated commiter$', '^Validated commiter and reviewer$', '^Validated ChangeLog and Reviewer$',
                     '^Removed flags on bugzilla patch$', '^Checked change status on other queues$', '^Identifier:.*$',
                     '^Updated branch information$', '^worker .* ready$']
    DAYS_TO_CHECK_QUEUE_POSITION = 0.5
    DAYS_TO_HIDE_BUBBLE = 7
    BUILDER_ICON = u'\U0001f6e0'
    TESTER_ICON = u'\U0001f9ea'
    BUILD_RETRY_MSG = 'retrying build'
    UNKNOWN_QUEUE_POSITION = '?'

    def _build_bubble(self, change, queue, hide_icons=False, sent_to_buildbot=True):
        bubble = {
            'name': queue,
        }
        is_tester_queue = Buildbot.is_tester_queue(queue)
        is_builder_queue = Buildbot.is_builder_queue(queue)
        if hide_icons == False:
            if is_tester_queue:
                bubble['name'] = StatusBubble.TESTER_ICON + '  ' + bubble['name']
            if is_builder_queue:
                bubble['name'] = StatusBubble.BUILDER_ICON + '  ' + bubble['name']

        builds, is_parent_build = self.get_all_builds_for_queue(change, queue, Buildbot.get_parent_queue(queue))
        build = None
        if builds:
            build = builds[0]
            builds = builds[:10]  # Limit number of builds to display in status-bubble hover over message
        if not self._should_show_bubble_for_build(build, sent_to_buildbot):
            return None

        if not build:
            bubble['state'] = 'none'
            queue_position = self._queue_position(change, queue, Buildbot.get_parent_queue(queue))
            if not queue_position:
                return None
            if queue_position != StatusBubble.UNKNOWN_QUEUE_POSITION:
                bubble['queue_position'] = queue_position
            if Buildbot.get_parent_queue(queue):
                queue = Buildbot.get_parent_queue(queue)
            queue_full_name = Buildbot.queue_name_by_shortname_mapping.get(queue)
            if queue_full_name:
                bubble['url'] = 'https://{}/#/builders/{}'.format(config.BUILDBOT_SERVER_HOST, queue_full_name)
            bubble['details_message'] = 'Waiting in queue, processing has not started yet.\n\nPosition in queue: {}'.format(queue_position)
            return bubble

        bubble['url'] = 'https://{}/#/builders/{}/builds/{}'.format(config.BUILDBOT_SERVER_HOST, build.builder_id, build.number)
        builder_full_name = build.builder_name.replace('-', ' ')

        if build.result is None:  # In-progress build
            if self._does_build_contains_any_failed_step(build):
                bubble['state'] = 'provisional-fail'
            else:
                bubble['state'] = 'started'
            bubble['details_message'] = 'Build is in-progress. Recent messages:' + self._steps_messages_from_multiple_builds(builds)
        elif build.retried:
            bubble['state'] = 'started'
            bubble['details_message'] = 'Waiting for available bot to retry the build.'
            bubble['url'] = None
            queue_full_name = Buildbot.queue_name_by_shortname_mapping.get(queue)
            if queue_full_name:
                bubble['url'] = 'https://{}/#/builders/{}'.format(config.BUILDBOT_SERVER_HOST, queue_full_name)
        elif build.result == Buildbot.SUCCESS:
            if is_parent_build:
                if change.created < (timezone.now() - datetime.timedelta(days=StatusBubble.DAYS_TO_HIDE_BUBBLE)):
                    # Do not display bubble for old change for which no build has been reported on given queue.
                    # Most likely the change would never be processed on this queue, since either the queue was
                    # added after the change was submitted, or build request for that change was cancelled.
                    return None
                bubble['state'] = 'started'
                bubble['details_message'] = 'Waiting to run tests.'
                queue_full_name = Buildbot.queue_name_by_shortname_mapping.get(queue)
                if queue_full_name:
                    bubble['url'] = 'https://{}/#/builders/{}'.format(config.BUILDBOT_SERVER_HOST, queue_full_name)
                    builder_full_name = queue_full_name.replace('-', ' ')
            else:
                bubble['state'] = 'pass'
                if is_builder_queue and is_tester_queue:
                    bubble['details_message'] = 'Built successfully and passed tests'
                elif is_builder_queue:
                    bubble['details_message'] = 'Built successfully'
                elif is_tester_queue:
                    if queue == 'style':
                        bubble['details_message'] = 'Passed style check'
                    else:
                        bubble['details_message'] = 'Passed tests'
                else:
                    bubble['details_message'] = 'Pass'
        elif build.result == Buildbot.WARNINGS:
            bubble['state'] = 'pass'
            bubble['details_message'] = 'Warning' + self._steps_messages_from_multiple_builds(builds)
        elif build.result == Buildbot.FAILURE:
            bubble['state'] = 'fail'
            bubble['details_message'] = self._most_recent_failure_message(build)
            if StatusBubble.BUILD_RETRY_MSG in bubble['details_message']:
                bubble['state'] = 'provisional-fail'
        elif build.result == Buildbot.SKIPPED:
            bubble['state'] = 'skipped'
            bubble['details_message'] = 'The change is no longer eligible for processing.'
            if re.search(r'Bug .* is already closed', build.state_string):
                bubble['details_message'] += ' Bug was already closed when EWS attempted to process it.'
            elif re.search(r'Patch .* is marked r-', build.state_string):
                bubble['details_message'] += ' Patch was already marked r- when EWS attempted to process it.'
            elif re.search(r'Patch .* is obsolete', build.state_string):
                bubble['details_message'] += ' Patch was obsolete when EWS attempted to process it.'

            if len(builds) > 1:
                bubble['details_message'] += '\nSome messages were logged while the change was still eligible:'
                bubble['details_message'] += self._steps_messages_from_multiple_builds(builds)

        elif build.result == Buildbot.EXCEPTION:
            bubble['state'] = 'error'
            bubble['details_message'] = 'An unexpected error occured. Recent messages:' + self._steps_messages_from_multiple_builds(builds)
        elif build.result == Buildbot.RETRY:
            bubble['state'] = 'provisional-fail'
            bubble['details_message'] = 'Build is being retried. Recent messages:' + self._steps_messages_from_multiple_builds(builds)
        elif build.result == Buildbot.CANCELLED:
            bubble['state'] = 'cancelled'
            bubble['details_message'] = 'Build was cancelled. Recent messages:' + self._steps_messages_from_multiple_builds(builds)
        else:
            bubble['state'] = 'error'
            bubble['details_message'] = 'An unexpected error occured. Recent messages:' + self._steps_messages_from_multiple_builds(builds)

        if 'details_message' in bubble:
            bubble['details_message'] = builder_full_name + '\n\n' + bubble['details_message']
            os_details = self.get_os_details(build)
            timestamp = self.get_build_timestamp(build)
            if os_details:
                bubble['details_message'] += '\n\n' + os_details + '\n' + timestamp
            else:
                bubble['details_message'] += '\n\n' + timestamp

        return bubble

    def get_os_details(self, build):
        for step in build.step_set.all():
            if step.state_string.startswith('OS:'):
                return step.state_string
        return ''

    def get_build_timestamp(self, build):
        if build.complete_at:
            return self._iso_time(build.complete_at)

        recent_build_step = build.step_set.last()
        if recent_build_step:
            return self._iso_time(recent_build_step.started_at)

        return self._iso_time(build.started_at)

    def _iso_time(self, time):
        return '[[' + datetime.datetime.fromtimestamp(time).isoformat() + 'Z]]'

    def _steps_messages(self, build):
        return '\n'.join([step.state_string for step in build.step_set.all().order_by('uid') if self._should_display_step(step)])

    def _steps_messages_from_multiple_builds(self, builds):
        message = ''
        for build in reversed(builds):
            message += '\n\n' + self._steps_messages(build)
        return message

    def _should_display_step(self, step):
        return not [step_to_hide for step_to_hide in StatusBubble.STEPS_TO_HIDE if re.search(step_to_hide, step.state_string)]

    def _does_build_contains_any_failed_step(self, build):
        for step in build.step_set.all():
            if step.result and step.result != Buildbot.SUCCESS and step.result != Buildbot.WARNINGS and step.result != Buildbot.SKIPPED:
                return True
        return False

    def _most_recent_failure_message(self, build):
        for step in build.step_set.all().order_by('-uid'):
            if step.result == Buildbot.SUCCESS and StatusBubble.BUILD_RETRY_MSG in step.state_string:
                return step.state_string
            if step.result == Buildbot.FAILURE:
                return step.state_string
        return ''

    def get_latest_build_for_queue(self, change, queue, parent_queue=None):
        builds, is_parent_build = self.get_all_builds_for_queue(change, queue, parent_queue)
        if not builds:
            return (None, None)
        return (builds[0], is_parent_build)

    def get_all_builds_for_queue(self, change, queue, parent_queue=None):
        builds = self.get_builds_for_queue(change, queue)
        is_parent_build = False
        if not builds and parent_queue:
            builds = self.get_builds_for_queue(change, parent_queue)
            is_parent_build = True
        if not builds:
            return (None, None)
        builds.sort(key=lambda build: build.number, reverse=True)
        return (builds, is_parent_build)

    def get_builds_for_queue(self, change, queue):
        return [build for build in change.build_set.all() if build.builder_display_name == queue]

    def find_failed_builds_for_change(self, change_id):
        change = Change.get_change(change_id)
        if not change:
            return []
        failed_builds = []
        for queue in StatusBubble.ALL_QUEUES:
            build, _ = self.get_latest_build_for_queue(change, queue)
            if not build:
                continue
            if build.result in (Buildbot.FAILURE, Buildbot.EXCEPTION, Buildbot.CANCELLED):
                failed_builds.append(build)
        return failed_builds

    def _should_show_bubble_for_build(self, build, sent_to_buildbot=True):
        if build and build.result == Buildbot.SKIPPED and re.search(r'Patch .* doesn\'t have relevant changes', build.state_string):
            return False
        if (not build) and (not sent_to_buildbot):
            return False
        return True

    def _queue_position(self, change, queue, parent_queue=None):
        # FIXME: Handle retried builds and cancelled build-requests as well.
        from_timestamp = timezone.now() - datetime.timedelta(days=StatusBubble.DAYS_TO_CHECK_QUEUE_POSITION)
        hide_from_timestamp = timezone.now() - datetime.timedelta(days=StatusBubble.DAYS_TO_HIDE_BUBBLE)

        if change.created < hide_from_timestamp:
            # Do not display bubble for old change for which no build has been reported on given queue.
            # Most likely the change would never be processed on this queue, since either the queue was
            # added after the change was submitted, or build request for that change was cancelled.
            return None

        if change.created < from_timestamp:
            # This means change has been waiting on given queue for long time, but not long enough to hide the status-bubble.
            # Instead of calculating exact queue position (which might be slow), we display a fixed high queue position.
            return StatusBubble.UNKNOWN_QUEUE_POSITION

        sent = 'sent_to_commit_queue' if queue == 'commit' else 'sent_to_buildbot'
        previously_sent_changes = set(Change.objects
                                          .filter(created__gte=from_timestamp)
                                          .filter(**{sent: True})
                                          .filter(obsolete=False)
                                          .filter(created__lt=change.created))
        if parent_queue:
            recent_builds_parent_queue = Build.objects \
                                             .filter(created__gte=from_timestamp) \
                                             .filter(builder_display_name=parent_queue)
            processed_changes_parent_queue = set([build.change for build in recent_builds_parent_queue])
            return len(previously_sent_changes - processed_changes_parent_queue) + 1

        recent_builds = Build.objects \
                            .filter(created__gte=from_timestamp) \
                            .filter(builder_display_name=queue)
        processed_changes = set([build.change for build in recent_builds])
        _log.debug('Change: {}, queue: {}, previous changes: {}'.format(change.change_id, queue, previously_sent_changes - processed_changes))
        return len(previously_sent_changes - processed_changes) + 1

    def _build_bubbles_for_change(self, change, hide_icons=False):
        show_submit_to_ews = True
        failed_to_apply = False  # TODO: https://bugs.webkit.org/show_bug.cgi?id=194598
        show_retry = False
        bubbles = []

        if not change:
            return (None, show_submit_to_ews, failed_to_apply, show_retry)

        for queue in StatusBubble.ALL_QUEUES:
            bubble = self._build_bubble(change, queue, hide_icons, change.sent_to_buildbot)
            if bubble:
                bubbles.append(bubble)
                if bubble['state'] in ('fail', 'error'):
                    show_retry = True

        if change.sent_to_commit_queue:
            if not change.sent_to_buildbot:
                hide_icons = True
            cq_bubble = self._build_bubble(change, 'commit', hide_icons)
            if cq_bubble:
                bubbles.insert(0, cq_bubble)

        show_submit_to_ews = not change.sent_to_buildbot
        return (bubbles, show_submit_to_ews, failed_to_apply, show_retry)

    @xframe_options_exempt
    def get(self, request, change_id):
        hide_icons = request.GET.get('hide_icons', False)
        change = Change.get_change(change_id)
        bubbles, show_submit_to_ews, show_failure_to_apply, show_retry = self._build_bubbles_for_change(change, hide_icons)

        template_values = {
            'bubbles': bubbles,
            'change_id': change_id,
            'show_submit_to_ews': show_submit_to_ews,
            'show_failure_to_apply': show_failure_to_apply,
            'show_retry_button': show_retry,
        }
        return render(request, 'statusbubble.html', template_values)
