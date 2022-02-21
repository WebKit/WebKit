/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "AudioMediaStreamTrackRenderer.h"

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"

#if PLATFORM(COCOA)
#include "AudioMediaStreamTrackRendererCocoa.h"
#endif

#if USE(LIBWEBRTC)
#include "LibWebRTCAudioModule.h"
#endif

namespace WTF {
class MediaTime;
}

namespace WebCore {

std::unique_ptr<AudioMediaStreamTrackRenderer> AudioMediaStreamTrackRenderer::create(Init&& init)
{
#if PLATFORM(COCOA)
    return makeUnique<AudioMediaStreamTrackRendererCocoa>(WTFMove(init));
#else
    UNUSED_PARAM(init);
    return nullptr;
#endif
}

AudioMediaStreamTrackRenderer::AudioMediaStreamTrackRenderer(Init&& init)
    : m_crashCallback(WTFMove(init.crashCallback))
#if USE(LIBWEBRTC)
    , m_audioModule(WTFMove(init.audioModule))
#endif
#if !RELEASE_LOG_DISABLED
    , m_logger(init.logger)
    , m_logIdentifier(init.logIdentifier)
#endif
{
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& AudioMediaStreamTrackRenderer::logChannel() const
{
    return LogMedia;
}
#endif

}

#endif // ENABLE(MEDIA_STREAM)
