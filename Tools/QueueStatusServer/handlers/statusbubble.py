# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

import operator

from google.appengine.ext import webapp
from google.appengine.ext.webapp import template

from model.attachment import Attachment
from model.activeworkitems import ActiveWorkItems
from model.patchlog import PatchLog
from model.queues import Queue
from model.queuestatus import QueueStatus
from model.workitems import WorkItems
from sets import Set

progress_statuses = Set([
    "Started processing patch",
    "Cleaned working directory",
    "Updated working directory",
    "Checked relevance of patch",
    "Applied patch",
    "Built patch",
    "Watchlist applied",
    "Style checked",
    "ChangeLog validated",
    "Built patch",
    "Able to build without patch",
    "Passed tests",
    "Able to pass tests without patch",
    "Landed patch"
])

class StatusBubble(webapp.RequestHandler):
    def _iso_time(self, time):
        return "[[" + time.isoformat() + "Z]]"

    # queue_position includes items that are already active, so it's misleading.
    # For a queue that has 8 bots, being #9 in the queue actually means being #1.
    def _real_queue_position(self, queue, queue_position):
        active_work_items = queue.active_work_items().item_ids
        if active_work_items:
            return queue_position - len(active_work_items)
        else:
            return queue_position

    def _latest_resultative_status(self, statuses):
        for status in statuses:
            if not status.message in progress_statuses:
                return status
        return None

    def _build_message_for_provisional_failure(self, queue, attachment, queue_position, statuses):
        patch_log = PatchLog.lookup_if_exists(attachment.id, queue.name())
        if not patch_log:
            return "Internal error. No PatchLog entry in database."

        is_active = attachment.id in queue.active_work_items().item_ids
        try_count = patch_log.retry_count + (not is_active)  # retry_count is updated when a new attempt starts.
        latest_resultative_status = self._latest_resultative_status(statuses)
        tree_is_red = latest_resultative_status.message == "Unable to pass tests without patch (tree is red?)" or latest_resultative_status.message == "Unable to build without patch"

        message = latest_resultative_status.message + "."
        if is_active:
            if tree_is_red:
                message += "\n\nTrying again now."
            else:
                message += "\n\nThis result is not final, as the issue could be a pre-existing one. Trying to determine that now."
                if try_count == 1:
                    message += "\n\nPreviously completed a round of testing, but couldn't arrive at a definitive conclusion."
                elif try_count > 1:
                    message += "\n\nPreviously completed " + str(try_count) + " rounds of testing, but couldn't arrive at a definitive conclusion."
        else:
            real_queue_position = self._real_queue_position(queue, queue_position)
            if tree_is_red:
                message += "\n\nWill try again, currently #" + str(real_queue_position) + " in queue."
            else:
                message += "\n\nThis result is not final, as the issue can be a pre-existing one. "
                if try_count == 1:
                    message += "Completed one round "
                else:
                    message += "Completed " + str(try_count) + " rounds "
                message += "of testing trying to determine that, but couldn't arrive at a definitive conclusion yet.\n\nWill try again, currently #" + str(real_queue_position) + " in queue."
        message += "\n\nPlease click the bubble for detailed results.\n\n" + self._iso_time(statuses[0].date)
        return message

    def _build_bubble(self, queue, attachment, queue_position):
        bubble = {
            "name": queue.short_name().lower(),
            "attachment_id": attachment.id,
            "queue_name": queue.name(),
        }
        # 10 recent statuses is enough to always include a resultative one, if there were any at all.
        statuses = QueueStatus.all().filter('queue_name =', queue.name()).filter('active_patch_id =', attachment.id).order('-date').fetch(limit=10)
        if not statuses:
            bubble["had_resultative_status_other_than_failure_to_apply"] = False
            if attachment.id in queue.active_work_items().item_ids:
                bubble["state"] = "started"
                bubble["details_message"] = "Started processing, no output yet.\n\n" + self._iso_time(queue.active_work_items().time_for_item(attachment.id))
            else:
                real_queue_position = self._real_queue_position(queue, queue_position)
                bubble["state"] = "none"
                bubble["details_message"] = "Waiting in queue, processing has not started yet.\n\nPosition in queue: " + str(real_queue_position)
                bubble["queue_position"] = real_queue_position
        else:
            latest_resultative_status = self._latest_resultative_status(statuses)
            bubble["had_resultative_status_other_than_failure_to_apply"] = any(map(lambda status:
                latest_resultative_status and latest_resultative_status.message != "Error: " + queue.name() + " unable to apply patch.",
                statuses))
            if not latest_resultative_status:
                bubble["state"] = "started"
                bubble["details_message"] = ("Recent messages:\n\n"
                    + "\n".join([status.message for status in statuses]) + "\n\n" + self._iso_time(statuses[0].date))
            elif statuses[0].message == "Pass":
                bubble["state"] = "pass"
                bubble["details_message"] = "Pass\n\n" + self._iso_time(statuses[0].date)
            elif statuses[0].message == "Fail":
                bubble["state"] = "fail"
                message_to_display = statuses[1].message if len(statuses) > 1 else statuses[0].message
                bubble["details_message"] = message_to_display + "\n\n" + self._iso_time(statuses[0].date)
            elif "did not process patch" in statuses[0].message:
                bubble["state"] = "none"
                bubble["details_message"] = "The patch is no longer eligible for processing."

                if "Bug is already closed" in statuses[0].message:
                    bubble["details_message"] += " Bug was already closed when EWS attempted to process it."
                elif "Patch is marked r-" in statuses[0].message:
                    bubble["details_message"] += " Patch was already marked r- when EWS attempted to process it."
                elif "Patch is obsolete" in statuses[0].message:
                    bubble["details_message"] += " Patch was obsolete when EWS attempted to process it."
                elif "No patch committer found" in statuses[0].message:
                    bubble["details_message"] += " Patch was not authorized by a commmitter."

                if len(statuses) > 1:
                    if len(statuses) == 2:
                        bubble["details_message"] += "\nOne message was logged while the patch was still eligible:\n\n"
                    else:
                        bubble["details_message"] += "\nSome messages were logged while the patch was still eligible:\n\n"
                    bubble["details_message"] += "\n".join([status.message for status in statuses[1:]]) + "\n\n" + self._iso_time(statuses[0].date)
            elif statuses[0].message == "Error: " + queue.name() + " unable to apply patch.":
                bubble["state"] = "fail"
                message_to_display = statuses[1].message if len(statuses) > 1 else statuses[0].message
                bubble["details_message"] = message_to_display + "\n\n" + self._iso_time(statuses[0].date)
                bubble["failed_to_apply"] = True
            elif statuses[0].message.startswith("Error: "):
                bubble["state"] = "error"
                bubble["details_message"] = "\n".join([status.message for status in statuses]) + "\n\n" + self._iso_time(statuses[0].date)
            elif queue_position:
                bubble["state"] = "provisional-fail"
                bubble["details_message"] = self._build_message_for_provisional_failure(queue, attachment, queue_position, statuses)
            else:
                bubble["state"] = "error"
                bubble["details_message"] = ("Internal error. Latest status implies that the patch should be in queue, but it is not. Recent messages:\n\n"
                    + "\n".join([status.message for status in statuses]) + "\n\n" + self._iso_time(statuses[0].date))

        if "details_message" in bubble:
            bubble["details_message"] = queue.display_name() + "\n\n" + bubble["details_message"]

        return bubble

    def _should_show_bubble_for(self, attachment, queue):
        # Any pending queue is shown.
        if attachment.position_in_queue(queue):
            return True

        if not queue.is_ews():
            return False

        status = attachment.status_for_queue(queue)
        return bool(status and not status.did_skip())

    def _build_bubbles_for_attachment(self, attachment):
        show_submit_to_ews = True
        bubbles = []
        for queue in Queue.all():
            if not self._should_show_bubble_for(attachment, queue):
                continue
            queue_position = attachment.position_in_queue(queue)
            bubble = self._build_bubble(queue, attachment, queue_position)
            if bubble:
                bubbles.append(bubble)
            # If at least one EWS queue has status, we don't show the submit-to-ews button.
            if queue.is_ews():
                show_submit_to_ews = False

        failed_to_apply = any(map(lambda bubble: "failed_to_apply" in bubble, bubbles))
        had_resultative_status_other_than_failure_to_apply = any(map(lambda bubble: bubble["had_resultative_status_other_than_failure_to_apply"], bubbles))

        return (bubbles, show_submit_to_ews, failed_to_apply and not had_resultative_status_other_than_failure_to_apply)

    def get(self, attachment_id_string):
        attachment_id = int(attachment_id_string)
        attachment = Attachment(attachment_id)
        bubbles, show_submit_to_ews, show_failure_to_apply = self._build_bubbles_for_attachment(attachment)

        template_values = {
            "bubbles": bubbles,
            "attachment_id": attachment_id,
            "show_submit_to_ews": show_submit_to_ews,
            "show_failure_to_apply": show_failure_to_apply,
        }
        self.response.out.write(template.render("templates/statusbubble.html", template_values))
