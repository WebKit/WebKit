/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "GThreadSafeMainLoopSource.h"

#if USE(GLIB)

#include <gio/gio.h>
#include <wtf/gobject/GMutexLocker.h>

namespace WTF {

GThreadSafeMainLoopSource::GThreadSafeMainLoopSource()
{
    g_rec_mutex_init(&m_mutex);
}

GThreadSafeMainLoopSource::~GThreadSafeMainLoopSource()
{
    cancel();
    g_rec_mutex_clear(&m_mutex);
}

void GThreadSafeMainLoopSource::cancel()
{
    GMutexLocker<GRecMutex> locker(m_mutex);

    // The general cancellable object should only be present if we're currently dispatching this GMainLoopSource.
    ASSERT(!m_cancellable || m_status == Dispatching);

    // The source is perhaps being cancelled in the middle of a callback dispatch.
    // Cancelling this GCancellable object will convey this information to the
    // current execution context when the callback dispatch is finished.
    g_cancellable_cancel(m_cancellable.get());
    m_cancellable = nullptr;

    GMainLoopSource::cancel();
}

void GThreadSafeMainLoopSource::schedule(const char* name, std::function<void ()> function, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    GMainLoopSource::schedule(name, function, priority, destroyFunction, context);
    m_context.cancellable = adoptGRef(g_cancellable_new());
}

void GThreadSafeMainLoopSource::schedule(const char* name, std::function<bool ()> function, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    GMainLoopSource::schedule(name, function, priority, destroyFunction, context);
    m_context.cancellable = adoptGRef(g_cancellable_new());
}

void GThreadSafeMainLoopSource::scheduleAfterDelay(const char* name, std::function<void ()> function, std::chrono::milliseconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    GMainLoopSource::scheduleAfterDelay(name, function, delay, priority, destroyFunction, context);
    m_context.cancellable = adoptGRef(g_cancellable_new());
}

void GThreadSafeMainLoopSource::scheduleAfterDelay(const char* name, std::function<bool ()> function, std::chrono::milliseconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    GMainLoopSource::scheduleAfterDelay(name, function, delay, priority, destroyFunction, context);
    m_context.cancellable = adoptGRef(g_cancellable_new());
}

void GThreadSafeMainLoopSource::scheduleAfterDelay(const char* name, std::function<void ()> function, std::chrono::seconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    GMainLoopSource::scheduleAfterDelay(name, function, delay, priority, destroyFunction, context);
    m_context.cancellable = adoptGRef(g_cancellable_new());
}

void GThreadSafeMainLoopSource::scheduleAfterDelay(const char* name, std::function<bool ()> function, std::chrono::seconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    GMainLoopSource::scheduleAfterDelay(name, function, delay, priority, destroyFunction, context);
    m_context.cancellable = adoptGRef(g_cancellable_new());
}

bool GThreadSafeMainLoopSource::prepareVoidCallback(Context& context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    bool retval = GMainLoopSource::prepareVoidCallback(context);
    m_cancellable = context.cancellable;
    return retval;
}

void GThreadSafeMainLoopSource::finishVoidCallback()
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    GMainLoopSource::finishVoidCallback();
    m_cancellable = nullptr;
}

void GThreadSafeMainLoopSource::voidCallback()
{
    Context context;
    if (!prepareVoidCallback(context))
        return;

    context.voidCallback();

    if (g_cancellable_is_cancelled(context.cancellable.get())) {
        context.destroySource();
        return;
    }

    finishVoidCallback();
    context.destroySource();
}

bool GThreadSafeMainLoopSource::prepareBoolCallback(Context& context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    bool retval = GMainLoopSource::prepareBoolCallback(context);
    m_cancellable = context.cancellable;
    return retval;
}

void GThreadSafeMainLoopSource::finishBoolCallback(bool retval, Context& context)
{
    GMutexLocker<GRecMutex> locker(m_mutex);
    GMainLoopSource::finishBoolCallback(retval, context);
    m_cancellable = nullptr;
}

bool GThreadSafeMainLoopSource::boolCallback()
{
    Context context;
    if (!prepareBoolCallback(context))
        return Stop;

    bool retval = context.boolCallback();

    if (g_cancellable_is_cancelled(context.cancellable.get())) {
        context.destroySource();
        return Stop;
    }

    finishBoolCallback(retval, context);
    if (context.source)
        context.destroySource();
    return retval;
}

} // namespace WTF

#endif // USE(GLIB)
