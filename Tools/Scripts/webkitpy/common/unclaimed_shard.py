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

import logging

from dataclasses import dataclass, field
from typing import List, Optional

_log = logging.getLogger(__name__)


@dataclass
class UnclaimedShard:
    """Tests a worker returns because its device was unusable, so they run elsewhere instead of failing."""

    tests: List = field(default_factory=list)
    name: Optional[str] = None


@dataclass(frozen=True)
class ShardAttempts:
    total: int = 0
    fruitless: int = 0


class UnclaimedShardTracker(object):
    FEWEST_FRUITLESS_ATTEMPTS = 3

    def __init__(self, port, num_workers):
        self._port = port
        self._fruitless_attempt_limit = max(self.FEWEST_FRUITLESS_ATTEMPTS, num_workers)
        # A device dying after every test shrinks a shard one test at a time, so stop after two rounds of every worker.
        self._attempt_limit = 2 * self._fruitless_attempt_limit
        self.abandoned_test_count = 0

    def attempts_for_redispatch(self, name, dispatched_count, returned_tests, attempts):
        """Returns the attempts to send the returned tests back out with, or None if they will not run."""
        if not returned_tests:
            return None

        attempts = ShardAttempts(
            total=attempts.total + 1,
            fruitless=0 if len(returned_tests) < dispatched_count else attempts.fruitless + 1,
        )
        if not self._should_abandon(attempts):
            return attempts

        self.abandoned_test_count += len(returned_tests)
        if self._port.has_usable_device():
            reason = f'No device took {name} after {attempts.total} attempts'
        else:
            reason = 'No usable devices remain'
        _log.error(f'{reason}; {len(returned_tests)} tests in {name} will not run')
        return None

    def _should_abandon(self, attempts):
        if attempts.total >= self._attempt_limit:
            return True
        if self._port.expects_more_devices():
            return False
        if not self._port.has_usable_device():
            return True
        return attempts.fruitless >= self._fruitless_attempt_limit
