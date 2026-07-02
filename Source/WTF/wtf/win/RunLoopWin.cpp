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
static const UINT SetTimerMessage = WM_USER + 2;
static const UINT FireTimerMessage = WM_USER + 3;
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
    case SetTimerMessage:
        return 0;
    case FireTimerMessage:
        RunLoop::TimerBase* timer = nullptr;
        timer = std::bit_cast<RunLoop::TimerBase*>(wParam);
        if (timer != nullptr)
            timer->timerFired();
        return 0;
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

void RunLoop::run()
{
    auto& runLoop = RunLoop::currentSingleton();
    while (true) {
        runLoop.fireTimers();
        MSG message;
        while (BOOL result = ::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (result == -1)
                break;
            if (message.message == WM_QUIT)
                return;
            runLoop.dispatchMessage(message);
        }

        DWORD timeout = runLoop.msTillNextTimer();
        if (timeout > 0)
            ::MsgWaitForMultipleObjectsEx(0, nullptr, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}

DWORD RunLoop::msTillNextTimer()
{
    Locker locker { m_loopLock };
    Seconds timeout = Seconds(3600);

    if (!m_timers.isEmpty()) {
        auto now = MonotonicTime::now();
        auto firstTimer = m_timers.last();

        timeout = std::max<Seconds>(firstTimer->m_nextFireDate - now, 0_s);
    }
    return timeout.milliseconds();
}

void RunLoop::fireTimers()
{
    Locker locker { m_loopLock };

    // Can bail here if there's no timers before we check the time
    if (m_timers.isEmpty())
        return;

    // Fire any timers ready from the front of the queue
    auto now = MonotonicTime::now();

    while (!m_timers.isEmpty()) {
        auto timer = m_timers.last();
        if (timer->m_nextFireDate > now)
            return;
        ::PostMessage(m_runLoopMessageWindow, FireTimerMessage, std::bit_cast<uintptr_t>(timer), 0LL);
        m_timers.removeLast();
    }
}

void RunLoop::setWakeUpCallback(WTF::Function<void()>&& function)
{
    RunLoop::currentSingleton().m_wakeUpCallback = WTF::move(function);
}

void RunLoop::setWindowsMessageHandler(WindowsMessageHandler&& handler)
{
    RunLoop::currentSingleton().m_windowsMessageHandler = WTF::move(handler);
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
    WNDCLASS windowClass = { };
    windowClass.lpfnWndProc = RunLoop::RunLoopWndProc;
    windowClass.cbWndExtra = sizeof(RunLoop*);
    windowClass.lpszClassName = kRunLoopMessageWindowClassName;
    bool result = ::RegisterClass(&windowClass);
    RELEASE_ASSERT(result);
}

RunLoop::RunLoop()
{
    m_runLoopMessageWindow = ::CreateWindow(kRunLoopMessageWindowClassName, nullptr, 0,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, nullptr, nullptr, this);
    RELEASE_ASSERT(::IsWindow(m_runLoopMessageWindow));
}

RunLoop::~RunLoop()
{
    ::DestroyWindow(m_runLoopMessageWindow);
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
    auto& runLoop = RunLoop::currentSingleton();
    runLoop.fireTimers();
    MSG message;
    while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT)
            return CycleResult::Stop;

        runLoop.dispatchMessage(message);
    }
    return CycleResult::Continue;
}

void RunLoop::dispatchMessage(MSG& message)
{
    if (m_windowsMessageHandler && m_windowsMessageHandler(message))
        return;

    ::TranslateMessage(&message);
    ::DispatchMessage(&message);
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
            m_nextFireDate = MonotonicTime::infinity();
        } else {
            m_nextFireDate = MonotonicTime::timePointFromNow(m_interval);
            m_runLoop->m_timers.appendAndBubble(this, [&] (TimerBase* otherTimer) -> bool {
                return m_nextFireDate > otherTimer->m_nextFireDate;
            });
        }
    }

    fired();
}

RunLoop::TimerBase::TimerBase(Ref<RunLoop>&& runLoop, ASCIILiteral description)
    : m_runLoop(WTF::move(runLoop))
    , m_description(description)
{
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

void RunLoop::TimerBase::start(Seconds interval, bool repeat)
{
    Locker locker { m_runLoop->m_loopLock };
    if (isActiveWithLock()) {
        // Rescheduling timer that's already started
        m_runLoop->m_timers.removeFirstMatching([&] (TimerBase* t) -> bool {
            return this == t;
        });
    }

    m_isRepeating = repeat;
    m_isActive = true;
    m_interval = interval;
    m_nextFireDate = MonotonicTime::timePointFromNow(m_interval);
    m_runLoop->m_timers.appendAndBubble(this, [&] (TimerBase* otherTimer) -> bool {
        return m_nextFireDate > otherTimer->m_nextFireDate;
    });
    // If this is the first timer now, we need to cycle the run loop so we don't sleep through it
    if (m_runLoop->m_timers.last() == this)
        ::PostMessage(m_runLoop->m_runLoopMessageWindow, SetTimerMessage, std::bit_cast<uintptr_t>(this), interval.millisecondsAs<UINT>());
}

void RunLoop::TimerBase::stop()
{
    Locker locker { m_runLoop->m_loopLock };
    if (!isActiveWithLock())
        return;

    m_isActive = false;
    m_nextFireDate = MonotonicTime::infinity();

    m_runLoop->m_timers.removeFirstMatching([&] (TimerBase* t) -> bool {
        return this == t;
    });
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
