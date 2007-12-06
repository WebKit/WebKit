/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "RenderImage.h"
#include "RenderVideo.h"

namespace WebCore {

using namespace HTMLNames;

HTMLVideoElement::HTMLVideoElement(Document* doc)
    : HTMLMediaElement(HTMLNames::videoTag, doc)
    , m_shouldShowPosterImage(false)
{
}
    
bool HTMLVideoElement::rendererIsNeeded(RenderStyle* style) 
{
    return HTMLElement::rendererIsNeeded(style); 
}

RenderObject* HTMLVideoElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (m_shouldShowPosterImage)
        return new (arena) RenderImage(this);
    return new (arena) RenderVideo(this);
}

void HTMLVideoElement::attach()
{
    HTMLMediaElement::attach();
    
    if (m_shouldShowPosterImage) {
        if (!m_imageLoader)
            m_imageLoader.set(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        if (renderer() && renderer()->isImage()) {
            RenderImage* imageRenderer = static_cast<RenderImage*>(renderer());
            imageRenderer->setCachedImage(m_imageLoader->image()); 
        }
    }

}

void HTMLVideoElement::detach()
{
    HTMLMediaElement::detach();
    
    if (!m_shouldShowPosterImage)
        if (m_imageLoader)
            m_imageLoader.clear();
}

void HTMLVideoElement::parseMappedAttribute(MappedAttribute *attr)
{
    const QualifiedName& attrName = attr->name();

    if (attrName == posterAttr) {
        updatePosterImage();
        if (m_shouldShowPosterImage) {
            if (!m_imageLoader)
                m_imageLoader.set(new HTMLImageLoader(this));
            m_imageLoader->updateFromElement();
        }
    } else if (attrName == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attrName == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else
        HTMLMediaElement::parseMappedAttribute(attr);
}

int HTMLVideoElement::videoWidth() const
{
    if (!m_player)
        return 0;
    return m_player->naturalSize().width();
}

int HTMLVideoElement::videoHeight() const
{
    if (!m_player)
        return 0;
    return m_player->naturalSize().height();
}

int HTMLVideoElement::width() const
{
    bool ok;
    int w = getAttribute(widthAttr).toInt(&ok);
    return ok ? w : 0;
}

void HTMLVideoElement::setWidth(int value)
{
    setAttribute(widthAttr, String::number(value));
}
    
int HTMLVideoElement::height() const
{
    bool ok;
    int h = getAttribute(heightAttr).toInt(&ok);
    return ok ? h : 0;
}
    
void HTMLVideoElement::setHeight(int value)
{
    setAttribute(heightAttr, String::number(value));
}

String HTMLVideoElement::poster() const
{
    return document()->completeURL(getAttribute(posterAttr));
}

void HTMLVideoElement::setPoster(const String& value)
{
    setAttribute(posterAttr, value);
}

bool HTMLVideoElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == posterAttr;
}

const QualifiedName& HTMLVideoElement::imageSourceAttributeName() const
{
    return posterAttr;
}

void HTMLVideoElement::updatePosterImage()
{
    bool oldShouldShowPosterImage = m_shouldShowPosterImage;
    m_shouldShowPosterImage = !poster().isEmpty() && m_networkState < LOADED_FIRST_FRAME;
    if (attached() && oldShouldShowPosterImage != m_shouldShowPosterImage) {
        detach();
        attach();
    }
}

}
#endif
