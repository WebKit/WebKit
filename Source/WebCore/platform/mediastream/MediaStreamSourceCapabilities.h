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

#ifndef MediaStreamSourceCapabilities_h
#define MediaStreamSourceCapabilities_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamSourceStates.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class MediaStreamSourceCapabilityRange {
public:
    
    MediaStreamSourceCapabilityRange(float min, float max, bool supported = true)
        : m_type(Float)
    {
        m_min.asFloat = min;
        m_max.asFloat = max;
        m_supported = supported;
    }
    
    MediaStreamSourceCapabilityRange(unsigned long min, unsigned long max, bool supported = true)
        : m_type(ULong)
    {
        m_min.asULong = min;
        m_max.asULong = max;
        m_supported = supported;
    }
    
    MediaStreamSourceCapabilityRange()
    {
        m_type = Undefined;
        m_min.asULong = 0;
        m_max.asULong = 0;
        m_supported = false;
    }
    
    enum Type { Undefined, Float, ULong };
    
    union ValueUnion {
        unsigned long asULong;
        float asFloat;
    };
    
    const ValueUnion& min() const { return m_min; }
    const ValueUnion& max() const { return m_max; }
    Type type() const { return m_type; }
    bool supported() const { return m_supported; }
    
private:
    ValueUnion m_min;
    ValueUnion m_max;
    Type m_type;
    bool m_supported;
};

class MediaStreamSourceCapabilities : public RefCounted<MediaStreamSourceCapabilities> {
public:
    static PassRefPtr<MediaStreamSourceCapabilities> create()
    {
        return adoptRef(new MediaStreamSourceCapabilities());
    }

    ~MediaStreamSourceCapabilities() { }

    const Vector<MediaStreamSourceStates::SourceType>& sourceTypes() { return m_sourceType; }
    void setSourceType(MediaStreamSourceStates::SourceType sourceType) { m_sourceType.resizeToFit(1); addSourceType(sourceType); }
    void addSourceType(MediaStreamSourceStates::SourceType sourceType)
    {
        if (sourceType == MediaStreamSourceStates::Camera)
            m_videoSource = true;
        m_sourceType.append(sourceType);
    }

    const Vector<AtomicString>& sourceId() { return m_sourceId; }
    void setSourceId(const AtomicString& id)  { m_sourceId.reserveCapacity(1); m_sourceId.insert(0, id); }

    const Vector<MediaStreamSourceStates::VideoFacingMode>& facingModes() { return m_facingMode; }
    void addFacingMode(MediaStreamSourceStates::VideoFacingMode mode) { m_facingMode.append(mode); }

    const MediaStreamSourceCapabilityRange& width() { return m_width; }
    void setWidthRange(const MediaStreamSourceCapabilityRange& width) { m_width = width; }

    const MediaStreamSourceCapabilityRange& height() { return m_height; }
    void setHeightRange(const MediaStreamSourceCapabilityRange& height) { m_height = height; }

    const MediaStreamSourceCapabilityRange& frameRate() { return m_frameRate; }
    void setFrameRateRange(const MediaStreamSourceCapabilityRange& frameRate) { m_frameRate = frameRate; }

    const MediaStreamSourceCapabilityRange& aspectRatio() { return m_aspectRatio; }
    void setAspectRatioRange(const MediaStreamSourceCapabilityRange& aspectRatio) { m_aspectRatio = aspectRatio; }

    const MediaStreamSourceCapabilityRange& volume() { return m_volume; }
    void setVolumeRange(const MediaStreamSourceCapabilityRange& volume) { m_volume = volume; }

    bool hasVideoSource() { return m_videoSource; }
    void setHasVideoSource(bool isVideo) { m_videoSource = isVideo;; }

private:
    MediaStreamSourceCapabilities()
        : m_videoSource(false)
    {
    }

    Vector<AtomicString> m_sourceId;
    Vector<MediaStreamSourceStates::SourceType> m_sourceType;
    Vector<MediaStreamSourceStates::VideoFacingMode> m_facingMode;

    MediaStreamSourceCapabilityRange m_width;
    MediaStreamSourceCapabilityRange m_height;
    MediaStreamSourceCapabilityRange m_frameRate;
    MediaStreamSourceCapabilityRange m_aspectRatio;
    MediaStreamSourceCapabilityRange m_volume;

    bool m_videoSource;
};

} // namespace WebCore

#endif // MediaStreamSourceCapabilities_h

#endif
