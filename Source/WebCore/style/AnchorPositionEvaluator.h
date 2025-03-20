/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSValueKeywords.h"
#include "EventTarget.h"
#include "LayoutUnit.h"
#include "PositionTryOrder.h"
#include "ScopedName.h"
#include "WritingMode.h"
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class Document;
class Element;
class LayoutRect;
class RenderBlock;
class RenderBox;
class RenderBoxModelObject;
class RenderStyle;

enum CSSPropertyID : uint16_t;

namespace Style {

class BuilderState;
struct BuilderPositionTryFallback;

enum class AnchorPositionResolutionStage : uint8_t {
    FindAnchors,
    ResolveAnchorFunctions,
    Resolved,
    Positioned,
};

using AnchorElements = HashMap<AtomString, WeakPtr<Element, WeakPtrImplWithEventTargetData>>;

struct AnchorPositionedState {
    WTF_MAKE_TZONE_ALLOCATED(AnchorPositionedState);
public:
    AnchorElements anchorElements;
    UncheckedKeyHashSet<AtomString> anchorNames;
    AnchorPositionResolutionStage stage;
};

using AnchorsForAnchorName = HashMap<AtomString, Vector<SingleThreadWeakRef<const RenderBoxModelObject>>>;

// https://drafts.csswg.org/css-anchor-position-1/#typedef-anchor-size
enum class AnchorSizeDimension : uint8_t {
    Width,
    Height,
    Block,
    Inline,
    SelfBlock,
    SelfInline
};

using AnchorPositionedStates = WeakHashMap<Element, std::unique_ptr<AnchorPositionedState>, WeakPtrImplWithEventTargetData>;

using AnchorPositionedToAnchorMap = WeakHashMap<Element, Vector<SingleThreadWeakPtr<RenderBoxModelObject>>, WeakPtrImplWithEventTargetData>;
using AnchorToAnchorPositionedMap = SingleThreadWeakHashMap<const RenderBoxModelObject, Vector<Ref<Element>>>;

class AnchorPositionEvaluator {
public:
    // Find the anchor element indicated by `elementName` and update the associated anchor resolution data.
    // Returns nullptr if the anchor element can't be found.
    static RefPtr<Element> findAnchorForAnchorFunctionAndAttemptResolution(BuilderState&, std::optional<ScopedName> elementName);

    using Side = std::variant<CSSValueID, double>;
    static bool propertyAllowsAnchorFunction(CSSPropertyID);
    static std::optional<double> evaluate(BuilderState&, std::optional<ScopedName> elementName, Side);

    static bool propertyAllowsAnchorSizeFunction(CSSPropertyID);
    static std::optional<double> evaluateSize(BuilderState&, std::optional<ScopedName> elementName, std::optional<AnchorSizeDimension>);

    static void updateAnchorPositioningStatesAfterInterleavedLayout(Document&, AnchorPositionedStates&);
    static void updateSnapshottedScrollOffsets(Document&);
    static void updateAnchorPositionedStateForLayoutTimePositioned(Element&, const RenderStyle&, AnchorPositionedStates&);

    static LayoutRect computeAnchorRectRelativeToContainingBlock(CheckedRef<const RenderBoxModelObject> anchorBox, const RenderBlock& containingBlock);

    static AnchorToAnchorPositionedMap makeAnchorPositionedForAnchorMap(AnchorPositionedToAnchorMap&);

    static bool isLayoutTimeAnchorPositioned(const RenderStyle&);
    static CSSPropertyID resolvePositionTryFallbackProperty(CSSPropertyID, WritingMode, const BuilderPositionTryFallback&);

    static bool overflowsInsetModifiedContainingBlock(const RenderBox& anchoredBox);

private:
    static AnchorElements findAnchorsForAnchorPositionedElement(const Element&, const UncheckedKeyHashSet<AtomString>& anchorNames, const AnchorsForAnchorName&);
};

} // namespace Style

} // namespace WebCore
