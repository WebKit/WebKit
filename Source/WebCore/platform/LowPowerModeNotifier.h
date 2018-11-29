/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Function.h>

#if PLATFORM(IOS_FAMILY)
#include <wtf/RetainPtr.h>
OBJC_CLASS WebLowPowerModeObserver;
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
typedef struct _GDBusProxy GDBusProxy;
#endif

namespace WebCore {

class LowPowerModeNotifier {
public:
    using LowPowerModeChangeCallback = WTF::Function<void(bool isLowPowerModeEnabled)>;
    WEBCORE_EXPORT explicit LowPowerModeNotifier(LowPowerModeChangeCallback&&);
    WEBCORE_EXPORT ~LowPowerModeNotifier();

    WEBCORE_EXPORT bool isLowPowerModeEnabled() const;

private:
#if PLATFORM(IOS_FAMILY)
    void notifyLowPowerModeChanged(bool);
    friend void notifyLowPowerModeChanged(LowPowerModeNotifier&, bool);

    RetainPtr<WebLowPowerModeObserver> m_observer;
    LowPowerModeChangeCallback m_callback;
#elif USE(GLIB)
    void updateWarningLevel();
    void warningLevelChanged();
    static void gPropertiesChangedCallback(LowPowerModeNotifier*, GVariant* changedProperties);

    GRefPtr<GDBusProxy> m_displayDeviceProxy;
    GRefPtr<GCancellable> m_cancellable;
    LowPowerModeChangeCallback m_callback;
    bool m_lowPowerModeEnabled { false };
#endif
};

}
