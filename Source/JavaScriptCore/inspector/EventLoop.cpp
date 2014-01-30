/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EventLoop.h"

#if OS(WINDOWS)
#include <windows.h>
#elif USE(CF)
#include <CoreFoundation/CFRunLoop.h>
#elif PLATFORM(EFL)
#include <Ecore.h>
#elif PLATFORM(GTK)
#include <glib.h>
#endif

namespace Inspector {

void EventLoop::cycle()
{
#if OS(WINDOWS)
    MSG msg;
    if (!GetMessage(&msg, 0, 0, 0)) {
        m_ended = true;
        return;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
#elif USE(CF)
    // Run the RunLoop in a custom run loop mode to prevent default observers
    // to run and potentially evaluate JavaScript in this context while we are
    // nested. Only the debugger should control things until we continue.
    // FIXME: This is not a perfect solution, as background threads are not
    // paused and can still access and evalute script in the JSContext.
    static const CFStringRef kRWIRunLoopMode = CFSTR("com.apple.JavaScriptCore.remote-inspector-runloop-mode");
    CFTimeInterval timeInterval = 0.05;
    CFRunLoopRunInMode(kRWIRunLoopMode, timeInterval, true);
#elif PLATFORM(EFL)
    ecore_main_loop_iterate();
#elif PLATFORM(GTK)
    g_main_context_iteration(NULL, FALSE);
#endif
}

} // namespace Inspector
