/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionAlarmParameters.h"
#include <WebCore/Timer.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebExtensionContext;

class WebExtensionAlarm : public RefCounted<WebExtensionAlarm> {
    WTF_MAKE_NONCOPYABLE(WebExtensionAlarm);
    WTF_MAKE_FAST_ALLOCATED;

public:
    template<typename... Args>
    static Ref<WebExtensionAlarm> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionAlarm(std::forward<Args>(args)...));
    }

    explicit WebExtensionAlarm(String name, Seconds initialInterval, Seconds repeatInterval, Function<void(WebExtensionAlarm&)>&& handler = nullptr)
        : m_parameters({ name, initialInterval, repeatInterval, MonotonicTime::nan() })
        , m_handler(WTFMove(handler))
    {
        ASSERT(!name.isNull());
        schedule();
    }

    const WebExtensionAlarmParameters& parameters() const { return m_parameters; }

    const String& name() const { return m_parameters.name; }
    Seconds initialInterval() const { return m_parameters.initialInterval; }
    Seconds repeatInterval() const { return m_parameters.repeatInterval; }
    MonotonicTime nextScheduledTime() const { return m_parameters.nextScheduledTime; }

private:
    void schedule();
    void fire();

    WebExtensionAlarmParameters m_parameters;

    Function<void(WebExtensionAlarm&)> m_handler;
    std::unique_ptr<WebCore::Timer> m_timer;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
