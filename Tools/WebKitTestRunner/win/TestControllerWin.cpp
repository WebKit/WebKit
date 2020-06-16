/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestController.h"

#include <WebCore/NotImplemented.h>
#include <WinBase.h>
#include <fcntl.h>
#include <io.h>
#include <shlwapi.h>
#include <string>
#include <windows.h>
#include <wtf/RunLoop.h>


#define INJECTED_BUNDLE_DLL_NAME "TestRunnerInjectedBundle.dll"

namespace WTR {

static HANDLE webProcessCrashingEvent;
static const char webProcessCrashingEventName[] = "WebKitTestRunner.WebProcessCrashing";
// This is the longest we'll wait (in seconds) for the web process to finish crashing and a crash
// log to be saved. This interval should be just a tiny bit longer than it will ever reasonably
// take to save a crash log.
static const double maximumWaitForWebProcessToCrash = 60;


static LONG WINAPI exceptionFilter(EXCEPTION_POINTERS*)
{
    fputs("#CRASHED\n", stderr);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

enum RunLoopResult { TimedOut, ObjectSignaled, ConditionSatisfied };

static RunLoopResult runRunLoopUntil(bool& condition, HANDLE object, double timeout)
{
    DWORD end = ::GetTickCount() + timeout * 1000;
    if (timeout < 0)
        end = ULONG_MAX;

    while (!condition) {
        DWORD now = ::GetTickCount();
        if (now > end)
            return TimedOut;

        DWORD wait = end - now;
        DWORD objectCount = object ? 1 : 0;
        const HANDLE* objects = object ? &object : 0;

        DWORD result = ::MsgWaitForMultipleObjectsEx(objectCount, objects, wait, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_TIMEOUT)
            return TimedOut;

        if (objectCount && result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + objectCount)
            return ObjectSignaled;

        ASSERT(result == WAIT_OBJECT_0 + objectCount);
        // There are messages in the queue. Process them.
        MSG msg;
        while (::PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
            // WM_MOUSELEAVE is dispatched because the mouse cursor is not on the WebKitTestRunner's window.
            // Ignore WM_MOUSELEAVE because it discontinues mouse dragging events.
            if (msg.message == WM_MOUSELEAVE)
                continue;
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    return ConditionSatisfied;
}

void TestController::notifyDone()
{
}

void TestController::setHidden(bool)
{
}

void TestController::platformInitialize()
{
    // Cygwin calls ::SetErrorMode(SEM_FAILCRITICALERRORS), which we will inherit. This is bad for
    // testing/debugging, as it causes the post-mortem debugger not to be invoked. We reset the
    // error mode here to work around Cygwin's behavior. See <http://webkit.org/b/55222>.
    ::SetErrorMode(0);

    ::SetUnhandledExceptionFilter(exceptionFilter);

    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);

    webProcessCrashingEvent = ::CreateEventA(0, FALSE, FALSE, webProcessCrashingEventName);
}

WKPreferencesRef TestController::platformPreferences()
{
    return WKPageGroupGetPreferences(m_pageGroup.get());
}

void TestController::platformDestroy()
{
}

static WKRetainPtr<WKStringRef> toWK(const char* string)
{
    return adoptWK(WKStringCreateWithUTF8CString(string));
}

void TestController::platformInitializeContext()
{
    // FIXME: FIXME! NO WKContextSetShouldPaintNativeControls in API list
    // FIXME: Make DRT pass with Windows native controls. <http://webkit.org/b/25592>
    // WKContextSetShouldPaintNativeControls(m_context.get(), false);

    WKContextSetInitializationUserDataForInjectedBundle(m_context.get(), toWK(webProcessCrashingEventName).get());
}

void TestController::platformRunUntil(bool& condition, WTF::Seconds timeout)
{
    RunLoopResult result = runRunLoopUntil(condition, webProcessCrashingEvent, timeout.seconds());
    if (result == TimedOut || result == ConditionSatisfied)
        return;
    ASSERT(result == ObjectSignaled);

    // The web process is crashing. A crash log might be being saved, which can take a long
    // time, and we don't want to time out while that happens.

    // First, let the test harness know this happened so it won't think we've hung. But
    // make sure we don't exit just yet!
    m_shouldExitWhenWebProcessCrashes = false;
    processDidCrash();
    m_shouldExitWhenWebProcessCrashes = true;

    // Then spin a run loop until it finishes crashing to give time for a crash log to be saved. If
    // it takes too long for a crash log to be saved, we'll just give up.
    bool neverSetCondition = false;
    result = runRunLoopUntil(neverSetCondition, 0, maximumWaitForWebProcessToCrash);
    ASSERT_UNUSED(result, result == TimedOut);
    exit(1);
}

void TestController::platformDidCommitLoadForFrame(WKPageRef, WKFrameRef)
{
    notImplemented();
}

void TestController::initializeInjectedBundlePath()
{
    m_injectedBundlePath.adopt(WKStringCreateWithUTF8CString(INJECTED_BUNDLE_DLL_NAME));
}

void TestController::initializeTestPluginDirectory()
{
    wchar_t exePath[MAX_PATH];
    if (::GetModuleFileName(nullptr, exePath, MAX_PATH)) {
        wchar_t drive[_MAX_DRIVE];
        wchar_t dir[_MAX_DIR];
        _wsplitpath(exePath, drive, dir, nullptr, nullptr);

        wchar_t bundleDir[MAX_PATH];
        wcsncpy(bundleDir, drive, MAX_PATH);
        wcsncat(bundleDir, dir, MAX_PATH - _MAX_DRIVE);

        char bundleDirUTF8[MAX_PATH];
        ::WideCharToMultiByte(CP_UTF8, 0, bundleDir, -1, bundleDirUTF8, MAX_PATH, nullptr, nullptr);

        m_testPluginDirectory.adopt(WKStringCreateWithUTF8CString(bundleDirUTF8));
    }
}

void TestController::runModal(PlatformWebView*)
{
    // FIXME: Need to implement this to test showModalDialog.
    notImplemented();
}

void TestController::abortModal()
{
    notImplemented();
}

WKContextRef TestController::platformContext()
{
    return m_context.get();
}

const char* TestController::platformLibraryPathForTesting()
{
    notImplemented();
    return nullptr;
}

void TestController::platformConfigureViewForTest(const TestInvocation&)
{
    notImplemented();
}

void TestController::platformResetPreferencesToConsistentValues()
{
    notImplemented();
}

void TestController::updatePlatformSpecificTestOptionsForTest(TestOptions&, const std::string&) const
{
    notImplemented();
}

} // namespace WTR
