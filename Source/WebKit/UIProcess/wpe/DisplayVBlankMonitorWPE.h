/*
 * Copyright (C) 2025 Igalia S.L.
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

#pragma once

#if ENABLE(WPE_PLATFORM)
#include "DisplayVBlankMonitor.h"
#include <wtf/glib/GRefPtr.h>

typedef struct _WPEScreenSyncObserver WPEScreenSyncObserver;

namespace WebKit {

class DisplayVBlankMonitorWPE final : public DisplayVBlankMonitor {
public:
    static std::unique_ptr<DisplayVBlankMonitor> create(PlatformDisplayID);
    DisplayVBlankMonitorWPE(unsigned, GRefPtr<WPEScreenSyncObserver>&&);
    virtual ~DisplayVBlankMonitorWPE();

    WPEScreenSyncObserver* observer() const { return m_observer.get(); }

private:
    Type type() const override { return Type::Wpe; }

    void start() final;
    void stop() final;
    bool isActive() final;
    void invalidate() final;

    GRefPtr<WPEScreenSyncObserver> m_observer;
};

} // namespace WebKit

#endif // ENABLE(WPE_PLATFORM)
