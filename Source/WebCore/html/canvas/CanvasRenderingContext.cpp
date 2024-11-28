/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
#include "CanvasRenderingContext.h"

#include "CachedImage.h"
#include "CanvasPattern.h"
#include "DestinationColorSpace.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "OriginAccessPatterns.h"
#include "PixelFormat.h"
#include "SVGImageElement.h"
#include "SecurityOrigin.h"
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

#if USE(SKIA)
#include "CanvasRenderingContext2DBase.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CanvasRenderingContext);

Lock CanvasRenderingContext::s_instancesLock;

HashSet<CanvasRenderingContext*>& CanvasRenderingContext::instances()
{
    static NeverDestroyed<HashSet<CanvasRenderingContext*>> instances;
    return instances;
}

Lock& CanvasRenderingContext::instancesLock()
{
    return s_instancesLock;
}

CanvasRenderingContext::CanvasRenderingContext(CanvasBase& canvas, Type type)
    : m_canvas(canvas)
    , m_type(type)
{
    Locker locker { instancesLock() };
    instances().add(this);
}

CanvasRenderingContext::~CanvasRenderingContext()
{
    Locker locker { instancesLock() };
    ASSERT(instances().contains(this));
    instances().remove(this);
}

void CanvasRenderingContext::ref() const
{
    m_canvas.refCanvasBase();
}

void CanvasRenderingContext::deref() const
{
    m_canvas.derefCanvasBase();
}

RefPtr<ImageBuffer> CanvasRenderingContext::surfaceBufferToImageBuffer(SurfaceBuffer)
{
    // This will be removed once all contexts store their own buffers.
    return canvasBase().buffer();
}

bool CanvasRenderingContext::isSurfaceBufferTransparentBlack(SurfaceBuffer) const
{
    return false;
}

bool CanvasRenderingContext::delegatesDisplay() const
{
#if USE(SKIA)
    if (auto* context2D = dynamicDowncast<CanvasRenderingContext2DBase>(*this))
        return context2D->isAccelerated();
#endif
    return isPlaceholder() || isGPUBased();
}

RefPtr<GraphicsLayerContentsDisplayDelegate> CanvasRenderingContext::layerContentsDisplayDelegate()
{
    return nullptr;
}

void CanvasRenderingContext::setContentsToLayer(GraphicsLayer& layer)
{
    layer.setContentsDisplayDelegate(layerContentsDisplayDelegate(), GraphicsLayer::ContentsLayerPurpose::Canvas);
}

RefPtr<ImageBuffer> CanvasRenderingContext::transferToImageBuffer()
{
    ASSERT_NOT_REACHED(); // Implemented and called only for offscreen capable contexts.
    return nullptr;
}

ImageBufferPixelFormat CanvasRenderingContext::pixelFormat() const
{
    return ImageBufferPixelFormat::BGRA8;
}

DestinationColorSpace CanvasRenderingContext::colorSpace() const
{
    return DestinationColorSpace::SRGB();
}

bool CanvasRenderingContext::willReadFrequently() const
{
    return false;
}

bool CanvasRenderingContext::taintsOrigin(const CanvasPattern* pattern)
{
    return pattern && !pattern->originClean();
}

bool CanvasRenderingContext::taintsOrigin(const CanvasBase* sourceCanvas)
{
    return sourceCanvas && !sourceCanvas->originClean();
}

bool CanvasRenderingContext::taintsOrigin(const CachedImage* cachedImage)
{
    if (!cachedImage)
        return false;

    RefPtr image = cachedImage->image();
    if (!image)
        return false;

    if (image->sourceURL().protocolIsData())
        return false;

    if (image->renderingTaintsOrigin())
        return true;

    if (cachedImage->isCORSCrossOrigin())
        return true;

    ASSERT(m_canvas.securityOrigin());
    ASSERT(cachedImage->origin());
    ASSERT(m_canvas.securityOrigin()->toString() == cachedImage->origin()->toString());
    return false;
}

bool CanvasRenderingContext::taintsOrigin(const HTMLImageElement* element)
{
    return element && taintsOrigin(element->cachedImage());
}

bool CanvasRenderingContext::taintsOrigin(const SVGImageElement* element)
{
    return element && taintsOrigin(element->cachedImage());
}

bool CanvasRenderingContext::taintsOrigin(const HTMLVideoElement* video)
{
#if ENABLE(VIDEO)
    return video && video->taintsOrigin(*m_canvas.securityOrigin());
#else
    UNUSED_PARAM(video);
    return false;
#endif
}

bool CanvasRenderingContext::taintsOrigin(const ImageBitmap* imageBitmap)
{
    return imageBitmap && !imageBitmap->originClean();
}

bool CanvasRenderingContext::taintsOrigin(const URL& url)
{
    return !url.protocolIsData() && !m_canvas.securityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton());
}

void CanvasRenderingContext::checkOrigin(const URL& url)
{
    if (m_canvas.originClean() && taintsOrigin(url))
        m_canvas.setOriginTainted();
}

void CanvasRenderingContext::checkOrigin(const CSSStyleImageValue&)
{
    m_canvas.setOriginTainted();
}

} // namespace WebCore
