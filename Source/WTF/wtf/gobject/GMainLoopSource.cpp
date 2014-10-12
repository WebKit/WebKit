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
#include "GMainLoopSource.h"

#if USE(GLIB)

#include <gio/gio.h>

namespace WTF {

GMainLoopSource& GMainLoopSource::create()
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
    // Delete-on-destroy GMainLoopSource objects can't be cancelled.
    if (m_deleteOnDestroy == DeleteOnDestroy)
        return;

    // A valid context should only be present if GMainLoopSource is in the Scheduled or Dispatching state.
    ASSERT(!m_context.source || m_status == Scheduled || m_status == Dispatching);

    m_status = Ready;

    g_cancellable_cancel(m_context.socketCancellable.get());

    if (!m_context.source)
        return;

    Context context = WTF::move(m_context);
    context.destroySource();
}

void GMainLoopSource::scheduleIdleSource(const char* name, GSourceFunc sourceFunction, int priority, GMainContext* context)
{
    ASSERT(m_status == Ready);
    m_status = Scheduled;

    g_source_set_name(m_context.source.get(), name);
    if (priority != G_PRIORITY_DEFAULT_IDLE)
        g_source_set_priority(m_context.source.get(), priority);
    g_source_set_callback(m_context.source.get(), sourceFunction, this, nullptr);
    g_source_attach(m_context.source.get(), context);
}

void GMainLoopSource::schedule(const char* name, std::function<void ()> function, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();

    ASSERT(!m_context.source);
    m_context = {
        adoptGRef(g_idle_source_new()),
        nullptr, // cancellable
        nullptr, // socketCancellable
        WTF::move(function),
        nullptr, // boolCallback
        nullptr, // socketCallback
        WTF::move(destroyFunction)
    };
    scheduleIdleSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::schedule(const char* name, std::function<bool ()> function, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();

    ASSERT(!m_context.source);
    m_context = {
        adoptGRef(g_idle_source_new()),
        nullptr, // cancellable
        nullptr, // socketCancellable
        nullptr, // voidCallback
        WTF::move(function),
        nullptr, // socketCallback
        WTF::move(destroyFunction)
    };
    scheduleIdleSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::schedule(const char* name, std::function<bool (GIOCondition)> function, GSocket* socket, GIOCondition condition, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();

    ASSERT(!m_context.source);
    GCancellable* socketCancellable = g_cancellable_new();
    m_context = {
        adoptGRef(g_socket_create_source(socket, condition, socketCancellable)),
        nullptr, // cancellable
        adoptGRef(socketCancellable),
        nullptr, // voidCallback
        nullptr, // boolCallback
        WTF::move(function),
        WTF::move(destroyFunction)
    };

    ASSERT(m_status == Ready);
    m_status = Scheduled;
    g_source_set_name(m_context.source.get(), name);
    g_source_set_callback(m_context.source.get(), reinterpret_cast<GSourceFunc>(socketSourceCallback), this, nullptr);
    g_source_attach(m_context.source.get(), context);
}

void GMainLoopSource::scheduleTimeoutSource(const char* name, GSourceFunc sourceFunction, int priority, GMainContext* context)
{
    ASSERT(m_status == Ready);
    m_status = Scheduled;

    g_source_set_name(m_context.source.get(), name);
    if (priority != G_PRIORITY_DEFAULT)
        g_source_set_priority(m_context.source.get(), priority);
    g_source_set_callback(m_context.source.get(), sourceFunction, this, nullptr);
    g_source_attach(m_context.source.get(), context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<void ()> function, std::chrono::milliseconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();

    ASSERT(!m_context.source);
    m_context = {
        adoptGRef(g_timeout_source_new(delay.count())),
        nullptr, // cancellable
        nullptr, // socketCancellable
        WTF::move(function),
        nullptr, // boolCallback
        nullptr, // socketCallback
        WTF::move(destroyFunction)
    };
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<bool ()> function, std::chrono::milliseconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();

    ASSERT(!m_context.source);
    m_context = {
        adoptGRef(g_timeout_source_new(delay.count())),
        nullptr, // cancellable
        nullptr, // socketCancellable
        nullptr, // voidCallback
        WTF::move(function),
        nullptr, // socketCallback
        WTF::move(destroyFunction)
    };
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<void ()> function, std::chrono::seconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();

    ASSERT(!m_context.source);
    m_context = {
        adoptGRef(g_timeout_source_new_seconds(delay.count())),
        nullptr, // cancellable
        nullptr, // socketCancellable
        WTF::move(function),
        nullptr, // boolCallback
        nullptr, // socketCallback
        WTF::move(destroyFunction)
    };
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(voidSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAfterDelay(const char* name, std::function<bool ()> function, std::chrono::seconds delay, int priority, std::function<void ()> destroyFunction, GMainContext* context)
{
    cancel();

    ASSERT(!m_context.source);
    m_context = {
        adoptGRef(g_timeout_source_new_seconds(delay.count())),
        nullptr, // cancellable
        nullptr, // socketCancellable
        nullptr, // voidCallback
        WTF::move(function),
        nullptr, // socketCallback
        WTF::move(destroyFunction)
    };
    scheduleTimeoutSource(name, reinterpret_cast<GSourceFunc>(boolSourceCallback), priority, context);
}

void GMainLoopSource::scheduleAndDeleteOnDestroy(const char* name, std::function<void()> function, int priority, std::function<void()> destroyFunction, GMainContext* context)
{
    create().schedule(name, function, priority, destroyFunction, context);
}

void GMainLoopSource::scheduleAndDeleteOnDestroy(const char* name, std::function<bool()> function, int priority, std::function<void()> destroyFunction, GMainContext* context)
{
    create().schedule(name, function, priority, destroyFunction, context);
}

void GMainLoopSource::scheduleAfterDelayAndDeleteOnDestroy(const char* name, std::function<void()> function, std::chrono::milliseconds delay, int priority, std::function<void()> destroyFunction, GMainContext* context)
{
    create().scheduleAfterDelay(name, function, delay, priority, destroyFunction, context);
}

void GMainLoopSource::scheduleAfterDelayAndDeleteOnDestroy(const char* name, std::function<bool()> function, std::chrono::milliseconds delay, int priority, std::function<void()> destroyFunction, GMainContext* context)
{
    create().scheduleAfterDelay(name, function, delay, priority, destroyFunction, context);
}

void GMainLoopSource::scheduleAfterDelayAndDeleteOnDestroy(const char* name, std::function<void()> function, std::chrono::seconds delay, int priority, std::function<void()> destroyFunction, GMainContext* context)
{
    create().scheduleAfterDelay(name, function, delay, priority, destroyFunction, context);
}

void GMainLoopSource::scheduleAfterDelayAndDeleteOnDestroy(const char* name, std::function<bool()> function, std::chrono::seconds delay, int priority, std::function<void()> destroyFunction, GMainContext* context)
{
    create().scheduleAfterDelay(name, function, delay, priority, destroyFunction, context);
}

bool GMainLoopSource::prepareVoidCallback(Context& context)
{
    if (!m_context.source)
        return false;

    context = WTF::move(m_context);

    ASSERT(context.voidCallback);
    ASSERT(m_status == Scheduled);
    m_status = Dispatching;

    return true;
}

void GMainLoopSource::finishVoidCallback()
{
    m_status = Ready;
}

void GMainLoopSource::voidCallback()
{
    Context context;
    if (!prepareVoidCallback(context))
        return;

    context.voidCallback();
    if (m_status != Ready && !m_context.source) {
        // Switch to Ready if it hasn't been re-scheduled or cancelled.
        finishVoidCallback();
    }

    context.destroySource();
    if (m_deleteOnDestroy == DeleteOnDestroy)
        delete this;
}

bool GMainLoopSource::prepareBoolCallback(Context& context)
{
    if (!m_context.source)
        return false;

    context = WTF::move(m_context);

    ASSERT(context.boolCallback);
    ASSERT(m_status == Scheduled || m_status == Dispatching);
    m_status = Dispatching;
    return true;
}

void GMainLoopSource::finishBoolCallback(bool retval, Context& context)
{
    // m_status should reflect whether the GMainLoopSource has been rescheduled during dispatch.
    ASSERT((!m_context.source && m_status == Dispatching) || m_status == Scheduled);
    if (retval && !m_context.source)
        m_context = WTF::move(context);
    else if (!retval)
        m_status = Ready;
}

bool GMainLoopSource::boolCallback()
{
    Context context;
    if (!prepareBoolCallback(context))
        return Stop;

    bool retval = context.boolCallback();
    if (m_status != Ready && !m_context.source) {
        // Prepare for a new iteration or switch to Ready if it hasn't been re-scheduled or cancelled.
        finishBoolCallback(retval, context);
    }

    if (context.source) {
        context.destroySource();
        if (m_deleteOnDestroy == DeleteOnDestroy)
            delete this;
    }

    return retval;
}

bool GMainLoopSource::socketCallback(GIOCondition condition)
{
    if (!m_context.source)
        return Stop;

    Context context = WTF::move(m_context);

    ASSERT(context.socketCallback);
    ASSERT(m_status == Scheduled || m_status == Dispatching);
    m_status = Dispatching;

    if (g_cancellable_is_cancelled(context.socketCancellable.get())) {
        context.destroySource();
        return Stop;
    }

    bool retval = context.socketCallback(condition);

    if (m_status != Ready && !m_context.source) {
        // m_status should reflect whether the GMainLoopSource has been rescheduled during dispatch.
        ASSERT((!m_context.source && m_status == Dispatching) || m_status == Scheduled);
        if (retval && !m_context.source)
            m_context = WTF::move(context);
        else if (!retval)
            m_status = Ready;
    }

    if (context.source)
        context.destroySource();

    return retval;
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

void GMainLoopSource::Context::destroySource()
{
    g_source_destroy(source.get());
    if (destroyCallback)
        destroyCallback();
}

} // namespace WTF

#endif // USE(GLIB)
