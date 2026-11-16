/*
 * Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayoutElementBox.h"

#include "RenderElement.h"
#include "RenderListOutsideMarker.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Layout {

ElementBox::ElementBox(Style::ComputedStyle&& style, ElementAttributes elementAttributes, EnumSet<BaseTypeFlag> baseTypeFlags)
    : Box(WTF::move(style), elementAttributes, baseTypeFlags | ElementBoxFlag)
{
}

ElementBox::~ElementBox() = default;

const Box* ElementBox::firstInFlowChild() const
{
    if (auto* firstChild = this->firstChild()) {
        if (firstChild->isInFlow())
            return firstChild;
        return firstChild->nextInFlowSibling();
    }
    return nullptr;
}

const Box* ElementBox::firstInFlowOrFloatingChild() const
{
    if (auto* firstChild = this->firstChild()) {
        if (firstChild->isInFlow() || firstChild->isFloatingPositioned())
            return firstChild;
        return firstChild->nextInFlowOrFloatingSibling();
    }
    return nullptr;
}

const Box* ElementBox::firstOutOfFlowChild() const
{
    if (auto* firstChild = this->firstChild()) {
        if (firstChild->isOutOfFlowPositioned())
            return firstChild;
        return firstChild->nextOutOfFlowSibling();
    }
    return nullptr;
}

const Box* ElementBox::lastInFlowChild() const
{
    if (auto* lastChild = this->lastChild()) {
        if (lastChild->isInFlow())
            return lastChild;
        return lastChild->previousInFlowSibling();
    }
    return nullptr;
}

const Box* ElementBox::lastInFlowOrFloatingChild() const
{
    if (auto* lastChild = this->lastChild()) {
        if (lastChild->isInFlow() || lastChild->isFloatingPositioned())
            return lastChild;
        return lastChild->previousInFlowOrFloatingSibling();
    }
    return nullptr;
}

const Box* ElementBox::lastOutOfFlowChild() const
{
    if (auto* lastChild = this->lastChild()) {
        if (lastChild->isOutOfFlowPositioned())
            return lastChild;
        return lastChild->previousOutOfFlowSibling();
    }
    return nullptr;
}

bool ElementBox::hasOutOfFlowChild() const
{
    return !!firstOutOfFlowChild();
}

bool ElementBox::isListMarkerImage() const
{
    CheckedPtr listMarkerRenderer = dynamicDowncast<RenderListOutsideMarker>(rendererForIntegration());
    return listMarkerRenderer && listMarkerRenderer->isImage();
}

std::pair<float, float> ElementBox::layoutBoundsForListMarker() const
{
    CheckedPtr listMarkerRenderer = dynamicDowncast<RenderListOutsideMarker>(rendererForIntegration());
    ASSERT(listMarkerRenderer);
    return listMarkerRenderer->layoutBounds();
}

}
}
