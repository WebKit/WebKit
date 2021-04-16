/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
OBJC_CLASS AVOutputContext;
OBJC_CLASS NSData;
#endif

namespace WebCore {

class MediaPlaybackTargetContext {
public:
    enum class Type : uint8_t {
        None,
        AVOutputContext,
        SerializedAVOutputContext,
        Mock,
    };

    enum class MockState : uint8_t {
        Unknown = 0,
        OutputDeviceUnavailable = 1,
        OutputDeviceAvailable = 2,
    };

    MediaPlaybackTargetContext() = default;
    WEBCORE_EXPORT explicit MediaPlaybackTargetContext(RetainPtr<AVOutputContext>&&);

    MediaPlaybackTargetContext(RetainPtr<NSData>&& serializedOutputContext, bool hasActiveRoute)
        : m_type(Type::SerializedAVOutputContext)
        , m_serializedOutputContext(WTFMove(serializedOutputContext))
        , m_cachedHasActiveRoute(hasActiveRoute)
    {
        ASSERT(m_serializedOutputContext);
    }

    MediaPlaybackTargetContext(const String& mockDeviceName, MockState state)
        : m_type(Type::Mock)
        , m_mockDeviceName(mockDeviceName)
        , m_mockState(state)
    {
    }

    Type type() const { return m_type; }
    WEBCORE_EXPORT String deviceName() const;
    WEBCORE_EXPORT bool hasActiveRoute() const;
    bool supportsRemoteVideoPlayback() const;

    MockState mockState() const
    {
        ASSERT(m_type == Type::Mock);
        return m_mockState;
    }

    RetainPtr<AVOutputContext> outputContext() const
    {
        ASSERT(m_type == Type::AVOutputContext);
        return m_outputContext;
    }

    RetainPtr<NSData> serializedOutputContext() const
    {
        ASSERT(m_type == Type::SerializedAVOutputContext);
        return m_serializedOutputContext;
    }

    WEBCORE_EXPORT bool serializeOutputContext();
    WEBCORE_EXPORT bool deserializeOutputContext();

    bool encodingRequiresPlatformData() const { return m_type == Type::AVOutputContext || m_type == Type::SerializedAVOutputContext; }

private:
    Type m_type { Type::None };
    RetainPtr<AVOutputContext> m_outputContext;
    RetainPtr<NSData> m_serializedOutputContext;
    bool m_cachedHasActiveRoute { false };

    String m_mockDeviceName;
    MockState m_mockState { MockState::Unknown };
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::MediaPlaybackTargetContext::Type> {
    using values = EnumValues<
        WebCore::MediaPlaybackTargetContext::Type,
        WebCore::MediaPlaybackTargetContext::Type::None,
        WebCore::MediaPlaybackTargetContext::Type::AVOutputContext,
        WebCore::MediaPlaybackTargetContext::Type::SerializedAVOutputContext,
        WebCore::MediaPlaybackTargetContext::Type::Mock
    >;
};

template<> struct EnumTraits<WebCore::MediaPlaybackTargetContext::MockState> {
    using values = EnumValues<
        WebCore::MediaPlaybackTargetContext::MockState,
        WebCore::MediaPlaybackTargetContext::MockState::Unknown,
        WebCore::MediaPlaybackTargetContext::MockState::OutputDeviceUnavailable,
        WebCore::MediaPlaybackTargetContext::MockState::OutputDeviceAvailable
    >;
};

} // namespace WTF

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
