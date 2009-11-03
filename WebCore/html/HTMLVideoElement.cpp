/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(VIDEO)
#include "HTMLVideoElement.h"

#include "ChromeClient.h"
#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "Page.h"
#include "RenderImage.h"
#include "RenderVideo.h"

namespace WebCore {

using namespace HTMLNames;

HTMLVideoElement::HTMLVideoElement(const QualifiedName& tagName, Document* doc)
    : HTMLMediaElement(tagName, doc)
    , m_shouldShowPosterImage(false)
{
    ASSERT(hasTagName(videoTag));
}
    
bool HTMLVideoElement::rendererIsNeeded(RenderStyle* style) 
{
    return HTMLElement::rendererIsNeeded(style); 
}

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
RenderObject* HTMLVideoElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (m_shouldShowPosterImage)
        return new (arena) RenderImage(this);
    return new (arena) RenderVideo(this);
}
#endif

void HTMLVideoElement::attach()
{
    HTMLMediaElement::attach();

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    if (m_shouldShowPosterImage) {
        if (!m_imageLoader)
            m_imageLoader.set(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        if (renderer() && renderer()->isImage()) {
            RenderImage* imageRenderer = toRenderImage(renderer());
            imageRenderer->setCachedImage(m_imageLoader->image()); 
        }
    }
#endif
}

void HTMLVideoElement::detach()
{
    HTMLMediaElement::detach();
    
    if (!m_shouldShowPosterImage)
        if (m_imageLoader)
            m_imageLoader.clear();
}

void HTMLVideoElement::parseMappedAttribute(MappedAttribute* attr)
{
    const QualifiedName& attrName = attr->name();

    if (attrName == posterAttr) {
        updatePosterImage();
        if (m_shouldShowPosterImage) {
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
            if (!m_imageLoader)
                m_imageLoader.set(new HTMLImageLoader(this));
            m_imageLoader->updateFromElementIgnoringPreviousError();
#else
            if (m_player)
                m_player->setPoster(poster());
#endif
        }
    } else if (attrName == widthAttr)
        addCSSLength(attr, CSSPropertyWidth, attr->value());
    else if (attrName == heightAttr)
        addCSSLength(attr, CSSPropertyHeight, attr->value());
    else
        HTMLMediaElement::parseMappedAttribute(attr);
}

bool HTMLVideoElement::supportsFullscreen() const
{
    Page* page = document() ? document()->page() : 0;
    if (!page) 
        return false;

    if (!m_player || !m_player->supportsFullscreen())
        return false;
    
    return page->chrome()->client()->supportsFullscreenForNode(this);
}

unsigned HTMLVideoElement::videoWidth() const
{
    if (!m_player)
        return 0;
    return m_player->naturalSize().width();
}

unsigned HTMLVideoElement::videoHeight() const
{
    if (!m_player)
        return 0;
    return m_player->naturalSize().height();
}

unsigned HTMLVideoElement::width() const
{
    bool ok;
    unsigned w = getAttribute(widthAttr).string().toUInt(&ok);
    return ok ? w : 0;
}

void HTMLVideoElement::setWidth(unsigned value)
{
    setAttribute(widthAttr, String::number(value));
}
    
unsigned HTMLVideoElement::height() const
{
    bool ok;
    unsigned h = getAttribute(heightAttr).string().toUInt(&ok);
    return ok ? h : 0;
}
    
void HTMLVideoElement::setHeight(unsigned value)
{
    setAttribute(heightAttr, String::number(value));
}

KURL HTMLVideoElement::poster() const
{
    return document()->completeURL(getAttribute(posterAttr));
}

void HTMLVideoElement::setPoster(const String& value)
{
    setAttribute(posterAttr, value);
}

bool HTMLVideoElement::isURLAttribute(Attribute* attr) const
{
    return attr->name() == posterAttr;
}

const QualifiedName& HTMLVideoElement::imageSourceAttributeName() const
{
    return posterAttr;
}

void HTMLVideoElement::updatePosterImage()
{
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    bool oldShouldShowPosterImage = m_shouldShowPosterImage;
#endif

    m_shouldShowPosterImage = !poster().isEmpty() && readyState() < HAVE_CURRENT_DATA;

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    if (attached() && oldShouldShowPosterImage != m_shouldShowPosterImage) {
        detach();
        attach();
    }
#endif
}

void HTMLVideoElement::paint(GraphicsContext* context, const IntRect& destRect)
{
    // FIXME: We should also be able to paint the poster image.

    MediaPlayer* player = HTMLMediaElement::player();
    if (!player)
        return;

    player->setVisible(true); // Make player visible or it won't draw.
    player->paint(context, destRect);
}

void HTMLVideoElement::paintCurrentFrameInContext(GraphicsContext* context, const IntRect& destRect)
{
    // FIXME: We should also be able to paint the poster image.
    
    MediaPlayer* player = HTMLMediaElement::player();
    if (!player)
        return;
    
    player->setVisible(true); // Make player visible or it won't draw.
    player->paintCurrentFrameInContext(context, destRect);
}

}
#endif
