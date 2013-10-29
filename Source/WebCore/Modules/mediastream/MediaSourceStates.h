/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef MediaSourceStates_h
#define MediaSourceStates_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamSourceCapabilities.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaSourceStates : public RefCounted<MediaSourceStates>, public ScriptWrappable {
public:
    static RefPtr<MediaSourceStates> create(const MediaStreamSourceStates&);

    const AtomicString& sourceType() const;
    const AtomicString& sourceId() const { return m_sourceStates.sourceId(); }
    unsigned long width() const { return m_sourceStates.width(); }
    unsigned long height() const { return m_sourceStates.height(); }
    float frameRate() const { return m_sourceStates.frameRate(); }
    float aspectRatio() const { return m_sourceStates.aspectRatio(); }
    const AtomicString& facingMode() const;
    unsigned long volume() const { return m_sourceStates.volume(); }
    
    bool hasVideoSource() const { return m_sourceStates.sourceType() == MediaStreamSourceStates::Camera; }

private:
    explicit MediaSourceStates(const MediaStreamSourceStates&);

    MediaStreamSourceStates m_sourceStates;
};

} // namespace WebCore

#endif // MediaSourceStates_h

#endif
