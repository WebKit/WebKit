/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/Element.h>

namespace WebCore {

inline bool Element::hasTagName(const MathMLQualifiedName& tagName) const
{
    return ContainerNode::hasTagName(tagName);
}

inline bool Element::isInActiveChain() const
{
    return isUserActionElement() && isUserActionElementInActiveChain();
}

inline bool Element::active() const
{
    return isUserActionElement() && isUserActionElementActive();
}

inline bool Element::hovered() const
{
    return isUserActionElement() && isUserActionElementHovered();
}

inline bool Element::focused() const
{
    return isUserActionElement() && isUserActionElementFocused();
}

inline bool Element::isBeingDragged() const
{
    return isUserActionElement() && isUserActionElementDragged();
}

inline bool Element::hasFocusVisible() const
{
    return isUserActionElement() && isUserActionElementHasFocusVisible();
}

inline bool Element::hasFocusWithin() const
{
    return isUserActionElement() && isUserActionElementHasFocusWithin();
}

inline unsigned Element::childIndex() const
{
    return hasRareData() ? rareDataChildIndex() : 0;
}

inline void Element::setSavedLayerScrollPosition(const ScrollPosition& position)
{
    if (position.isZero() && !hasRareData())
        return;
    setSavedLayerScrollPositionSlow(position);
}

inline void Element::clearBeforePseudoElement()
{
    if (hasRareData())
        clearBeforePseudoElementSlow();
}

inline void Element::clearAfterPseudoElement()
{
    if (hasRareData())
        clearAfterPseudoElementSlow();
}

inline void Element::disconnectFromIntersectionObservers()
{
    auto* observerData = intersectionObserverDataIfExists();
    if (!observerData) [[likely]]
        return;
    disconnectFromIntersectionObserversSlow(*observerData);
}

inline void Element::disconnectFromResizeObservers()
{
    auto* observerData = resizeObserverDataIfExists();
    if (!observerData) [[likely]]
        return;
    disconnectFromResizeObserversSlow(*observerData);
}

} // namespace WebCore
