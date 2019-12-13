/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaPlayerEnums {
public:
    enum class NetworkState : uint8_t {
        Empty,
        Idle,
        Loading,
        Loaded,
        FormatError,
        NetworkError,
        DecodeError
    };

    enum class ReadyState : uint8_t {
        HaveNothing,
        HaveMetadata,
        HaveCurrentData,
        HaveFutureData,
        HaveEnoughData
    };

    enum class MovieLoadType : uint8_t {
        Unknown,
        Download,
        StoredStream,
        LiveStream
    };

    enum class Preload : uint8_t {
        None,
        MetaData,
        Auto
    };

    enum class VideoGravity : uint8_t {
        Resize,
        ResizeAspect,
        ResizeAspectFill
    };

    enum class SupportsType : uint8_t {
        IsNotSupported,
        IsSupported,
        MayBeSupported
    };

    enum {
        VideoFullscreenModeNone = 0,
        VideoFullscreenModeStandard = 1 << 0,
        VideoFullscreenModePictureInPicture = 1 << 1,
    };
    typedef uint32_t VideoFullscreenMode;

    enum class BufferingPolicy : uint8_t {
        Default,
        LimitReadAhead,
        MakeResourcesPurgeable,
        PurgeResources,
    };

    enum class MediaEngineIdentifier : uint8_t {
        AVFoundation,
        AVFoundationMSE,
        AVFoundationMediaStream,
        AVFoundationCF,
        GStreamer,
        GStreamerMSE,
        HolePunch,
        MediaFoundation,
        MockMSE,
    };
};

WTF::String convertEnumerationToString(MediaPlayerEnums::ReadyState);
WTF::String convertEnumerationToString(MediaPlayerEnums::NetworkState);
WTF::String convertEnumerationToString(MediaPlayerEnums::Preload);
WTF::String convertEnumerationToString(MediaPlayerEnums::SupportsType);
WTF::String convertEnumerationToString(MediaPlayerEnums::BufferingPolicy);

} // namespace WebCore


namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::MediaPlayerEnums::ReadyState> {
    static WTF::String toString(const WebCore::MediaPlayerEnums::ReadyState state)
    {
        return convertEnumerationToString(state);
    }
};

template <>
struct LogArgument<WebCore::MediaPlayerEnums::NetworkState> {
    static WTF::String toString(const WebCore::MediaPlayerEnums::NetworkState state)
    {
        return convertEnumerationToString(state);
    }
};

template <>
struct LogArgument<WebCore::MediaPlayerEnums::BufferingPolicy> {
    static WTF::String toString(const WebCore::MediaPlayerEnums::BufferingPolicy policy)
    {
        return convertEnumerationToString(policy);
    }
};

template<> struct EnumTraits<WebCore::MediaPlayerEnums::NetworkState> {
    using values = EnumValues<
        WebCore::MediaPlayerEnums::NetworkState,
        WebCore::MediaPlayerEnums::NetworkState::Empty,
        WebCore::MediaPlayerEnums::NetworkState::Idle,
        WebCore::MediaPlayerEnums::NetworkState::Loading,
        WebCore::MediaPlayerEnums::NetworkState::Loaded,
        WebCore::MediaPlayerEnums::NetworkState::FormatError,
        WebCore::MediaPlayerEnums::NetworkState::NetworkError,
        WebCore::MediaPlayerEnums::NetworkState::DecodeError
    >;
};

template<> struct EnumTraits<WebCore::MediaPlayerEnums::ReadyState> {
    using values = EnumValues<
        WebCore::MediaPlayerEnums::ReadyState,
        WebCore::MediaPlayerEnums::ReadyState::HaveNothing,
        WebCore::MediaPlayerEnums::ReadyState::HaveMetadata,
        WebCore::MediaPlayerEnums::ReadyState::HaveCurrentData,
        WebCore::MediaPlayerEnums::ReadyState::HaveFutureData,
        WebCore::MediaPlayerEnums::ReadyState::HaveEnoughData
    >;
};

template<> struct EnumTraits<WebCore::MediaPlayerEnums::MovieLoadType> {
    using values = EnumValues<
        WebCore::MediaPlayerEnums::MovieLoadType,
        WebCore::MediaPlayerEnums::MovieLoadType::Unknown,
        WebCore::MediaPlayerEnums::MovieLoadType::Download,
        WebCore::MediaPlayerEnums::MovieLoadType::StoredStream,
        WebCore::MediaPlayerEnums::MovieLoadType::LiveStream
    >;
};

template<> struct EnumTraits<WebCore::MediaPlayerEnums::Preload> {
    using values = EnumValues<
        WebCore::MediaPlayerEnums::Preload,
        WebCore::MediaPlayerEnums::Preload::None,
        WebCore::MediaPlayerEnums::Preload::MetaData,
        WebCore::MediaPlayerEnums::Preload::Auto
    >;
};

template<> struct EnumTraits<WebCore::MediaPlayerEnums::VideoGravity> {
    using values = EnumValues<
        WebCore::MediaPlayerEnums::VideoGravity,
        WebCore::MediaPlayerEnums::VideoGravity::Resize,
        WebCore::MediaPlayerEnums::VideoGravity::ResizeAspect,
        WebCore::MediaPlayerEnums::VideoGravity::ResizeAspectFill
    >;
};

template<> struct EnumTraits<WebCore::MediaPlayerEnums::SupportsType> {
    using values = EnumValues<
        WebCore::MediaPlayerEnums::SupportsType,
        WebCore::MediaPlayerEnums::SupportsType::IsNotSupported,
        WebCore::MediaPlayerEnums::SupportsType::IsSupported,
        WebCore::MediaPlayerEnums::SupportsType::MayBeSupported
    >;
};

template<> struct EnumTraits<WebCore::MediaPlayerEnums::BufferingPolicy> {
    using values = EnumValues<
        WebCore::MediaPlayerEnums::BufferingPolicy,
        WebCore::MediaPlayerEnums::BufferingPolicy::Default,
        WebCore::MediaPlayerEnums::BufferingPolicy::LimitReadAhead,
        WebCore::MediaPlayerEnums::BufferingPolicy::MakeResourcesPurgeable,
        WebCore::MediaPlayerEnums::BufferingPolicy::PurgeResources
        >;
    };

template<> struct EnumTraits<WebCore::MediaPlayerEnums::MediaEngineIdentifier> {
    using values = EnumValues<
        WebCore::MediaPlayerEnums::MediaEngineIdentifier,
        WebCore::MediaPlayerEnums::MediaEngineIdentifier::AVFoundation,
        WebCore::MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE,
        WebCore::MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMediaStream,
        WebCore::MediaPlayerEnums::MediaEngineIdentifier::GStreamer,
        WebCore::MediaPlayerEnums::MediaEngineIdentifier::GStreamerMSE,
        WebCore::MediaPlayerEnums::MediaEngineIdentifier::HolePunch,
        WebCore::MediaPlayerEnums::MediaEngineIdentifier::MediaFoundation,
        WebCore::MediaPlayerEnums::MediaEngineIdentifier::MockMSE
    >;
};

}; // namespace WTF
