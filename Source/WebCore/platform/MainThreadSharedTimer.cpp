/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2015 Igalia S.L.
 * Copyright (C) 2018 Sony Interactive Entertainment.
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
#include "MainThreadSharedTimer.h"

#include <wtf/NeverDestroyed.h>

#if USE(GLIB)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebCore {

MainThreadSharedTimer& MainThreadSharedTimer::singleton()
{
    static NeverDestroyed<MainThreadSharedTimer> instance;
    return instance;
}

#if USE(CF) || OS(WINDOWS)
MainThreadSharedTimer::MainThreadSharedTimer() = default;
#else
MainThreadSharedTimer::MainThreadSharedTimer()
    : m_timer(RunLoop::main(), this, &MainThreadSharedTimer::fired)
{
#if USE(GLIB)
    m_timer.setPriority(RunLoopSourcePriority::MainThreadSharedTimer);
    m_timer.setName("[WebKit] MainThreadSharedTimer");
#endif
}

void MainThreadSharedTimer::setFireInterval(Seconds interval)
{
    ASSERT(m_firedFunction);
    m_timer.startOneShot(interval);
}

void MainThreadSharedTimer::stop()
{
    m_timer.stop();
}

void MainThreadSharedTimer::invalidate()
{
}
#endif

void MainThreadSharedTimer::setFiredFunction(WTF::Function<void()>&& firedFunction)
{
    RELEASE_ASSERT(!m_firedFunction || !firedFunction);
    m_firedFunction = WTFMove(firedFunction);
}

void MainThreadSharedTimer::fired()
{
    ASSERT(m_firedFunction);
    m_firedFunction();
}

} // namespace WebCore
