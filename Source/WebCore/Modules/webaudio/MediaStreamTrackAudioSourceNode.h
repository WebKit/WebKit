/*
 * Copyright (C) 2026 Sergey Rubanov. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#include "MediaStreamAudioSourceNode.h"
#include "MediaStreamTrackAudioSourceOptions.h"

namespace WebCore {

class MediaStreamTrackAudioSourceNode final : public MediaStreamAudioSourceNodeBase {
    WTF_MAKE_TZONE_ALLOCATED(MediaStreamTrackAudioSourceNode);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MediaStreamTrackAudioSourceNode);
public:
    static ExceptionOr<Ref<MediaStreamTrackAudioSourceNode>> create(BaseAudioContext&, MediaStreamTrackAudioSourceOptions&&);

    ~MediaStreamTrackAudioSourceNode();

private:
    MediaStreamTrackAudioSourceNode(BaseAudioContext&, Ref<MediaStreamTrack>&&, Ref<WebAudioSourceProvider>&&);

    // ~MediaStreamTrack stops the source. The node keeps this track after script drops it.
    [[maybe_unused]] const Ref<MediaStreamTrack> m_track;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_AUDIONODE(MediaStreamTrackAudioSourceNode, NodeTypeMediaStreamTrackAudioSource);

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
