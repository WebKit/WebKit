/*
 * Copyright (C) 2012 ProFUSION embedded systems. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics
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
#include "RunLoop.h"

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_File.h>
#include <Edje.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

static const int ecorePipeMessageSize = 1;
static const char wakupEcorePipeMessage[] = "W";

namespace WebCore {

RunLoop::RunLoop()
    : m_initEfl(false)
{
    if (!ecore_init()) {
        LOG_ERROR("could not init ecore.");
        return;
    }

    if (!ecore_evas_init()) {
        LOG_ERROR("could not init ecore_evas.");
        goto errorEcoreEvas;
    }

    if (!ecore_file_init()) {
        LOG_ERROR("could not init ecore_file.");
        goto errorEcoreFile;
    }

    if (!edje_init()) {
        LOG_ERROR("could not init edje.");
        goto errorEdje;
    }

    m_pipe = adoptPtr(ecore_pipe_add(wakeUpEvent, this));
    m_initEfl = true;

    return;

errorEdje:
    ecore_file_shutdown();
errorEcoreFile:
    ecore_evas_shutdown();
errorEcoreEvas:
    ecore_shutdown();
}

RunLoop::~RunLoop()
{
    if (m_initEfl) {
        edje_shutdown();
        ecore_file_shutdown();
        ecore_evas_shutdown();
        ecore_shutdown();
    }
}

void RunLoop::run()
{
    ecore_main_loop_begin();
}

void RunLoop::stop()
{
    ecore_main_loop_quit();
}

void RunLoop::wakeUpEvent(void* data, void*, unsigned int)
{
    static_cast<RunLoop*>(data)->performWork();
}

void RunLoop::wakeUp()
{
    ecore_pipe_write(m_pipe.get(), wakupEcorePipeMessage, ecorePipeMessageSize);
}

RunLoop::TimerBase::TimerBase(RunLoop*)
    : m_isRepeating(false)
{
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

bool RunLoop::TimerBase::timerFired(void* data)
{
    RunLoop::TimerBase* timer = static_cast<RunLoop::TimerBase*>(data);

    timer->fired();

    if (!timer->m_isRepeating) {
        timer->m_timer = nullptr;
        return ECORE_CALLBACK_CANCEL;
    }

    return ECORE_CALLBACK_RENEW;
}

void RunLoop::TimerBase::start(double nextFireInterval, bool repeat)
{
    m_isRepeating = repeat;
    m_timer = adoptPtr(ecore_timer_add(nextFireInterval, reinterpret_cast<Ecore_Task_Cb>(timerFired), this));
}

void RunLoop::TimerBase::stop()
{
    m_timer = nullptr;
}

bool RunLoop::TimerBase::isActive() const
{
    return (m_timer) ? true : false;
}

} // namespace WebCore
