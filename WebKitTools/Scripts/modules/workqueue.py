#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

import os
import time

from datetime import datetime, timedelta

from modules.logging import log, OutputTee
from modules.scm import ScriptError
from modules.statusbot import StatusBot

class WorkQueueDelegate:
    def queue_name(self):
        raise NotImplementedError, "subclasses must implement"

    def queue_log_path(self):
        raise NotImplementedError, "subclasses must implement"

    def work_logs_directory(self):
        raise NotImplementedError, "subclasses must implement"

    def status_host(self):
        raise NotImplementedError, "subclasses must implement"

    def begin_work_queue(self):
        raise NotImplementedError, "subclasses must implement"

    def should_continue_work_queue(self):
        raise NotImplementedError, "subclasses must implement"

    def next_work_item(self):
        raise NotImplementedError, "subclasses must implement"

    def should_proceed_with_work_item(self, work_item):
        # returns (safe_to_proceed, waiting_message, patch)
        raise NotImplementedError, "subclasses must implement"

    def process_work_item(self, work_item):
        raise NotImplementedError, "subclasses must implement"

    def handle_unexpected_error(self, work_item, message):
        raise NotImplementedError, "subclasses must implement"


class WorkQueue:
    def __init__(self, name, delegate):
        self._name = name
        self._delegate = delegate
        self._output_tee = OutputTee()

    log_date_format = "%Y-%m-%d %H:%M:%S"
    sleep_duration_text = "5 mins"
    seconds_to_sleep = 300
    handled_error_code = 2

    # Child processes exit with a special code to the parent queue process can detect the error was handled.
    @classmethod
    def exit_after_handled_error(cls, error):
        log(error)
        exit(cls.handled_error_code)

    def run(self):
        self._begin_logging()
        self.status_bot = StatusBot(host=self._delegate.status_host())

        self._delegate.begin_work_queue()
        while (self._delegate.should_continue_work_queue()):
            self._ensure_work_log_closed()
            try:
                work_item = self._delegate.next_work_item()
                if not work_item:
                    self._update_status_and_sleep("Empty queue.")
                    continue
                (safe_to_proceed, waiting_message, patch) = self._delegate.should_proceed_with_work_item(work_item)
                if not safe_to_proceed:
                    self._update_status_and_sleep(waiting_message)
                    continue
                self.status_bot.update_status(self._name, waiting_message, patch)
            except Exception, e:
                # Don't try tell the status bot, in case telling it causes an exception.
                self._sleep("Exception while preparing queue: %s." % e)
                continue

            self._open_work_log(bug_id)
            try:
                self._delegate.process_work_item(work_item)
            except ScriptError, e:
                # Use a special exit code to indicate that the error was already
                # handled in the child process and we should just keep looping.
                if e.exit_code == self.handled_error_code:
                    continue
                message = "Unexpected failure when landing patch!  Please file a bug against bugzilla-tool.\n%s" % e.message_with_output()
                self._delegate.handle_unexpected_error(work_item, message)
        # Never reached.
        self._ensure_work_log_closed()

    def _begin_logging(self):
        self._queue_log = self._output_tee.add_log(self._delegate.queue_log_path())
        self._work_log = None

    def _open_work_log(self, bug_id):
        work_log_path = os.path.join(self._delegate.work_logs_directory(), "%s.log" % bug_id)
        self._work_log = self._output_tee.add_log(work_log_path)

    def _ensure_work_log_closed(self):
        # If we still have a bug log open, close it.
        if self._work_log:
            self._output_tee.remove_log(self._work_log)
            self._work_log = None

    @classmethod
    def _sleep_message(cls, message):
        wake_time = datetime.now() + timedelta(seconds=cls.seconds_to_sleep)
        return "%s Sleeping until %s (%s)." % (message, wake_time.strftime(cls.log_date_format), cls.sleep_duration_text)

    @classmethod
    def _sleep(cls, message):
        log(cls._sleep_message(message))
        time.sleep(cls.seconds_to_sleep)

    def _update_status_and_sleep(self, message):
        status_message = self._sleep_message(message)
        self.status_bot.update_status(status_message)
        log(status_message)
        time.sleep(self.seconds_to_sleep)
