/*
 * Copyright (C) 2008 Kenneth Rohde Christiansen
 *           (C) 2008 Afonso Rabelo Costa Jr.
 *           (C) 2009-2010 ProFUSION embedded systems
 *           (C) 2009-2010 Samsung Electronics
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

#include <Ecore.h>
#include <pthread.h>
#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

namespace WebCore {

static pthread_mutex_t timerMutex = PTHREAD_MUTEX_INITIALIZER;
static Ecore_Timer *_sharedTimer = 0;
static Ecore_Pipe *_pipe = 0;

static void (*_timerFunction)();

struct timerOp {
    double time;
    unsigned char op; // 0 - add a timer; 1 - del a timer;
};

void setSharedTimerFiredFunction(void (*func)())
{
    _timerFunction = func;
}

static Eina_Bool timerEvent(void*)
{
    if (_timerFunction)
        _timerFunction();

    _sharedTimer = 0;

    return ECORE_CALLBACK_CANCEL;
}

void processTimers(struct timerOp *tOp)
{
    if (_sharedTimer) {
        ecore_timer_del(_sharedTimer);
        _sharedTimer = 0;
    }

    if (tOp->op == 1)
        return;

    double interval = tOp->time - currentTime();

    if (interval <= ecore_animator_frametime_get()) {
        if (_timerFunction)
            _timerFunction();
        return;
    }

    _sharedTimer = ecore_timer_add(interval, timerEvent, 0);
}

void pipeHandlerCb(void *data, void *buffer, unsigned int nbyte)
{
    ASSERT(nbyte == sizeof(struct timerOp));

    struct timerOp *tOp = (struct timerOp *)buffer;
    processTimers(tOp);
}

void stopSharedTimer()
{
    struct timerOp tOp;
    pthread_mutex_lock(&timerMutex);
    if (!_pipe)
        _pipe = ecore_pipe_add(pipeHandlerCb, 0);
    pthread_mutex_unlock(&timerMutex);

    tOp.op = 1;
    ecore_pipe_write(_pipe, &tOp, sizeof(tOp));
}

void addNewTimer(double fireTime)
{
    struct timerOp tOp;
    pthread_mutex_lock(&timerMutex);
    if (!_pipe)
        _pipe = ecore_pipe_add(pipeHandlerCb, 0);
    pthread_mutex_unlock(&timerMutex);

    tOp.time = fireTime;
    tOp.op = 0;
    ecore_pipe_write(_pipe, &tOp, sizeof(tOp));
}

void setSharedTimerFireTime(double fireTime)
{
    addNewTimer(fireTime);
}

}

