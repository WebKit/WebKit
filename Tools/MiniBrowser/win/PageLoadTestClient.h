/*
* Copyright (C) 2014 Apple Inc. All Rights Reserved.
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

#ifndef PageLoadTestClient_h
#define PageLoadTestClient_h

#include <CoreFoundation/CFDate.h>
#include <WebKitLegacy/WebKit.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

template <class TimerFiredClass>
class Timer {
    WTF_MAKE_NONCOPYABLE(Timer);
public:
    typedef void (TimerFiredClass::*TimerFiredFunction)(Timer*);

    Timer(TimerFiredClass* object, TimerFiredFunction function)
        : m_object(object)
        , m_function(function)
    {
        ASSERT_ARG(object, object);
        ASSERT_ARG(function, function);
    }

    ~Timer() { invalidate(); }

    void invalidate()
    {
        if (!isValid())
            return;

        CFRunLoopTimerInvalidate(m_timer.get());
        m_timer = nullptr;
    }

    bool isValid() const
    {
        return m_timer && CFRunLoopTimerIsValid(m_timer.get());
    }

    void schedule(CFTimeInterval interval, bool repeating)
    {
        ASSERT(!isValid());

        CFTimeInterval repeatInterval = repeating ? interval : 0;
        CFRunLoopTimerContext context = { 0, this, 0, 0, 0 };
        m_timer = adoptCF(CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + interval, repeatInterval, 0, 0, runLoopTimerFired, &context));

        CFRunLoopAddTimer(CFRunLoopGetCurrent(), m_timer.get(), kCFRunLoopCommonModes);
    }

private:
    void fire() { (m_object->*m_function)(this); }

    static void runLoopTimerFired(CFRunLoopTimerRef timerRef, void* info)
    {
        Timer* timer = static_cast<Timer*>(info);
        ASSERT_ARG(timerRef, timer->m_timer == timerRef);

        if (!CFRunLoopTimerDoesRepeat(timerRef))
            CFRunLoopTimerInvalidate(timerRef);

#ifdef __OBJC__
        @autoreleasepool
#endif
        {
            timer->fire();
        }
    }

    TimerFiredClass* m_object;
    TimerFiredFunction m_function;

    RetainPtr<CFRunLoopTimerRef> m_timer;
};

class WebKitLegacyBrowserWindow;

class PageLoadTestClient {
    WTF_MAKE_NONCOPYABLE(PageLoadTestClient);
public:
    PageLoadTestClient(WebKitLegacyBrowserWindow* host, bool pageLoadTesting = false);

#if OS(WINDOWS)
    void setPageURL(const _bstr_t&);
#endif

    void didStartProvisionalLoad(IWebFrame&);
    void didCommitLoad();
    void didFirstLayoutForMainFrame();
    void didHandleOnLoadEvents();
    void didFinishLoad();
    void didFailLoad();
    void didInitiateResourceLoad(uint64_t);
    void didEndResourceLoad(uint64_t);

private:
    void endPageLoad(Timer<PageLoadTestClient>*);
    void maybeEndPageLoadSoon();
    void clearPageLoadState();
    bool shouldConsiderPageLoadEnded() const;

    void pageLoadStartedAtTime(CFAbsoluteTime);
    void pageLoadEndedAtTime(CFAbsoluteTime);
    void dumpRunStatistics();

    WebKitLegacyBrowserWindow* m_host;
    CFAbsoluteTime m_pageLoadEndTime { 0 };
    CFTimeInterval m_totalTime { 0 };
    CFTimeInterval m_totalSquareRootsOfTime { 0 };
    CFTimeInterval m_longestTime { 0 };
    Vector<CFAbsoluteTime> m_startTimes;
    Vector<CFAbsoluteTime> m_endTimes;
    HashSet<uint64_t> m_loadingSubresources;
    Timer<PageLoadTestClient> m_waitForLoadToReallyEnd;
#if OS(WINDOWS)
    _bstr_t m_url;
#endif
    double m_geometricMeanProductSum { 1.0 };
    unsigned m_frames { 0 };
    unsigned m_onLoadEvents { 0 };
    unsigned m_currentRepetition { 0 };
    unsigned m_pagesTimed { 0 };
    unsigned m_repetitions;

    bool m_currentPageLoadFinished { false };
    bool m_pageLoadTesting;
};

#endif // PageLoadTestClient_h
