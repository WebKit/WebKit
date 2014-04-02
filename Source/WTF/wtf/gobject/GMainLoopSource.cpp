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

namespace WTF {

GMainLoopSource& GMainLoopSource::createAndDeleteOnDestroy()
{
    return *new GMainLoopSource(DeleteOnDestroy);
}

GMainLoopSource::GMainLoopSource()
    : m_deleteOnDestroy(DoNotDeleteOnDestroy)
    , m_status(Ready)
{
}

GMainLoopSource::GMainLoopSource(DeleteOnDestroyType deleteOnDestroy)
    : m_deleteOnDestroy(deleteOnDestroy)
    , m_status(Ready)
{
}

GMainLoopSource::~GMainLoopSource()
{
    cancel();
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
    if (!m_source)
        return;

    GRefPtr<GSource> source;
    m_source.swap(source);

    if (m_cancellable)
        g_cancellable_cancel(m_cancellable.get());
    g_source_destroy(source.get());
    destroy();
}

void GMainLoopSource::reset()
{
    m_status = Ready;
    m_source = nullptr;
    m_cancellable = nullptr;
    m_voidCallback = nullptr;
    m_boolCallback = nullptr;
    m_destroyCallback = nullptr;
}

void GMainLoopSource::scheduleIdleSource(const char* name, GSourceFunc sourceFunction, int priority, GMainContext* context)
{
    ASSERT(m_status == Ready);
    m_status = Scheduled;

    m_source = adoptGRef(g_idle_source_new());
    g_source_set_name(m_source.get(), name);
    if (priority != G_PRIORITY_DEFAULT_IDLE)
        g_source_set_priority(m_source.get(), priority);
    g_source_set_callback(m_source.get(), sourceFunction, this, nullptr);
    g_source_attach(m_source.get(), context);
}

void GMainLoopSource::schedule(const char* name, std::function<void ()> function, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();
    m_voidCallback = std::move(function);
    m_destroyCallback = std::move(destroyFunction);
    scheduleIdleSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::schedule(const char* name, std::function<bool ()> function, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();
    m_boolCallback = std::move(function);
    m_destroyCallback = std::move(destroyFunction);
    scheduleIdleSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::schedule(const char* name, std::function<bool (GIOCondition)> function, GSocket* socket, GIOCondition condition, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();
    ASSERT(m_status == Ready);
    m_status = Scheduled;

    m_socketCallback = std::move(function);
    m_destroyCallback = std::move(destroyFunction);
    m_cancellable = adoptGRef(g_cancellable_new());
    m_source = adoptGRef(g_socket_create_source(socket, condition, m_cancellable.get()));
    g_source_set_name(m_source.get(), name);
    g_source_set_callback(m_source.get(), reinterpret_cast<GSourceFunc>(socketSourceCallback), this, nullptr);
    g_source_attach(m_source.get(), context);
}

void GMainLoopSource::scheduleTimeoutSource(const char* name, GSourceFunc sourceFunction, int priority, GMainContext* context)
{
    ASSERT(m_status == Ready);
    m_status = Scheduled;

    ASSERT(m_source);
    g_source_set_name(m_source.get(), name);
    if (priority != G_PRIORITY_DEFAULT)
        g_source_set_priority(m_source.get(), priority);
    g_source_set_callback(m_source.get(), sourceFunction, this, nullptr);
    g_source_attach(m_source.get(), context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<void ()> function, std::chrono::milliseconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();
    m_source = adoptGRef(g_timeout_source_new(delay.count()));
    m_voidCallback = std::move(function);
    m_destroyCallback = std::move(destroyFunction);
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<bool ()> function, std::chrono::milliseconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();
    m_source = adoptGRef(g_timeout_source_new(delay.count()));
    m_boolCallback = std::move(function);
    m_destroyCallback = std::move(destroyFunction);
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<void ()> function, std::chrono::seconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();
    m_source = adoptGRef(g_timeout_source_new_seconds(delay.count()));
    m_voidCallback = std::move(function);
    m_destroyCallback = std::move(destroyFunction);
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<bool ()> function, std::chrono::seconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();
    m_source = adoptGRef(g_timeout_source_new_seconds(delay.count()));
    m_boolCallback = std::move(function);
    m_destroyCallback = std::move(destroyFunction);
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::voidCallback()
{
    if (!m_source)
        return;

    ASSERT(m_voidCallback);
    ASSERT(m_status == Scheduled);
    m_status = Dispatched;

    GSource* source = m_source.get();
    m_voidCallback();
    if (source == m_source.get())
        destroy();
}

bool GMainLoopSource::boolCallback()
{
    if (!m_source)
        return false;

    ASSERT(m_boolCallback);
    ASSERT(m_status == Scheduled);
    m_status = Dispatched;

    GSource* source = m_source.get();
    bool retval = m_boolCallback();
    if (!retval && source == m_source.get())
        destroy();

    return retval;
}

bool GMainLoopSource::socketCallback(GIOCondition condition)
{
    if (!m_source)
        return false;

    ASSERT(m_socketCallback);
    ASSERT(m_status == Scheduled);
    m_status = Dispatched;

    if (g_cancellable_is_cancelled(m_cancellable.get())) {
        destroy();
        return false;
    }

    GSource* source = m_source.get();
    bool retval = m_socketCallback(condition);
    if (!retval && source == m_source.get())
        destroy();

    return retval;
}

void GMainLoopSource::destroy()
{
    auto destroyCallback = std::move(m_destroyCallback);
    auto deleteOnDestroy = m_deleteOnDestroy;
    reset();
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
