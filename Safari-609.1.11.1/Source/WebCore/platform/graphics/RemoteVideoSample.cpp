/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "RemoteVideoSample.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)

#include "Logging.h"
#include "MediaSample.h"

#if HAVE(IOSURFACE)
#include "GraphicsContextCG.h"
#endif

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

#if HAVE(IOSURFACE)
std::unique_ptr<RemoteVideoSample> RemoteVideoSample::create(MediaSample&& sample)
{
    ASSERT(sample.platformSample().type == PlatformSample::CMSampleBufferType);

    auto imageBuffer = CMSampleBufferGetImageBuffer(sample.platformSample().sample.cmSampleBuffer);
    if (!imageBuffer) {
        RELEASE_LOG_ERROR(Media, "RemoteVideoSample::create: CMSampleBufferGetImageBuffer returned nullptr");
        return nullptr;
    }

    auto surface = CVPixelBufferGetIOSurface(imageBuffer);
    if (!surface) {
        RELEASE_LOG_ERROR(Media, "RemoteVideoSample::create: CVPixelBufferGetIOSurface returned nullptr");
        return nullptr;
    }

    return std::unique_ptr<RemoteVideoSample>(new RemoteVideoSample(surface, sRGBColorSpaceRef(), sample.presentationTime(), sample.videoRotation(), sample.videoMirrored()));
}

RemoteVideoSample::RemoteVideoSample(IOSurfaceRef surface, CGColorSpaceRef colorSpace, MediaTime&& time, MediaSample::VideoRotation rotation, bool mirrored)
    : m_ioSurface(WebCore::IOSurface::createFromSurface(surface, colorSpace))
    , m_rotation(rotation)
    , m_time(WTFMove(time))
    , m_videoFormat(IOSurfaceGetPixelFormat(surface))
    , m_size(IntSize(IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface)))
    , m_mirrored(mirrored)
{
}

IOSurfaceRef RemoteVideoSample::surface()
{
    if (!m_ioSurface && m_sendRight)
        m_ioSurface = WebCore::IOSurface::createFromSendRight(WTFMove(m_sendRight), sRGBColorSpaceRef());

    return m_ioSurface ? m_ioSurface->surface() : nullptr;
}
#endif

}

#endif // ENABLE(MEDIA_STREAM)
