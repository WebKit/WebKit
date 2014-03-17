/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#ifndef RunLoop_h
#define RunLoop_h

#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/Functional.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/Threading.h>

#if USE(GLIB)
#include <wtf/gobject/GRefPtr.h>
#endif

#if PLATFORM(EFL)
#include <Ecore.h>
#endif

namespace WTF {

class RunLoop : public FunctionDispatcher {
    WTF_MAKE_NONCOPYABLE(RunLoop);
public:
    // Must be called from the main thread (except for the Mac platform, where it
    // can be called from any thread).
    WTF_EXPORT_PRIVATE static void initializeMainRunLoop();

    WTF_EXPORT_PRIVATE static RunLoop& current();
    WTF_EXPORT_PRIVATE static RunLoop& main();
    WTF_EXPORT_PRIVATE static bool isMain();
    ~RunLoop();

    virtual void dispatch(std::function<void()>) override;

    WTF_EXPORT_PRIVATE static void run();
    WTF_EXPORT_PRIVATE void stop();
    WTF_EXPORT_PRIVATE void wakeUp();

#if PLATFORM(COCOA)
    WTF_EXPORT_PRIVATE void runForDuration(double duration);
#endif
    
    class TimerBase {
        friend class RunLoop;
    public:
        WTF_EXPORT_PRIVATE explicit TimerBase(RunLoop&);
        WTF_EXPORT_PRIVATE virtual ~TimerBase();

        void startRepeating(double repeatInterval) { start(repeatInterval, true); }
        void startOneShot(double interval) { start(interval, false); }

        WTF_EXPORT_PRIVATE void stop();
        WTF_EXPORT_PRIVATE bool isActive() const;

        virtual void fired() = 0;

    private:
        WTF_EXPORT_PRIVATE void start(double nextFireInterval, bool repeat);

        RunLoop& m_runLoop;

#if PLATFORM(WIN)
        static void timerFired(RunLoop*, uint64_t ID);
        uint64_t m_ID;
        bool m_isRepeating;
#elif PLATFORM(COCOA)
        static void timerFired(CFRunLoopTimerRef, void*);
        RetainPtr<CFRunLoopTimerRef> m_timer;
#elif PLATFORM(EFL)
        static bool timerFired(void* data);
        Ecore_Timer* m_timer;
        bool m_isRepeating;
#elif USE(GLIB)
        static gboolean timerFiredCallback(RunLoop::TimerBase*);
        gboolean isRepeating() const { return m_isRepeating; }
        void clearTimerSource();
        GRefPtr<GSource> m_timerSource;
        gboolean m_isRepeating;
#endif
    };

    template <typename TimerFiredClass>
    class Timer : public TimerBase {
    public:
        typedef void (TimerFiredClass::*TimerFiredFunction)();

        Timer(RunLoop& runLoop, TimerFiredClass* o, TimerFiredFunction f)
            : TimerBase(runLoop)
            , m_object(o)
            , m_function(f)
        {
        }

    private:
        virtual void fired() { (m_object->*m_function)(); }

        TimerFiredClass* m_object;
        TimerFiredFunction m_function;
    };

    class Holder;

private:
    RunLoop();

    void performWork();

    Mutex m_functionQueueLock;
    Deque<std::function<void ()>> m_functionQueue;

#if PLATFORM(WIN)
    static bool registerRunLoopMessageWindowClass();
    static LRESULT CALLBACK RunLoopWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    HWND m_runLoopMessageWindow;

    typedef HashMap<uint64_t, TimerBase*> TimerMap;
    TimerMap m_activeTimers;
#elif PLATFORM(COCOA)
    static void performWork(void*);
    RetainPtr<CFRunLoopRef> m_runLoop;
    RetainPtr<CFRunLoopSourceRef> m_runLoopSource;
    int m_nestingLevel;
#elif PLATFORM(EFL)
    Mutex m_pipeLock;
    OwnPtr<Ecore_Pipe> m_pipe;

    Mutex m_wakeUpEventRequestedLock;
    bool m_wakeUpEventRequested;

    static void wakeUpEvent(void* data, void*, unsigned);
#elif USE(GLIB)
public:
    static gboolean queueWork(RunLoop*);
    GMainLoop* innermostLoop();
    void pushNestedMainLoop(GMainLoop*);
    void popNestedMainLoop();
private:
    GRefPtr<GMainContext> m_runLoopContext;
    Vector<GRefPtr<GMainLoop>> m_runLoopMainLoops;
#endif
};

} // namespace WTF

using WTF::RunLoop;

#endif // RunLoop_h
