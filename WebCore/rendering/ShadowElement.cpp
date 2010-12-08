/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ShadowElement.h"

#include "HTMLNames.h"
#include "RenderTheme.h"
#include "RenderView.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<ShadowBlockElement> ShadowBlockElement::create(HTMLElement* shadowParent)
{
    return adoptRef(new ShadowBlockElement(shadowParent));
}

ShadowBlockElement::ShadowBlockElement(HTMLElement* shadowParent)
    : ShadowElement<HTMLDivElement>(divTag, shadowParent)
{
}

void ShadowBlockElement::layoutAsPart(const IntRect& partRect)
{
    RenderBox* parentRenderer = toRenderBox(renderer()->parent());
    RenderBox* selfRenderer = toRenderBox(renderer());
    IntRect oldRect = selfRenderer->frameRect();

    LayoutStateMaintainer statePusher(parentRenderer->view(), parentRenderer, parentRenderer->size(), parentRenderer->style()->isFlippedBlocksWritingMode());

    if (oldRect.size() != partRect.size())
        selfRenderer->setChildNeedsLayout(true, false);

    selfRenderer->layoutIfNeeded();
    selfRenderer->setFrameRect(partRect);

    if (selfRenderer->checkForRepaintDuringLayout())
        selfRenderer->repaintDuringLayoutIfMoved(oldRect);
        
    statePusher.pop();
    parentRenderer->addOverflowFromChild(selfRenderer);
}

void ShadowBlockElement::updateStyleForPart(PseudoId pseudoId)
{
    if (renderer()->style()->styleType() != pseudoId)
        renderer()->setStyle(createStyleForPart(renderer()->parent(), pseudoId));
}

PassRefPtr<ShadowBlockElement> ShadowBlockElement::createForPart(HTMLElement* shadowParent, PseudoId pseudoId)
{
    RefPtr<ShadowBlockElement> part = create(shadowParent);
    part->initAsPart(pseudoId);
    return part.release();
}

void ShadowBlockElement::initAsPart(PseudoId pseudoId)
{
    RenderObject* parentRenderer = shadowParent()->renderer();
    RefPtr<RenderStyle> styleForPart = createStyleForPart(parentRenderer, pseudoId);
    setRenderer(createRenderer(parentRenderer->renderArena(), styleForPart.get()));
    renderer()->setStyle(styleForPart.release());
    setAttached();
    setInDocument();
}

PassRefPtr<RenderStyle> ShadowBlockElement::createStyleForPart(RenderObject* parentRenderer, PseudoId pseudoId)
{
    RefPtr<RenderStyle> styleForPart;
    RenderStyle* pseudoStyle = parentRenderer->getCachedPseudoStyle(pseudoId);
    if (pseudoStyle)
        styleForPart = RenderStyle::clone(pseudoStyle);
    else
        styleForPart = RenderStyle::create();
    
    styleForPart->inheritFrom(parentRenderer->style());
    styleForPart->setDisplay(BLOCK);
    styleForPart->setAppearance(NoControlPart);
    return styleForPart.release();
}

bool ShadowBlockElement::partShouldHaveStyle(const RenderObject* parentRenderer, PseudoId pseudoId)
{
    // We have some -webkit-appearance values for default styles of parts and
    // that appearance get turned off during RenderStyle creation 
    // if they have background styles specified.
    // So !hasAppearance() implies that there are something to be styled.
    RenderStyle* pseudoStyle = parentRenderer->getCachedPseudoStyle(pseudoId);
    return !(pseudoStyle && pseudoStyle->hasAppearance());
}

PassRefPtr<ShadowInputElement> ShadowInputElement::create(HTMLElement* shadowParent)
{
    return adoptRef(new ShadowInputElement(shadowParent));
}

ShadowInputElement::ShadowInputElement(HTMLElement* shadowParent)
    : ShadowElement<HTMLInputElement>(inputTag, shadowParent)
{
}

} // namespace WebCore
