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

#if USE(GLIB)

#include <functional>
#include <glib.h>
#include <wtf/Noncopyable.h>
#include <wtf/glib/GRefPtr.h>

namespace WTF {

class GMainLoopSource {
    WTF_MAKE_NONCOPYABLE(GMainLoopSource);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE GMainLoopSource() = default;
    WTF_EXPORT_PRIVATE virtual ~GMainLoopSource();

    static const bool Stop = false;
    static const bool Continue = true;

    WTF_EXPORT_PRIVATE bool isScheduled() const;
    WTF_EXPORT_PRIVATE bool isActive() const;

    WTF_EXPORT_PRIVATE virtual void schedule(const char* name, std::function<void()>&&, int priority = G_PRIORITY_DEFAULT, std::function<void()>&& destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE virtual void schedule(const char* name, std::function<bool()>&&, int priority = G_PRIORITY_DEFAULT, std::function<void()>&& destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<void()>&&, std::chrono::milliseconds, int priority = G_PRIORITY_DEFAULT, std::function<void()>&& destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<bool()>&&, std::chrono::milliseconds, int priority = G_PRIORITY_DEFAULT, std::function<void()>&& destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<void()>&&, std::chrono::seconds, int priority = G_PRIORITY_DEFAULT, std::function<void()>&& destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<bool()>&&, std::chrono::seconds, int priority = G_PRIORITY_DEFAULT, std::function<void()>&& destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<void()>&&, std::chrono::microseconds, int priority = G_PRIORITY_DEFAULT, std::function<void()>&& destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<bool()>&&, std::chrono::microseconds, int priority = G_PRIORITY_DEFAULT, std::function<void()>&& destroyFunction = nullptr, GMainContext* = nullptr);
    WTF_EXPORT_PRIVATE virtual void cancel();

protected:
    enum Status { Ready, Scheduled, Dispatching };

    struct Context {
        Context() = default;
        Context& operator=(Context&& c)
        {
            source = WTF::move(c.source);
            cancellable = WTF::move(c.cancellable);
            voidCallback = WTF::move(c.voidCallback);
            boolCallback = WTF::move(c.boolCallback);
            destroyCallback = WTF::move(c.destroyCallback);
            return *this;
        }

        void destroySource();

        GRefPtr<GSource> source;
        GRefPtr<GCancellable> cancellable;
        std::function<void ()> voidCallback;
        std::function<bool ()> boolCallback;
        std::function<void ()> destroyCallback;
    };

    virtual void voidCallback();
    virtual bool boolCallback();

    virtual bool prepareVoidCallback(Context&);
    virtual void finishVoidCallback();
    virtual bool prepareBoolCallback(Context&);
    virtual void finishBoolCallback(bool retval, Context&);

private:
    void scheduleIdleSource(const char* name, GSourceFunc, int priority, GMainContext*);
    void scheduleTimeoutSource(const char* name, GSourceFunc, int priority, GMainContext*);

    static gboolean voidSourceCallback(GMainLoopSource*);
    static gboolean boolSourceCallback(GMainLoopSource*);

protected:
    Context m_context;
    Status m_status { Ready };
};

} // namespace WTF

using WTF::GMainLoopSource;

#endif // USE(GLIB)

#endif // GMainLoopSource_h
