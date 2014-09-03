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

#if USE(GLIB)

#include "GMainLoopSource.h"
#include <gio/gio.h>
#include <wtf/gobject/GMutexLocker.h>

namespace WTF {

GMainLoopSource& GMainLoopSource::createAndDeleteOnDestroy()
{
    return *new GMainLoopSource(DeleteOnDestroy);
}

GMainLoopSource::GMainLoopSource()
    : m_deleteOnDestroy(DoNotDeleteOnDestroy)
    , m_status(Ready)
{
    g_mutex_init(&m_mutex);
}

GMainLoopSource::GMainLoopSource(DeleteOnDestroyType deleteOnDestroy)
    : m_deleteOnDestroy(deleteOnDestroy)
    , m_status(Ready)
{
    g_mutex_init(&m_mutex);
}

GMainLoopSource::~GMainLoopSource()
{
    cancel();
    g_mutex_clear(&m_mutex);
}

bool GMainLoopSource::isScheduled() const
{
    return m_status == Scheduled;
}

bool GMainLoopSource::isActive() const
{
    return m_status != Ready;
}

void GMainLoopSource::cancel()
{
    GMutexLocker locker(m_mutex);
    cancelWithoutLocking();
}

void GMainLoopSource::cancelWithoutLocking()
{
    if (!m_context.source) {
        m_status = Ready;
        return;
    }

    Context context = WTF::move(m_context);

    if (context.cancellable)
        g_cancellable_cancel(context.cancellable.get());

    g_source_destroy(context.source.get());
    destroy(context.destroyCallback);
}

void GMainLoopSource::scheduleIdleSource(const char* name, GSourceFunc sourceFunction, int priority, GMainContext* context)
{
    ASSERT(m_status == Ready);
    m_status = Scheduled;

    m_context.source = adoptGRef(g_idle_source_new());
    g_source_set_name(m_context.source.get(), name);
    if (priority != G_PRIORITY_DEFAULT_IDLE)
        g_source_set_priority(m_context.source.get(), priority);
    g_source_set_callback(m_context.source.get(), sourceFunction, this, nullptr);
    g_source_attach(m_context.source.get(), context);
}

void GMainLoopSource::schedule(const char* name, std::function<void ()> function, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker locker(m_mutex);
    cancelWithoutLocking();
    m_context.voidCallback = WTF::move(function);
    m_context.destroyCallback = WTF::move(destroyFunction);
    scheduleIdleSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::schedule(const char* name, std::function<bool ()> function, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker locker(m_mutex);
    cancelWithoutLocking();
    m_context.boolCallback = WTF::move(function);
    m_context.destroyCallback = WTF::move(destroyFunction);
    scheduleIdleSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::schedule(const char* name, std::function<bool (GIOCondition)> function, GSocket* socket, GIOCondition condition, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker locker(m_mutex);
    cancelWithoutLocking();
    ASSERT(m_status == Ready);
    m_status = Scheduled;

    m_context.socketCallback = WTF::move(function);
    m_context.destroyCallback = WTF::move(destroyFunction);
    m_context.cancellable = adoptGRef(g_cancellable_new());
    m_context.source = adoptGRef(g_socket_create_source(socket, condition, m_context.cancellable.get()));
    g_source_set_name(m_context.source.get(), name);
    g_source_set_callback(m_context.source.get(), reinterpret_cast<GSourceFunc>(socketSourceCallback), this, nullptr);
    g_source_attach(m_context.source.get(), context);
}

void GMainLoopSource::scheduleTimeoutSource(const char* name, GSourceFunc sourceFunction, int priority, GMainContext* context)
{
    ASSERT(m_status == Ready);
    m_status = Scheduled;

    ASSERT(m_context.source);
    g_source_set_name(m_context.source.get(), name);
    if (priority != G_PRIORITY_DEFAULT)
        g_source_set_priority(m_context.source.get(), priority);
    g_source_set_callback(m_context.source.get(), sourceFunction, this, nullptr);
    g_source_attach(m_context.source.get(), context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<void ()> function, std::chrono::milliseconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker locker(m_mutex);
    cancelWithoutLocking();
    m_context.source = adoptGRef(g_timeout_source_new(delay.count()));
    m_context.voidCallback = WTF::move(function);
    m_context.destroyCallback = WTF::move(destroyFunction);
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<bool ()> function, std::chrono::milliseconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker locker(m_mutex);
    cancelWithoutLocking();
    m_context.source = adoptGRef(g_timeout_source_new(delay.count()));
    m_context.boolCallback = WTF::move(function);
    m_context.destroyCallback = WTF::move(destroyFunction);
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<void ()> function, std::chrono::seconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker locker(m_mutex);
    cancelWithoutLocking();
    m_context.source = adoptGRef(g_timeout_source_new_seconds(delay.count()));
    m_context.voidCallback = WTF::move(function);
    m_context.destroyCallback = WTF::move(destroyFunction);
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<bool ()> function, std::chrono::seconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    GMutexLocker locker(m_mutex);
    cancelWithoutLocking();
    m_context.source = adoptGRef(g_timeout_source_new_seconds(delay.count()));
    m_context.boolCallback = WTF::move(function);
    m_context.destroyCallback = WTF::move(destroyFunction);
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::voidCallback()
{
    Context context;

    {
        GMutexLocker locker(m_mutex);
        if (!m_context.source)
            return;

        context = WTF::move(m_context);

        ASSERT(context.voidCallback);
        ASSERT(m_status == Scheduled);
        m_status = Dispatched;
    }

    context.voidCallback();

    bool shouldDestroy = false;
    {
        GMutexLocker locker(m_mutex);
        shouldDestroy = !m_context.source;
    }

    if (shouldDestroy)
        destroy(context.destroyCallback);
}

bool GMainLoopSource::boolCallback()
{
    Context context;

    {
        GMutexLocker locker(m_mutex);
        if (!m_context.source)
            return Stop;

        context = WTF::move(m_context);

        ASSERT(context.boolCallback);
        ASSERT(m_status == Scheduled || m_status == Dispatched);
        m_status = Dispatched;
    }

    bool retval = context.boolCallback();

    bool shouldDestroy = false;
    {
        GMutexLocker locker(m_mutex);
        if (retval && !m_context.source)
            m_context = WTF::move(context);
        else
            shouldDestroy = !m_context.source;
    }

    if (shouldDestroy)
        destroy(context.destroyCallback);
    return retval;
}

bool GMainLoopSource::socketCallback(GIOCondition condition)
{
    Context context;

    {
        GMutexLocker locker(m_mutex);
        if (!m_context.source)
            return Stop;

        context = WTF::move(m_context);

        ASSERT(context.socketCallback);
        ASSERT(m_status == Scheduled || m_status == Dispatched);
        m_status = Dispatched;
    }

    if (g_cancellable_is_cancelled(context.cancellable.get())) {
        destroy(context.destroyCallback);
        return Stop;
    }

    bool retval = context.socketCallback(condition);

    bool shouldDestroy = false;
    {
        GMutexLocker locker(m_mutex);
        if (retval && !m_context.source)
            m_context = WTF::move(context);
        else
            shouldDestroy = !m_context.source;
    }

    if (shouldDestroy)
        destroy(context.destroyCallback);
    return retval;
}

void GMainLoopSource::destroy(const std::function<void ()>& destroyCallback)
{
    // Nothing should be scheduled on this object at this point.
    ASSERT(!m_context.source);
    m_status = Ready;
    DeleteOnDestroyType deleteOnDestroy = m_deleteOnDestroy;

    if (destroyCallback)
        destroyCallback();

    if (deleteOnDestroy == DoNotDeleteOnDestroy)
        return;

    delete this;
}

gboolean GMainLoopSource::voidSourceCallback(GMainLoopSource* source)
{
    source->voidCallback();
    return G_SOURCE_REMOVE;
}

gboolean GMainLoopSource::boolSourceCallback(GMainLoopSource* source)
{
    return source->boolCallback() == Continue;
}

gboolean GMainLoopSource::socketSourceCallback(GSocket*, GIOCondition condition, GMainLoopSource* source)
{
    return source->socketCallback(condition) == Continue;
}

} // namespace WTF

#endif // USE(GLIB)
