/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include <optional>

namespace WebKit {

class WebPreferences;

struct GPUProcessPreferences {
    void copyEnabledWebPreferences(const WebPreferences&);
    
#if ENABLE(OPUS)
    std::optional<bool> opusDecoderEnabled;
#endif
    
#if ENABLE(VORBIS)
    std::optional<bool> vorbisDecoderEnabled;
#endif
    
#if ENABLE(WEBM_FORMAT_READER)
    std::optional<bool> webMFormatReaderEnabled;
#endif
    
#if ENABLE(MEDIA_SOURCE) && ENABLE(VP9)
    std::optional<bool> webMParserEnabled;
#endif
    
#if ENABLE(MEDIA_SOURCE) && HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    std::optional<bool> mediaSourceInlinePaintingEnabled;
#endif
    
#if HAVE(AVCONTENTKEYSPECIFIER)
    std::optional<bool> sampleBufferContentKeySessionSupportEnabled;
#endif
    
#if ENABLE(ALTERNATE_WEBM_PLAYER)
    std::optional<bool> alternateWebMPlayerEnabled;
#endif

#if HAVE(SC_CONTENT_SHARING_PICKER)
    std::optional<bool> useSCContentSharingPicker;
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    std::optional<bool> mediaCapabilityGrantsEnabled;
#endif

#if ENABLE(VP9)
    std::optional<bool> vp8DecoderEnabled;
    std::optional<bool> vp9DecoderEnabled;
    std::optional<bool> vp9SWDecoderEnabled;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
