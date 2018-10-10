# Copyright (C) 2014, 2017 Apple Inc. All rights reserved.
# Copyright (C) 2013 Google Inc. All rights reserved.
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

from datetime import timedelta

# Eventually the list of queues may be stored in the data store.
all_queue_names = [
    "commit-queue",
    "style-queue",
    "gtk-wk2-ews",
    "ios-ews",
    "ios-sim-ews",
    "bindings-ews",
    "jsc-ews",
    "jsc-mips-ews",
    "jsc-armv7-ews",
    "mac-ews",
    "mac-wk2-ews",
    "mac-debug-ews",
    "mac-32bit-ews",
    "webkitpy-ews",
    "win-ews",
    "wincairo-ews",
    "wpe-ews",
]

# If the patch is still active after this much time, then a bot must have frozen or rebooted,
# and dropped the patch on the floor. We will ignore the lock in this case, and let another bot pick up.
work_item_lock_timeout = timedelta(minutes=120)
