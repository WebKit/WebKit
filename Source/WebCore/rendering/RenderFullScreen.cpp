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

#include "RenderBlockFlow.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"

namespace WebCore {

class RenderFullScreenPlaceholder final : public RenderBlockFlow {
public:
    RenderFullScreenPlaceholder(RenderFullScreen& owner, PassRef<RenderStyle> style)
        : RenderBlockFlow(owner.document(), WTF::move(style))
        , m_owner(owner) 
    {
    }

private:
    virtual bool isRenderFullScreenPlaceholder() const { return true; }
    virtual void willBeDestroyed();
    RenderFullScreen& m_owner;
};

void RenderFullScreenPlaceholder::willBeDestroyed()
{
    m_owner.setPlaceholder(0);
    RenderBlockFlow::willBeDestroyed();
}

RenderFullScreen::RenderFullScreen(Document& document, PassRef<RenderStyle> style)
    : RenderFlexibleBox(document, WTF::move(style))
    , m_placeholder(0)
{
    setReplaced(false); 
}

void RenderFullScreen::willBeDestroyed()
{
    if (m_placeholder) {
        removeFromParent();
        if (!m_placeholder->beingDestroyed())
            m_placeholder->destroy();
        ASSERT(!m_placeholder);
    }

    // RenderObjects are unretained, so notify the document (which holds a pointer to a RenderFullScreen)
    // if it's RenderFullScreen is destroyed.
    if (document().fullScreenRenderer() == this)
        document().fullScreenRendererDestroyed();

    RenderFlexibleBox::willBeDestroyed();
}

static PassRef<RenderStyle> createFullScreenStyle()
{
    auto fullscreenStyle = RenderStyle::createDefaultStyle();

    // Create a stacking context:
    fullscreenStyle.get().setZIndex(INT_MAX);

    fullscreenStyle.get().setFontDescription(FontDescription());
    fullscreenStyle.get().font().update(0);

    fullscreenStyle.get().setDisplay(FLEX);
    fullscreenStyle.get().setJustifyContent(JustifyCenter);
    fullscreenStyle.get().setAlignItems(AlignCenter);
    fullscreenStyle.get().setFlexDirection(FlowColumn);
    
    fullscreenStyle.get().setPosition(FixedPosition);
    fullscreenStyle.get().setWidth(Length(100.0, Percent));
    fullscreenStyle.get().setHeight(Length(100.0, Percent));
    fullscreenStyle.get().setLeft(Length(0, WebCore::Fixed));
    fullscreenStyle.get().setTop(Length(0, WebCore::Fixed));
    
    fullscreenStyle.get().setBackgroundColor(Color::black);

    return fullscreenStyle;
}

RenderFullScreen* RenderFullScreen::wrapRenderer(RenderObject* object, RenderElement* parent, Document& document)
{
    RenderFullScreen* fullscreenRenderer = new RenderFullScreen(document, createFullScreenStyle());
    fullscreenRenderer->initializeStyle();
    if (parent && !parent->isChildAllowed(*fullscreenRenderer, fullscreenRenderer->style())) {
        fullscreenRenderer->destroy();
        return 0;
    }
    if (object) {
        // |object->parent()| can be null if the object is not yet attached
        // to |parent|.
        if (RenderElement* parent = object->parent()) {
            RenderBlock* containingBlock = object->containingBlock();
            ASSERT(containingBlock);
            // Since we are moving the |object| to a new parent |fullscreenRenderer|,
            // the line box tree underneath our |containingBlock| is not longer valid.
            containingBlock->deleteLines();

            parent->addChild(fullscreenRenderer, object);
            object->removeFromParent();
            
            // Always just do a full layout to ensure that line boxes get deleted properly.
            // Because objects moved from |parent| to |fullscreenRenderer|, we want to
            // make new line boxes instead of leaving the old ones around.
            parent->setNeedsLayoutAndPrefWidthsRecalc();
            containingBlock->setNeedsLayoutAndPrefWidthsRecalc();
        }
        fullscreenRenderer->addChild(object);
        fullscreenRenderer->setNeedsLayoutAndPrefWidthsRecalc();
    }
    document.setFullScreenRenderer(fullscreenRenderer);
    return fullscreenRenderer;
}

void RenderFullScreen::unwrapRenderer(bool& requiresRenderTreeRebuild)
{
    requiresRenderTreeRebuild = false;
    if (parent()) {
        auto* child = firstChild();
        // Things can get very complicated with anonymous block generation.
        // We can restore correctly without rebuild in simple cases only.
        // FIXME: We should have a mechanism for removing a block without reconstructing the tree.
        if (child != lastChild())
            requiresRenderTreeRebuild = true;
        else if (child && child->isAnonymousBlock()) {
            auto& anonymousBlock = toRenderBlock(*child);
            if (anonymousBlock.firstChild() != anonymousBlock.lastChild())
                requiresRenderTreeRebuild = true;
        }

        while ((child = firstChild())) {
            if (child->isAnonymousBlock() && !requiresRenderTreeRebuild) {
                if (auto* nonAnonymousChild = toRenderBlock(*child).firstChild())
                    child = nonAnonymousChild;
                else {
                    child->removeFromParent();
                    child->destroy();
                    continue;
                }
            }
            // We have to clear the override size, because as a flexbox, we
            // may have set one on the child, and we don't want to leave that
            // lying around on the child.
            if (child->isBox())
                toRenderBox(child)->clearOverrideSize();
            child->removeFromParent();
            parent()->addChild(child, this);
            parent()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    }
    if (placeholder())
        placeholder()->removeFromParent();
    removeFromParent();
    document().setFullScreenRenderer(0);
}

void RenderFullScreen::setPlaceholder(RenderBlock* placeholder)
{
    m_placeholder = placeholder;
}

void RenderFullScreen::createPlaceholder(PassRef<RenderStyle> style, const LayoutRect& frameRect)
{
    if (style.get().width().isAuto())
        style.get().setWidth(Length(frameRect.width(), Fixed));
    if (style.get().height().isAuto())
        style.get().setHeight(Length(frameRect.height(), Fixed));

    if (m_placeholder) {
        m_placeholder->setStyle(WTF::move(style));
        return;
    }

    m_placeholder = new RenderFullScreenPlaceholder(*this, WTF::move(style));
    m_placeholder->initializeStyle();
    if (parent()) {
        parent()->addChild(m_placeholder, this);
        parent()->setNeedsLayoutAndPrefWidthsRecalc();
    }
}

}

#endif
