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

PixelFormat CanvasRenderingContext::pixelFormat() const
{
    return PixelFormat::BGRA8;
}

DestinationColorSpace CanvasRenderingContext::colorSpace() const
{
    return DestinationColorSpace::SRGB();
}

bool CanvasRenderingContext::wouldTaintOrigin(const CanvasPattern* pattern)
{
    if (m_canvas.originClean() && pattern && !pattern->originClean())
        return true;
    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const CanvasBase* sourceCanvas)
{
    if (m_canvas.originClean() && sourceCanvas && !sourceCanvas->originClean())
        return true;
    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLImageElement* element)
{
    if (!element || !m_canvas.originClean())
        return false;

    auto* cachedImage = element->cachedImage();
    if (!cachedImage)
        return false;

    RefPtr image = cachedImage->image();
    if (!image)
        return false;

    if (image->sourceURL().protocolIsData())
        return false;

    // FIXME: SVG image shouldn't taint the canvas if their cross-origin data says not to.
    // https://bugs.webkit.org/show_bug.cgi?id=248437
    if (image->isSVGImage() && !image->hasSingleSecurityOrigin())
        return true;

    if (cachedImage->isCORSCrossOrigin())
        return true;

    ASSERT(m_canvas.securityOrigin());
    ASSERT(cachedImage->origin());
    ASSERT(m_canvas.securityOrigin()->toString() == cachedImage->origin()->toString());
    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLVideoElement* video)
{
#if ENABLE(VIDEO)
    if (!video || !m_canvas.originClean())
        return false;

    // FIXME: video->wouldTainOrigin is incorrectly implemented, it only checks that the origin
    // doesn't change, rather than being based on CORS cross-origin data.
    // This is tracked in https://bugs.webkit.org/show_bug.cgi?id=242889
    if (!video->didPassCORSAccessCheck() && video->wouldTaintOrigin(*m_canvas.securityOrigin()))
        return true;

#else
    UNUSED_PARAM(video);
#endif

    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const ImageBitmap* imageBitmap)
{
    if (!imageBitmap || !m_canvas.originClean())
        return false;

    return !imageBitmap->originClean();
}

bool CanvasRenderingContext::wouldTaintOrigin(const URL& url)
{
    if (!m_canvas.originClean())
        return false;

    if (url.protocolIsData())
        return false;

    return !m_canvas.securityOrigin()->canRequest(url);
}

void CanvasRenderingContext::checkOrigin(const URL& url)
{
    if (wouldTaintOrigin(url))
        m_canvas.setOriginTainted();
}

void CanvasRenderingContext::checkOrigin(const CSSStyleImageValue&)
{
    m_canvas.setOriginTainted();
}

} // namespace WebCore
