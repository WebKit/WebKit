/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <shlwapi.h>
#include <sys/stat.h>

// Default timeout in ms for file page loads when in layout test mode.
const int kDefaultFileTestTimeoutMillisecs = 10 * 1000;
const int kDefaultWatchDogTimeoutMillisecs = kDefaultFileTestTimeoutMillisecs + 1 * 1000;

// Thread main to run for the thread which just tests for timeout.
unsigned int __stdcall watchDogThread(void *arg)
{
    // If we're debugging a layout test, don't timeout.
    if (::IsDebuggerPresent())
    return 0;

    TestShell* shell = static_cast<TestShell*>(arg);
    // FIXME: Do we need user-specified time settings as with the original
    // Chromium implementation?
    DWORD timeout = static_cast<DWORD>(kDefaultWatchDogTimeoutMillisecs);
    DWORD rv = WaitForSingleObject(shell->finishedEvent(), timeout);
    if (rv == WAIT_TIMEOUT) {
        // Print a warning to be caught by the layout-test script.
        // Note: the layout test driver may or may not recognize
        // this as a timeout.
        puts("#TEST_TIMED_OUT\n");
        puts("#EOF\n");
        fflush(stdout);
        TerminateProcess(GetCurrentProcess(), 0);
    }
    // Finished normally.
    return 0;
}

void TestShell::waitTestFinished()
{
    DCHECK(!m_testIsPending) << "cannot be used recursively";

    m_testIsPending = true;

    // Create a watchdog thread which just sets a timer and
    // kills the process if it times out.  This catches really
    // bad hangs where the shell isn't coming back to the
    // message loop.  If the watchdog is what catches a
    // timeout, it can't do anything except terminate the test
    // shell, which is unfortunate.
    m_finishedEvent = CreateEvent(0, TRUE, FALSE, 0);
    DCHECK(m_finishedEvent);

    HANDLE threadHandle = reinterpret_cast<HANDLE>(_beginthreadex(
                                                       0,
                                                       0,
                                                       &watchDogThread,
                                                       this,
                                                       0,
                                                       0));
    DCHECK(threadHandle);

    // TestFinished() will post a quit message to break this loop when the page
    // finishes loading.
    while (m_testIsPending)
        webkit_support::RunMessageLoop();

    // Tell the watchdog that we are finished.
    SetEvent(m_finishedEvent);

    // Wait to join the watchdog thread.  (up to 1s, then quit)
    WaitForSingleObject(threadHandle, 1000);
}

void platformInit()
{
    // Set stdout/stderr binary mode.
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);

    // Load Ahem font.
    // AHEM____.TTF is copied to the directory of DumpRenderTree.exe by WebKit.gyp.
    WCHAR path[_MAX_PATH];
    if (!::GetModuleFileName(0, path, _MAX_PATH)) {
        fprintf(stderr, "Can't get the module path.\n");
        exit(1);
    }
    ::PathRemoveFileSpec(path);
    wcscat_s(path, _MAX_PATH, L"/AHEM____.TTF");
    struct _stat ahemStat;
    if (_wstat(path, &ahemStat) == -1) {
        fprintf(stderr, "Can't access: '%S'\n", path);
        exit(1);
    }

    FILE* fp = _wfopen(path, L"rb");
    if (!fp) {
        _wperror(path);
        exit(1);
    }
    size_t size = ahemStat.st_size;
    char* fontBuffer = new char[size];
    if (fread(fontBuffer, 1, size, fp) != size) {
        fprintf(stderr, "Can't read the font: '%S'\n", path);
        fclose(fp);
        exit(1);
    }
    fclose(fp);
    DWORD numFonts = 1;
    HANDLE fontHandle = ::AddFontMemResourceEx(fontBuffer, size, 0, &numFonts);
    delete[] fontBuffer; // OS owns a copy of the buffer.
    if (!fontHandle) {
        fprintf(stderr, "Failed to register Ahem font: '%S'\n", path);
        exit(1);
    }
    // We don't need to release the font explicitly.
}
