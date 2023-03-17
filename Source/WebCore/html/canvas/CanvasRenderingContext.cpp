/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "PixelFormat.h"
#include "SVGImageElement.h"
#include "SecurityOrigin.h"
#include <wtf/HashSet.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CanvasRenderingContext);

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

CanvasRenderingContext::CanvasRenderingContext(CanvasBase& canvas)
    : m_canvas(canvas)
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

void CanvasRenderingContext::ref()
{
    m_canvas.refCanvasBase();
}

void CanvasRenderingContext::deref()
{
    m_canvas.derefCanvasBase();
}

RefPtr<GraphicsLayerContentsDisplayDelegate> CanvasRenderingContext::layerContentsDisplayDelegate()
{
    return nullptr;
}

void CanvasRenderingContext::setContentsToLayer(GraphicsLayer& layer)
{
    layer.setContentsDisplayDelegate(layerContentsDisplayDelegate(), GraphicsLayer::ContentsLayerPurpose::Canvas);
}

PixelFormat CanvasRenderingContext::pixelFormat() const
{
    return PixelFormat::BGRA8;
}

DestinationColorSpace CanvasRenderingContext::colorSpace() const
{
    return DestinationColorSpace::SRGB();
}

bool CanvasRenderingContext::taintsOrigin(const CanvasPattern* pattern)
{
    if (m_canvas.originClean() && pattern && !pattern->originClean())
        return true;
    return false;
}

bool CanvasRenderingContext::taintsOrigin(const CanvasBase* sourceCanvas)
{
    if (m_canvas.originClean() && sourceCanvas && !sourceCanvas->originClean())
        return true;
    return false;
}

bool CanvasRenderingContext::taintsOrigin(const CachedImage* cachedImage)
{
    if (!m_canvas.originClean() || !cachedImage)
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
    return m_canvas.originClean() && element && !element->originClean(*m_canvas.securityOrigin());
}

bool CanvasRenderingContext::taintsOrigin(const SVGImageElement* element)
{
    if (!element)
        return false;
    return taintsOrigin(element->cachedImage());
}

bool CanvasRenderingContext::taintsOrigin(const HTMLVideoElement* video)
{
#if ENABLE(VIDEO)
    if (!video || !m_canvas.originClean())
        return false;

    return video->taintsOrigin(*m_canvas.securityOrigin());
#else
    UNUSED_PARAM(video);
    return false;
#endif
}

bool CanvasRenderingContext::taintsOrigin(const ImageBitmap* imageBitmap)
{
    if (!imageBitmap || !m_canvas.originClean())
        return false;

    return !imageBitmap->originClean();
}

bool CanvasRenderingContext::taintsOrigin(const URL& url)
{
    if (!m_canvas.originClean())
        return false;

    if (url.protocolIsData())
        return false;

    return !m_canvas.securityOrigin()->canRequest(url);
}

void CanvasRenderingContext::checkOrigin(const URL& url)
{
    if (taintsOrigin(url))
        m_canvas.setOriginTainted();
}

void CanvasRenderingContext::checkOrigin(const CSSStyleImageValue&)
{
    m_canvas.setOriginTainted();
}

} // namespace WebCore
