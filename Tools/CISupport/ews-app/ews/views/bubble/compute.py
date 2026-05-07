# Copyright (C) 2026 Apple Inc. All rights reserved.
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
import re
from typing import Iterable, Optional

from ews.common.buildbot import Buildbot
from ews.views.bubble.config import BubbleConfig
from ews.views.bubble.model import (
    BubbleData,
    BubbleInputs,
    BuildSnapshot,
)


SKIPPED_IRRELEVANT_PATTERNS = (
    re.compile(r"Patch .* doesn't have relevant changes"),
    re.compile(r"Pull request .* doesn't have relevant changes"),
)

SKIPPED_REASON_MESSAGES = (
    (re.compile(r'Bug .* is already closed'),
     'Bug was already closed when EWS attempted to process it.'),
    (re.compile(r'Patch .* is marked r-'),
     'Patch was already marked r- when EWS attempted to process it.'),
    (re.compile(r'Patch .* is obsolete'),
     'Patch was obsolete when EWS attempted to process it.'),
    (re.compile(r'Pull request .* is already closed'),
     'Pull Request was already closed when EWS attempted to process it.'),
    (re.compile(r'Hash .* on PR .* is outdated'),
     'Commit was outdated when EWS attempted to process it.'),
)

SKIPPED_OVERRIDE_MESSAGES = (
    (re.compile(r'Skipping as PR .* has skip-ews label'),
     'EWS skipped this build as PR had skip-ews label when EWS attempted to process it.'),
)


def _decorated_queue_name(queue: str, hide_icons: bool, config: BubbleConfig) -> str:
    name = queue
    if hide_icons:
        return name
    if config.is_tester_queue(queue):
        name = config.tester_icon + '  ' + name
    if config.is_builder_queue(queue):
        name = config.builder_icon + '  ' + name
    return name


def _should_show_bubble_for_build(build: Optional[BuildSnapshot], sent_to_buildbot: bool) -> bool:
    if build is not None and build.result == Buildbot.SKIPPED:
        for pattern in SKIPPED_IRRELEVANT_PATTERNS:
            if pattern.search(build.state_string):
                return False
    if build is None and not sent_to_buildbot:
        return False
    return True


def _does_build_contain_failed_step(build: BuildSnapshot) -> bool:
    for step in build.steps_by_uid_asc:
        if step.result and step.result not in (Buildbot.SUCCESS, Buildbot.WARNINGS, Buildbot.SKIPPED):
            return True
    return False


def _most_recent_failure_message(build: BuildSnapshot, retry_msg: str) -> str:
    for step in build.steps_by_uid_desc:
        if step.result == Buildbot.SUCCESS and retry_msg in step.state_string:
            return step.state_string
        if step.result == Buildbot.FAILURE:
            return step.state_string
    return ''


def _steps_messages(build: BuildSnapshot, *, sep: str = '\n') -> str:
    return sep.join(step.state_string for step in build.steps_by_uid_asc)


def _steps_messages_from_multiple_builds(builds: Iterable[BuildSnapshot], *, sep: str = '\n') -> str:
    out = ''
    for build in reversed(list(builds)):
        out += '\n\n' + _steps_messages(build, sep=sep)
    return out


def _iso_time(unix_ts: int) -> str:
    return '[[' + datetime.datetime.fromtimestamp(unix_ts).isoformat() + 'Z]]'


def _get_os_details(build: BuildSnapshot) -> str:
    for step in build.steps_by_uid_asc:
        if step.state_string.startswith('OS:'):
            return step.state_string
    return ''


def _get_build_timestamp_iso(build: BuildSnapshot) -> Optional[str]:
    if build.complete_at:
        return _iso_time(build.complete_at)
    if build.last_step and build.last_step.started_at:
        return _iso_time(build.last_step.started_at)
    if build.started_at:
        return _iso_time(build.started_at)
    return None


def _no_build_bubble(inputs: BubbleInputs, config: BubbleConfig, name: str) -> Optional[BubbleData]:
    queue_position = inputs.queue_position
    if not queue_position:
        return None
    display_position = queue_position if queue_position != config.unknown_queue_position else None

    effective_queue = config.get_parent_queue(inputs.queue) or inputs.queue
    queue_full_name = config.queue_full_name(effective_queue)
    url = config.builder_url(queue_full_name) if queue_full_name else None
    details = 'Waiting in queue, processing has not started yet.\n\nPosition in queue: {}'.format(queue_position)
    return BubbleData(
        name=name,
        state='none',
        url=url,
        details_message=details,
        builder_full_name=None,
        os_details=None,
        build_timestamp_iso=None,
        queue_position=display_position,
    )


def _success_parent_build_message(inputs: BubbleInputs, config: BubbleConfig, name: str) -> Optional[BubbleData]:
    queue_full_name = config.queue_full_name(inputs.queue)
    url = config.builder_url(queue_full_name) if queue_full_name else config.build_url(
        inputs.builds[0].builder_id, inputs.builds[0].number)
    builder_full_name = (queue_full_name.replace('-', ' ')
                         if queue_full_name else inputs.builds[0].builder_name.replace('-', ' '))
    return BubbleData(
        name=name,
        state='started',
        url=url,
        details_message='Waiting to run tests.',
        builder_full_name=builder_full_name,
        os_details=_get_os_details(inputs.builds[0]) or None,
        build_timestamp_iso=_get_build_timestamp_iso(inputs.builds[0]),
        queue_position=None,
    )


def _success_message(queue: str, config: BubbleConfig) -> str:
    is_builder = config.is_builder_queue(queue)
    is_tester = config.is_tester_queue(queue)
    if is_builder and is_tester:
        return 'Built successfully and passed tests'
    if is_builder:
        return 'Built successfully'
    if is_tester:
        return 'Passed style check' if queue == 'style' else 'Passed tests'
    return 'Pass'


def _skipped_message(build: BuildSnapshot, builds_seen: int) -> str:
    for pattern, override in SKIPPED_OVERRIDE_MESSAGES:
        if pattern.search(build.state_string):
            base = override
            break
    else:
        base = 'The change is no longer eligible for processing.'
        for pattern, suffix in SKIPPED_REASON_MESSAGES:
            if pattern.search(build.state_string):
                base += ' ' + suffix
                break
    if builds_seen > 1:
        base += '\nSome messages were logged while the change was still eligible:'
    return base


def compute_bubble(inputs: BubbleInputs, config: BubbleConfig, *, now: datetime.datetime) -> Optional[BubbleData]:
    name = _decorated_queue_name(inputs.queue, inputs.hide_icons, config)

    builds = inputs.builds
    build = builds[0] if builds else None

    if not _should_show_bubble_for_build(build, inputs.sent_to_buildbot):
        return None

    if build is None:
        return _no_build_bubble(inputs, config, name)

    builder_full_name = build.builder_name.replace('-', ' ')
    url = config.build_url(build.builder_id, build.number)

    if build.result is None:
        state = 'provisional-fail' if _does_build_contain_failed_step(build) else 'started'
        details = 'Build is in-progress. Recent messages:' + _steps_messages_from_multiple_builds(builds)
    elif build.result == Buildbot.SUCCESS:
        if inputs.is_parent_build:
            hide_threshold = now - datetime.timedelta(days=config.days_to_hide_bubble)
            if inputs.change.created < hide_threshold:
                return None
            return _success_parent_build_message(inputs, config, name)
        state = 'pass'
        details = _success_message(inputs.queue, config)
    elif build.result == Buildbot.WARNINGS:
        state = 'pass'
        details = 'Warning' + _steps_messages_from_multiple_builds(builds)
    elif build.result == Buildbot.FAILURE:
        details = _most_recent_failure_message(build, config.build_retry_msg)
        state = 'provisional-fail' if config.build_retry_msg in details else 'fail'
    elif build.result == Buildbot.SKIPPED:
        state = 'skipped'
        details = _skipped_message(build, len(builds))
        if len(builds) > 1:
            details += _steps_messages_from_multiple_builds(builds)
    elif build.result == Buildbot.EXCEPTION:
        state = 'error'
        details = 'An unexpected error occured. Recent messages:' + _steps_messages_from_multiple_builds(builds)
    elif build.result == Buildbot.RETRY:
        state = 'provisional-fail'
        details = 'Build is being retried. Recent messages:' + _steps_messages_from_multiple_builds(builds)
    elif build.result == Buildbot.CANCELLED:
        state = 'cancelled'
        details = 'Build was cancelled. Recent messages:' + _steps_messages_from_multiple_builds(builds)
    else:
        state = 'error'
        details = 'An unexpected error occured. Recent messages:' + _steps_messages_from_multiple_builds(builds)

    return BubbleData(
        name=name,
        state=state,
        url=url,
        details_message=details,
        builder_full_name=builder_full_name,
        os_details=_get_os_details(build) or None,
        build_timestamp_iso=_get_build_timestamp_iso(build),
        queue_position=None,
    )
