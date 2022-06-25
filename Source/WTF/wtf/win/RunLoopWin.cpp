/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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
#include <wtf/RunLoop.h>

#include <wtf/WindowsExtras.h>

namespace WTF {

static const UINT PerformWorkMessage = WM_USER + 1;
static const LPCWSTR kRunLoopMessageWindowClassName = L"RunLoopMessageWindow";

LRESULT CALLBACK RunLoop::RunLoopWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (RunLoop* runLoop = static_cast<RunLoop*>(getWindowPointer(hWnd, 0)))
        return runLoop->wndProc(hWnd, message, wParam, lParam);

    if (message == WM_CREATE) {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);

        // Associate the RunLoop with the window.
        setWindowPointer(hWnd, 0, createStruct->lpCreateParams);
        return 0;
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT RunLoop::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case PerformWorkMessage:
        performWork();
        return 0;
    case WM_TIMER:
        bitwise_cast<RunLoop::TimerBase*>(wParam)->timerFired();
        return 0;
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

void RunLoop::run()
{
    MSG message;
    while (BOOL result = ::GetMessage(&message, nullptr, 0, 0)) {
        if (result == -1)
            break;
        ::TranslateMessage(&message);
        ::DispatchMessage(&message);
    }
}

void RunLoop::iterate()
{
    MSG message;
    while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&message);
        ::DispatchMessage(&message);
    }
}

void RunLoop::setWakeUpCallback(WTF::Function<void()>&& function)
{
    RunLoop::current().m_wakeUpCallback = WTFMove(function);
}

void RunLoop::stop()
{
    // RunLoop::stop() can be called from threads unrelated to this RunLoop.
    // We should post a message that call PostQuitMessage in RunLoop's thread.
    dispatch([] {
        ::PostQuitMessage(0);
    });
}

void RunLoop::registerRunLoopMessageWindowClass()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        WNDCLASS windowClass = { };
        windowClass.lpfnWndProc     = RunLoop::RunLoopWndProc;
        windowClass.cbWndExtra      = sizeof(RunLoop*);
        windowClass.lpszClassName   = kRunLoopMessageWindowClassName;
        bool result = ::RegisterClass(&windowClass);
        RELEASE_ASSERT(result);
    });
}

RunLoop::RunLoop()
{
    registerRunLoopMessageWindowClass();
    m_runLoopMessageWindow = ::CreateWindow(kRunLoopMessageWindowClassName, nullptr, 0,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, nullptr, nullptr, this);
    RELEASE_ASSERT(::IsWindow(m_runLoopMessageWindow));
}

RunLoop::~RunLoop()
{
}

void RunLoop::wakeUp()
{
    // FIXME: No need to wake up the run loop if we've already called dispatch
    // before the run loop has had the time to respond.
    ::PostMessage(m_runLoopMessageWindow, PerformWorkMessage, reinterpret_cast<WPARAM>(this), 0);

    if (m_wakeUpCallback)
        m_wakeUpCallback();
}

RunLoop::CycleResult RunLoop::cycle(RunLoopMode)
{
    MSG message;
    if (!::GetMessage(&message, nullptr, 0, 0))
        return CycleResult::Stop;

    ::TranslateMessage(&message);
    ::DispatchMessage(&message);

    return CycleResult::Continue;
}

// RunLoop::Timer

void RunLoop::TimerBase::timerFired()
{
    {
        Locker locker { m_runLoop->m_loopLock };

        if (!m_isActive)
            return;

        if (!m_isRepeating) {
            m_isActive = false;
            ::KillTimer(m_runLoop->m_runLoopMessageWindow, bitwise_cast<uintptr_t>(this));
        } else
            m_nextFireDate = MonotonicTime::now() + m_interval;
    }

    fired();
}

RunLoop::TimerBase::TimerBase(RunLoop& runLoop)
    : m_runLoop(runLoop)
{
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

void RunLoop::TimerBase::start(Seconds interval, bool repeat)
{
    Locker locker { m_runLoop->m_loopLock };
    m_isRepeating = repeat;
    m_isActive = true;
    m_interval = interval;
    m_nextFireDate = MonotonicTime::now() + m_interval;
    ::SetTimer(m_runLoop->m_runLoopMessageWindow, bitwise_cast<uintptr_t>(this), interval.millisecondsAs<UINT>(), nullptr);
}

void RunLoop::TimerBase::stop()
{
    Locker locker { m_runLoop->m_loopLock };
    if (!isActiveWithLock())
        return;

    m_isActive = false;
    ::KillTimer(m_runLoop->m_runLoopMessageWindow, bitwise_cast<uintptr_t>(this));
}

bool RunLoop::TimerBase::isActiveWithLock() const
{
    return m_isActive;
}

bool RunLoop::TimerBase::isActive() const
{
    Locker locker { m_runLoop->m_loopLock };
    return isActiveWithLock();
}

Seconds RunLoop::TimerBase::secondsUntilFire() const
{
    Locker locker { m_runLoop->m_loopLock };
    if (isActiveWithLock())
        return std::max<Seconds>(m_nextFireDate - MonotonicTime::now(), 0_s);
    return 0_s;
}

} // namespace WTF
