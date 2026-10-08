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
import unittest

from webkitcorepy import OutputCapture

from webkitpy.common.unclaimed_shard import ShardAttempts, UnclaimedShardTracker


class FakePort(object):
    def __init__(self, usable=True, expecting=False):
        self.usable = usable
        self.expecting = expecting

    def has_usable_device(self):
        return self.usable

    def expects_more_devices(self):
        return self.expecting


class UnclaimedShardTrackerTest(unittest.TestCase):

    def _return_whole(self, tracker, times, size=8):
        attempts = ShardAttempts()
        for _ in range(times):
            attempts = tracker.attempts_for_redispatch('shard', size, list(range(size)), attempts)
            if attempts is None:
                break
        return attempts

    def test_nothing_returned_is_not_redispatched(self):
        tracker = UnclaimedShardTracker(FakePort(), 4)
        self.assertIsNone(tracker.attempts_for_redispatch('shard', 8, [], ShardAttempts()))
        self.assertEqual(tracker.abandoned_test_count, 0)

    def test_a_shard_that_shrinks_is_not_fruitless(self):
        tracker = UnclaimedShardTracker(FakePort(), 4)
        attempts = tracker.attempts_for_redispatch('shard', 8, list(range(5)), ShardAttempts(total=1, fruitless=2))
        self.assertEqual(attempts, ShardAttempts(total=2, fruitless=0))

    def test_a_whole_shard_is_fruitless(self):
        tracker = UnclaimedShardTracker(FakePort(), 4)
        attempts = tracker.attempts_for_redispatch('shard', 8, list(range(8)), ShardAttempts(total=1, fruitless=1))
        self.assertEqual(attempts, ShardAttempts(total=2, fruitless=2))

    def test_a_shard_no_device_will_run_is_abandoned_after_one_round_of_workers(self):
        tracker = UnclaimedShardTracker(FakePort(), 4)
        with OutputCapture():
            self.assertIsNotNone(self._return_whole(tracker, 3))
            self.assertIsNone(self._return_whole(tracker, 4))
        self.assertEqual(tracker.abandoned_test_count, 8)

    def test_fewer_workers_still_get_the_floor(self):
        tracker = UnclaimedShardTracker(FakePort(), 1)
        with OutputCapture():
            self.assertIsNotNone(self._return_whole(tracker, 2))
            self.assertIsNone(self._return_whole(tracker, 3))

    def test_a_shard_that_keeps_shrinking_stops_after_two_rounds_of_workers(self):
        tracker = UnclaimedShardTracker(FakePort(), 4)
        attempts = ShardAttempts()
        remaining = 40
        with OutputCapture():
            while attempts is not None:
                dispatched, remaining = remaining, remaining - 1
                last = attempts
                attempts = tracker.attempts_for_redispatch('shard', dispatched, list(range(remaining)), attempts)
        self.assertEqual(last.fruitless, 0)
        self.assertEqual(last.total, 7)

    def test_nothing_is_abandoned_while_devices_are_coming(self):
        tracker = UnclaimedShardTracker(FakePort(usable=False, expecting=True), 4)
        self.assertIsNotNone(self._return_whole(tracker, 7))

    def test_everything_is_abandoned_once_no_device_is_usable(self):
        tracker = UnclaimedShardTracker(FakePort(usable=False), 4)
        with OutputCapture() as captured:
            self.assertIsNone(tracker.attempts_for_redispatch('shard', 8, list(range(8)), ShardAttempts()))
        self.assertIn('No usable devices remain', captured.root.log.getvalue())
        self.assertEqual(tracker.abandoned_test_count, 8)
