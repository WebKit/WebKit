# Copyright (C) 2019 Apple Inc. All rights reserved.
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
import subprocess
import time

DUMP_PID = 0
DUMP_START_TIME = 0

while(1):
    time.sleep(10)
    CUR_TIME = int(time.time())

    pid_fetch = subprocess.Popen("ps -e | grep \"DumpRenderTree\" | awk '{print $1}'", shell=True, stdout=subprocess.PIPE,)
    CURRENT_PID = pid_fetch.communicate()[0].split('\n')[0]
    CHECKED = 1
    # We only care about doing anything if there is a DumpRenderTree running
    if (CURRENT_PID):
        # If the PID has changed, reset the timer and the active pid
        if (DUMP_PID != CURRENT_PID):
            DUMP_PID = CURRENT_PID
            DUMP_START_TIME = time.time()
        else:
        # If the active and current PID are the same and an hour has elapsed, kill DumpRenderTree and reset the active PID to 0
            if (CUR_TIME - DUMP_START_TIME >= 3600):
                os.system("taskkill /F /IM DumpRenderTree.exe")
                os.system("taskkill /F /IM ImageDiff.exe")
                DUMP_PID = 0
    else:
        DUMP_PID = 0
