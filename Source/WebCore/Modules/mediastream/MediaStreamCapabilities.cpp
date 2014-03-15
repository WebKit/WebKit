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

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamCapabilities.h"

#include "AllAudioCapabilities.h"
#include "AllVideoCapabilities.h"
#include "CapabilityRange.h"
#include "MediaSourceStates.h"
#include "MediaStreamSourceCapabilities.h"

namespace WebCore {

RefPtr<MediaStreamCapabilities> MediaStreamCapabilities::create(PassRefPtr<MediaStreamSourceCapabilities> capabilities)
{
    if (capabilities->hasVideoSource())
        return AllVideoCapabilities::create(capabilities);
    
    return AllAudioCapabilities::create(capabilities);
}

MediaStreamCapabilities::MediaStreamCapabilities(PassRefPtr<MediaStreamSourceCapabilities> capabilities)
    : m_SourceCapabilities(capabilities)
{
}

Vector<String> MediaStreamCapabilities::sourceType() const
{
    ASSERT(m_SourceCapabilities->hasVideoSource());
    
    size_t count = m_SourceCapabilities->sourceTypes().size();
    if (!count)
        return Vector<String>();
    
    const Vector<MediaStreamSourceStates::SourceType>& sourceTypes = m_SourceCapabilities->sourceTypes();
    Vector<String> capabilities;
    capabilities.reserveCapacity(count);
    
    for (size_t i = 0; i < count; ++i)
        capabilities.append(MediaStreamSourceStates::sourceType(sourceTypes[i]));
    
    return capabilities;
}

Vector<String> MediaStreamCapabilities::sourceId() const
{
    size_t count = m_SourceCapabilities->sourceId().size();
    if (!count)
        return Vector<String>();
    
    const Vector<AtomicString>& sourceIds = m_SourceCapabilities->sourceId();
    Vector<String> capabilities;
    capabilities.reserveCapacity(count);
    
    for (size_t i = 0; i < count; ++i)
        capabilities.append(sourceIds[i]);
    
    return capabilities;
}

Vector<String> MediaStreamCapabilities::facingMode() const
{
    ASSERT(m_SourceCapabilities->hasVideoSource());
    
    size_t count = m_SourceCapabilities->facingModes().size();
    if (!count)
        return Vector<String>();
    
    const Vector<MediaStreamSourceStates::VideoFacingMode>& facingModes = m_SourceCapabilities->facingModes();
    Vector<String> capabilities;
    capabilities.reserveCapacity(count);
    
    for (size_t i = 0; i < count; ++i)
        capabilities.append(MediaStreamSourceStates::facingMode(facingModes[i]));
    
    return capabilities;
}

RefPtr<CapabilityRange> MediaStreamCapabilities::width() const
{
    ASSERT(m_SourceCapabilities->hasVideoSource());
    
    return CapabilityRange::create(m_SourceCapabilities->width());
}

RefPtr<CapabilityRange> MediaStreamCapabilities::height() const
{
    ASSERT(m_SourceCapabilities->hasVideoSource());
    
    return CapabilityRange::create(m_SourceCapabilities->height());
}

RefPtr<CapabilityRange> MediaStreamCapabilities::frameRate() const
{
    ASSERT(m_SourceCapabilities->hasVideoSource());
    
    return CapabilityRange::create(m_SourceCapabilities->frameRate());
}

RefPtr<CapabilityRange> MediaStreamCapabilities::aspectRatio() const
{
    ASSERT(m_SourceCapabilities->hasVideoSource());
    
    return CapabilityRange::create(m_SourceCapabilities->aspectRatio());
}

RefPtr<CapabilityRange> MediaStreamCapabilities::volume() const
{
    ASSERT(!m_SourceCapabilities->hasVideoSource());
    
    return CapabilityRange::create(m_SourceCapabilities->volume());
}

} // namespace WebCore

#endif
