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

#include "FullscreenManager.h"
#include "RenderBlockFlow.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderTreeBuilder.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderFullScreen);

RenderFullScreenPlaceholder::RenderFullScreenPlaceholder(Document& document, RenderStyle&& style)
    : RenderBlockFlow(document, WTFMove(style))
{
}

bool RenderFullScreenPlaceholder::isRenderFullScreenPlaceholder() const
{
    return true;
}

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderFullScreenPlaceholder);

RenderFullScreen::RenderFullScreen(Document& document, RenderStyle&& style)
    : RenderFlexibleBox(document, WTFMove(style))
{
    setReplaced(false); 
}

RenderFullScreen::~RenderFullScreen()
{
    ASSERT(!m_placeholder);
}

static RenderStyle createFullScreenStyle()
{
    auto fullscreenStyle = RenderStyle::create();

    // Create a stacking context:
    fullscreenStyle.setUsedZIndex(INT_MAX);

    fullscreenStyle.setFontDescription({ });
    fullscreenStyle.fontCascade().update(nullptr);

    fullscreenStyle.setDisplay(DisplayType::Flex);
    fullscreenStyle.setJustifyContentPosition(ContentPosition::Center);
    fullscreenStyle.setAlignItemsPosition(ItemPosition::Center);
    fullscreenStyle.setFlexDirection(FlexDirection::Column);
    
    fullscreenStyle.setPosition(PositionType::Fixed);
    fullscreenStyle.setWidth(Length(100.0, LengthType::Percent));
    fullscreenStyle.setHeight(Length(100.0, LengthType::Percent));
    fullscreenStyle.setLeft(Length(0, LengthType::Fixed));
    fullscreenStyle.setTop(Length(0, LengthType::Fixed));
    
    fullscreenStyle.setBackgroundColor(Color::black);

    return fullscreenStyle;
}

RenderPtr<RenderFullScreen> RenderFullScreen::wrapNewRenderer(RenderTreeBuilder& builder, RenderPtr<RenderElement> renderer, RenderElement& parent, Document& document)
{
    auto newFullscreenRenderer = createRenderer<RenderFullScreen>(document, createFullScreenStyle());
    newFullscreenRenderer->initializeStyle();

    auto& fullscreenRenderer = *newFullscreenRenderer;
    if (!parent.isChildAllowed(fullscreenRenderer, fullscreenRenderer.style()))
        return nullptr;

    builder.attach(fullscreenRenderer, WTFMove(renderer));
    fullscreenRenderer.setNeedsLayoutAndPrefWidthsRecalc();

    document.fullscreenManager().setFullscreenRenderer(builder, fullscreenRenderer);

    return newFullscreenRenderer;
}

void RenderFullScreen::wrapExistingRenderer(RenderElement& renderer, Document& document)
{
    // FIXME: This should be done by RenderTreeUpdater.
    RenderTreeBuilder builder(*document.renderView());

    auto newFullscreenRenderer = createRenderer<RenderFullScreen>(document, createFullScreenStyle());
    newFullscreenRenderer->initializeStyle();

    auto& fullscreenRenderer = *newFullscreenRenderer;
    auto& parent = *renderer.parent();
    if (!parent.isChildAllowed(fullscreenRenderer, fullscreenRenderer.style()))
        return;

    RenderBlock* containingBlock = renderer.containingBlock();
    ASSERT(containingBlock);
    // Since we are moving the |object| to a new parent |fullscreenRenderer|,
    // the line box tree underneath our |containingBlock| is not longer valid.
    containingBlock->deleteLines();

    builder.attach(parent, WTFMove(newFullscreenRenderer), &renderer);

    auto toMove = builder.detach(parent, renderer);

    // Always just do a full layout to ensure that line boxes get deleted properly.
    // Because objects moved from |parent| to |fullscreenRenderer|, we want to
    // make new line boxes instead of leaving the old ones around.
    parent.setNeedsLayoutAndPrefWidthsRecalc();
    containingBlock->setNeedsLayoutAndPrefWidthsRecalc();

    builder.attach(fullscreenRenderer, WTFMove(toMove));
    fullscreenRenderer.setNeedsLayoutAndPrefWidthsRecalc();

    document.fullscreenManager().setFullscreenRenderer(builder, fullscreenRenderer);
}

void RenderFullScreen::unwrapRenderer(bool& requiresRenderTreeRebuild)
{
    RenderTreeBuilder builder(view());

    requiresRenderTreeRebuild = false;
    if (parent()) {
        auto* child = firstChild();
        // Things can get very complicated with anonymous block generation.
        // We can restore correctly without rebuild in simple cases only.
        // FIXME: We should have a mechanism for removing a block without reconstructing the tree.
        if (child != lastChild())
            requiresRenderTreeRebuild = true;
        else if (child && child->isAnonymousBlock()) {
            auto& anonymousBlock = downcast<RenderBlock>(*child);
            if (anonymousBlock.firstChild() != anonymousBlock.lastChild())
                requiresRenderTreeRebuild = true;
        }

        while ((child = firstChild())) {
            if (child->isAnonymousBlock() && !requiresRenderTreeRebuild) {
                if (auto* nonAnonymousChild = downcast<RenderBlock>(*child).firstChild())
                    child = nonAnonymousChild;
                else {
                    builder.destroy(*child);
                    continue;
                }
            }
            // We have to clear the override size, because as a flexbox, we
            // may have set one on the child, and we don't want to leave that
            // lying around on the child.
            if (is<RenderBox>(*child))
                downcast<RenderBox>(*child).clearOverridingContentSize();
            auto childToMove = builder.detach(*child->parent(), *child);
            builder.attach(*parent(), WTFMove(childToMove), this);
            parent()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    }
    if (placeholder())
        builder.destroy(*placeholder());
    ASSERT(!placeholder());

    builder.destroy(*this);
}

}

#endif
