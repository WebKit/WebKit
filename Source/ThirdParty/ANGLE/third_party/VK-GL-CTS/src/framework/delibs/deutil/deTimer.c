/*-------------------------------------------------------------------------
 * drawElements Utility Library
 * ----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Periodic timer.
 *//*--------------------------------------------------------------------*/

#include "deTimer.h"
#include "deMemory.h"
#include "deThread.h"

#if (DE_OS == DE_OS_WIN32)

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct deTimer_s
{
    deTimerCallback callback;
    void *callbackArg;

    HANDLE timer;
};

static void CALLBACK timerCallback(PVOID lpParameter, BOOLEAN timerOrWaitFired)
{
    const deTimer *timer = (const deTimer *)lpParameter;
    DE_UNREF(timerOrWaitFired);

    timer->callback(timer->callbackArg);
}

deTimer *deTimer_create(deTimerCallback callback, void *arg)
{
    deTimer *timer = (deTimer *)deCalloc(sizeof(deTimer));

    if (!timer)
        return NULL;

    timer->callback    = callback;
    timer->callbackArg = arg;
    timer->timer       = 0;

    return timer;
}

void deTimer_destroy(deTimer *timer)
{
    DE_ASSERT(timer);

    if (deTimer_isActive(timer))
        deTimer_disable(timer);

    deFree(timer);
}

bool deTimer_isActive(const deTimer *timer)
{
    return timer->timer != 0;
}

bool deTimer_scheduleSingle(deTimer *timer, int milliseconds)
{
    BOOL ret;

    DE_ASSERT(timer && milliseconds > 0);

    if (deTimer_isActive(timer))
        return false;

    ret = CreateTimerQueueTimer(&timer->timer, NULL, timerCallback, timer, (DWORD)milliseconds, 0, WT_EXECUTEDEFAULT);

    if (!ret)
    {
        DE_ASSERT(!timer->timer);
        return false;
    }

    return true;
}

bool deTimer_scheduleInterval(deTimer *timer, int milliseconds)
{
    BOOL ret;

    DE_ASSERT(timer && milliseconds > 0);

    if (deTimer_isActive(timer))
        return false;

    ret = CreateTimerQueueTimer(&timer->timer, NULL, timerCallback, timer, (DWORD)milliseconds, (DWORD)milliseconds,
                                WT_EXECUTEDEFAULT);

    if (!ret)
    {
        DE_ASSERT(!timer->timer);
        return false;
    }

    return true;
}

void deTimer_disable(deTimer *timer)
{
    if (timer->timer)
    {
        const int maxTries = 100;
        HANDLE waitEvent   = CreateEvent(NULL, FALSE, FALSE, NULL);
        int tryNdx         = 0;
        DE_ASSERT(waitEvent);

        for (tryNdx = 0; tryNdx < maxTries; tryNdx++)
        {
            BOOL success = DeleteTimerQueueTimer(NULL, timer->timer, waitEvent);
            if (success)
            {
                /* Wait for all callbacks to complete. */
                DWORD res = WaitForSingleObject(waitEvent, INFINITE);
                DE_ASSERT(res == WAIT_OBJECT_0);
                DE_UNREF(res);
                break;
            }
            else
            {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING)
                    break; /* \todo [2013-03-21 pyry] Does this mean that callback is still in progress? */
                deYield();
            }
        }

        DE_ASSERT(tryNdx < maxTries);

        CloseHandle(waitEvent);
        timer->timer = 0;
    }
}

#elif (DE_OS == DE_OS_UNIX || DE_OS == DE_OS_ANDROID || DE_OS == DE_OS_SYMBIAN || DE_OS == DE_OS_QNX)

#include <signal.h>
#include <time.h>

struct deTimer_s
{
    deTimerCallback callback;
    void *callbackArg;

    timer_t timer;

    bool isActive;
};

static void timerCallback(union sigval val)
{
    const deTimer *timer = (const deTimer *)val.sival_ptr;
    timer->callback(timer->callbackArg);
}

deTimer *deTimer_create(deTimerCallback callback, void *arg)
{
    deTimer *timer = (deTimer *)deCalloc(sizeof(deTimer));
    struct sigevent sevp;

    if (!timer)
        return NULL;

    deMemset(&sevp, 0, sizeof(sevp));
    sevp.sigev_notify          = SIGEV_THREAD;
    sevp.sigev_value.sival_ptr = timer;
    sevp.sigev_notify_function = timerCallback;

    if (timer_create(CLOCK_REALTIME, &sevp, &timer->timer) != 0)
    {
        deFree(timer);
        return NULL;
    }

    timer->callback    = callback;
    timer->callbackArg = arg;
    timer->isActive    = false;

    return timer;
}

void deTimer_destroy(deTimer *timer)
{
    DE_ASSERT(timer);

    timer_delete(timer->timer);
    deFree(timer);
}

bool deTimer_isActive(const deTimer *timer)
{
    return timer->isActive;
}

bool deTimer_scheduleSingle(deTimer *timer, int milliseconds)
{
    struct itimerspec tspec;

    DE_ASSERT(timer && milliseconds > 0);

    if (timer->isActive)
        return false;

    tspec.it_value.tv_sec     = milliseconds / 1000;
    tspec.it_value.tv_nsec    = (milliseconds % 1000) * 1000;
    tspec.it_interval.tv_sec  = 0;
    tspec.it_interval.tv_nsec = 0;

    if (timer_settime(timer->timer, 0, &tspec, NULL) != 0)
        return false;

    timer->isActive = true;
    return true;
}

bool deTimer_scheduleInterval(deTimer *timer, int milliseconds)
{
    struct itimerspec tspec;

    DE_ASSERT(timer && milliseconds > 0);

    if (timer->isActive)
        return false;

    tspec.it_value.tv_sec     = milliseconds / 1000;
    tspec.it_value.tv_nsec    = (milliseconds % 1000) * 1000;
    tspec.it_interval.tv_sec  = tspec.it_value.tv_sec;
    tspec.it_interval.tv_nsec = tspec.it_value.tv_nsec;

    if (timer_settime(timer->timer, 0, &tspec, NULL) != 0)
        return false;

    timer->isActive = true;
    return true;
}

void deTimer_disable(deTimer *timer)
{
    struct itimerspec tspec;

    DE_ASSERT(timer);

    tspec.it_value.tv_sec     = 0;
    tspec.it_value.tv_nsec    = 0;
    tspec.it_interval.tv_sec  = 0;
    tspec.it_interval.tv_nsec = 0;

    timer_settime(timer->timer, 0, &tspec, NULL);

    /* \todo [2012-07-10 pyry] How to wait until all pending callbacks have finished? */

    timer->isActive = false;
}

#else

/* Generic thread-based implementation for OSes that lack proper timers. */

#include "deThread.h"
#include "deMutex.h"
#include "deClock.h"

typedef enum TimerState_e
{
    TIMERSTATE_INTERVAL = 0, /*!< Active interval timer.        */
    TIMERSTATE_SINGLE,       /*!< Single callback timer.        */
    TIMERSTATE_DISABLED,     /*!< Disabled timer.            */

    TIMERSTATE_LAST
} TimerState;

typedef struct deTimerThread_s
{
    deTimerCallback callback; /*!< Callback function.        */
    void *callbackArg;        /*!< User pointer.            */

    deThread thread; /*!< Thread.                */
    int interval;    /*!< Timer interval.        */

    deMutex lock;              /*!< State lock.            */
    volatile TimerState state; /*!< Timer state.            */
} deTimerThread;

struct deTimer_s
{
    deTimerCallback callback; /*!< Callback function.        */
    void *callbackArg;        /*!< User pointer.            */
    deTimerThread *curThread; /*!< Current timer thread.    */
};

static void timerThread(void *arg)
{
    deTimerThread *thread = (deTimerThread *)arg;
    int numCallbacks      = 0;
    bool destroy          = true;
    int64_t lastCallback  = (int64_t)deGetMicroseconds();

    for (;;)
    {
        int sleepTime = 0;

        deMutex_lock(thread->lock);

        if (thread->state == TIMERSTATE_SINGLE && numCallbacks > 0)
        {
            destroy       = false; /* Will be destroyed by deTimer_disable(). */
            thread->state = TIMERSTATE_DISABLED;
            break;
        }
        else if (thread->state == TIMERSTATE_DISABLED)
            break;

        deMutex_unlock(thread->lock);

        sleepTime = thread->interval - (int)(((int64_t)deGetMicroseconds() - lastCallback) / 1000);
        if (sleepTime > 0)
            deSleep(sleepTime);

        lastCallback = (int64_t)deGetMicroseconds();
        thread->callback(thread->callbackArg);
        numCallbacks += 1;
    }

    /* State lock is held when loop is exited. */
    deMutex_unlock(thread->lock);

    if (destroy)
    {
        /* Destroy thread except thread->thread. */
        deMutex_destroy(thread->lock);
        deFree(thread);
    }
}

static deTimerThread *deTimerThread_create(deTimerCallback callback, void *arg, int interval, TimerState state)
{
    deTimerThread *thread = (deTimerThread *)deCalloc(sizeof(deTimerThread));

    DE_ASSERT(state == TIMERSTATE_INTERVAL || state == TIMERSTATE_SINGLE);

    if (!thread)
        return NULL;

    thread->callback    = callback;
    thread->callbackArg = arg;
    thread->interval    = interval;
    thread->lock        = deMutex_create(NULL);
    thread->state       = state;

    thread->thread = deThread_create(timerThread, thread, NULL);
    if (!thread->thread)
    {
        deMutex_destroy(thread->lock);
        deFree(thread);
        return NULL;
    }

    return thread;
}

deTimer *deTimer_create(deTimerCallback callback, void *arg)
{
    deTimer *timer = (deTimer *)deCalloc(sizeof(deTimer));

    if (!timer)
        return NULL;

    timer->callback    = callback;
    timer->callbackArg = arg;

    return timer;
}

void deTimer_destroy(deTimer *timer)
{
    if (timer->curThread)
        deTimer_disable(timer);
    deFree(timer);
}

bool deTimer_isActive(const deTimer *timer)
{
    if (timer->curThread)
    {
        bool isActive = false;

        deMutex_lock(timer->curThread->lock);
        isActive = timer->curThread->state != TIMERSTATE_LAST;
        deMutex_unlock(timer->curThread->lock);

        return isActive;
    }
    else
        return false;
}

bool deTimer_scheduleSingle(deTimer *timer, int milliseconds)
{
    if (timer->curThread)
        deTimer_disable(timer);

    DE_ASSERT(!timer->curThread);
    timer->curThread = deTimerThread_create(timer->callback, timer->callbackArg, milliseconds, TIMERSTATE_SINGLE);

    return timer->curThread != NULL;
}

bool deTimer_scheduleInterval(deTimer *timer, int milliseconds)
{
    if (timer->curThread)
        deTimer_disable(timer);

    DE_ASSERT(!timer->curThread);
    timer->curThread = deTimerThread_create(timer->callback, timer->callbackArg, milliseconds, TIMERSTATE_INTERVAL);

    return timer->curThread != NULL;
}

void deTimer_disable(deTimer *timer)
{
    if (!timer->curThread)
        return;

    deMutex_lock(timer->curThread->lock);

    if (timer->curThread->state != TIMERSTATE_DISABLED)
    {
        /* Just set state to disabled and destroy thread handle. */
        /* \note Assumes that deThread_destroy() can be called while thread is still running
         *       and it will not terminate the thread.
         */
        timer->curThread->state = TIMERSTATE_DISABLED;
        deThread_destroy(timer->curThread->thread);
        timer->curThread->thread = 0;
        deMutex_unlock(timer->curThread->lock);

        /* Thread will destroy timer->curThread. */
    }
    else
    {
        /* Single timer has expired - we must destroy whole thread structure. */
        deMutex_unlock(timer->curThread->lock);
        deThread_destroy(timer->curThread->thread);
        deMutex_destroy(timer->curThread->lock);
        deFree(timer->curThread);
    }

    timer->curThread = NULL;
}

#endif
