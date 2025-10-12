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

#include <WebCore/CSSValueKeywords.h>
#include <WebCore/EventTarget.h>
#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutSize.h>
#include <WebCore/LayoutUnit.h>
#include <WebCore/PositionTryOrder.h>
#include <WebCore/PseudoElementIdentifier.h>
#include <WebCore/ResolvedScopedName.h>
#include <WebCore/ScopedName.h>
#include <WebCore/WritingMode.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class Document;
class Element;
class LayoutPoint;
class LayoutRect;
class LayoutSize;
class RenderBlock;
class RenderBox;
class RenderBoxModelObject;
class RenderElement;
class RenderStyle;
class RenderView;

enum CSSPropertyID : uint16_t;

struct AnchorScrollSnapshot {
    SingleThreadWeakPtr<const RenderBox> m_scroller;
    LayoutPoint m_scrollSnapshot { };
    inline LayoutSize adjustmentForCurrentScrollPosition() const;
    AnchorScrollSnapshot(const RenderBox& scroller, LayoutPoint snapshot);
    AnchorScrollSnapshot(LayoutPoint snapshot);
    bool operator==(const AnchorScrollSnapshot&) const = default;
};

class AnchorScrollAdjuster {
public:
    AnchorScrollAdjuster(RenderBox& anchored, const RenderBoxModelObject& defaultAnchor);
    RenderBox* anchored() const;

    bool mayNeedAdjustment() const { return m_needsXAdjustment | m_needsYAdjustment; }
    inline bool isEmpty() const;
    bool isHidden() const { return m_isHidden; }
    void setHidden(bool hide) { m_isHidden = hide; }

    inline void addSnapshot(const RenderBox& scroller);
    inline void addViewportSnapshot(const RenderView&);

    enum Diff : uint8_t { New, SnapshotsDiffer, SnapshotsMatch };
    bool recaptureDiffers(const AnchorScrollAdjuster&) const; // Snapshot differences can require invalidation.

    void setFallbackLimits(const RenderBox& anchored);
    bool hasFallbackLimits() const { return m_hasFallback; }
    bool exceedsFallbackLimits(LayoutSize adjustment) { return !m_fallbackLimits.fits(adjustment); }

    LayoutSize accumulateAdjustments(const RenderView&, const RenderBox& anchored) const;

    bool invalidateForScroller(const RenderBox& scroller);
private:
    LayoutSize adjustmentForViewport(const RenderView&) const;

    CheckedRef<RenderBox> m_anchored;
    Vector<AnchorScrollSnapshot, 1> m_scrollSnapshots;
    bool m_needsXAdjustment : 1 { false };
    bool m_needsYAdjustment : 1 { false };
    bool m_adjustForViewport : 1 { false };
    bool m_hasChainedAnchor : 1 { false };
    bool m_hasStickyAnchor : 1 { false };
    bool m_isHidden : 1 { false };
    bool m_hasFallback : 1 { false };
    LayoutSize m_stickySnapshot;
    LayoutSizeLimits m_fallbackLimits;
};

namespace Style {

class BuilderState;
struct BuilderPositionTryFallback;

enum class AnchorPositionResolutionStage : uint8_t {
    FindAnchors,
    ResolveAnchorFunctions,
    Resolved,
    Positioned,
};

using AnchorElements = HashMap<ResolvedScopedName, WeakPtr<Element, WeakPtrImplWithEventTargetData>>;

struct AnchorPositionedState {
    AnchorElements anchorElements;
    HashSet<ResolvedScopedName> anchorNames;
    AnchorPositionResolutionStage stage;

    WTF_MAKE_STRUCT_TZONE_ALLOCATED(AnchorPositionedState);
};

using AnchorPositionedKey = std::pair<RefPtr<const Element>, std::optional<PseudoElementIdentifier>>;
using AnchorPositionedStates = HashMap<AnchorPositionedKey, std::unique_ptr<AnchorPositionedState>>;

using AnchorsForAnchorName = HashMap<ResolvedScopedName, Vector<SingleThreadWeakRef<const RenderBoxModelObject>>>;

// https://drafts.csswg.org/css-anchor-position-1/#typedef-anchor-size
enum class AnchorSizeDimension : uint8_t {
    Width,
    Height,
    Block,
    Inline,
    SelfBlock,
    SelfInline
};

struct ResolvedAnchor {
    SingleThreadWeakPtr<RenderBoxModelObject> renderer;
    ResolvedScopedName name;
};

struct AnchorPositionedToAnchorEntry {
    // The pseudo-element identifier can be used to access the AnchorPositionedState struct
    // of the current element in an AnchorPositionedStates map, in combination with the relevant
    // Element object.
    std::optional<PseudoElementIdentifier> pseudoElementIdentifier;

    Vector<ResolvedAnchor> anchors;

    WTF_MAKE_STRUCT_TZONE_ALLOCATED(AnchorPositionedToAnchorEntry);
};

using AnchorPositionedToAnchorMap = WeakHashMap<Element, AnchorPositionedToAnchorEntry, WeakPtrImplWithEventTargetData>;
using AnchorToAnchorPositionedMap = SingleThreadWeakHashMap<const RenderBoxModelObject, Vector<Ref<Element>>>;

class AnchorPositionEvaluator {
public:
    using Side = Variant<CSSValueID, double>;
    static bool propertyAllowsAnchorFunction(CSSPropertyID);
    static std::optional<double> evaluate(BuilderState&, std::optional<ScopedName> elementName, Side);

    static bool propertyAllowsAnchorSizeFunction(CSSPropertyID);
    static std::optional<double> evaluateSize(BuilderState&, std::optional<ScopedName> elementName, std::optional<AnchorSizeDimension>);

    static void updateAnchorPositioningStatesAfterInterleavedLayout(Document&, AnchorPositionedStates&);
    static void updateScrollAdjustments(RenderView&);
    static void updateAnchorPositionedStateForDefaultAnchorAndPositionVisibility(Element&, const RenderStyle&, AnchorPositionedStates&);

    static LayoutRect computeAnchorRectRelativeToContainingBlock(CheckedRef<const RenderBoxModelObject> anchorBox, const RenderElement& containingBlock, const RenderBox& anchoredBox);
    static void captureScrollSnapshots(RenderBox& anchored, bool invalidateStyleForScrollPositionChanges = true);

    static AnchorToAnchorPositionedMap makeAnchorPositionedForAnchorMap(AnchorPositionedToAnchorMap&);

    static bool isAnchorPositioned(const RenderStyle&);
    static bool isStyleTimeAnchorPositioned(const RenderStyle&);
    static bool isLayoutTimeAnchorPositioned(const RenderStyle&);

    static CSSPropertyID resolvePositionTryFallbackProperty(CSSPropertyID, WritingMode, const BuilderPositionTryFallback&);

    static bool overflowsInsetModifiedContainingBlock(const RenderBox& anchoredBox);
    static bool isDefaultAnchorInvisibleOrClippedByInterveningBoxes(const RenderBox& anchoredBox);

    static ScopedName defaultAnchorName(const RenderStyle&);
    static bool isAnchor(const RenderStyle&);
    static bool isImplicitAnchor(const RenderStyle&);

    static CheckedPtr<RenderBoxModelObject> defaultAnchorForBox(const RenderBox&);

    static HashMap<AnchorPositionedKey, size_t> recordLastSuccessfulPositionOptions(const SingleThreadWeakHashSet<const RenderBox>& positionTryBoxes);

private:
    static CheckedPtr<RenderBoxModelObject> findAnchorForAnchorFunctionAndAttemptResolution(BuilderState&, std::optional<ScopedName> elementName);
    static AnchorElements findAnchorsForAnchorPositionedElement(const Element&, const HashSet<ResolvedScopedName>& anchorNames, const AnchorsForAnchorName&);
    static RefPtr<const Element> anchorPositionedElementOrPseudoElement(BuilderState&);
    static AnchorPositionedKey keyForElementOrPseudoElement(const Element&);
    static void addAnchorFunctionScrollCompensatedAxis(RenderStyle&, const RenderBox& anchored, const RenderBoxModelObject& anchor, BoxAxis);
    static LayoutSize scrollOffsetFromAnchor(const RenderBoxModelObject& anchor, const RenderBox& anchored);
};

} // namespace Style

} // namespace WebCore
