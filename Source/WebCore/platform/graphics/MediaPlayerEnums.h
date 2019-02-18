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
    enum NetworkState { Empty, Idle, Loading, Loaded, FormatError, NetworkError, DecodeError };
    enum ReadyState  { HaveNothing, HaveMetadata, HaveCurrentData, HaveFutureData, HaveEnoughData };
    enum MovieLoadType { Unknown, Download, StoredStream, LiveStream };
    enum Preload { None, MetaData, Auto };
    enum VideoGravity { VideoGravityResize, VideoGravityResizeAspect, VideoGravityResizeAspectFill };
    enum SupportsType { IsNotSupported, IsSupported, MayBeSupported };
    enum {
        VideoFullscreenModeNone = 0,
        VideoFullscreenModeStandard = 1 << 0,
        VideoFullscreenModePictureInPicture = 1 << 1,
    };
    typedef uint32_t VideoFullscreenMode;
};

WTF::String convertEnumerationToString(MediaPlayerEnums::ReadyState);
WTF::String convertEnumerationToString(MediaPlayerEnums::NetworkState);
WTF::String convertEnumerationToString(MediaPlayerEnums::Preload);
WTF::String convertEnumerationToString(MediaPlayerEnums::SupportsType);

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

}; // namespace WTF
