/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(FULLSCREEN_API)

#include "RenderFullScreen.h"

#include "RenderLayer.h"

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#endif

using namespace WebCore;

class RenderFullScreenPlaceholder : public RenderBlock {
public:
    RenderFullScreenPlaceholder(RenderFullScreen* owner) 
        : RenderBlock(owner->document())
        , m_owner(owner) 
    { 
    }
private:
    virtual bool isRenderFullScreenPlaceholder() const { return true; }
    virtual void willBeDestroyed();
    RenderFullScreen* m_owner;
};

void RenderFullScreenPlaceholder::willBeDestroyed()
{
    m_owner->setPlaceholder(0);
    RenderBlock::willBeDestroyed();
}

RenderFullScreen::RenderFullScreen(Node* node) 
    : RenderDeprecatedFlexibleBox(node)
    , m_placeholder(0)
{ 
    setReplaced(false); 
}

void RenderFullScreen::willBeDestroyed()
{
    if (m_placeholder) {
        remove();
        if (!m_placeholder->beingDestroyed())
            m_placeholder->destroy();
        ASSERT(!m_placeholder);
    }

    // RenderObjects are unretained, so notify the document (which holds a pointer to a RenderFullScreen)
    // if it's RenderFullScreen is destroyed.
    if (document() && document()->fullScreenRenderer() == this)
        document()->fullScreenRendererDestroyed();

    RenderDeprecatedFlexibleBox::willBeDestroyed();
}

static PassRefPtr<RenderStyle> createFullScreenStyle()
{
    RefPtr<RenderStyle> fullscreenStyle = RenderStyle::createDefaultStyle();

    // Create a stacking context:
    fullscreenStyle->setZIndex(INT_MAX);

    fullscreenStyle->setFontDescription(FontDescription());
    fullscreenStyle->font().update(0);

    fullscreenStyle->setDisplay(BOX);
    fullscreenStyle->setBoxPack(Center);
    fullscreenStyle->setBoxAlign(BCENTER);
    fullscreenStyle->setBoxOrient(VERTICAL);
    
    fullscreenStyle->setPosition(FixedPosition);
    fullscreenStyle->setWidth(Length(100.0, Percent));
    fullscreenStyle->setHeight(Length(100.0, Percent));
    fullscreenStyle->setLeft(Length(0, WebCore::Fixed));
    fullscreenStyle->setTop(Length(0, WebCore::Fixed));
    
    fullscreenStyle->setBackgroundColor(Color::black);
    
    return fullscreenStyle.release();
}

RenderObject* RenderFullScreen::wrapRenderer(RenderObject* object, Document* document)
{
    RenderFullScreen* fullscreenRenderer = new (document->renderArena()) RenderFullScreen(document);
    fullscreenRenderer->setStyle(createFullScreenStyle());
    if (object) {
        if (RenderObject* parent = object->parent()) {
            parent->addChild(fullscreenRenderer, object);
            object->remove();
            parent->setNeedsLayoutAndPrefWidthsRecalc();
        }
        fullscreenRenderer->addChild(object);
        fullscreenRenderer->setNeedsLayoutAndPrefWidthsRecalc();
    }
    document->setFullScreenRenderer(fullscreenRenderer);
    return fullscreenRenderer;
}

void RenderFullScreen::unwrapRenderer()
{
    if (parent()) {
        RenderObject* child;
        while ((child = firstChild())) {
            child->remove();
            parent()->addChild(child, this);
            parent()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    }
    if (placeholder())
        placeholder()->remove();
    remove();
    document()->setFullScreenRenderer(0);
}

void RenderFullScreen::setPlaceholder(RenderBlock* placeholder)
{
    m_placeholder = placeholder;
}

void RenderFullScreen::createPlaceholder(PassRefPtr<RenderStyle> style, const LayoutRect& frameRect)
{
    if (style->width().isAuto())
        style->setWidth(Length(frameRect.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(frameRect.height(), Fixed));

    if (!m_placeholder) {
        m_placeholder = new (document()->renderArena()) RenderFullScreenPlaceholder(this);
        m_placeholder->setStyle(style);
        if (parent()) {
            parent()->addChild(m_placeholder, this);
            parent()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    } else
        m_placeholder->setStyle(style);
}

#endif
