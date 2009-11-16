#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
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
import shutil
import tempfile
import unittest

from modules.workqueue import WorkQueue, WorkQueueDelegate

class WorkQueueTest(unittest.TestCase, WorkQueueDelegate):
    def test_trivial(self):
        self.set_up()
        work_queue = WorkQueue(self)
        work_queue.run()
        self.assertEquals(self.callbacks, [
            'queue_log_path',
            'status_host',
            'begin_work_queue',
            'should_continue_work_queue',
            'next_work_item',
            'should_proceed_with_work_item',
            'work_logs_directory',
            'process_work_item',
            'should_continue_work_queue'])
        self.clean_up()

    def set_up(self):
        self.callbacks = []
        self.run_before = False
        self.temp_dir = tempfile.mkdtemp(suffix="work_queue_test_logs")

    def clean_up(self):
        os.path.exists(self.queue_log_path())
        os.path.exists(os.path.join(self.work_logs_directory(), "42.log"))
        shutil.rmtree(self.temp_dir)

    def queue_log_path(self):
        self.callbacks.append("queue_log_path")
        return os.path.join(self.temp_dir, "queue_log_path")

    def work_logs_directory(self):
        self.callbacks.append("work_logs_directory")
        return os.path.join(self.temp_dir, "work_log_path")

    def status_host(self):
        self.callbacks.append("status_host")
        return None

    def begin_work_queue(self):
        self.callbacks.append("begin_work_queue")

    def should_continue_work_queue(self):
        self.callbacks.append("should_continue_work_queue")
        if not self.run_before:
            self.run_before = True
            return True
        return False

    def next_work_item(self):
        self.callbacks.append("next_work_item")
        return "work_item"

    def should_proceed_with_work_item(self, work_item):
        self.callbacks.append("should_proceed_with_work_item")
        self.assertEquals(work_item, "work_item")
        return (True, "waiting_message", 42)

    def process_work_item(self, work_item):
        self.callbacks.append("process_work_item")
        self.assertEquals(work_item, "work_item")

    def handle_unexpected_error(self, work_item, message):
        self.callbacks.append("handle_unexpected_error")
        self.assertEquals(work_item, "work_item")


if __name__ == '__main__':
    unittest.main()
