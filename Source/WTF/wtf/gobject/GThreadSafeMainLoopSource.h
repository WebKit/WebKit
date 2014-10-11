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

#ifndef GThreadSafeMainLoopSource_h
#define GThreadSafeMainLoopSource_h

#if USE(GLIB)

#include <wtf/gobject/GMainLoopSource.h>

typedef struct _GRecMutex GRecMutex;

namespace WTF {

class GThreadSafeMainLoopSource final : public GMainLoopSource {
    WTF_MAKE_NONCOPYABLE(GThreadSafeMainLoopSource);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE GThreadSafeMainLoopSource();
    WTF_EXPORT_PRIVATE virtual ~GThreadSafeMainLoopSource();

    WTF_EXPORT_PRIVATE virtual void schedule(const char* name, std::function<void()>, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr) override;
    WTF_EXPORT_PRIVATE virtual void schedule(const char* name, std::function<bool()>, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr) override;
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<void()>, std::chrono::milliseconds, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr) override;
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<bool()>, std::chrono::milliseconds, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr) override;
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<void()>, std::chrono::seconds, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr) override;
    WTF_EXPORT_PRIVATE virtual void scheduleAfterDelay(const char* name, std::function<bool()>, std::chrono::seconds, int priority = G_PRIORITY_DEFAULT, std::function<void()> destroyFunction = nullptr, GMainContext* = nullptr) override;
    WTF_EXPORT_PRIVATE virtual void cancel() override;

private:
    virtual void voidCallback() override;
    virtual bool boolCallback() override;

    virtual bool prepareVoidCallback(Context&) override;
    virtual void finishVoidCallback() override;
    virtual bool prepareBoolCallback(Context&) override;
    virtual void finishBoolCallback(bool retval, Context&) override;

    GRecMutex m_mutex;
    GRefPtr<GCancellable> m_cancellable;
};

} // namespace WTF

using WTF::GThreadSafeMainLoopSource;

#endif // USE(GLIB)

#endif // GThreadSafeMainLoopSource_h
