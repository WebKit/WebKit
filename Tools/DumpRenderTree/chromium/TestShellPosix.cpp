/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestShell.h"

#include "webkit/support/webkit_support.h"

#include <signal.h>
#include <unistd.h>

static void AlarmHandler(int)
{
    // If the alarm alarmed, kill the process since we have a really bad hang.
    puts("\n#TEST_TIMED_OUT\n");
    puts("#EOF\n");
    fflush(stdout);
    exit(0);
}

void TestShell::waitTestFinished()
{
    ASSERT(!m_testIsPending);
    m_testIsPending = true;

    // Install an alarm signal handler that will kill us if we time out.
    struct sigaction alarmAction;
    alarmAction.sa_handler = AlarmHandler;
    sigemptyset(&alarmAction.sa_mask);
    alarmAction.sa_flags = 0;

    struct sigaction oldAction;
    sigaction(SIGALRM, &alarmAction, &oldAction);
    alarm(layoutTestTimeoutForWatchDog() / 1000);

    // TestFinished() will post a quit message to break this loop when the page
    // finishes loading.
    while (m_testIsPending)
        webkit_support::RunMessageLoop();

    // Remove the alarm.
    alarm(0);
    sigaction(SIGALRM, &oldAction, 0);
}
