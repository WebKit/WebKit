/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "GPUProcessPreferences.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

namespace WebKit {

GPUProcessPreferences::GPUProcessPreferences() = default;

void GPUProcessPreferences::encode(IPC::Encoder& encoder) const
{
#if ENABLE(OPUS)
    encoder << opusDecoderEnabled;
#endif
    
#if ENABLE(VORBIS)
    encoder << vorbisDecoderEnabled;
#endif
    
#if ENABLE(WEBM_FORMAT_READER)
    encoder << webMFormatReaderEnabled;
#endif
    
#if ENABLE(MEDIA_SOURCE) && ENABLE(VP9)
    encoder << webMParserEnabled;
#endif
    
#if ENABLE(MEDIA_SOURCE) && HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    encoder << mediaSourceInlinePaintingEnabled;
#endif
    
#if HAVE(AVCONTENTKEYSPECIFIER)
    encoder << sampleBufferContentKeySessionSupportEnabled;
#endif
}

bool GPUProcessPreferences::decode(IPC::Decoder& decoder, GPUProcessPreferences& result)
{
#if ENABLE(OPUS)
    if (!decoder.decode(result.opusDecoderEnabled))
        return false;
#endif
    
#if ENABLE(VORBIS)
    if (!decoder.decode(result.vorbisDecoderEnabled))
        return false;
#endif
    
#if ENABLE(WEBM_FORMAT_READER)
    if (!decoder.decode(result.webMFormatReaderEnabled))
        return false;
#endif
    
#if ENABLE(MEDIA_SOURCE) && ENABLE(VP9)
    if (!decoder.decode(result.webMParserEnabled))
        return false;
#endif
    
#if ENABLE(MEDIA_SOURCE) && HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    if (!decoder.decode(result.mediaSourceInlinePaintingEnabled))
        return false;
#endif
    
#if HAVE(AVCONTENTKEYSPECIFIER)
    if (!decoder.decode(result.sampleBufferContentKeySessionSupportEnabled))
        return false;
#endif
    return true;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
