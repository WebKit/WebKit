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

#ifndef MediaStreamSourceStates_h
#define MediaStreamSourceStates_h

#if ENABLE(MEDIA_STREAM)

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class MediaStreamSourceStates {
public:
    enum SourceType { None, Camera, Microphone };
    enum VideoFacingMode { Unknown, User, Environment, Left, Right };

    MediaStreamSourceStates()
        : m_sourceType(None)
        , m_facingMode(Unknown)
        , m_width(0)
        , m_height(0)
        , m_frameRate(0)
        , m_aspectRatio(0)
        , m_volume(0)
    {
    }

    static const AtomicString& sourceType(MediaStreamSourceStates::SourceType);
    static const AtomicString& facingMode(MediaStreamSourceStates::VideoFacingMode);

    SourceType sourceType() const { return m_sourceType; }
    void setSourceType(SourceType type) { m_sourceType = type; }

    const AtomicString& sourceId() const { return m_sourceId; }
    void setSourceId(const AtomicString& sourceId) { m_sourceId = sourceId; }

    VideoFacingMode facingMode() const { return m_facingMode; }
    void setFacingMode(VideoFacingMode facingMode) { m_facingMode = facingMode; }
    
    unsigned long width() const { return m_width; }
    void setWidth(unsigned long width) { m_width = width; }
    
    unsigned long height() const { return m_height; }
    void setHeight(unsigned long height) { m_height = height; }
    
    float frameRate() const { return m_frameRate; }
    void setFrameRate(float frameRate) { m_frameRate = frameRate; }
    
    float aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(float aspectRatio) { m_aspectRatio = aspectRatio; }
    
    unsigned long volume() const { return m_volume; }
    void setVolume(unsigned long volume) { m_volume = volume; }
    
private:
    SourceType m_sourceType;
    AtomicString m_sourceId;
    VideoFacingMode m_facingMode;
    unsigned long m_width;
    unsigned long m_height;
    float m_frameRate;
    float m_aspectRatio;
    unsigned long m_volume;
};

} // namespace WebCore

#endif // MediaStreamSourceStates_h

#endif
