/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "OffscreenCanvas.h"
#include <wtf/URL.h>
#include "SecurityOrigin.h"

namespace WebCore {

CanvasRenderingContext::CanvasRenderingContext(CanvasBase& canvas)
    : m_canvas(canvas)
{
}

void CanvasRenderingContext::ref()
{
    m_canvas.refCanvasBase();
}

void CanvasRenderingContext::deref()
{
    m_canvas.derefCanvasBase();
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

    auto image = makeRefPtr(cachedImage->image());
    if (!image)
        return false;

    if (image->sourceURL().protocolIsData())
        return false;
    
    if (!image->hasSingleSecurityOrigin())
        return true;

    if (!cachedImage->isCORSSameOrigin())
        return true;

    ASSERT(m_canvas.securityOrigin());
    ASSERT(cachedImage->origin());
    ASSERT(m_canvas.securityOrigin()->toString() == cachedImage->origin()->toString());
    return false;
}

bool CanvasRenderingContext::wouldTaintOrigin(const HTMLVideoElement* video)
{
#if ENABLE(VIDEO)
    // FIXME: This check is likely wrong when a redirect is involved. We need
    // to test the finalURL. Please be careful when fixing this issue not to
    // make currentSrc be the final URL because then the
    // HTMLMediaElement.currentSrc DOM API would leak redirect destinations!
    if (!video || !m_canvas.originClean())
        return false;

    if (!video->hasSingleSecurityOrigin())
        return true;

    if (!(video->player() && video->player()->didPassCORSAccessCheck()) && video->wouldTaintOrigin(*m_canvas.securityOrigin()))
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

void CanvasRenderingContext::checkOrigin(const TypedOMCSSImageValue&)
{
    m_canvas.setOriginTainted();
}

} // namespace WebCore
