# Copyright (C) 2017 Apple Inc. All rights reserved.
# Copyright (C) 2020 Igalia S.L.
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
import logging
import re

import requests

from webkitpy.common.config.urls import ewsserver_default_host
from webkitpy.common.net.bugzilla import bugzilla
from webkitpy.common.net.bugzilla.ews import EWS

try:
    from urllib.parse import urljoin, urlsplit
except ImportError:
    from urlparse import urljoin, urlsplit


# Buildbot status codes referenced from https://github.com/buildbot/buildbot/blob/master/master/buildbot/process/results.py
EWS_STATECODES = {
    "SUCCESS": 0,
    "WARNINGS": 1,
    "FAILURE": 2,
    "SKIPPED": 3,
    "EXCEPTION": 4,
    "RETRY": 5,
    "CANCELLED": 6,
}


_log = logging.getLogger(__name__)


def _platform_name_for_bot(bot_name):
    name = bot_name
    if "mac" in name and name.endswith("-wk2"):
        return "mac-wk2"
    if "mac" in name and not name.endswith("-wk2"):
        return "mac-wk1"
    if "simulator" in name and name.startswith("ios"):
        return "ios"
    if "win-future" in name:
        return "win"
    if "gtk" in name:
        return "gtk"
    if "wpe" in name:
        return "wpe"
    if "-debug" in name:
        name = name.replace("-debug", "")
    if "-wpt" in name:
        name = name.replace("-wpt", "")
    return name


def _get_first_layout_test_step(steps):
    # pick the first run as the one for updating the layout tests.
    layout_tests_steps = [step for step in steps if step["name"] == "layout-tests"]

    if not layout_tests_steps:
        return None

    if len(layout_tests_steps) > 1:
        msg = "multiple layout-tests steps in one build?!"
        raise ValueError(msg)

    return layout_tests_steps[0]


def _is_relevant_results(bot_name, step, bot_filter_name=None):
    if step["name"] != "layout-tests":
        return False

    if not step["complete"]:
        _log.info(
            (
                "Skipping to process unfinished layout-tests run for bot {}. "
                "Please retry later"
            ).format(bot_name)
        )
        return False

    if bot_filter_name and bot_filter_name not in bot_name:
        return False

    if step["results"] not in (
        EWS_STATECODES["FAILURE"],
        EWS_STATECODES["WARNINGS"],
    ):
        return False

    return True


def lookup_ews_results(ews_build_urls, bot_filter_name=None):
    ews_results = {}
    ews_servers = {}

    for url in ews_build_urls:
        m = re.match("(http.*webkit.org)/#/builders/([0-9]+)/builds/([0-9]+)$", url)
        if not m:
            msg = "The URL {} from EWS has an unexpected format".format(url)
            raise ValueError(msg)

        buildbot_server, builder_id, build_number = m.groups()

        ews = ews_servers.setdefault(buildbot_server, EWS(buildbot_server))
        builder = ews.builders()[int(builder_id)]
        steps = ews.steps_from_build_number(builder_id, build_number)

        bot_name = builder["description"]
        platform = _platform_name_for_bot(bot_name)

        layout_tests_step = _get_first_layout_test_step(steps)
        if layout_tests_step is None:
            continue

        if not _is_relevant_results(bot_name, layout_tests_step, bot_filter_name):
            continue

        for tests_urls in layout_tests_step["urls"]:
            tests_url = urljoin(buildbot_server, tests_urls["url"])
            if not urlsplit(tests_url).path.endswith(".zip"):
                continue

            ews_results.setdefault(platform, []).append(tests_url)
            break

    return ews_results


def lookup_ews_results_from_bugzilla_patch(patch, bot_filter_name=None):
    _log.info(
        "Looking for EWS results in patch attachment %s from bug %s"
        % (patch.id(), patch.bug().id())
    )
    ews_bubbles_url = "https://{}/status/{}/".format(ewsserver_default_host, patch.id())
    _log.debug("Querying bubble status at {}".format(ews_bubbles_url))
    ews_bubbles_status_request = requests.get(ews_bubbles_url)
    ews_bubbles_status_request.raise_for_status()
    ews_bubbles_status = json.loads(ews_bubbles_status_request.text)

    ews_bubbles_urls = [v["url"] for v in ews_bubbles_status.values()]

    return lookup_ews_results(ews_bubbles_urls, bot_filter_name)


def lookup_ews_results_from_bugzilla(
    bugzilla_id, is_bug_id=True, attachment_fetcher=None, bot_filter_name=None
):
    attachment_fetcher = (
        bugzilla.Bugzilla() if attachment_fetcher is None else attachment_fetcher
    )

    if is_bug_id:
        bug_info = attachment_fetcher.fetch_bug(bugzilla_id)
        attachments = [
            attachments
            for attachments in bug_info.attachments(include_obsolete=False)
            if attachments.is_patch()
        ]

        if len(attachments) > 1:
            msg = (
                "Found more than one non-obsolete patch in bug {}. "
                "Please specify which one to process."
            ).format(bugzilla_id)
            raise RuntimeError(msg)

        if len(attachments) < 1:
            msg = (
                "Couldn't find any non-obsolete patch in bug {}. "
                "Please specify which one to process."
            ).format(bugzilla_id)
            raise RuntimeError(msg)

        patch = attachments[0]

    else:
        patch = attachment_fetcher.fetch_attachment(bugzilla_id)

    if not patch.is_patch():
        msg = "Attachment {} its not a patch. Can't continue.".format(bugzilla_id)
        raise RuntimeError(msg)

    return lookup_ews_results_from_bugzilla_patch(patch, bot_filter_name)


def lookup_ews_results_from_pr(pr):
    urls = [status.url for status in pr.statuses]
    return lookup_ews_results(urls)


def lookup_ews_results_from_repo_via_pr(repository):
    source_remote = repository.default_remote
    remote_repo = repository.remote(name=source_remote)
    if not remote_repo.pull_requests:
        msg = "Remote repo doesn't support pull requests"
        raise ValueError(msg)

    branch = repository.branch
    existing_pr = None
    for pr in remote_repo.pull_requests.find(opened=None, head=branch):
        existing_pr = pr
        break

    if existing_pr is None:
        msg = "No PR found for current branch"
        raise ValueError(msg)

    return lookup_ews_results_from_pr(existing_pr)
