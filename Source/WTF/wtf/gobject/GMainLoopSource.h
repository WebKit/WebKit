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

#ifndef GMainLoopSource_h
#define GMainLoopSource_h

#include <functional>
#include <glib.h>
#include <wtf/Noncopyable.h>
#include <wtf/gobject/GRefPtr.h>

typedef struct _GSocket GSocket;

namespace WTF {

class GMainLoopSource {
    WTF_MAKE_NONCOPYABLE(GMainLoopSource);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static GMainLoopSource& createAndDeleteOnDestroy();

    WTF_EXPORT_PRIVATE GMainLoopSource();
    WTF_EXPORT_PRIVATE ~GMainLoopSource();

    static const bool Stop = false;
    static const bool Continue = true;

    WTF_EXPORT_PRIVATE bool isScheduled() const;
    WTF_EXPORT_PRIVATE bool isActive() const;

    WTF_EXPORT_PRIVATE void schedule(const char* name, std::function<void()>, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE void schedule(const char* name, std::function<bool()>, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE void schedule(const char* name, std::function<bool(GIOCondition)>, GSocket*, GIOCondition, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE void scheduleAfterDelay(const char* name, std::function<void()>, std::chrono::milliseconds, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE void scheduleAfterDelay(const char* name, std::function<bool()>, std::chrono::milliseconds, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE void scheduleAfterDelay(const char* name, std::function<void()>, std::chrono::seconds, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE void scheduleAfterDelay(const char* name, std::function<bool()>, std::chrono::seconds, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE void cancel();

private:
    enum DeleteOnDestroyType { DeleteOnDestroy, DoNotDeleteOnDestroy };
    GMainLoopSource(DeleteOnDestroyType);

    enum Status { Ready, Scheduled, Dispatched };

    void reset();
    void scheduleIdleSource(const char* name, GSourceFunc, int priority, GMainContext*);
    void scheduleTimeoutSource(const char* name, GSourceFunc, int priority, GMainContext*);
    void voidCallback();
    bool boolCallback();
    bool socketCallback(GIOCondition);
    void destroy();

    static gboolean voidSourceCallback(GMainLoopSource*);
    static gboolean boolSourceCallback(GMainLoopSource*);
    static gboolean socketSourceCallback(GSocket*, GIOCondition, GMainLoopSource*);

    DeleteOnDestroyType m_deleteOnDestroy;
    Status m_status;
    GRefPtr<GSource> m_source;
    GRefPtr<GCancellable> m_cancellable;
    std::function<void ()> m_voidCallback;
    std::function<bool ()> m_boolCallback;
    std::function<bool (GIOCondition)> m_socketCallback;
    std::function<void ()> m_destroyCallback;
};

} // namespace WTF

using WTF::GMainLoopSource;

#endif // GMainLoopSource_h
