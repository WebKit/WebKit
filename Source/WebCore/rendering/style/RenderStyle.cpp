/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
#include "RenderStyle.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSCustomPropertyValue.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "ContentData.h"
#include "CursorList.h"
#include "FloatRoundedRect.h"
#include "FontCascade.h"
#include "FontSelector.h"
#include "InlineTextBoxStyle.h"
#include "Pagination.h"
#include "QuotesData.h"
#include "RenderObject.h"
#include "RenderTheme.h"
#include "ScaleTransformOperation.h"
#include "ShadowData.h"
#include "StyleBuilderConverter.h"
#include "StyleImage.h"
#include "StyleInheritedData.h"
#include "StyleResolver.h"
#include "StyleScrollSnapPoints.h"
#include "StyleSelfAlignmentData.h"
#include "StyleTreeResolver.h"
#include "WillChangeData.h"
#include <wtf/MathExtras.h>
#include <wtf/PointerComparison.h>
#include <wtf/StdLibExtras.h>
#include <algorithm>

#if ENABLE(TEXT_AUTOSIZING)
#include <wtf/text/StringHash.h>
#endif

namespace WebCore {

struct SameSizeAsBorderValue {
    Color m_color;
    float m_width;
    int m_restBits;
};

COMPILE_ASSERT(sizeof(BorderValue) == sizeof(SameSizeAsBorderValue), BorderValue_should_not_grow);

struct SameSizeAsRenderStyle {
    void* dataRefs[7];
    void* ownPtrs[1];
    void* dataRefSvgStyle;
    struct InheritedFlags {
        unsigned m_bitfields[2];
    } m_inheritedFlags;

    struct NonInheritedFlags {
        unsigned m_bitfields[2];
    } m_nonInheritedFlags;
#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionCheck;
#endif
};

static_assert(sizeof(RenderStyle) == sizeof(SameSizeAsRenderStyle), "RenderStyle should stay small");

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyle);

RenderStyle& RenderStyle::defaultStyle()
{
    static NeverDestroyed<RenderStyle> style { CreateDefaultStyle };
    return style;
}

RenderStyle RenderStyle::create()
{
    return clone(defaultStyle());
}

std::unique_ptr<RenderStyle> RenderStyle::createPtr()
{
    return clonePtr(defaultStyle());
}

RenderStyle RenderStyle::clone(const RenderStyle& style)
{
    return RenderStyle(style, Clone);
}

std::unique_ptr<RenderStyle> RenderStyle::clonePtr(const RenderStyle& style)
{
    return makeUnique<RenderStyle>(style, Clone);
}

RenderStyle RenderStyle::createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, DisplayType display)
{
    auto newStyle = create();
    newStyle.inheritFrom(parentStyle);
    newStyle.inheritUnicodeBidiFrom(&parentStyle);
    newStyle.setDisplay(display);
    return newStyle;
}

RenderStyle RenderStyle::createStyleInheritingFromPseudoStyle(const RenderStyle& pseudoStyle)
{
    ASSERT(pseudoStyle.styleType() == PseudoId::Before || pseudoStyle.styleType() == PseudoId::After);

    auto style = create();
    style.inheritFrom(pseudoStyle);
    return style;
}

RenderStyle::RenderStyle(RenderStyle&&) = default;
RenderStyle& RenderStyle::operator=(RenderStyle&&) = default;

RenderStyle::RenderStyle(CreateDefaultStyleTag)
    : m_boxData(StyleBoxData::create())
    , m_visualData(StyleVisualData::create())
    , m_backgroundData(StyleBackgroundData::create())
    , m_surroundData(StyleSurroundData::create())
    , m_rareNonInheritedData(StyleRareNonInheritedData::create())
    , m_rareInheritedData(StyleRareInheritedData::create())
    , m_inheritedData(StyleInheritedData::create())
    , m_svgStyle(SVGRenderStyle::create())
{
    m_inheritedFlags.emptyCells = static_cast<unsigned>(initialEmptyCells());
    m_inheritedFlags.captionSide = static_cast<unsigned>(initialCaptionSide());
    m_inheritedFlags.listStyleType = static_cast<unsigned>(initialListStyleType());
    m_inheritedFlags.listStylePosition = static_cast<unsigned>(initialListStylePosition());
    m_inheritedFlags.visibility = static_cast<unsigned>(initialVisibility());
    m_inheritedFlags.textAlign = static_cast<unsigned>(initialTextAlign());
    m_inheritedFlags.textTransform = static_cast<unsigned>(initialTextTransform());
    m_inheritedFlags.textDecorations = initialTextDecoration().toRaw();
    m_inheritedFlags.cursor = static_cast<unsigned>(initialCursor());
#if ENABLE(CURSOR_VISIBILITY)
    m_inheritedFlags.cursorVisibility = static_cast<unsigned>(initialCursorVisibility());
#endif
    m_inheritedFlags.direction = static_cast<unsigned>(initialDirection());
    m_inheritedFlags.whiteSpace = static_cast<unsigned>(initialWhiteSpace());
    m_inheritedFlags.borderCollapse = static_cast<unsigned>(initialBorderCollapse());
    m_inheritedFlags.rtlOrdering = static_cast<unsigned>(initialRTLOrdering());
    m_inheritedFlags.boxDirection = static_cast<unsigned>(initialBoxDirection());
    m_inheritedFlags.printColorAdjust = static_cast<unsigned>(initialPrintColorAdjust());
    m_inheritedFlags.pointerEvents = static_cast<unsigned>(initialPointerEvents());
    m_inheritedFlags.insideLink = static_cast<unsigned>(InsideLink::NotInside);
    m_inheritedFlags.insideDefaultButton = false;
    m_inheritedFlags.writingMode = initialWritingMode();
#if ENABLE(TEXT_AUTOSIZING)
    m_inheritedFlags.autosizeStatus = 0;
#endif

    m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(initialDisplay());
    m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(initialDisplay());
    m_nonInheritedFlags.overflowX = static_cast<unsigned>(initialOverflowX());
    m_nonInheritedFlags.overflowY = static_cast<unsigned>(initialOverflowY());
    m_nonInheritedFlags.verticalAlign = static_cast<unsigned>(initialVerticalAlign());
    m_nonInheritedFlags.clear = static_cast<unsigned>(initialClear());
    m_nonInheritedFlags.position = static_cast<unsigned>(initialPosition());
    m_nonInheritedFlags.unicodeBidi = initialUnicodeBidi();
    m_nonInheritedFlags.floating = static_cast<unsigned>(initialFloating());
    m_nonInheritedFlags.tableLayout = static_cast<unsigned>(initialTableLayout());
    m_nonInheritedFlags.hasExplicitlySetBorderRadius = false;
    m_nonInheritedFlags.hasExplicitlySetDirection = false;
    m_nonInheritedFlags.hasExplicitlySetWritingMode = false;
    m_nonInheritedFlags.hasExplicitlySetTextAlign = false;
    m_nonInheritedFlags.hasViewportUnits = false;
    m_nonInheritedFlags.hasExplicitlyInheritedProperties = false;
    m_nonInheritedFlags.isUnique = false;
    m_nonInheritedFlags.emptyState = false;
    m_nonInheritedFlags.firstChildState = false;
    m_nonInheritedFlags.lastChildState = false;
    m_nonInheritedFlags.isLink = false;
    m_nonInheritedFlags.styleType = static_cast<unsigned>(PseudoId::None);
    m_nonInheritedFlags.pseudoBits = static_cast<unsigned>(PseudoId::None);

    static_assert((sizeof(InheritedFlags) <= 8), "InheritedFlags does not grow");
    static_assert((sizeof(NonInheritedFlags) <= 8), "NonInheritedFlags does not grow");
}

inline RenderStyle::RenderStyle(const RenderStyle& other, CloneTag)
    : m_boxData(other.m_boxData)
    , m_visualData(other.m_visualData)
    , m_backgroundData(other.m_backgroundData)
    , m_surroundData(other.m_surroundData)
    , m_rareNonInheritedData(other.m_rareNonInheritedData)
    , m_nonInheritedFlags(other.m_nonInheritedFlags)
    , m_rareInheritedData(other.m_rareInheritedData)
    , m_inheritedData(other.m_inheritedData)
    , m_inheritedFlags(other.m_inheritedFlags)
    , m_svgStyle(other.m_svgStyle)
{
}

inline RenderStyle::RenderStyle(RenderStyle& a, RenderStyle&& b)
    : m_boxData(a.m_boxData.replace(WTFMove(b.m_boxData)))
    , m_visualData(a.m_visualData.replace(WTFMove(b.m_visualData)))
    , m_backgroundData(a.m_backgroundData.replace(WTFMove(b.m_backgroundData)))
    , m_surroundData(a.m_surroundData.replace(WTFMove(b.m_surroundData)))
    , m_rareNonInheritedData(a.m_rareNonInheritedData.replace(WTFMove(b.m_rareNonInheritedData)))
    , m_nonInheritedFlags(std::exchange(a.m_nonInheritedFlags, b.m_nonInheritedFlags))
    , m_rareInheritedData(a.m_rareInheritedData.replace(WTFMove(b.m_rareInheritedData)))
    , m_inheritedData(a.m_inheritedData.replace(WTFMove(b.m_inheritedData)))
    , m_inheritedFlags(std::exchange(a.m_inheritedFlags, b.m_inheritedFlags))
    , m_cachedPseudoStyles(std::exchange(a.m_cachedPseudoStyles, WTFMove(b.m_cachedPseudoStyles)))
    , m_svgStyle(a.m_svgStyle.replace(WTFMove(b.m_svgStyle)))
{
}

RenderStyle::~RenderStyle()
{
#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    ASSERT_WITH_SECURITY_IMPLICATION(!m_deletionHasBegun);
    m_deletionHasBegun = true;
#endif
}

RenderStyle RenderStyle::replace(RenderStyle&& newStyle)
{
    return RenderStyle { *this, WTFMove(newStyle) };
}

static StyleSelfAlignmentData resolvedSelfAlignment(const StyleSelfAlignmentData& value, ItemPosition normalValueBehavior)
{
    if (value.position() == ItemPosition::Legacy || value.position() == ItemPosition::Normal || value.position() == ItemPosition::Auto)
        return { normalValueBehavior, OverflowAlignment::Default };
    return value;
}

StyleSelfAlignmentData RenderStyle::resolvedAlignItems(ItemPosition normalValueBehaviour) const
{
    return resolvedSelfAlignment(alignItems(), normalValueBehaviour);
}

StyleSelfAlignmentData RenderStyle::resolvedAlignSelf(const RenderStyle* parentStyle, ItemPosition normalValueBehaviour) const
{
    // The auto keyword computes to the parent's align-items computed value.
    // We will return the behaviour of 'normal' value if needed, which is specific of each layout model.
    if (!parentStyle || alignSelf().position() != ItemPosition::Auto)
        return resolvedSelfAlignment(alignSelf(), normalValueBehaviour);
    return parentStyle->resolvedAlignItems(normalValueBehaviour);
}

StyleSelfAlignmentData RenderStyle::resolvedJustifyItems(ItemPosition normalValueBehaviour) const
{
    return resolvedSelfAlignment(justifyItems(), normalValueBehaviour);
}

StyleSelfAlignmentData RenderStyle::resolvedJustifySelf(const RenderStyle* parentStyle, ItemPosition normalValueBehaviour) const
{
    // The auto keyword computes to the parent's justify-items computed value.
    // We will return the behaviour of 'normal' value if needed, which is specific of each layout model.
    if (!parentStyle || justifySelf().position() != ItemPosition::Auto)
        return resolvedSelfAlignment(justifySelf(), normalValueBehaviour);
    return parentStyle->resolvedJustifyItems(normalValueBehaviour);
}

static inline StyleContentAlignmentData resolvedContentAlignment(const StyleContentAlignmentData& value, const StyleContentAlignmentData& normalValueBehavior)
{
    return (value.position() == ContentPosition::Normal && value.distribution() == ContentDistribution::Default) ? normalValueBehavior : value;
}

StyleContentAlignmentData RenderStyle::resolvedAlignContent(const StyleContentAlignmentData& normalValueBehavior) const
{
    // We will return the behaviour of 'normal' value if needed, which is specific of each layout model.
    return resolvedContentAlignment(alignContent(), normalValueBehavior);
}

StyleContentAlignmentData RenderStyle::resolvedJustifyContent(const StyleContentAlignmentData& normalValueBehavior) const
{
    // We will return the behaviour of 'normal' value if needed, which is specific of each layout model.
    return resolvedContentAlignment(justifyContent(), normalValueBehavior);
}

static inline ContentPosition resolvedContentAlignmentPosition(const StyleContentAlignmentData& value, const StyleContentAlignmentData& normalValueBehavior)
{
    return (value.position() == ContentPosition::Normal && value.distribution() == ContentDistribution::Default) ? normalValueBehavior.position() : value.position();
}

static inline ContentDistribution resolvedContentAlignmentDistribution(const StyleContentAlignmentData& value, const StyleContentAlignmentData& normalValueBehavior)
{
    return (value.position() == ContentPosition::Normal && value.distribution() == ContentDistribution::Default) ? normalValueBehavior.distribution() : value.distribution();
}

ContentPosition RenderStyle::resolvedJustifyContentPosition(const StyleContentAlignmentData& normalValueBehavior) const
{
    return resolvedContentAlignmentPosition(justifyContent(), normalValueBehavior);
}

ContentDistribution RenderStyle::resolvedJustifyContentDistribution(const StyleContentAlignmentData& normalValueBehavior) const
{
    return resolvedContentAlignmentDistribution(justifyContent(), normalValueBehavior);
}

ContentPosition RenderStyle::resolvedAlignContentPosition(const StyleContentAlignmentData& normalValueBehavior) const
{
    return resolvedContentAlignmentPosition(alignContent(), normalValueBehavior);
}

ContentDistribution RenderStyle::resolvedAlignContentDistribution(const StyleContentAlignmentData& normalValueBehavior) const
{
    return resolvedContentAlignmentDistribution(alignContent(), normalValueBehavior);
}

void RenderStyle::inheritFrom(const RenderStyle& inheritParent)
{
    m_rareInheritedData = inheritParent.m_rareInheritedData;
    m_inheritedData = inheritParent.m_inheritedData;
    m_inheritedFlags = inheritParent.m_inheritedFlags;

    if (m_svgStyle != inheritParent.m_svgStyle)
        m_svgStyle.access().inheritFrom(inheritParent.m_svgStyle.get());
}

void RenderStyle::copyNonInheritedFrom(const RenderStyle& other)
{
    m_boxData = other.m_boxData;
    m_visualData = other.m_visualData;
    m_backgroundData = other.m_backgroundData;
    m_surroundData = other.m_surroundData;
    m_rareNonInheritedData = other.m_rareNonInheritedData;
    m_nonInheritedFlags.copyNonInheritedFrom(other.m_nonInheritedFlags);

    if (m_svgStyle != other.m_svgStyle)
        m_svgStyle.access().copyNonInheritedFrom(other.m_svgStyle.get());

    ASSERT(zoom() == initialZoom());
}

void RenderStyle::copyContentFrom(const RenderStyle& other)
{
    if (!other.m_rareNonInheritedData->content)
        return;
    m_rareNonInheritedData.access().content = other.m_rareNonInheritedData->content->clone();
}

bool RenderStyle::operator==(const RenderStyle& other) const
{
    // compare everything except the pseudoStyle pointer
    return m_inheritedFlags == other.m_inheritedFlags
        && m_nonInheritedFlags == other.m_nonInheritedFlags
        && m_boxData == other.m_boxData
        && m_visualData == other.m_visualData
        && m_backgroundData == other.m_backgroundData
        && m_surroundData == other.m_surroundData
        && m_rareNonInheritedData == other.m_rareNonInheritedData
        && m_rareInheritedData == other.m_rareInheritedData
        && m_inheritedData == other.m_inheritedData
        && m_svgStyle == other.m_svgStyle;
}

bool RenderStyle::hasUniquePseudoStyle() const
{
    if (!m_cachedPseudoStyles || styleType() != PseudoId::None)
        return false;

    for (auto& pseudoStyle : *m_cachedPseudoStyles) {
        if (pseudoStyle->unique())
            return true;
    }

    return false;
}

RenderStyle* RenderStyle::getCachedPseudoStyle(PseudoId pid) const
{
    if (!m_cachedPseudoStyles || !m_cachedPseudoStyles->size())
        return nullptr;

    if (styleType() != PseudoId::None) 
        return nullptr;

    for (auto& pseudoStyle : *m_cachedPseudoStyles) {
        if (pseudoStyle->styleType() == pid)
            return pseudoStyle.get();
    }

    return nullptr;
}

RenderStyle* RenderStyle::addCachedPseudoStyle(std::unique_ptr<RenderStyle> pseudo)
{
    if (!pseudo)
        return nullptr;

    ASSERT(pseudo->styleType() > PseudoId::None);

    RenderStyle* result = pseudo.get();

    if (!m_cachedPseudoStyles)
        m_cachedPseudoStyles = makeUnique<PseudoStyleCache>();

    m_cachedPseudoStyles->append(WTFMove(pseudo));

    return result;
}

void RenderStyle::removeCachedPseudoStyle(PseudoId pid)
{
    if (!m_cachedPseudoStyles)
        return;
    for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
        RenderStyle* pseudoStyle = m_cachedPseudoStyles->at(i).get();
        if (pseudoStyle->styleType() == pid) {
            m_cachedPseudoStyles->remove(i);
            return;
        }
    }
}

bool RenderStyle::inheritedEqual(const RenderStyle& other) const
{
    return m_inheritedFlags == other.m_inheritedFlags
        && m_inheritedData == other.m_inheritedData
        && (m_svgStyle.ptr() == other.m_svgStyle.ptr() || m_svgStyle->inheritedEqual(other.m_svgStyle))
        && m_rareInheritedData == other.m_rareInheritedData;
}

bool RenderStyle::descendantAffectingNonInheritedPropertiesEqual(const RenderStyle& other) const
{
    if (m_rareNonInheritedData.ptr() == other.m_rareNonInheritedData.ptr())
        return true;
    
    return m_rareNonInheritedData->alignItems == other.m_rareNonInheritedData->alignItems
        && m_rareNonInheritedData->justifyItems == other.m_rareNonInheritedData->justifyItems;
}

#if ENABLE(TEXT_AUTOSIZING)

static inline unsigned computeFontHash(const FontCascade& font)
{
    IntegerHasher hasher;
    hasher.add(ASCIICaseInsensitiveHash::hash(font.fontDescription().firstFamily()));
    hasher.add(font.fontDescription().specifiedSize());
    return hasher.hash();
}

unsigned RenderStyle::hashForTextAutosizing() const
{
    // FIXME: Not a very smart hash. Could be improved upon. See <https://bugs.webkit.org/show_bug.cgi?id=121131>.
    unsigned hash = m_rareNonInheritedData->appearance;
    hash ^= m_rareNonInheritedData->marginBeforeCollapse;
    hash ^= m_rareNonInheritedData->marginAfterCollapse;
    hash ^= m_rareNonInheritedData->lineClamp.value();
    hash ^= m_rareInheritedData->overflowWrap;
    hash ^= m_rareInheritedData->nbspMode;
    hash ^= m_rareInheritedData->lineBreak;
    hash ^= WTF::FloatHash<float>::hash(m_inheritedData->specifiedLineHeight.value());
    hash ^= computeFontHash(m_inheritedData->fontCascade);
    hash ^= WTF::FloatHash<float>::hash(m_inheritedData->horizontalBorderSpacing);
    hash ^= WTF::FloatHash<float>::hash(m_inheritedData->verticalBorderSpacing);
    hash ^= m_inheritedFlags.boxDirection;
    hash ^= m_inheritedFlags.rtlOrdering;
    hash ^= m_nonInheritedFlags.position;
    hash ^= m_nonInheritedFlags.floating;
    hash ^= m_rareNonInheritedData->textOverflow;
    hash ^= m_rareInheritedData->textSecurity;
    return hash;
}

bool RenderStyle::equalForTextAutosizing(const RenderStyle& other) const
{
    return m_rareNonInheritedData->appearance == other.m_rareNonInheritedData->appearance
        && m_rareNonInheritedData->marginBeforeCollapse == other.m_rareNonInheritedData->marginBeforeCollapse
        && m_rareNonInheritedData->marginAfterCollapse == other.m_rareNonInheritedData->marginAfterCollapse
        && m_rareNonInheritedData->lineClamp == other.m_rareNonInheritedData->lineClamp
        && m_rareInheritedData->textSizeAdjust == other.m_rareInheritedData->textSizeAdjust
        && m_rareInheritedData->overflowWrap == other.m_rareInheritedData->overflowWrap
        && m_rareInheritedData->nbspMode == other.m_rareInheritedData->nbspMode
        && m_rareInheritedData->lineBreak == other.m_rareInheritedData->lineBreak
        && m_rareInheritedData->textSecurity == other.m_rareInheritedData->textSecurity
        && m_inheritedData->specifiedLineHeight == other.m_inheritedData->specifiedLineHeight
        && m_inheritedData->fontCascade.equalForTextAutoSizing(other.m_inheritedData->fontCascade)
        && m_inheritedData->horizontalBorderSpacing == other.m_inheritedData->horizontalBorderSpacing
        && m_inheritedData->verticalBorderSpacing == other.m_inheritedData->verticalBorderSpacing
        && m_inheritedFlags.boxDirection == other.m_inheritedFlags.boxDirection
        && m_inheritedFlags.rtlOrdering == other.m_inheritedFlags.rtlOrdering
        && m_nonInheritedFlags.position == other.m_nonInheritedFlags.position
        && m_nonInheritedFlags.floating == other.m_nonInheritedFlags.floating
        && m_rareNonInheritedData->textOverflow == other.m_rareNonInheritedData->textOverflow;
}

bool RenderStyle::isIdempotentTextAutosizingCandidate(Optional<AutosizeStatus> overrideStatus) const
{
    // Refer to <rdar://problem/51826266> for more information regarding how this function was generated.
    auto fields = OptionSet<AutosizeStatus::Fields>::fromRaw(m_inheritedFlags.autosizeStatus);
    if (overrideStatus)
        fields = overrideStatus->fields();

    if (fields.contains(AutosizeStatus::Fields::AvoidSubtree))
        return false;

    const float smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText = 5;
    const float largeMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText = 25;

    if (fields.contains(AutosizeStatus::Fields::FixedHeight)) {
        if (fields.contains(AutosizeStatus::Fields::FixedWidth)) {
            if (whiteSpace() == WhiteSpace::NoWrap) {
                if (width().isFixed())
                    return false;

                if (height().isFixed() && specifiedLineHeight().isFixed()) {
                    float specifiedSize = specifiedFontSize();
                    if (height().value() == specifiedSize && specifiedLineHeight().value() == specifiedSize)
                        return false;
                }

                return true;
            }

            if (fields.contains(AutosizeStatus::Fields::Floating)) {
                if (specifiedLineHeight().isFixed() && height().isFixed()) {
                    float specifiedSize = specifiedFontSize();
                    if (specifiedLineHeight().value() - specifiedSize > smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText
                        && height().value() - specifiedSize > smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText) {
                        return true;
                    }
                }
                return false;
            }

            if (fields.contains(AutosizeStatus::Fields::OverflowXHidden))
                return false;

            return true;
        }

        if (fields.contains(AutosizeStatus::Fields::OverflowXHidden)) {
            if (fields.contains(AutosizeStatus::Fields::Floating))
                return false;

            return true;
        }

        return true;
    }

    if (width().isFixed()) {
        if (breakWords())
            return true;

        return false;
    }

    if (textSizeAdjust().isPercentage() && textSizeAdjust().percentage() == 100) {
        if (fields.contains(AutosizeStatus::Fields::Floating))
            return true;

        if (fields.contains(AutosizeStatus::Fields::FixedWidth))
            return true;

        if (specifiedLineHeight().isFixed() && specifiedLineHeight().value() - specifiedFontSize() > largeMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText)
            return true;

        return false;
    }

    if (hasBackgroundImage() && backgroundRepeatX() == FillRepeat::NoRepeat && backgroundRepeatY() == FillRepeat::NoRepeat)
        return false;

    return true;
}

AutosizeStatus RenderStyle::autosizeStatus() const
{
    return OptionSet<AutosizeStatus::Fields>::fromRaw(m_inheritedFlags.autosizeStatus);
}

void RenderStyle::setAutosizeStatus(AutosizeStatus autosizeStatus)
{
    m_inheritedFlags.autosizeStatus = autosizeStatus.fields().toRaw();
}

#endif // ENABLE(TEXT_AUTOSIZING)

static bool positionChangeIsMovementOnly(const LengthBox& a, const LengthBox& b, const Length& width)
{
    // If any unit types are different, then we can't guarantee
    // that this was just a movement.
    if (a.left().type() != b.left().type()
        || a.right().type() != b.right().type()
        || a.top().type() != b.top().type()
        || a.bottom().type() != b.bottom().type())
        return false;

    // Only one unit can be non-auto in the horizontal direction and
    // in the vertical direction.  Otherwise the adjustment of values
    // is changing the size of the box.
    if (!a.left().isIntrinsicOrAuto() && !a.right().isIntrinsicOrAuto())
        return false;
    if (!a.top().isIntrinsicOrAuto() && !a.bottom().isIntrinsicOrAuto())
        return false;
    // If our width is auto and left or right is specified then this 
    // is not just a movement - we need to resize to our container.
    if ((!a.left().isIntrinsicOrAuto() || !a.right().isIntrinsicOrAuto()) && width.isIntrinsicOrAuto())
        return false;

    // One of the units is fixed or percent in both directions and stayed
    // that way in the new style.  Therefore all we are doing is moving.
    return true;
}

inline bool RenderStyle::changeAffectsVisualOverflow(const RenderStyle& other) const
{
    if (m_rareNonInheritedData.ptr() != other.m_rareNonInheritedData.ptr()
        && !arePointingToEqualData(m_rareNonInheritedData->boxShadow, other.m_rareNonInheritedData->boxShadow))
        return true;

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()
        && !arePointingToEqualData(m_rareInheritedData->textShadow, other.m_rareInheritedData->textShadow))
        return true;

    if (m_inheritedFlags.textDecorations != other.m_inheritedFlags.textDecorations
        || m_rareNonInheritedData->textDecorationStyle != other.m_rareNonInheritedData->textDecorationStyle
        || m_rareInheritedData->textDecorationThickness != other.m_rareInheritedData->textDecorationThickness
        || m_rareInheritedData->textUnderlineOffset != other.m_rareInheritedData->textUnderlineOffset
        || m_rareInheritedData->textUnderlinePosition != other.m_rareInheritedData->textUnderlinePosition) {
        // Underlines are always drawn outside of their textbox bounds when text-underline-position: under;
        // is specified. We can take an early out here.
        if (textUnderlinePosition() == TextUnderlinePosition::Under || other.textUnderlinePosition() == TextUnderlinePosition::Under)
            return true;
        return visualOverflowForDecorations(*this, nullptr) != visualOverflowForDecorations(other, nullptr);
    }

    if (hasOutlineInVisualOverflow() != other.hasOutlineInVisualOverflow())
        return true;
    return false;
}

static bool rareNonInheritedDataChangeRequiresLayout(const StyleRareNonInheritedData& first, const StyleRareNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
    ASSERT(&first != &second);

    if (first.appearance != second.appearance
        || first.marginBeforeCollapse != second.marginBeforeCollapse
        || first.marginAfterCollapse != second.marginAfterCollapse
        || first.lineClamp != second.lineClamp
        || first.initialLetter != second.initialLetter
        || first.textOverflow != second.textOverflow)
        return true;

    if (first.shapeMargin != second.shapeMargin)
        return true;

    if (first.deprecatedFlexibleBox != second.deprecatedFlexibleBox)
        return true;

    if (first.flexibleBox != second.flexibleBox)
        return true;

    if (first.order != second.order
        || first.alignContent != second.alignContent
        || first.alignItems != second.alignItems
        || first.alignSelf != second.alignSelf
        || first.justifyContent != second.justifyContent
        || first.justifyItems != second.justifyItems
        || first.justifySelf != second.justifySelf)
        return true;

    if (!arePointingToEqualData(first.boxReflect, second.boxReflect))
        return true;

    if (first.multiCol != second.multiCol)
        return true;

    if (first.transform.ptr() != second.transform.ptr()) {
        if (first.transform->hasTransform() != second.transform->hasTransform())
            return true;
        if (*first.transform != *second.transform) {
            changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Transform);
            // Don't return; keep looking for another change
        }
    }

    if (first.grid != second.grid
        || first.gridItem != second.gridItem)
        return true;

    if (!arePointingToEqualData(first.willChange, second.willChange)) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::WillChange);
        // Don't return; keep looking for another change
    }

    if (first.textCombine != second.textCombine)
        return true;

    if (first.breakBefore != second.breakBefore
        || first.breakAfter != second.breakAfter
        || first.breakInside != second.breakInside)
        return true;

    if (first.hasOpacity() != second.hasOpacity()) {
        // FIXME: We would like to use SimplifiedLayout here, but we can't quite do that yet.
        // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
        // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
        // In addition we need to solve the floating object issue when layers come and go. Right now
        // a full layout is necessary to keep floating object lists sane.
        return true;
    }

#if ENABLE(CSS_COMPOSITING)
    if (first.isolation != second.isolation) {
        // Ideally this would trigger a cheaper layout that just updates layer z-order trees (webit.org/b/190088).
        return true;
    }
#endif

    if (first.hasFilters() != second.hasFilters())
        return true;

#if ENABLE(FILTERS_LEVEL_2)
    if (first.hasBackdropFilters() != second.hasBackdropFilters())
        return true;
#endif

    return false;
}

static bool rareInheritedDataChangeRequiresLayout(const StyleRareInheritedData& first, const StyleRareInheritedData& second)
{
    ASSERT(&first != &second);

    if (first.indent != second.indent
#if ENABLE(CSS3_TEXT)
        || first.textAlignLast != second.textAlignLast
        || first.textJustify != second.textJustify
        || first.textIndentLine != second.textIndentLine
#endif
        || first.effectiveZoom != second.effectiveZoom
        || first.textZoom != second.textZoom
#if ENABLE(TEXT_AUTOSIZING)
        || first.textSizeAdjust != second.textSizeAdjust
#endif
        || first.wordBreak != second.wordBreak
        || first.overflowWrap != second.overflowWrap
        || first.nbspMode != second.nbspMode
        || first.lineBreak != second.lineBreak
        || first.textSecurity != second.textSecurity
        || first.hyphens != second.hyphens
        || first.hyphenationLimitBefore != second.hyphenationLimitBefore
        || first.hyphenationLimitAfter != second.hyphenationLimitAfter
        || first.hyphenationString != second.hyphenationString
        || first.rubyPosition != second.rubyPosition
        || first.textEmphasisMark != second.textEmphasisMark
        || first.textEmphasisPosition != second.textEmphasisPosition
        || first.textEmphasisCustomMark != second.textEmphasisCustomMark
        || first.textOrientation != second.textOrientation
        || first.tabSize != second.tabSize
        || first.lineBoxContain != second.lineBoxContain
        || first.lineGrid != second.lineGrid
        || first.imageOrientation != second.imageOrientation
#if ENABLE(CSS_IMAGE_RESOLUTION)
        || first.imageResolutionSource != second.imageResolutionSource
        || first.imageResolutionSnap != second.imageResolutionSnap
        || first.imageResolution != second.imageResolution
#endif
        || first.lineSnap != second.lineSnap
        || first.lineAlign != second.lineAlign
        || first.hangingPunctuation != second.hangingPunctuation
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
        || first.useTouchOverflowScrolling != second.useTouchOverflowScrolling
#endif
        || first.listStyleImage != second.listStyleImage) // FIXME: needs arePointingToEqualData()?
        return true;

    if (first.textStrokeWidth != second.textStrokeWidth)
        return true;

    // These properties affect the cached stroke bounding box rects.
    if (first.capStyle != second.capStyle
        || first.joinStyle != second.joinStyle
        || first.strokeWidth != second.strokeWidth
        || first.miterLimit != second.miterLimit)
        return true;

    if (!arePointingToEqualData(first.quotes, second.quotes))
        return true;

    return false;
}

bool RenderStyle::changeRequiresLayout(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    if (m_boxData.ptr() != other.m_boxData.ptr()) {
        if (m_boxData->width() != other.m_boxData->width()
            || m_boxData->minWidth() != other.m_boxData->minWidth()
            || m_boxData->maxWidth() != other.m_boxData->maxWidth()
            || m_boxData->height() != other.m_boxData->height()
            || m_boxData->minHeight() != other.m_boxData->minHeight()
            || m_boxData->maxHeight() != other.m_boxData->maxHeight())
            return true;

        if (m_boxData->verticalAlign() != other.m_boxData->verticalAlign())
            return true;

        if (m_boxData->boxSizing() != other.m_boxData->boxSizing())
            return true;

        if (m_boxData->hasAutoUsedZIndex() != other.m_boxData->hasAutoUsedZIndex())
            return true;
    }

    if (m_surroundData->margin != other.m_surroundData->margin)
        return true;

    if (m_surroundData->padding != other.m_surroundData->padding)
        return true;

    // FIXME: We should add an optimized form of layout that just recomputes visual overflow.
    if (changeAffectsVisualOverflow(other))
        return true;

    if (m_rareNonInheritedData.ptr() != other.m_rareNonInheritedData.ptr()
        && rareNonInheritedDataChangeRequiresLayout(*m_rareNonInheritedData, *other.m_rareNonInheritedData, changedContextSensitiveProperties))
        return true;

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()
        && rareInheritedDataChangeRequiresLayout(*m_rareInheritedData, *other.m_rareInheritedData))
        return true;

    if (m_inheritedData.ptr() != other.m_inheritedData.ptr()) {
        if (m_inheritedData->lineHeight != other.m_inheritedData->lineHeight
#if ENABLE(TEXT_AUTOSIZING)
            || m_inheritedData->specifiedLineHeight != other.m_inheritedData->specifiedLineHeight
#endif
            || m_inheritedData->fontCascade != other.m_inheritedData->fontCascade
            || m_inheritedData->horizontalBorderSpacing != other.m_inheritedData->horizontalBorderSpacing
            || m_inheritedData->verticalBorderSpacing != other.m_inheritedData->verticalBorderSpacing)
            return true;
    }

    if (m_inheritedFlags.boxDirection != other.m_inheritedFlags.boxDirection
        || m_inheritedFlags.rtlOrdering != other.m_inheritedFlags.rtlOrdering
        || m_nonInheritedFlags.position != other.m_nonInheritedFlags.position
        || m_nonInheritedFlags.floating != other.m_nonInheritedFlags.floating
        || m_nonInheritedFlags.originalDisplay != other.m_nonInheritedFlags.originalDisplay
        || m_nonInheritedFlags.verticalAlign != other.m_nonInheritedFlags.verticalAlign)
        return true;

    if (static_cast<DisplayType>(m_nonInheritedFlags.effectiveDisplay) >= DisplayType::Table) {
        if (m_inheritedFlags.borderCollapse != other.m_inheritedFlags.borderCollapse
            || m_inheritedFlags.emptyCells != other.m_inheritedFlags.emptyCells
            || m_inheritedFlags.captionSide != other.m_inheritedFlags.captionSide
            || m_nonInheritedFlags.tableLayout != other.m_nonInheritedFlags.tableLayout)
            return true;

        // In the collapsing border model, 'hidden' suppresses other borders, while 'none'
        // does not, so these style differences can be width differences.
        if (m_inheritedFlags.borderCollapse
            && ((borderTopStyle() == BorderStyle::Hidden && other.borderTopStyle() == BorderStyle::None)
                || (borderTopStyle() == BorderStyle::None && other.borderTopStyle() == BorderStyle::Hidden)
                || (borderBottomStyle() == BorderStyle::Hidden && other.borderBottomStyle() == BorderStyle::None)
                || (borderBottomStyle() == BorderStyle::None && other.borderBottomStyle() == BorderStyle::Hidden)
                || (borderLeftStyle() == BorderStyle::Hidden && other.borderLeftStyle() == BorderStyle::None)
                || (borderLeftStyle() == BorderStyle::None && other.borderLeftStyle() == BorderStyle::Hidden)
                || (borderRightStyle() == BorderStyle::Hidden && other.borderRightStyle() == BorderStyle::None)
                || (borderRightStyle() == BorderStyle::None && other.borderRightStyle() == BorderStyle::Hidden)))
            return true;
    }

    if (static_cast<DisplayType>(m_nonInheritedFlags.effectiveDisplay) == DisplayType::ListItem) {
        if (m_inheritedFlags.listStyleType != other.m_inheritedFlags.listStyleType
            || m_inheritedFlags.listStylePosition != other.m_inheritedFlags.listStylePosition)
            return true;
    }

    if (m_inheritedFlags.textAlign != other.m_inheritedFlags.textAlign
        || m_inheritedFlags.textTransform != other.m_inheritedFlags.textTransform
        || m_inheritedFlags.direction != other.m_inheritedFlags.direction
        || m_inheritedFlags.whiteSpace != other.m_inheritedFlags.whiteSpace
        || m_nonInheritedFlags.clear != other.m_nonInheritedFlags.clear
        || m_nonInheritedFlags.unicodeBidi != other.m_nonInheritedFlags.unicodeBidi)
        return true;

    // Check block flow direction.
    if (m_inheritedFlags.writingMode != other.m_inheritedFlags.writingMode)
        return true;

    // Overflow returns a layout hint.
    if (m_nonInheritedFlags.overflowX != other.m_nonInheritedFlags.overflowX
        || m_nonInheritedFlags.overflowY != other.m_nonInheritedFlags.overflowY)
        return true;

    // If our border widths change, then we need to layout.  Other changes to borders
    // only necessitate a repaint.
    if (borderLeftWidth() != other.borderLeftWidth()
        || borderTopWidth() != other.borderTopWidth()
        || borderBottomWidth() != other.borderBottomWidth()
        || borderRightWidth() != other.borderRightWidth())
        return true;

    // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
    if (!arePointingToEqualData(m_rareNonInheritedData->counterDirectives, other.m_rareNonInheritedData->counterDirectives))
        return true;

    if ((visibility() == Visibility::Collapse) != (other.visibility() == Visibility::Collapse))
        return true;

    if (position() != PositionType::Static) {
        if (m_surroundData->offset != other.m_surroundData->offset) {
            // FIXME: We would like to use SimplifiedLayout for relative positioning, but we can't quite do that yet.
            // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
            // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
            if (position() != PositionType::Absolute)
                return true;

            // Optimize for the case where a positioned layer is moving but not changing size.
            if (!positionChangeIsMovementOnly(m_surroundData->offset, other.m_surroundData->offset, m_boxData->width()))
                return true;
        }
    }

    bool hasFirstLineStyle = hasPseudoStyle(PseudoId::FirstLine);
    if (hasFirstLineStyle != other.hasPseudoStyle(PseudoId::FirstLine))
        return true;
    if (hasFirstLineStyle) {
        auto* firstLineStyle = getCachedPseudoStyle(PseudoId::FirstLine);
        if (!firstLineStyle)
            return true;
        auto* otherFirstLineStyle = other.getCachedPseudoStyle(PseudoId::FirstLine);
        if (!otherFirstLineStyle)
            return true;
        // FIXME: Not all first line style changes actually need layout.
        if (*firstLineStyle != *otherFirstLineStyle)
            return true;
    }

    return false;
}

bool RenderStyle::changeRequiresPositionedLayoutOnly(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>&) const
{
    if (position() == PositionType::Static)
        return false;

    if (m_surroundData->offset != other.m_surroundData->offset) {
        // Optimize for the case where a positioned layer is moving but not changing size.
        if (position() == PositionType::Absolute && positionChangeIsMovementOnly(m_surroundData->offset, other.m_surroundData->offset, m_boxData->width()))
            return true;
    }
    
    return false;
}

static bool rareNonInheritedDataChangeRequiresLayerRepaint(const StyleRareNonInheritedData& first, const StyleRareNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
#if ENABLE(CSS_COMPOSITING)
    if (first.effectiveBlendMode != second.effectiveBlendMode)
        return true;
#endif

    if (first.opacity != second.opacity) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Opacity);
        // Don't return true; keep looking for another change.
    }

    if (first.filter != second.filter) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Filter);
        // Don't return true; keep looking for another change.
    }

#if ENABLE(FILTERS_LEVEL_2)
    if (first.backdropFilter != second.backdropFilter) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Filter);
        // Don't return true; keep looking for another change.
    }
#endif

    if (first.mask != second.mask || first.maskBoxImage != second.maskBoxImage)
        return true;

    return false;
}

bool RenderStyle::changeRequiresLayerRepaint(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    // Style::Resolver has ensured that zIndex is non-auto only if it's applicable.
    if (m_boxData->usedZIndex() != other.m_boxData->usedZIndex() || m_boxData->hasAutoUsedZIndex() != other.m_boxData->hasAutoUsedZIndex())
        return true;

    if (position() != PositionType::Static) {
        if (m_visualData->clip != other.m_visualData->clip || m_visualData->hasClip != other.m_visualData->hasClip) {
            changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::ClipRect);
            return true;
        }
    }

    if (m_rareNonInheritedData.ptr() != other.m_rareNonInheritedData.ptr()
        && rareNonInheritedDataChangeRequiresLayerRepaint(*m_rareNonInheritedData, *other.m_rareNonInheritedData, changedContextSensitiveProperties))
        return true;

    return false;
}

static bool requiresPainting(const RenderStyle& style)
{
    if (style.visibility() == Visibility::Hidden)
        return false;
    if (!style.opacity())
        return false;
    return true;
}

static bool rareNonInheritedDataChangeRequiresRepaint(const StyleRareNonInheritedData& first, const StyleRareNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
    if (first.userDrag != second.userDrag
        || first.borderFit != second.borderFit
        || first.objectFit != second.objectFit
        || first.objectPosition != second.objectPosition)
        return true;

    if (first.isNotFinal != second.isNotFinal)
        return true;

    if (first.shapeOutside != second.shapeOutside)
        return true;

    // FIXME: this should probably be moved to changeRequiresLayerRepaint().
    if (first.clipPath != second.clipPath) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::ClipPath);
        // Don't return true; keep looking for another change.
    }

    return false;
}

static bool rareInheritedDataChangeRequiresRepaint(const StyleRareInheritedData& first, const StyleRareInheritedData& second)
{
    return first.userModify != second.userModify
        || first.userSelect != second.userSelect
        || first.appleColorFilter != second.appleColorFilter
        || first.imageRendering != second.imageRendering
#if ENABLE(DARK_MODE_CSS)
        || first.colorScheme != second.colorScheme
#endif
    ;
}

#if ENABLE(CSS_PAINTING_API)
void RenderStyle::addCustomPaintWatchProperty(const String& name)
{
    auto& data = m_rareNonInheritedData.access();
    if (!data.customPaintWatchedProperties)
        data.customPaintWatchedProperties = makeUnique<HashSet<String>>();
    data.customPaintWatchedProperties->add(name);
}

inline static bool changedCustomPaintWatchedProperty(const RenderStyle& a, const StyleRareNonInheritedData& aData, const RenderStyle& b, const StyleRareNonInheritedData& bData)
{
    auto* propertiesA = aData.customPaintWatchedProperties.get();
    auto* propertiesB = bData.customPaintWatchedProperties.get();

    if (UNLIKELY(propertiesA || propertiesB)) {
        // FIXME: We should not need to use ComputedStyleExtractor here.
        ComputedStyleExtractor extractor((Element*) nullptr);

        for (auto* watchPropertiesMap : { propertiesA, propertiesB }) {
            if (!watchPropertiesMap)
                continue;

            for (auto& name : *watchPropertiesMap) {
                RefPtr<CSSValue> valueA;
                RefPtr<CSSValue> valueB;
                if (isCustomPropertyName(name)) {
                    if (a.getCustomProperty(name))
                        valueA = CSSCustomPropertyValue::create(*a.getCustomProperty(name));
                    if (b.getCustomProperty(name))
                        valueB = CSSCustomPropertyValue::create(*b.getCustomProperty(name));
                } else {
                    CSSPropertyID propertyID = cssPropertyID(name);
                    if (!propertyID)
                        continue;
                    valueA = extractor.valueForPropertyInStyle(a, propertyID);
                    valueB = extractor.valueForPropertyInStyle(b, propertyID);
                }

                if ((valueA && !valueB) || (!valueA && valueB))
                    return true;

                if (!valueA)
                    continue;

                if (!(*valueA == *valueB))
                    return true;
            }
        }
    }

    return false;
}
#endif

bool RenderStyle::changeRequiresRepaint(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    if (!requiresPainting(*this) && !requiresPainting(other))
        return false;

    if (m_inheritedFlags.visibility != other.m_inheritedFlags.visibility
        || m_inheritedFlags.printColorAdjust != other.m_inheritedFlags.printColorAdjust
        || m_inheritedFlags.insideLink != other.m_inheritedFlags.insideLink
        || m_inheritedFlags.insideDefaultButton != other.m_inheritedFlags.insideDefaultButton
        || m_surroundData->border != other.m_surroundData->border
        || !m_backgroundData->isEquivalentForPainting(*other.m_backgroundData))
        return true;

    if (m_rareNonInheritedData.ptr() != other.m_rareNonInheritedData.ptr()
        && rareNonInheritedDataChangeRequiresRepaint(*m_rareNonInheritedData, *other.m_rareNonInheritedData, changedContextSensitiveProperties))
        return true;

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()
        && rareInheritedDataChangeRequiresRepaint(*m_rareInheritedData, *other.m_rareInheritedData))
        return true;

#if ENABLE(CSS_PAINTING_API)
    if (changedCustomPaintWatchedProperty(*this, *m_rareNonInheritedData, other, *other.m_rareNonInheritedData))
        return true;
#endif

    return false;
}

bool RenderStyle::changeRequiresRepaintIfTextOrBorderOrOutline(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>&) const
{
    if (m_inheritedData->color != other.m_inheritedData->color
        || m_inheritedFlags.textDecorations != other.m_inheritedFlags.textDecorations
        || m_visualData->textDecoration != other.m_visualData->textDecoration
        || m_rareNonInheritedData->textDecorationStyle != other.m_rareNonInheritedData->textDecorationStyle
        || m_rareNonInheritedData->textDecorationColor != other.m_rareNonInheritedData->textDecorationColor
        || m_rareInheritedData->textDecorationSkip != other.m_rareInheritedData->textDecorationSkip
        || m_rareInheritedData->textFillColor != other.m_rareInheritedData->textFillColor
        || m_rareInheritedData->textStrokeColor != other.m_rareInheritedData->textStrokeColor
        || m_rareInheritedData->textEmphasisColor != other.m_rareInheritedData->textEmphasisColor
        || m_rareInheritedData->textEmphasisFill != other.m_rareInheritedData->textEmphasisFill
        || m_rareInheritedData->strokeColor != other.m_rareInheritedData->strokeColor
        || m_rareInheritedData->caretColor != other.m_rareInheritedData->caretColor)
        return true;

    return false;
}

bool RenderStyle::changeRequiresRecompositeLayer(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>&) const
{
    if (m_rareNonInheritedData.ptr() != other.m_rareNonInheritedData.ptr()) {
        if (m_rareNonInheritedData->transformStyle3D != other.m_rareNonInheritedData->transformStyle3D
            || m_rareNonInheritedData->backfaceVisibility != other.m_rareNonInheritedData->backfaceVisibility
            || m_rareNonInheritedData->perspective != other.m_rareNonInheritedData->perspective
            || m_rareNonInheritedData->perspectiveOriginX != other.m_rareNonInheritedData->perspectiveOriginX
            || m_rareNonInheritedData->perspectiveOriginY != other.m_rareNonInheritedData->perspectiveOriginY)
            return true;
    }

    return false;
}

StyleDifference RenderStyle::diff(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    changedContextSensitiveProperties = OptionSet<StyleDifferenceContextSensitiveProperty>();

    StyleDifference svgChange = StyleDifference::Equal;
    if (m_svgStyle != other.m_svgStyle) {
        svgChange = m_svgStyle->diff(other.m_svgStyle.get());
        if (svgChange == StyleDifference::Layout)
            return svgChange;
    }

    if (changeRequiresLayout(other, changedContextSensitiveProperties))
        return StyleDifference::Layout;

    // SVGRenderStyle::diff() might have returned StyleDifference::Repaint, eg. if fill changes.
    // If eg. the font-size changed at the same time, we're not allowed to return StyleDifference::Repaint,
    // but have to return StyleDifference::Layout, that's why  this if branch comes after all branches
    // that are relevant for SVG and might return StyleDifference::Layout.
    if (svgChange != StyleDifference::Equal)
        return svgChange;

    if (changeRequiresPositionedLayoutOnly(other, changedContextSensitiveProperties))
        return StyleDifference::LayoutPositionedMovementOnly;

    if (changeRequiresLayerRepaint(other, changedContextSensitiveProperties))
        return StyleDifference::RepaintLayer;

    if (changeRequiresRepaint(other, changedContextSensitiveProperties))
        return StyleDifference::Repaint;

    if (changeRequiresRecompositeLayer(other, changedContextSensitiveProperties))
        return StyleDifference::RecompositeLayer;

    if (changeRequiresRepaintIfTextOrBorderOrOutline(other, changedContextSensitiveProperties))
        return StyleDifference::RepaintIfTextOrBorderOrOutline;

    // Cursors are not checked, since they will be set appropriately in response to mouse events,
    // so they don't need to cause any repaint or layout.

    // Animations don't need to be checked either.  We always set the new style on the RenderObject, so we will get a chance to fire off
    // the resulting transition properly.
    return StyleDifference::Equal;
}

bool RenderStyle::diffRequiresLayerRepaint(const RenderStyle& style, bool isComposited) const
{
    OptionSet<StyleDifferenceContextSensitiveProperty> changedContextSensitiveProperties;

    if (changeRequiresRepaint(style, changedContextSensitiveProperties))
        return true;

    if (isComposited && changeRequiresLayerRepaint(style, changedContextSensitiveProperties))
        return changedContextSensitiveProperties.contains(StyleDifferenceContextSensitiveProperty::ClipRect);

    return false;
}

void RenderStyle::setClip(Length&& top, Length&& right, Length&& bottom, Length&& left)
{
    auto& data = m_visualData.access();
    data.clip.top() = WTFMove(top);
    data.clip.right() = WTFMove(right);
    data.clip.bottom() = WTFMove(bottom);
    data.clip.left() = WTFMove(left);
}

void RenderStyle::addCursor(RefPtr<StyleImage>&& image, const IntPoint& hotSpot)
{
    auto& cursorData = m_rareInheritedData.access().cursorData;
    if (!cursorData)
        cursorData = CursorList::create();
    cursorData->append(CursorData(WTFMove(image), hotSpot));
}

void RenderStyle::setCursorList(RefPtr<CursorList>&& list)
{
    m_rareInheritedData.access().cursorData = WTFMove(list);
}

void RenderStyle::setQuotes(RefPtr<QuotesData>&& q)
{
    if (m_rareInheritedData->quotes == q || (m_rareInheritedData->quotes && q && *m_rareInheritedData->quotes == *q))
        return;

    m_rareInheritedData.access().quotes = WTFMove(q);
}

void RenderStyle::setWillChange(RefPtr<WillChangeData>&& willChangeData)
{
    if (arePointingToEqualData(m_rareNonInheritedData->willChange.get(), willChangeData.get()))
        return;

    m_rareNonInheritedData.access().willChange = WTFMove(willChangeData);
}

void RenderStyle::clearCursorList()
{
    if (m_rareInheritedData->cursorData)
        m_rareInheritedData.access().cursorData = nullptr;
}

void RenderStyle::clearContent()
{
    if (m_rareNonInheritedData->content)
        m_rareNonInheritedData.access().content = nullptr;
}

static inline ContentData& lastContent(ContentData& firstContent)
{
    auto* lastContent = &firstContent;
    for (auto* content = &firstContent; content; content = content->next())
        lastContent = content;
    return *lastContent;
}

void RenderStyle::setContent(std::unique_ptr<ContentData> contentData, bool add)
{
    auto& data = m_rareNonInheritedData.access();
    if (add && data.content)
        lastContent(*data.content).setNext(WTFMove(contentData));
    else {
        data.content = WTFMove(contentData);
        auto& altText = data.altText;
        if (!altText.isNull())
            data.content->setAltText(altText);
    }
}

void RenderStyle::setContent(RefPtr<StyleImage>&& image, bool add)
{
    if (!image)
        return;
    setContent(makeUnique<ImageContentData>(image.releaseNonNull()), add);
}

void RenderStyle::setContent(const String& string, bool add)
{
    auto& data = m_rareNonInheritedData.access();
    if (add && data.content) {
        auto& last = lastContent(*data.content);
        if (!is<TextContentData>(last))
            last.setNext(makeUnique<TextContentData>(string));
        else {
            auto& textContent = downcast<TextContentData>(last);
            textContent.setText(textContent.text() + string);
        }
    } else {
        data.content = makeUnique<TextContentData>(string);
        auto& altText = data.altText;
        if (!altText.isNull())
            data.content->setAltText(altText);
    }
}

void RenderStyle::setContent(std::unique_ptr<CounterContent> counter, bool add)
{
    if (!counter)
        return;
    setContent(makeUnique<CounterContentData>(WTFMove(counter)), add);
}

void RenderStyle::setContent(QuoteType quote, bool add)
{
    setContent(makeUnique<QuoteContentData>(quote), add);
}

void RenderStyle::setContentAltText(const String& string)
{
    auto& data = m_rareNonInheritedData.access();
    data.altText = string;
    if (data.content)
        data.content->setAltText(string);
}

const String& RenderStyle::contentAltText() const
{
    return m_rareNonInheritedData->altText;
}

void RenderStyle::setHasAttrContent()
{
    setUnique();
    SET_VAR(m_rareNonInheritedData, hasAttrContent, true);
}

static inline bool requireTransformOrigin(const Vector<RefPtr<TransformOperation>>& transformOperations, RenderStyle::ApplyTransformOrigin applyOrigin)
{
    // The transform-origin property brackets the transform with translate operations.
    // When the only transform is a translation, the transform-origin is irrelevant.

    if (applyOrigin != RenderStyle::IncludeTransformOrigin)
        return false;

    for (auto& operation : transformOperations) {
        // FIXME: Use affectedByTransformOrigin().
        auto type = operation->type();
        if (type != TransformOperation::TRANSLATE
            && type != TransformOperation::TRANSLATE_3D
            && type != TransformOperation::TRANSLATE_X
            && type != TransformOperation::TRANSLATE_Y
            && type != TransformOperation::TRANSLATE_Z)
            return true;
    }

    return false;
}

void RenderStyle::applyTransform(TransformationMatrix& transform, const FloatRect& boundingBox, ApplyTransformOrigin applyOrigin) const
{
    auto& operations = m_rareNonInheritedData->transform->operations.operations();
    bool applyTransformOrigin = requireTransformOrigin(operations, applyOrigin);
    
    FloatPoint3D originTranslate;
    if (applyTransformOrigin) {
        originTranslate.setXY(boundingBox.location() + floatPointForLengthPoint(transformOriginXY(), boundingBox.size()));
        originTranslate.setZ(transformOriginZ());
        transform.translate3d(originTranslate.x(), originTranslate.y(), originTranslate.z());
    }

    for (auto& operation : operations)
        operation->apply(transform, boundingBox.size());

    if (applyTransformOrigin)
        transform.translate3d(-originTranslate.x(), -originTranslate.y(), -originTranslate.z());
}

void RenderStyle::setPageScaleTransform(float scale)
{
    if (scale == 1)
        return;
    TransformOperations transform;
    transform.operations().append(ScaleTransformOperation::create(scale, scale, ScaleTransformOperation::SCALE));
    setTransform(transform);
    setTransformOriginX(Length(0, Fixed));
    setTransformOriginY(Length(0, Fixed));
}

void RenderStyle::setTextShadow(std::unique_ptr<ShadowData> shadowData, bool add)
{
    ASSERT(!shadowData || (!shadowData->spread() && shadowData->style() == ShadowStyle::Normal));

    auto& rareData = m_rareInheritedData.access();
    if (!add) {
        rareData.textShadow = WTFMove(shadowData);
        return;
    }

    shadowData->setNext(WTFMove(rareData.textShadow));
    rareData.textShadow = WTFMove(shadowData);
}

void RenderStyle::setBoxShadow(std::unique_ptr<ShadowData> shadowData, bool add)
{
    auto& rareData = m_rareNonInheritedData.access();
    if (!add) {
        rareData.boxShadow = WTFMove(shadowData);
        return;
    }

    shadowData->setNext(WTFMove(rareData.boxShadow));
    rareData.boxShadow = WTFMove(shadowData);
}

static RoundedRect::Radii calcRadiiFor(const BorderData& border, const LayoutSize& size)
{
    return {
        sizeForLengthSize(border.topLeftRadius(), size),
        sizeForLengthSize(border.topRightRadius(), size),
        sizeForLengthSize(border.bottomLeftRadius(), size),
        sizeForLengthSize(border.bottomRightRadius(), size)
    };
}

StyleImage* RenderStyle::listStyleImage() const
{
    return m_rareInheritedData->listStyleImage.get();
}

void RenderStyle::setListStyleImage(RefPtr<StyleImage>&& v)
{
    if (m_rareInheritedData->listStyleImage != v)
        m_rareInheritedData.access().listStyleImage = WTFMove(v);
}

const Color& RenderStyle::color() const
{
    return m_inheritedData->color;
}

const Color& RenderStyle::visitedLinkColor() const
{
    return m_inheritedData->visitedLinkColor;
}

void RenderStyle::setColor(const Color& v)
{
    SET_VAR(m_inheritedData, color, v);
}

void RenderStyle::setVisitedLinkColor(const Color& v)
{
    SET_VAR(m_inheritedData, visitedLinkColor, v);
}

float RenderStyle::horizontalBorderSpacing() const
{
    return m_inheritedData->horizontalBorderSpacing;
}

float RenderStyle::verticalBorderSpacing() const
{
    return m_inheritedData->verticalBorderSpacing;
}

void RenderStyle::setHorizontalBorderSpacing(float v)
{
    SET_VAR(m_inheritedData, horizontalBorderSpacing, v);
}

void RenderStyle::setVerticalBorderSpacing(float v)
{
    SET_VAR(m_inheritedData, verticalBorderSpacing, v);
}

RoundedRect RenderStyle::getRoundedBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    RoundedRect roundedRect(borderRect);
    if (hasBorderRadius()) {
        RoundedRect::Radii radii = calcRadiiFor(m_surroundData->border, borderRect.size());
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));
        roundedRect.includeLogicalEdges(radii, isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    return roundedRect;
}

RoundedRect RenderStyle::getRoundedInnerBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    bool horizontal = isHorizontalWritingMode();
    LayoutUnit leftWidth { (!horizontal || includeLogicalLeftEdge) ? borderLeftWidth() : 0 };
    LayoutUnit rightWidth { (!horizontal || includeLogicalRightEdge) ? borderRightWidth() : 0 };
    LayoutUnit topWidth { (horizontal || includeLogicalLeftEdge) ? borderTopWidth() : 0 };
    LayoutUnit bottomWidth { (horizontal || includeLogicalRightEdge) ? borderBottomWidth() : 0 };
    return getRoundedInnerBorderFor(borderRect, topWidth, bottomWidth, leftWidth, rightWidth, includeLogicalLeftEdge, includeLogicalRightEdge);
}

RoundedRect RenderStyle::getRoundedInnerBorderFor(const LayoutRect& borderRect, LayoutUnit topWidth, LayoutUnit bottomWidth,
    LayoutUnit leftWidth, LayoutUnit rightWidth, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    RoundedRect roundedRect { { borderRect.x() + leftWidth, borderRect.y() + topWidth,
        borderRect.width() - leftWidth - rightWidth, borderRect.height() - topWidth - bottomWidth } };
    if (hasBorderRadius()) {
        auto radii = getRoundedBorderFor(borderRect).radii();
        radii.shrink(topWidth, bottomWidth, leftWidth, rightWidth);
        roundedRect.includeLogicalEdges(radii, isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    return roundedRect;
}

static bool allLayersAreFixed(const FillLayer& layers)
{
    for (auto* layer = &layers; layer; layer = layer->next()) {
        if (!(layer->image() && layer->attachment() == FillAttachment::FixedBackground))
            return false;
    }
    return true;
}

bool RenderStyle::hasEntirelyFixedBackground() const
{
    return allLayersAreFixed(backgroundLayers());
}

const CounterDirectiveMap* RenderStyle::counterDirectives() const
{
    return m_rareNonInheritedData->counterDirectives.get();
}

CounterDirectiveMap& RenderStyle::accessCounterDirectives()
{
    auto& map = m_rareNonInheritedData.access().counterDirectives;
    if (!map)
        map = makeUnique<CounterDirectiveMap>();
    return *map;
}

const AtomString& RenderStyle::hyphenString() const
{
    ASSERT(hyphens() != Hyphens::None);

    auto& hyphenationString = m_rareInheritedData->hyphenationString;
    if (!hyphenationString.isNull())
        return hyphenationString;

    // FIXME: This should depend on locale.
    static MainThreadNeverDestroyed<const AtomString> hyphenMinusString(&hyphenMinus, 1);
    static MainThreadNeverDestroyed<const AtomString> hyphenString(&hyphen, 1);
    return fontCascade().primaryFont().glyphForCharacter(hyphen) ? hyphenString : hyphenMinusString;
}

const AtomString& RenderStyle::textEmphasisMarkString() const
{
    switch (textEmphasisMark()) {
    case TextEmphasisMark::None:
        return nullAtom();
    case TextEmphasisMark::Custom:
        return textEmphasisCustomMark();
    case TextEmphasisMark::Dot: {
        static MainThreadNeverDestroyed<const AtomString> filledDotString(&bullet, 1);
        static MainThreadNeverDestroyed<const AtomString> openDotString(&whiteBullet, 1);
        return textEmphasisFill() == TextEmphasisFill::Filled ? filledDotString : openDotString;
    }
    case TextEmphasisMark::Circle: {
        static MainThreadNeverDestroyed<const AtomString> filledCircleString(&blackCircle, 1);
        static MainThreadNeverDestroyed<const AtomString> openCircleString(&whiteCircle, 1);
        return textEmphasisFill() == TextEmphasisFill::Filled ? filledCircleString : openCircleString;
    }
    case TextEmphasisMark::DoubleCircle: {
        static MainThreadNeverDestroyed<const AtomString> filledDoubleCircleString(&fisheye, 1);
        static MainThreadNeverDestroyed<const AtomString> openDoubleCircleString(&bullseye, 1);
        return textEmphasisFill() == TextEmphasisFill::Filled ? filledDoubleCircleString : openDoubleCircleString;
    }
    case TextEmphasisMark::Triangle: {
        static MainThreadNeverDestroyed<const AtomString> filledTriangleString(&blackUpPointingTriangle, 1);
        static MainThreadNeverDestroyed<const AtomString> openTriangleString(&whiteUpPointingTriangle, 1);
        return textEmphasisFill() == TextEmphasisFill::Filled ? filledTriangleString : openTriangleString;
    }
    case TextEmphasisMark::Sesame: {
        static MainThreadNeverDestroyed<const AtomString> filledSesameString(&sesameDot, 1);
        static MainThreadNeverDestroyed<const AtomString> openSesameString(&whiteSesameDot, 1);
        return textEmphasisFill() == TextEmphasisFill::Filled ? filledSesameString : openSesameString;
    }
    case TextEmphasisMark::Auto:
        ASSERT_NOT_REACHED();
        return nullAtom();
    }

    ASSERT_NOT_REACHED();
    return nullAtom();
}

void RenderStyle::adjustAnimations()
{
    auto* animationList = m_rareNonInheritedData->animations.get();
    if (!animationList)
        return;

    // Get rid of empty animations and anything beyond them
    for (size_t i = 0, size = animationList->size(); i < size; ++i) {
        if (animationList->animation(i).isEmpty()) {
            animationList->resize(i);
            break;
        }
    }

    if (animationList->isEmpty()) {
        clearAnimations();
        return;
    }

    // Repeat patterns into layers that don't have some properties set.
    animationList->fillUnsetProperties();
}

void RenderStyle::adjustTransitions()
{
    auto* transitionList = m_rareNonInheritedData->transitions.get();
    if (!transitionList)
        return;

    // Get rid of empty transitions and anything beyond them
    for (size_t i = 0, size = transitionList->size(); i < size; ++i) {
        if (transitionList->animation(i).isEmpty()) {
            transitionList->resize(i);
            break;
        }
    }

    if (transitionList->isEmpty()) {
        clearTransitions();
        return;
    }

    // Repeat patterns into layers that don't have some properties set.
    transitionList->fillUnsetProperties();

    // Make sure there are no duplicate properties.
    // This is an O(n^2) algorithm but the lists tend to be short, so it is probably OK.
    for (size_t i = 0; i < transitionList->size(); ++i) {
        for (size_t j = i + 1; j < transitionList->size(); ++j) {
            if (transitionList->animation(i).property().id == transitionList->animation(j).property().id) {
                // toss i
                transitionList->remove(i);
                j = i;
            }
        }
    }
}

AnimationList& RenderStyle::ensureAnimations()
{
    if (!m_rareNonInheritedData.access().animations)
        m_rareNonInheritedData.access().animations = AnimationList::create();
    return *m_rareNonInheritedData->animations;
}

AnimationList& RenderStyle::ensureTransitions()
{
    if (!m_rareNonInheritedData.access().transitions)
        m_rareNonInheritedData.access().transitions = AnimationList::create();
    return *m_rareNonInheritedData->transitions;
}

const Animation* RenderStyle::transitionForProperty(CSSPropertyID property) const
{
    auto* transitions = this->transitions();
    if (!transitions)
        return nullptr;
    for (size_t i = 0, size = transitions->size(); i < size; ++i) {
        auto& animation = transitions->animation(i);
        if (animation.property().mode == Animation::TransitionMode::All || animation.property().id == property)
            return &animation;
    }
    return nullptr;
}

const FontCascade& RenderStyle::fontCascade() const
{
    return m_inheritedData->fontCascade;
}

const FontMetrics& RenderStyle::fontMetrics() const
{
    return m_inheritedData->fontCascade.fontMetrics();
}

const FontCascadeDescription& RenderStyle::fontDescription() const
{
    return m_inheritedData->fontCascade.fontDescription();
}

float RenderStyle::specifiedFontSize() const
{
    return fontDescription().specifiedSize();
}

float RenderStyle::computedFontSize() const
{
    return fontDescription().computedSize();
}

unsigned RenderStyle::computedFontPixelSize() const
{
    return fontDescription().computedPixelSize();
}

const Length& RenderStyle::wordSpacing() const
{
    return m_rareInheritedData->wordSpacing;
}

float RenderStyle::letterSpacing() const
{
    return m_inheritedData->fontCascade.letterSpacing();
}

bool RenderStyle::setFontDescription(FontCascadeDescription&& description)
{
    if (m_inheritedData->fontCascade.fontDescription() == description)
        return false;
    auto& cascade = m_inheritedData.access().fontCascade;
    cascade = { WTFMove(description), cascade.letterSpacing(), cascade.wordSpacing() };
    return true;
}

const Length& RenderStyle::specifiedLineHeight() const
{
#if ENABLE(TEXT_AUTOSIZING)
    return m_inheritedData->specifiedLineHeight;
#else
    return m_inheritedData->lineHeight;
#endif
}

#if ENABLE(TEXT_AUTOSIZING)

void RenderStyle::setSpecifiedLineHeight(Length&& height)
{
    SET_VAR(m_inheritedData, specifiedLineHeight, WTFMove(height));
}

#endif

const Length& RenderStyle::lineHeight() const
{
    return m_inheritedData->lineHeight;
}

void RenderStyle::setLineHeight(Length&& height)
{
    SET_VAR(m_inheritedData, lineHeight, WTFMove(height));
}

int RenderStyle::computedLineHeight() const
{
    return computeLineHeight(lineHeight());
}

int RenderStyle::computeLineHeight(const Length& lineHeightLength) const
{
    // Negative value means the line height is not set. Use the font's built-in spacing.
    if (lineHeightLength.isNegative())
        return fontMetrics().lineSpacing();

    if (lineHeightLength.isPercentOrCalculated())
        return minimumValueForLength(lineHeightLength, computedFontPixelSize());

    return clampTo<int>(lineHeightLength.value());
}

void RenderStyle::setWordSpacing(Length&& value)
{
    float fontWordSpacing;
    switch (value.type()) {
    case Auto:
        fontWordSpacing = 0;
        break;
    case Percent:
        fontWordSpacing = value.percent() * fontCascade().spaceWidth() / 100;
        break;
    case Fixed:
        fontWordSpacing = value.value();
        break;
    case Calculated:
        fontWordSpacing = value.nonNanCalculatedValue(maxValueForCssLength);
        break;
    default:
        ASSERT_NOT_REACHED();
        fontWordSpacing = 0;
        break;
    }
    m_inheritedData.access().fontCascade.setWordSpacing(fontWordSpacing);
    m_rareInheritedData.access().wordSpacing = WTFMove(value);
}

void RenderStyle::setLetterSpacing(float letterSpacing)
{
    FontSelector* currentFontSelector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setShouldDisableLigaturesForSpacing(letterSpacing);
    setFontDescription(WTFMove(description));
    fontCascade().update(currentFontSelector);

    setLetterSpacingWithoutUpdatingFontDescription(letterSpacing);
}

void RenderStyle::setLetterSpacingWithoutUpdatingFontDescription(float letterSpacing)
{
    m_inheritedData.access().fontCascade.setLetterSpacing(letterSpacing);
}

void RenderStyle::setFontSize(float size)
{
    // size must be specifiedSize if Text Autosizing is enabled, but computedSize if text
    // zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).

    ASSERT(std::isfinite(size));
    if (!std::isfinite(size) || size < 0)
        size = 0;
    else
        size = std::min(maximumAllowedFontSize, size);

    FontSelector* currentFontSelector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setSpecifiedSize(size);
    description.setComputedSize(size);

    setFontDescription(WTFMove(description));
    fontCascade().update(currentFontSelector);
}

#if ENABLE(VARIATION_FONTS)
void RenderStyle::setFontVariationSettings(FontVariationSettings settings)
{
    FontSelector* currentFontSelector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setVariationSettings(WTFMove(settings));

    setFontDescription(WTFMove(description));
    fontCascade().update(currentFontSelector);
}
#endif

void RenderStyle::setFontWeight(FontSelectionValue value)
{
    FontSelector* currentFontSelector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setWeight(value);

    setFontDescription(WTFMove(description));
    fontCascade().update(currentFontSelector);
}

void RenderStyle::setFontStretch(FontSelectionValue value)
{
    FontSelector* currentFontSelector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setStretch(value);

    setFontDescription(WTFMove(description));
    fontCascade().update(currentFontSelector);
}

void RenderStyle::setFontItalic(Optional<FontSelectionValue> value)
{
    FontSelector* currentFontSelector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setItalic(value);

    setFontDescription(WTFMove(description));
    fontCascade().update(currentFontSelector);
}

void RenderStyle::getShadowExtent(const ShadowData* shadow, LayoutUnit& top, LayoutUnit& right, LayoutUnit& bottom, LayoutUnit& left) const
{
    top = 0;
    right = 0;
    bottom = 0;
    left = 0;

    for ( ; shadow; shadow = shadow->next()) {
        if (shadow->style() == ShadowStyle::Inset)
            continue;

        auto extentAndSpread = shadow->paintingExtent() + shadow->spread();
        top = std::min<LayoutUnit>(top, shadow->y() - extentAndSpread);
        right = std::max<LayoutUnit>(right, shadow->x() + extentAndSpread);
        bottom = std::max<LayoutUnit>(bottom, shadow->y() + extentAndSpread);
        left = std::min<LayoutUnit>(left, shadow->x() - extentAndSpread);
    }
}

LayoutBoxExtent RenderStyle::getShadowInsetExtent(const ShadowData* shadow) const
{
    LayoutUnit top;
    LayoutUnit right;
    LayoutUnit bottom;
    LayoutUnit left;

    for ( ; shadow; shadow = shadow->next()) {
        if (shadow->style() == ShadowStyle::Normal)
            continue;

        auto extentAndSpread = shadow->paintingExtent() + shadow->spread();
        top = std::max<LayoutUnit>(top, shadow->y() + extentAndSpread);
        right = std::min<LayoutUnit>(right, shadow->x() - extentAndSpread);
        bottom = std::min<LayoutUnit>(bottom, shadow->y() - extentAndSpread);
        left = std::max<LayoutUnit>(left, shadow->x() + extentAndSpread);
    }

    return LayoutBoxExtent(WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left));
}

void RenderStyle::getShadowHorizontalExtent(const ShadowData* shadow, LayoutUnit &left, LayoutUnit &right) const
{
    left = 0;
    right = 0;

    for ( ; shadow; shadow = shadow->next()) {
        if (shadow->style() == ShadowStyle::Inset)
            continue;

        auto extentAndSpread = shadow->paintingExtent() + shadow->spread();
        left = std::min<LayoutUnit>(left, shadow->x() - extentAndSpread);
        right = std::max<LayoutUnit>(right, shadow->x() + extentAndSpread);
    }
}

void RenderStyle::getShadowVerticalExtent(const ShadowData* shadow, LayoutUnit &top, LayoutUnit &bottom) const
{
    top = 0;
    bottom = 0;

    for ( ; shadow; shadow = shadow->next()) {
        if (shadow->style() == ShadowStyle::Inset)
            continue;

        auto extentAndSpread = shadow->paintingExtent() + shadow->spread();
        top = std::min<LayoutUnit>(top, shadow->y() - extentAndSpread);
        bottom = std::max<LayoutUnit>(bottom, shadow->y() + extentAndSpread);
    }
}

Color RenderStyle::unresolvedColorForProperty(CSSPropertyID colorProperty, bool visitedLink) const
{
    switch (colorProperty) {
    case CSSPropertyColor:
        return visitedLink ? visitedLinkColor() : color();
    case CSSPropertyBackgroundColor:
        return visitedLink ? visitedLinkBackgroundColor() : backgroundColor();
    case CSSPropertyBorderBottomColor:
        return visitedLink ? visitedLinkBorderBottomColor() : borderBottomColor();
    case CSSPropertyBorderLeftColor:
        return visitedLink ? visitedLinkBorderLeftColor() : borderLeftColor();
    case CSSPropertyBorderRightColor:
        return visitedLink ? visitedLinkBorderRightColor() : borderRightColor();
    case CSSPropertyBorderTopColor:
        return visitedLink ? visitedLinkBorderTopColor() : borderTopColor();
    case CSSPropertyFill:
        return fillPaintColor();
    case CSSPropertyFloodColor:
        return floodColor();
    case CSSPropertyLightingColor:
        return lightingColor();
    case CSSPropertyOutlineColor:
        return visitedLink ? visitedLinkOutlineColor() : outlineColor();
    case CSSPropertyStopColor:
        return stopColor();
    case CSSPropertyStroke:
        return strokePaintColor();
    case CSSPropertyStrokeColor:
        return visitedLink ? visitedLinkStrokeColor() : strokeColor();
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineStartColor:
        return unresolvedColorForProperty(CSSProperty::resolveDirectionAwareProperty(colorProperty, direction(), writingMode()));
    case CSSPropertyColumnRuleColor:
        return visitedLink ? visitedLinkColumnRuleColor() : columnRuleColor();
    case CSSPropertyWebkitTextEmphasisColor:
        return visitedLink ? visitedLinkTextEmphasisColor() : textEmphasisColor();
    case CSSPropertyWebkitTextFillColor:
        return visitedLink ? visitedLinkTextFillColor() : textFillColor();
    case CSSPropertyWebkitTextStrokeColor:
        return visitedLink ? visitedLinkTextStrokeColor() : textStrokeColor();
    case CSSPropertyTextDecorationColor:
        return visitedLink ? visitedLinkTextDecorationColor() : textDecorationColor();
    case CSSPropertyCaretColor:
        return visitedLink ? visitedLinkCaretColor() : caretColor();
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return { };
}

Color RenderStyle::colorResolvingCurrentColor(CSSPropertyID colorProperty, bool visitedLink) const
{
    auto computeBorderStyle = [&] {
        switch (colorProperty) {
        case CSSPropertyBorderLeftColor:
            return borderLeftStyle();
        case CSSPropertyBorderRightColor:
            return borderRightStyle();
        case CSSPropertyBorderTopColor:
            return borderTopStyle();
        case CSSPropertyBorderBottomColor:
            return borderBottomStyle();
        default:
            return BorderStyle::None;
        }
    };

    auto result = unresolvedColorForProperty(colorProperty, visitedLink);

    if (isCurrentColor(result)) {
        if (colorProperty == CSSPropertyTextDecorationColor) {
            if (hasPositiveStrokeWidth()) {
                // Prefer stroke color if possible but not if it's fully transparent.
                auto strokeColor = colorResolvingCurrentColor(effectiveStrokeColorProperty(), visitedLink);
                if (strokeColor.isVisible())
                    return strokeColor;
            }

            return colorResolvingCurrentColor(CSSPropertyWebkitTextFillColor, visitedLink);
        }

        auto borderStyle = computeBorderStyle();
        if (!visitedLink && (borderStyle == BorderStyle::Inset || borderStyle == BorderStyle::Outset || borderStyle == BorderStyle::Ridge || borderStyle == BorderStyle::Groove))
            return SRGBA<uint8_t> { 238, 238, 238 };

        return visitedLink ? visitedLinkColor() : color();
    }

    return result;
}

Color RenderStyle::colorResolvingCurrentColor(const Color& color) const
{
    if (isCurrentColor(color))
        return this->color();

    return color;
}

Color RenderStyle::visitedDependentColor(CSSPropertyID colorProperty) const
{
    Color unvisitedColor = colorResolvingCurrentColor(colorProperty, false);
    if (insideLink() != InsideLink::InsideVisited)
        return unvisitedColor;

    Color visitedColor = colorResolvingCurrentColor(colorProperty, true);

    // FIXME: Technically someone could explicitly specify the color transparent, but for now we'll just
    // assume that if the background color is transparent that it wasn't set. Note that it's weird that
    // we're returning unvisited info for a visited link, but given our restriction that the alpha values
    // have to match, it makes more sense to return the unvisited background color if specified than it
    // does to return black. This behavior matches what Firefox 4 does as well.
    if (colorProperty == CSSPropertyBackgroundColor && visitedColor == Color::transparentBlack)
        return unvisitedColor;

    // Take the alpha from the unvisited color, but get the RGB values from the visited color.
    return visitedColor.colorWithAlpha(unvisitedColor.alphaAsFloat());
}

Color RenderStyle::visitedDependentColorWithColorFilter(CSSPropertyID colorProperty) const
{
    if (!hasAppleColorFilter())
        return visitedDependentColor(colorProperty);

    return colorByApplyingColorFilter(visitedDependentColor(colorProperty));
}

Color RenderStyle::colorByApplyingColorFilter(const Color& color) const
{
    Color transformedColor = color;
    appleColorFilter().transformColor(transformedColor);
    return transformedColor;
}

const BorderValue& RenderStyle::borderBefore() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderTop();
    case BottomToTopWritingMode:
        return borderBottom();
    case LeftToRightWritingMode:
        return borderLeft();
    case RightToLeftWritingMode:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

const BorderValue& RenderStyle::borderAfter() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderBottom();
    case BottomToTopWritingMode:
        return borderTop();
    case LeftToRightWritingMode:
        return borderRight();
    case RightToLeftWritingMode:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderBottom();
}

const BorderValue& RenderStyle::borderStart() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderLeft() : borderRight();
    return isLeftToRightDirection() ? borderTop() : borderBottom();
}

const BorderValue& RenderStyle::borderEnd() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderRight() : borderLeft();
    return isLeftToRightDirection() ? borderBottom() : borderTop();
}

float RenderStyle::borderBeforeWidth() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderTopWidth();
    case BottomToTopWritingMode:
        return borderBottomWidth();
    case LeftToRightWritingMode:
        return borderLeftWidth();
    case RightToLeftWritingMode:
        return borderRightWidth();
    }
    ASSERT_NOT_REACHED();
    return borderTopWidth();
}

float RenderStyle::borderAfterWidth() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderBottomWidth();
    case BottomToTopWritingMode:
        return borderTopWidth();
    case LeftToRightWritingMode:
        return borderRightWidth();
    case RightToLeftWritingMode:
        return borderLeftWidth();
    }
    ASSERT_NOT_REACHED();
    return borderBottomWidth();
}

float RenderStyle::borderStartWidth() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderLeftWidth() : borderRightWidth();
    return isLeftToRightDirection() ? borderTopWidth() : borderBottomWidth();
}

float RenderStyle::borderEndWidth() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderRightWidth() : borderLeftWidth();
    return isLeftToRightDirection() ? borderBottomWidth() : borderTopWidth();
}

void RenderStyle::setMarginStart(Length&& margin)
{
    if (isHorizontalWritingMode()) {
        if (isLeftToRightDirection())
            setMarginLeft(WTFMove(margin));
        else
            setMarginRight(WTFMove(margin));
    } else {
        if (isLeftToRightDirection())
            setMarginTop(WTFMove(margin));
        else
            setMarginBottom(WTFMove(margin));
    }
}

void RenderStyle::setMarginEnd(Length&& margin)
{
    if (isHorizontalWritingMode()) {
        if (isLeftToRightDirection())
            setMarginRight(WTFMove(margin));
        else
            setMarginLeft(WTFMove(margin));
    } else {
        if (isLeftToRightDirection())
            setMarginBottom(WTFMove(margin));
        else
            setMarginTop(WTFMove(margin));
    }
}

TextEmphasisMark RenderStyle::textEmphasisMark() const
{
    auto mark = static_cast<TextEmphasisMark>(m_rareInheritedData->textEmphasisMark);
    if (mark != TextEmphasisMark::Auto)
        return mark;
    if (isHorizontalWritingMode())
        return TextEmphasisMark::Dot;
    return TextEmphasisMark::Sesame;
}

#if ENABLE(TOUCH_EVENTS)

Color RenderStyle::initialTapHighlightColor()
{
    return RenderTheme::tapHighlightColor();
}

#endif

LayoutBoxExtent RenderStyle::imageOutsets(const NinePieceImage& image) const
{
    return {
        NinePieceImage::computeOutset(image.outset().top(), LayoutUnit(borderTopWidth())),
        NinePieceImage::computeOutset(image.outset().right(), LayoutUnit(borderRightWidth())),
        NinePieceImage::computeOutset(image.outset().bottom(), LayoutUnit(borderBottomWidth())),
        NinePieceImage::computeOutset(image.outset().left(), LayoutUnit(borderLeftWidth()))
    };
}

std::pair<FontOrientation, NonCJKGlyphOrientation> RenderStyle::fontAndGlyphOrientation()
{
    // FIXME: TextOrientationSideways should map to sideways-left in vertical-lr, which is not supported yet.

    if (isHorizontalWritingMode())
        return { FontOrientation::Horizontal, NonCJKGlyphOrientation::Mixed };

    switch (textOrientation()) {
    case TextOrientation::Mixed:
        return { FontOrientation::Vertical, NonCJKGlyphOrientation::Mixed };
    case TextOrientation::Upright:
        return { FontOrientation::Vertical, NonCJKGlyphOrientation::Upright };
    case TextOrientation::Sideways:
        return { FontOrientation::Horizontal, NonCJKGlyphOrientation::Mixed };
    default:
        ASSERT_NOT_REACHED();
        return { FontOrientation::Horizontal, NonCJKGlyphOrientation::Mixed };
    }
}

void RenderStyle::setBorderImageSource(RefPtr<StyleImage>&& image)
{
    if (m_surroundData->border.m_image.image() == image.get())
        return;
    m_surroundData.access().border.m_image.setImage(WTFMove(image));
}

void RenderStyle::setBorderImageSlices(LengthBox&& slices)
{
    if (m_surroundData->border.m_image.imageSlices() == slices)
        return;
    m_surroundData.access().border.m_image.setImageSlices(WTFMove(slices));
}

void RenderStyle::setBorderImageWidth(LengthBox&& slices)
{
    if (m_surroundData->border.m_image.borderSlices() == slices)
        return;
    m_surroundData.access().border.m_image.setBorderSlices(WTFMove(slices));
}

void RenderStyle::setBorderImageOutset(LengthBox&& outset)
{
    if (m_surroundData->border.m_image.outset() == outset)
        return;
    m_surroundData.access().border.m_image.setOutset(WTFMove(outset));
}

void RenderStyle::setColumnStylesFromPaginationMode(const Pagination::Mode& paginationMode)
{
    if (paginationMode == Pagination::Unpaginated)
        return;
    
    setColumnFill(ColumnFill::Auto);
    
    switch (paginationMode) {
    case Pagination::LeftToRightPaginated:
        setColumnAxis(ColumnAxis::Horizontal);
        if (isHorizontalWritingMode())
            setColumnProgression(isLeftToRightDirection() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        else
            setColumnProgression(isFlippedBlocksWritingMode() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        break;
    case Pagination::RightToLeftPaginated:
        setColumnAxis(ColumnAxis::Horizontal);
        if (isHorizontalWritingMode())
            setColumnProgression(isLeftToRightDirection() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        else
            setColumnProgression(isFlippedBlocksWritingMode() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        break;
    case Pagination::TopToBottomPaginated:
        setColumnAxis(ColumnAxis::Vertical);
        if (isHorizontalWritingMode())
            setColumnProgression(isFlippedBlocksWritingMode() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        else
            setColumnProgression(isLeftToRightDirection() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        break;
    case Pagination::BottomToTopPaginated:
        setColumnAxis(ColumnAxis::Vertical);
        if (isHorizontalWritingMode())
            setColumnProgression(isFlippedBlocksWritingMode() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        else
            setColumnProgression(isLeftToRightDirection() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        break;
    case Pagination::Unpaginated:
        ASSERT_NOT_REACHED();
        break;
    }
}

#if ENABLE(CSS_SCROLL_SNAP)

ScrollSnapType RenderStyle::initialScrollSnapType()
{
    return { };
}

ScrollSnapAlign RenderStyle::initialScrollSnapAlign()
{
    return { };
}

const StyleScrollSnapArea& RenderStyle::scrollSnapArea() const
{
    return *m_rareNonInheritedData->scrollSnapArea;
}

const StyleScrollSnapPort& RenderStyle::scrollSnapPort() const
{
    return *m_rareNonInheritedData->scrollSnapPort;
}

const ScrollSnapType& RenderStyle::scrollSnapType() const
{
    return m_rareNonInheritedData->scrollSnapPort->type;
}

const LengthBox& RenderStyle::scrollPadding() const
{
    return m_rareNonInheritedData->scrollSnapPort->scrollPadding;
}

const Length& RenderStyle::scrollPaddingTop() const
{
    return scrollPadding().top();
}

const Length& RenderStyle::scrollPaddingBottom() const
{
    return scrollPadding().bottom();
}

const Length& RenderStyle::scrollPaddingLeft() const
{
    return scrollPadding().left();
}

const Length& RenderStyle::scrollPaddingRight() const
{
    return scrollPadding().right();
}

const ScrollSnapAlign& RenderStyle::scrollSnapAlign() const
{
    return m_rareNonInheritedData->scrollSnapArea->alignment;
}

const LengthBox& RenderStyle::scrollSnapMargin() const
{
    return m_rareNonInheritedData->scrollSnapArea->scrollSnapMargin;
}

const Length& RenderStyle::scrollSnapMarginTop() const
{
    return scrollSnapMargin().top();
}

const Length& RenderStyle::scrollSnapMarginBottom() const
{
    return scrollSnapMargin().bottom();
}

const Length& RenderStyle::scrollSnapMarginLeft() const
{
    return scrollSnapMargin().left();
}

const Length& RenderStyle::scrollSnapMarginRight() const
{
    return scrollSnapMargin().right();
}

void RenderStyle::setScrollSnapType(const ScrollSnapType& type)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapPort, type, type);
}

void RenderStyle::setScrollPaddingTop(Length&& length)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapPort, scrollPadding.top(), WTFMove(length));
}

void RenderStyle::setScrollPaddingBottom(Length&& length)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapPort, scrollPadding.bottom(), WTFMove(length));
}

void RenderStyle::setScrollPaddingLeft(Length&& length)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapPort, scrollPadding.left(), WTFMove(length));
}

void RenderStyle::setScrollPaddingRight(Length&& length)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapPort, scrollPadding.right(), WTFMove(length));
}

void RenderStyle::setScrollSnapAlign(const ScrollSnapAlign& alignment)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapArea, alignment, alignment);
}

void RenderStyle::setScrollSnapMarginTop(Length&& length)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapArea, scrollSnapMargin.top(), WTFMove(length));
}

void RenderStyle::setScrollSnapMarginBottom(Length&& length)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapArea, scrollSnapMargin.bottom(), WTFMove(length));
}

void RenderStyle::setScrollSnapMarginLeft(Length&& length)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapArea, scrollSnapMargin.left(), WTFMove(length));
}

void RenderStyle::setScrollSnapMarginRight(Length&& length)
{
    SET_NESTED_VAR(m_rareNonInheritedData, scrollSnapArea, scrollSnapMargin.right(), WTFMove(length));
}

#endif

bool RenderStyle::hasReferenceFilterOnly() const
{
    if (!hasFilter())
        return false;
    auto& filterOperations = m_rareNonInheritedData->filter->operations;
    return filterOperations.size() == 1 && filterOperations.at(0)->type() == FilterOperation::REFERENCE;
}

float RenderStyle::outlineWidth() const
{
    if (m_backgroundData->outline.style() == BorderStyle::None)
        return 0;
    if (outlineStyleIsAuto() == OutlineIsAuto::On)
        return std::max(m_backgroundData->outline.width(), RenderTheme::platformFocusRingWidth());
    return m_backgroundData->outline.width();
}

float RenderStyle::outlineOffset() const
{
    if (outlineStyleIsAuto() == OutlineIsAuto::On)
        return (m_backgroundData->outline.offset() + RenderTheme::platformFocusRingOffset(outlineWidth()));
    return m_backgroundData->outline.offset();
}

bool RenderStyle::shouldPlaceBlockDirectionScrollbarOnLeft() const
{
    return !isLeftToRightDirection() && isHorizontalWritingMode();
}

Vector<PaintType, 3> RenderStyle::paintTypesForPaintOrder(PaintOrder order)
{
    Vector<PaintType, 3> paintOrder;
    switch (order) {
    case PaintOrder::Normal:
        FALLTHROUGH;
    case PaintOrder::Fill:
        paintOrder.append(PaintType::Fill);
        paintOrder.append(PaintType::Stroke);
        paintOrder.append(PaintType::Markers);
        break;
    case PaintOrder::FillMarkers:
        paintOrder.append(PaintType::Fill);
        paintOrder.append(PaintType::Markers);
        paintOrder.append(PaintType::Stroke);
        break;
    case PaintOrder::Stroke:
        paintOrder.append(PaintType::Stroke);
        paintOrder.append(PaintType::Fill);
        paintOrder.append(PaintType::Markers);
        break;
    case PaintOrder::StrokeMarkers:
        paintOrder.append(PaintType::Stroke);
        paintOrder.append(PaintType::Markers);
        paintOrder.append(PaintType::Fill);
        break;
    case PaintOrder::Markers:
        paintOrder.append(PaintType::Markers);
        paintOrder.append(PaintType::Fill);
        paintOrder.append(PaintType::Stroke);
        break;
    case PaintOrder::MarkersStroke:
        paintOrder.append(PaintType::Markers);
        paintOrder.append(PaintType::Stroke);
        paintOrder.append(PaintType::Fill);
        break;
    };
    return paintOrder;
}

float RenderStyle::computedStrokeWidth(const IntSize& viewportSize) const
{
    // Use the stroke-width and stroke-color value combination only if stroke-color has been explicitly specified.
    // Since there will be no visible stroke when stroke-color is not specified (transparent by default), we fall
    // back to the legacy Webkit text stroke combination in that case.
    if (!hasExplicitlySetStrokeColor())
        return textStrokeWidth();
    
    const Length& length = strokeWidth();

    if (length.isPercent()) {
        // According to the spec, https://drafts.fxtf.org/paint/#stroke-width, the percentage is relative to the scaled viewport size.
        // The scaled viewport size is the geometric mean of the viewport width and height.
        ExceptionOr<float> result = length.value() * (viewportSize.width() + viewportSize.height()) / 200.0f;
        if (result.hasException())
            return 0;
        return result.releaseReturnValue();
    }
    
    if (length.isAuto() || !length.isSpecified())
        return 0;
    
    return floatValueForLength(length, viewportSize.width());
}

bool RenderStyle::hasPositiveStrokeWidth() const
{
    if (!hasExplicitlySetStrokeWidth())
        return textStrokeWidth() > 0;

    return strokeWidth().isPositive();
}

Color RenderStyle::computedStrokeColor() const
{
    return visitedDependentColor(effectiveStrokeColorProperty());
}

} // namespace WebCore
