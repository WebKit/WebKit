/*
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "SharedTimer.h"

#include <Looper.h>
#include <MessageFilter.h>
#include <MessageRunner.h>
#include <support/Locker.h>
#include <support/Autolock.h>
#include <stdio.h>

#define FIRE_MESSAGE 'fire'


namespace WebCore {

class TimerThread : public BLocker {
public:
	TimerThread(const BMessenger& timer)
		: m_timer(timer)
		, m_timerThread(B_BAD_THREAD_ID)
		, m_timerSem(B_BAD_SEM_ID)
		, m_nextFireTime(0)
		, m_threadWaitUntil(0)
		, m_terminating(false)
	{
		m_timerSem = create_sem(0, "timer thread control");
		if (m_timerSem >= 0) {
			m_timerThread = spawn_thread(timerThreadEntry, "timer thread",
				B_URGENT_DISPLAY_PRIORITY, this);
			if (m_timerThread >= 0)
				resume_thread(m_timerThread);
		}
	}

	~TimerThread()
	{
		m_terminating = true;
		if (delete_sem(m_timerSem) == B_OK) {
			int32 dummy;
			wait_for_thread(m_timerThread, &dummy);
		}
	}

	bool isValid() const
	{
		return m_timerThread >= 0 && m_timerSem >= 0;
	}

	void setNextEventTime(bigtime_t time)
	{
		Lock();
		m_nextFireTime = time;
		if (m_nextFireTime < m_threadWaitUntil)
			release_sem(m_timerSem);
		Unlock();
	}

private:
	static int32 timerThreadEntry(void *data)
	{
		return ((TimerThread*)data)->timerThread();
	}

	int32 timerThread()
	{
		bool running = true;
		while (running) {
			bigtime_t waitUntil = B_INFINITE_TIMEOUT;
			if (Lock()) {
				if (m_nextFireTime > 0)
				    waitUntil = m_nextFireTime;
				m_threadWaitUntil = waitUntil;
				Unlock();
			}
			status_t err = acquire_sem_etc(m_timerSem, 1, B_ABSOLUTE_TIMEOUT, waitUntil);
			switch (err) {
				case B_TIMED_OUT:
					// do events, that are supposed to go off
					if (!m_terminating && Lock() && system_time() >= m_nextFireTime) {
						bool sendMessage = m_nextFireTime > 0;
						m_nextFireTime = 0;
						Unlock();
						if (sendMessage)
                            m_timer.SendMessage(FIRE_MESSAGE);
					}
					if (IsLocked())
						Unlock();
					break;
				case B_BAD_SEM_ID:
					running = false;
					break;
				case B_OK:
				default:
					break;
			}
		}
		return 0;
	}

private:
	BMessenger m_timer;
	thread_id m_timerThread;
	sem_id m_timerSem;
	volatile bigtime_t m_nextFireTime;
	volatile bigtime_t m_threadWaitUntil;
	volatile bool m_terminating;
};

class SharedTimerHaiku : public BHandler {
    friend void setSharedTimerFiredFunction(void (*f)());
public:
    static SharedTimerHaiku* instance();

    void start(double);
    void stop();

	void setTimerThread(TimerThread* thread)
	{
		m_timerThread = thread;
	}

protected:
    virtual void MessageReceived(BMessage*);

private:
    SharedTimerHaiku();
    ~SharedTimerHaiku();

    void (*m_timerFunction)();
    bool m_shouldRun;
    TimerThread* m_timerThread;
};

SharedTimerHaiku::SharedTimerHaiku()
    : BHandler("WebKit shared timer")
    , m_timerFunction(0)
    , m_shouldRun(false)
    , m_timerThread(0)
{
}

SharedTimerHaiku::~SharedTimerHaiku()
{
	delete m_timerThread;
}

SharedTimerHaiku* SharedTimerHaiku::instance()
{
    static SharedTimerHaiku* timer;

    if (!timer) {
        BLooper* looper = BLooper::LooperForThread(find_thread(0));
        BAutolock lock(looper);
        timer = new SharedTimerHaiku();
        looper->AddHandler(timer);
        // Only at this time, the timer can be a valid BMessenger.
        timer->setTimerThread(new TimerThread(BMessenger(timer)));
    }

    return timer;
}

void SharedTimerHaiku::start(double interval)
{
    m_shouldRun = true;
    m_timerThread->setNextEventTime(system_time() + (bigtime_t)(interval * 1000000));
}

void SharedTimerHaiku::stop()
{
    m_shouldRun = false;
// FIXME: This breaks scrolling. Why?
//    m_timerThread->setNextEventTime(0);
}

void SharedTimerHaiku::MessageReceived(BMessage*)
{
    if (m_shouldRun && m_timerFunction)
        m_timerFunction();
}

// WebCore functions
void setSharedTimerFiredFunction(void (*f)())
{
    SharedTimerHaiku::instance()->m_timerFunction = f;
}

void setSharedTimerFireInterval(double interval)
{
    SharedTimerHaiku::instance()->start(interval);
}

void stopSharedTimer()
{
    SharedTimerHaiku::instance()->stop();
}

void invalidateSharedTimer()
{
}

} // namespace WebCore
