/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

// FIXME: We should probably move to making the WebCore/PlatformFooEvents trivial classes so that
// we can use them as the event type.

#include "WebEventModifier.h"
#include "WebEventType.h"
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

enum class WebEventInputSource : uint8_t { UserDriven, Automation };

#if PLATFORM(GTK) || PLATFORM(WPE)
uintptr_t generateSignpostIdentifier();
#endif

// Plain data for the fields common to every WebEvent. Passed to the create() function of each
// concrete event class, which forwards it to the WebEvent constructor.
struct WebEventData {
    WebEventType type;
    OptionSet<WebEventModifier> modifiers;
    MonotonicTime timestamp;
    WTF::UUID authorizationToken { WTF::UUID::createVersion4() };
#if PLATFORM(GTK) || PLATFORM(WPE)
    uintptr_t signpostIdentifier { generateSignpostIdentifier() };
#endif
};

class WebEvent : public ThreadSafeRefCounted<WebEvent> {
    WTF_MAKE_TZONE_ALLOCATED(WebEvent);
public:
    virtual ~WebEvent();

    WebEventType type() const { return m_data.type; }

    bool shiftKey() const { return modifiers().contains(WebEventModifier::ShiftKey); }
    bool controlKey() const { return modifiers().contains(WebEventModifier::ControlKey); }
    bool altKey() const { return modifiers().contains(WebEventModifier::AltKey); }
    bool metaKey() const { return modifiers().contains(WebEventModifier::MetaKey); }
    bool capsLockKey() const { return modifiers().contains(WebEventModifier::CapsLockKey); }

    OptionSet<WebEventModifier> modifiers() const { return m_data.modifiers; }

    MonotonicTime timestamp() const { return m_data.timestamp; }

    bool NODELETE isActivationTriggeringEvent() const;
    WTF::UUID authorizationToken() const { return m_data.authorizationToken; }

#if PLATFORM(GTK) || PLATFORM(WPE)
    uintptr_t signpostIdentifier() const { return m_data.signpostIdentifier; }
#endif

    const WebEventData& eventData() const LIFETIME_BOUND { return m_data; }

protected:
    explicit WebEvent(WebEventData&&);

private:
    WebEventData m_data;
};

WTF::TextStream& operator<<(WTF::TextStream&, WebEventType);

} // namespace WebKit
