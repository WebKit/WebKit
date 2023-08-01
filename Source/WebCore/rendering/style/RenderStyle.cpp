/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

#include "CSSCustomPropertyValue.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "ComputedStyleExtractor.h"
#include "ContentData.h"
#include "CursorList.h"
#include "CustomPropertyRegistry.h"
#include "FloatRoundedRect.h"
#include "FontCascade.h"
#include "FontSelector.h"
#include "InlineIteratorTextBox.h"
#include "InlineTextBoxStyle.h"
#include "Pagination.h"
#include "PathTraversalState.h"
#include "QuotesData.h"
#include "RenderBlock.h"
#include "RenderObject.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include "ScaleTransformOperation.h"
#include "ScrollbarGutter.h"
#include "ShadowData.h"
#include "StyleBuilderConverter.h"
#include "StyleImage.h"
#include "StyleInheritedData.h"
#include "StyleResolver.h"
#include "StyleScrollSnapPoints.h"
#include "StyleSelfAlignmentData.h"
#include "StyleTextBoxEdge.h"
#include "StyleTreeResolver.h"
#include <algorithm>
#include <wtf/MathExtras.h>
#include <wtf/PointerComparison.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(TEXT_AUTOSIZING)
#include <wtf/text/StringHash.h>
#endif

#define SET_VAR(group, variable, value) do { \
        if (!compareEqual(group->variable, value)) \
            group.access().variable = value; \
    } while (0)

#define SET_NESTED_VAR(group, parentVariable, variable, value) do { \
        if (!compareEqual(group->parentVariable->variable, value)) \
            group.access().parentVariable.access().variable = value; \
    } while (0)

namespace WebCore {

struct SameSizeAsBorderValue {
    StyleColor m_color;
    float m_width;
    int m_restBits;
};

static_assert(sizeof(BorderValue) == sizeof(SameSizeAsBorderValue), "BorderValue should not grow");

struct SameSizeAsRenderStyle {
    void* nonInheritedDataRefs[1];
    struct NonInheritedFlags {
        unsigned m_bitfields[2];
    } m_nonInheritedFlags;
    void* inheritedDataRefs[2];
    struct InheritedFlags {
        unsigned m_bitfields[2];
    } m_inheritedFlags;
    void* pseudos;
    void* dataRefSvgStyle;

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionCheck;
#endif
};

static_assert(sizeof(RenderStyle) == sizeof(SameSizeAsRenderStyle), "RenderStyle should stay small");

static_assert(PublicPseudoIDBits == static_cast<int>(PseudoId::FirstInternalPseudoId) - static_cast<int>(PseudoId::FirstPublicPseudoId));

static_assert(!(static_cast<unsigned>(maxTextDecorationLineValue) >> TextDecorationLineBits));

static_assert(!(static_cast<unsigned>(maxTextTransformValue) >> TextTransformBits));

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PseudoStyleCache);
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

std::unique_ptr<RenderStyle> RenderStyle::createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry& registry)
{
    return clonePtr(registry.initialValuePrototypeStyle());
}

RenderStyle RenderStyle::clone(const RenderStyle& style)
{
    return RenderStyle(style, Clone);
}

RenderStyle RenderStyle::cloneIncludingPseudoElements(const RenderStyle& style)
{
    auto newStyle = RenderStyle(style, Clone);
    newStyle.copyPseudoElementsFrom(style);
    return newStyle;
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
    : m_nonInheritedData(StyleNonInheritedData::create())
    , m_rareInheritedData(StyleRareInheritedData::create())
    , m_inheritedData(StyleInheritedData::create())
    , m_svgStyle(SVGRenderStyle::create())
{
    m_inheritedFlags.emptyCells = static_cast<unsigned>(initialEmptyCells());
    m_inheritedFlags.captionSide = static_cast<unsigned>(initialCaptionSide());
    m_inheritedFlags.listStylePosition = static_cast<unsigned>(initialListStylePosition());
    m_inheritedFlags.visibility = static_cast<unsigned>(initialVisibility());
    m_inheritedFlags.textAlign = static_cast<unsigned>(initialTextAlign());
    m_inheritedFlags.textTransform = initialTextTransform().toRaw();
    m_inheritedFlags.textDecorationLines = initialTextDecorationLine().toRaw();
    m_inheritedFlags.cursor = static_cast<unsigned>(initialCursor());
#if ENABLE(CURSOR_VISIBILITY)
    m_inheritedFlags.cursorVisibility = static_cast<unsigned>(initialCursorVisibility());
#endif
    m_inheritedFlags.direction = static_cast<unsigned>(initialDirection());
    m_inheritedFlags.whiteSpaceCollapse = static_cast<unsigned>(initialWhiteSpaceCollapse());
    m_inheritedFlags.textWrap = static_cast<unsigned>(initialTextWrap());
    m_inheritedFlags.borderCollapse = static_cast<unsigned>(initialBorderCollapse());
    m_inheritedFlags.rtlOrdering = static_cast<unsigned>(initialRTLOrdering());
    m_inheritedFlags.boxDirection = static_cast<unsigned>(initialBoxDirection());
    m_inheritedFlags.printColorAdjust = static_cast<unsigned>(initialPrintColorAdjust());
    m_inheritedFlags.pointerEvents = static_cast<unsigned>(initialPointerEvents());
    m_inheritedFlags.insideLink = static_cast<unsigned>(InsideLink::NotInside);
    m_inheritedFlags.insideDefaultButton = false;
    m_inheritedFlags.writingMode = static_cast<unsigned>(initialWritingMode());
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
    m_nonInheritedFlags.unicodeBidi = static_cast<unsigned>(initialUnicodeBidi());
    m_nonInheritedFlags.floating = static_cast<unsigned>(initialFloating());
    m_nonInheritedFlags.tableLayout = static_cast<unsigned>(initialTableLayout());
    m_nonInheritedFlags.textDecorationLine = initialTextDecorationLine().toRaw();
    m_nonInheritedFlags.hasExplicitlySetDirection = false;
    m_nonInheritedFlags.hasExplicitlySetWritingMode = false;
    m_nonInheritedFlags.usesViewportUnits = false;
    m_nonInheritedFlags.usesContainerUnits = false;
    m_nonInheritedFlags.hasExplicitlyInheritedProperties = false;
    m_nonInheritedFlags.disallowsFastPathInheritance = false;
    m_nonInheritedFlags.isUnique = false;
    m_nonInheritedFlags.emptyState = false;
    m_nonInheritedFlags.firstChildState = false;
    m_nonInheritedFlags.lastChildState = false;
    m_nonInheritedFlags.isLink = false;
    m_nonInheritedFlags.hasContentNone = false;
    m_nonInheritedFlags.styleType = static_cast<unsigned>(PseudoId::None);
    m_nonInheritedFlags.pseudoBits = static_cast<unsigned>(PseudoId::None);

    static_assert((sizeof(InheritedFlags) <= 8), "InheritedFlags does not grow");
    static_assert((sizeof(NonInheritedFlags) <= 8), "NonInheritedFlags does not grow");
}

inline RenderStyle::RenderStyle(const RenderStyle& other, CloneTag)
    : m_nonInheritedData(other.m_nonInheritedData)
    , m_nonInheritedFlags(other.m_nonInheritedFlags)
    , m_rareInheritedData(other.m_rareInheritedData)
    , m_inheritedData(other.m_inheritedData)
    , m_inheritedFlags(other.m_inheritedFlags)
    , m_svgStyle(other.m_svgStyle)
{
}

inline RenderStyle::RenderStyle(RenderStyle& a, RenderStyle&& b)
    : m_nonInheritedData(a.m_nonInheritedData.replace(WTFMove(b.m_nonInheritedData)))
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

void RenderStyle::inheritIgnoringCustomPropertiesFrom(const RenderStyle& inheritParent)
{
    auto oldCustomProperties = m_rareInheritedData->customProperties;
    inheritFrom(inheritParent);
    if (oldCustomProperties != m_rareInheritedData->customProperties)
        m_rareInheritedData.access().customProperties = oldCustomProperties;
}

void RenderStyle::fastPathInheritFrom(const RenderStyle& inheritParent)
{
    ASSERT(!disallowsFastPathInheritance());

    // FIXME: Use this mechanism for other properties too, like variables.
    m_inheritedFlags.visibility = inheritParent.m_inheritedFlags.visibility;

    if (m_inheritedData.ptr() != inheritParent.m_inheritedData.ptr()) {
        if (m_inheritedData->nonFastPathInheritedEqual(*inheritParent.m_inheritedData)) {
            m_inheritedData = inheritParent.m_inheritedData;
            return;
        }
        m_inheritedData.access().fastPathInheritFrom(*inheritParent.m_inheritedData);
    }
}

inline void RenderStyle::NonInheritedFlags::copyNonInheritedFrom(const NonInheritedFlags& other)
{
    // Only some flags are copied because NonInheritedFlags contains things that are not actually style data.
    clear = other.clear;
    disallowsFastPathInheritance = other.disallowsFastPathInheritance;
    effectiveDisplay = other.effectiveDisplay;
    floating = other.floating;
    hasExplicitlyInheritedProperties = other.hasExplicitlyInheritedProperties;
    originalDisplay = other.originalDisplay;
    overflowX = other.overflowX;
    overflowY = other.overflowY;
    position = other.position;
    tableLayout = other.tableLayout;
    textDecorationLine = other.textDecorationLine;
    unicodeBidi = other.unicodeBidi;
    usesContainerUnits = other.usesContainerUnits;
    usesViewportUnits = other.usesViewportUnits;
    verticalAlign = other.verticalAlign;
}

void RenderStyle::copyNonInheritedFrom(const RenderStyle& other)
{
    m_nonInheritedData = other.m_nonInheritedData;
    m_nonInheritedFlags.copyNonInheritedFrom(other.m_nonInheritedFlags);

    if (m_svgStyle != other.m_svgStyle)
        m_svgStyle.access().copyNonInheritedFrom(other.m_svgStyle.get());

    ASSERT(zoom() == initialZoom());
}

void RenderStyle::copyContentFrom(const RenderStyle& other)
{
    if (!other.m_nonInheritedData->miscData->content)
        return;
    m_nonInheritedData.access().miscData.access().content = other.m_nonInheritedData->miscData->content->clone();
}

void RenderStyle::copyPseudoElementsFrom(const RenderStyle& other)
{
    if (!other.m_cachedPseudoStyles)
        return;

    for (auto& pseudoElementStyle : other.m_cachedPseudoStyles->styles)
        addCachedPseudoStyle(makeUnique<RenderStyle>(cloneIncludingPseudoElements(*pseudoElementStyle)));
}

bool RenderStyle::operator==(const RenderStyle& other) const
{
    // compare everything except the pseudoStyle pointer
    return m_inheritedFlags == other.m_inheritedFlags
        && m_nonInheritedFlags == other.m_nonInheritedFlags
        && m_nonInheritedData == other.m_nonInheritedData
        && m_rareInheritedData == other.m_rareInheritedData
        && m_inheritedData == other.m_inheritedData
        && m_svgStyle == other.m_svgStyle;
}

bool RenderStyle::hasUniquePseudoStyle() const
{
    if (!m_cachedPseudoStyles || styleType() != PseudoId::None)
        return false;

    for (auto& pseudoStyle : m_cachedPseudoStyles->styles) {
        if (pseudoStyle->unique())
            return true;
    }

    return false;
}

RenderStyle* RenderStyle::getCachedPseudoStyle(PseudoId pid) const
{
    if (!m_cachedPseudoStyles)
        return nullptr;

    for (auto& pseudoStyle : m_cachedPseudoStyles->styles) {
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

    m_cachedPseudoStyles->styles.append(WTFMove(pseudo));

    return result;
}

bool RenderStyle::inheritedEqual(const RenderStyle& other) const
{
    return m_inheritedFlags == other.m_inheritedFlags
        && m_inheritedData == other.m_inheritedData
        && (m_svgStyle.ptr() == other.m_svgStyle.ptr() || m_svgStyle->inheritedEqual(other.m_svgStyle))
        && m_rareInheritedData == other.m_rareInheritedData;
}

bool RenderStyle::inheritedCustomPropertiesEqual(const RenderStyle& other) const
{
    return m_rareInheritedData->customProperties == other.m_rareInheritedData->customProperties;
}

bool RenderStyle::fastPathInheritedEqual(const RenderStyle& other) const
{
    if (m_inheritedFlags.visibility != other.m_inheritedFlags.visibility)
        return false;
    if (m_inheritedData.ptr() == other.m_inheritedData.ptr())
        return true;
    return m_inheritedData->fastPathInheritedEqual(*other.m_inheritedData);
}

bool RenderStyle::nonFastPathInheritedEqual(const RenderStyle& other) const
{
    auto withoutFastPathFlags = [](auto flags) {
        flags.visibility = 0;
        return flags;
    };
    if (withoutFastPathFlags(m_inheritedFlags) != withoutFastPathFlags(other.m_inheritedFlags))
        return false;
    if (m_inheritedData.ptr() != other.m_inheritedData.ptr() && !m_inheritedData->nonFastPathInheritedEqual(*other.m_inheritedData))
        return false;
    if (m_rareInheritedData != other.m_rareInheritedData)
        return false;
    if (m_svgStyle.ptr() != other.m_svgStyle.ptr() && !m_svgStyle->inheritedEqual(other.m_svgStyle))
        return false;
    return true;
}

bool RenderStyle::descendantAffectingNonInheritedPropertiesEqual(const RenderStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->miscData.ptr() == other.m_nonInheritedData->miscData.ptr())
        return true;

    return m_nonInheritedData->miscData->alignItems == other.m_nonInheritedData->miscData->alignItems
        && m_nonInheritedData->miscData->justifyItems == other.m_nonInheritedData->miscData->justifyItems;
}

bool RenderStyle::borderAndBackgroundEqual(const RenderStyle& other) const
{
    return border() == other.border()
        && backgroundLayers() == other.backgroundLayers()
        && backgroundColor() == other.backgroundColor();
}

#if ENABLE(TEXT_AUTOSIZING)

static inline unsigned computeFontHash(const FontCascade& font)
{
    // FIXME: Would be better to hash the family name rather than hashing a hash of the family name. Also, should this use FontCascadeDescription::familyNameHash?
    return computeHash(ASCIICaseInsensitiveHash::hash(font.fontDescription().firstFamily()), font.fontDescription().specifiedSize());
}

unsigned RenderStyle::hashForTextAutosizing() const
{
    // FIXME: Not a very smart hash. Could be improved upon. See <https://bugs.webkit.org/show_bug.cgi?id=121131>.
    unsigned hash = m_nonInheritedData->miscData->effectiveAppearance;
    hash ^= m_nonInheritedData->rareData->lineClamp.value();
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
    hash ^= m_nonInheritedData->miscData->textOverflow;
    hash ^= m_rareInheritedData->textSecurity;
    return hash;
}

bool RenderStyle::equalForTextAutosizing(const RenderStyle& other) const
{
    return m_nonInheritedData->miscData->effectiveAppearance == other.m_nonInheritedData->miscData->effectiveAppearance
        && m_nonInheritedData->rareData->lineClamp == other.m_nonInheritedData->rareData->lineClamp
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
        && m_nonInheritedData->miscData->textOverflow == other.m_nonInheritedData->miscData->textOverflow;
}

bool RenderStyle::isIdempotentTextAutosizingCandidate() const
{
    return isIdempotentTextAutosizingCandidate(OptionSet<AutosizeStatus::Fields>::fromRaw(m_inheritedFlags.autosizeStatus));
}

bool RenderStyle::isIdempotentTextAutosizingCandidate(AutosizeStatus status) const
{
    // Refer to <rdar://problem/51826266> for more information regarding how this function was generated.
    auto fields = status.fields();

    if (fields.contains(AutosizeStatus::Fields::AvoidSubtree))
        return false;

    constexpr float smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText = 5;
    constexpr float largeMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText = 25;

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

    if (hasBackgroundImage() && backgroundRepeat() == FillRepeatXY { FillRepeat::NoRepeat, FillRepeat::NoRepeat })
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
    if (m_nonInheritedData->miscData.ptr() != other.m_nonInheritedData->miscData.ptr()
        && !arePointingToEqualData(m_nonInheritedData->miscData->boxShadow, other.m_nonInheritedData->miscData->boxShadow))
        return true;

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()
        && !arePointingToEqualData(m_rareInheritedData->textShadow, other.m_rareInheritedData->textShadow))
        return true;

    if (m_inheritedFlags.textDecorationLines != other.m_inheritedFlags.textDecorationLines
        || m_nonInheritedData->rareData->textDecorationStyle != other.m_nonInheritedData->rareData->textDecorationStyle
        || m_nonInheritedData->rareData->textDecorationThickness != other.m_nonInheritedData->rareData->textDecorationThickness
        || m_rareInheritedData->textUnderlineOffset != other.m_rareInheritedData->textUnderlineOffset
        || m_rareInheritedData->textUnderlinePosition != other.m_rareInheritedData->textUnderlinePosition) {
        // Underlines are always drawn outside of their textbox bounds when text-underline-position: under;
        // is specified. We can take an early out here.
        if (textUnderlinePosition() == TextUnderlinePosition::Under || other.textUnderlinePosition() == TextUnderlinePosition::Under)
            return true;
        return visualOverflowForDecorations(*this) != visualOverflowForDecorations(other);
    }

    auto hasOutlineInVisualOverflow = this->hasOutlineInVisualOverflow();
    auto otherHasOutlineInVisualOverflow = other.hasOutlineInVisualOverflow();
    if (hasOutlineInVisualOverflow != otherHasOutlineInVisualOverflow
        || (hasOutlineInVisualOverflow && otherHasOutlineInVisualOverflow && outlineSize() != other.outlineSize()))
        return true;

    return false;
}

static bool miscDataChangeRequiresLayout(const StyleMiscNonInheritedData& first, const StyleMiscNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
    ASSERT(&first != &second);

    if (first.effectiveAppearance != second.effectiveAppearance
        || first.textOverflow != second.textOverflow)
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

    if (first.hasOpacity() != second.hasOpacity()) {
        // FIXME: We would like to use SimplifiedLayout here, but we can't quite do that yet.
        // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
        // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
        // In addition we need to solve the floating object issue when layers come and go. Right now
        // a full layout is necessary to keep floating object lists sane.
        return true;
    }

    if (first.hasFilters() != second.hasFilters())
        return true;

    if (first.aspectRatioType != second.aspectRatioType
        || first.aspectRatioWidth != second.aspectRatioWidth
        || first.aspectRatioHeight != second.aspectRatioHeight) {
        return true;
    }

    return false;
}

static bool rareDataChangeRequiresLayout(const StyleRareNonInheritedData& first, const StyleRareNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
    ASSERT(&first != &second);

    if (first.lineClamp != second.lineClamp
        || first.initialLetter != second.initialLetter)
        return true;

    if (first.shapeMargin != second.shapeMargin)
        return true;

    if (first.columnGap != second.columnGap || first.rowGap != second.rowGap)
        return true;

    if (!arePointingToEqualData(first.boxReflect, second.boxReflect))
        return true;

    // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
    if (first.counterDirectives.map != second.counterDirectives.map)
        return true;

    if (!arePointingToEqualData(first.scale, second.scale)
        || !arePointingToEqualData(first.rotate, second.rotate)
        || !arePointingToEqualData(first.translate, second.translate))
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Transform);

    if (!arePointingToEqualData(first.offsetPath, second.offsetPath)
        || first.offsetPosition != second.offsetPosition
        || first.offsetDistance != second.offsetDistance
        || first.offsetAnchor != second.offsetAnchor
        || first.offsetRotate != second.offsetRotate)
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Transform);

    if (first.grid != second.grid
        || first.gridItem != second.gridItem)
        return true;

    if (!arePointingToEqualData(first.willChange, second.willChange)) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::WillChange);
        // Don't return; keep looking for another change
    }

    if (first.breakBefore != second.breakBefore
        || first.breakAfter != second.breakAfter
        || first.breakInside != second.breakInside)
        return true;

#if ENABLE(CSS_COMPOSITING)
    if (first.isolation != second.isolation) {
        // Ideally this would trigger a cheaper layout that just updates layer z-order trees (webkit.org/b/190088).
        return true;
    }
#endif

#if ENABLE(FILTERS_LEVEL_2)
    if (first.hasBackdropFilters() != second.hasBackdropFilters())
        return true;
#endif

    if (first.inputSecurity != second.inputSecurity)
        return true;

    if (first.effectiveContainment().contains(Containment::Size) != second.effectiveContainment().contains(Containment::Size)
        || first.effectiveContainment().contains(Containment::InlineSize) != second.effectiveContainment().contains(Containment::InlineSize))
        return true;

    // content-visibiliy:hidden turns on contain:size which requires relayout.
    if ((static_cast<ContentVisibility>(first.contentVisibility) == ContentVisibility::Hidden) != (static_cast<ContentVisibility>(second.contentVisibility) == ContentVisibility::Hidden))
        return true;

    if (first.scrollPadding != second.scrollPadding)
        return true;

    if (first.scrollSnapType != second.scrollSnapType)
        return true;

    if (first.containIntrinsicWidth != second.containIntrinsicWidth || first.containIntrinsicHeight != second.containIntrinsicHeight)
        return true;

    if (first.marginTrim != second.marginTrim)
        return true;

    if (first.scrollbarGutter != second.scrollbarGutter)
        return true;

    if (first.scrollbarWidth != second.scrollbarWidth)
        return true;

    return false;
}

static bool rareInheritedDataChangeRequiresLayout(const StyleRareInheritedData& first, const StyleRareInheritedData& second)
{
    ASSERT(&first != &second);

    if (first.indent != second.indent
        || first.textAlignLast != second.textAlignLast
        || first.textJustify != second.textJustify
        || first.textIndentLine != second.textIndentLine
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
        || first.textCombine != second.textCombine
        || first.textEmphasisMark != second.textEmphasisMark
        || first.textEmphasisPosition != second.textEmphasisPosition
        || first.textEmphasisCustomMark != second.textEmphasisCustomMark
        || first.textOrientation != second.textOrientation
        || first.tabSize != second.tabSize
        || first.lineBoxContain != second.lineBoxContain
        || first.lineGrid != second.lineGrid
        || first.imageOrientation != second.imageOrientation
        || first.lineSnap != second.lineSnap
        || first.lineAlign != second.lineAlign
        || first.hangingPunctuation != second.hangingPunctuation
        || first.effectiveSkippedContent != second.effectiveSkippedContent
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
        || first.useTouchOverflowScrolling != second.useTouchOverflowScrolling
#endif
        || first.listStyleType != second.listStyleType
        || first.listStyleImage != second.listStyleImage)
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
    if (m_svgStyle->changeRequiresLayout(other.m_svgStyle))
        return true;

    if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr()) {
        if (m_nonInheritedData->boxData.ptr() != other.m_nonInheritedData->boxData.ptr()) {
            if (m_nonInheritedData->boxData->width() != other.m_nonInheritedData->boxData->width()
                || m_nonInheritedData->boxData->minWidth() != other.m_nonInheritedData->boxData->minWidth()
                || m_nonInheritedData->boxData->maxWidth() != other.m_nonInheritedData->boxData->maxWidth()
                || m_nonInheritedData->boxData->height() != other.m_nonInheritedData->boxData->height()
                || m_nonInheritedData->boxData->minHeight() != other.m_nonInheritedData->boxData->minHeight()
                || m_nonInheritedData->boxData->maxHeight() != other.m_nonInheritedData->boxData->maxHeight())
                return true;

            if (m_nonInheritedData->boxData->verticalAlign() != other.m_nonInheritedData->boxData->verticalAlign())
                return true;

            if (m_nonInheritedData->boxData->boxSizing() != other.m_nonInheritedData->boxData->boxSizing())
                return true;

            if (m_nonInheritedData->boxData->hasAutoUsedZIndex() != other.m_nonInheritedData->boxData->hasAutoUsedZIndex())
                return true;
        }

        if (m_nonInheritedData->surroundData.ptr() != other.m_nonInheritedData->surroundData.ptr()) {
            if (m_nonInheritedData->surroundData->margin != other.m_nonInheritedData->surroundData->margin)
                return true;

            if (m_nonInheritedData->surroundData->padding != other.m_nonInheritedData->surroundData->padding)
                return true;

            // If our border widths change, then we need to layout. Other changes to borders only necessitate a repaint.
            if (borderLeftWidth() != other.borderLeftWidth()
                || borderTopWidth() != other.borderTopWidth()
                || borderBottomWidth() != other.borderBottomWidth()
                || borderRightWidth() != other.borderRightWidth())
                return true;

            if (position() != PositionType::Static) {
                if (m_nonInheritedData->surroundData->offset != other.m_nonInheritedData->surroundData->offset) {
                    // FIXME: We would like to use SimplifiedLayout for relative positioning, but we can't quite do that yet.
                    // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
                    // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
                    if (position() != PositionType::Absolute)
                        return true;

                    // Optimize for the case where a positioned layer is moving but not changing size.
                    if (!positionChangeIsMovementOnly(m_nonInheritedData->surroundData->offset, other.m_nonInheritedData->surroundData->offset, m_nonInheritedData->boxData->width()))
                        return true;
                }
            }
        }
    }

    // FIXME: We should add an optimized form of layout that just recomputes visual overflow.
    if (changeAffectsVisualOverflow(other))
        return true;

    if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr()) {
        if (m_nonInheritedData->miscData.ptr() != other.m_nonInheritedData->miscData.ptr()
            && miscDataChangeRequiresLayout(*m_nonInheritedData->miscData, *other.m_nonInheritedData->miscData, changedContextSensitiveProperties))
            return true;

        if (m_nonInheritedData->rareData.ptr() != other.m_nonInheritedData->rareData.ptr()
            && rareDataChangeRequiresLayout(*m_nonInheritedData->rareData, *other.m_nonInheritedData->rareData, changedContextSensitiveProperties))
            return true;
    }

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
        if (m_inheritedFlags.listStylePosition != other.m_inheritedFlags.listStylePosition || m_rareInheritedData->listStyleType != other.m_rareInheritedData->listStyleType)
            return true;
    }

    if (m_inheritedFlags.textAlign != other.m_inheritedFlags.textAlign
        || m_inheritedFlags.textTransform != other.m_inheritedFlags.textTransform
        || m_inheritedFlags.direction != other.m_inheritedFlags.direction
        || m_inheritedFlags.whiteSpaceCollapse != other.m_inheritedFlags.whiteSpaceCollapse
        || m_inheritedFlags.textWrap != other.m_inheritedFlags.textWrap
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

    if ((visibility() == Visibility::Collapse) != (other.visibility() == Visibility::Collapse))
        return true;

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

    if (m_nonInheritedData->surroundData->offset != other.m_nonInheritedData->surroundData->offset) {
        // Optimize for the case where a positioned layer is moving but not changing size.
        if (position() == PositionType::Absolute && positionChangeIsMovementOnly(m_nonInheritedData->surroundData->offset, other.m_nonInheritedData->surroundData->offset, m_nonInheritedData->boxData->width()))
            return true;
    }
    
    return false;
}

static bool miscDataChangeRequiresLayerRepaint(const StyleMiscNonInheritedData& first, const StyleMiscNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
    if (first.opacity != second.opacity) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Opacity);
        // Don't return true; keep looking for another change.
    }

    if (first.filter != second.filter) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Filter);
        // Don't return true; keep looking for another change.
    }

    // FIXME: In SVG this needs to trigger a layout.
    if (first.mask != second.mask)
        return true;

    return false;
}

static bool rareDataChangeRequiresLayerRepaint(const StyleRareNonInheritedData& first, const StyleRareNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
#if ENABLE(CSS_COMPOSITING)
    if (first.effectiveBlendMode != second.effectiveBlendMode)
        return true;
#endif

#if ENABLE(FILTERS_LEVEL_2)
    if (first.backdropFilter != second.backdropFilter) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Filter);
        // Don't return true; keep looking for another change.
    }
#else
    UNUSED_PARAM(changedContextSensitiveProperties);
#endif

    // FIXME: In SVG this needs to trigger a layout.
    if (first.maskBoxImage != second.maskBoxImage)
        return true;

    return false;
}

bool RenderStyle::changeRequiresLayerRepaint(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    // Style::Resolver has ensured that zIndex is non-auto only if it's applicable.

    if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr()) {
        if (m_nonInheritedData->boxData.ptr() != other.m_nonInheritedData->boxData.ptr()) {
            if (m_nonInheritedData->boxData->usedZIndex() != other.m_nonInheritedData->boxData->usedZIndex() || m_nonInheritedData->boxData->hasAutoUsedZIndex() != other.m_nonInheritedData->boxData->hasAutoUsedZIndex())
                return true;
        }

        if (position() != PositionType::Static) {
            if (m_nonInheritedData->rareData.ptr() != other.m_nonInheritedData->rareData.ptr()) {
                if (m_nonInheritedData->rareData->clip != other.m_nonInheritedData->rareData->clip || m_nonInheritedData->rareData->hasClip != other.m_nonInheritedData->rareData->hasClip) {
                    changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::ClipRect);
                    return true;
                }
            }
        }

        if (m_nonInheritedData->miscData.ptr() != other.m_nonInheritedData->miscData.ptr()
            && miscDataChangeRequiresLayerRepaint(*m_nonInheritedData->miscData, *other.m_nonInheritedData->miscData, changedContextSensitiveProperties))
            return true;

        if (m_nonInheritedData->rareData.ptr() != other.m_nonInheritedData->rareData.ptr()
            && rareDataChangeRequiresLayerRepaint(*m_nonInheritedData->rareData, *other.m_nonInheritedData->rareData, changedContextSensitiveProperties))
            return true;
    }

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

static bool miscDataChangeRequiresRepaint(const StyleMiscNonInheritedData& first, const StyleMiscNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>&)
{
    if (first.userDrag != second.userDrag
        || first.objectFit != second.objectFit
        || first.objectPosition != second.objectPosition)
        return true;

    return false;
}

static bool rareDataChangeRequiresRepaint(const StyleRareNonInheritedData& first, const StyleRareNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
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
        || first.accentColor != second.accentColor
#if ENABLE(DARK_MODE_CSS)
        || first.colorScheme != second.colorScheme
#endif
    ;
}

#if ENABLE(CSS_PAINTING_API)
void RenderStyle::addCustomPaintWatchProperty(const AtomString& name)
{
    auto& data = m_nonInheritedData.access().rareData.access();
    data.customPaintWatchedProperties.add(name);
}

inline static bool changedCustomPaintWatchedProperty(const RenderStyle& a, const StyleRareNonInheritedData& aData, const RenderStyle& b, const StyleRareNonInheritedData& bData)
{
    auto& propertiesA = aData.customPaintWatchedProperties;
    auto& propertiesB = bData.customPaintWatchedProperties;

    if (UNLIKELY(!propertiesA.isEmpty() || !propertiesB.isEmpty())) {
        // FIXME: We should not need to use ComputedStyleExtractor here.
        ComputedStyleExtractor extractor((Element*) nullptr);

        for (auto& watchPropertiesMap : { propertiesA, propertiesB }) {
            for (auto& name : watchPropertiesMap) {
                RefPtr<const CSSValue> valueA;
                RefPtr<const CSSValue> valueB;
                if (isCustomPropertyName(name)) {
                    valueA = a.customPropertyValue(name);
                    valueB = b.customPropertyValue(name);
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
    bool currentColorDiffers = m_inheritedData->color != other.m_inheritedData->color;

    if (m_svgStyle->changeRequiresRepaint(other.m_svgStyle, currentColorDiffers))
        return true;

    if (!requiresPainting(*this) && !requiresPainting(other))
        return false;

    if (m_inheritedFlags.visibility != other.m_inheritedFlags.visibility
        || m_inheritedFlags.printColorAdjust != other.m_inheritedFlags.printColorAdjust
        || m_inheritedFlags.insideLink != other.m_inheritedFlags.insideLink
        || m_inheritedFlags.insideDefaultButton != other.m_inheritedFlags.insideDefaultButton)
        return true;


    if (currentColorDiffers || m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr()) {
        if (currentColorDiffers || m_nonInheritedData->backgroundData.ptr() != other.m_nonInheritedData->backgroundData.ptr()) {
            if (!m_nonInheritedData->backgroundData->isEquivalentForPainting(*other.m_nonInheritedData->backgroundData, currentColorDiffers))
                return true;
        }

        if (currentColorDiffers || m_nonInheritedData->surroundData.ptr() != other.m_nonInheritedData->surroundData.ptr()) {
            if (!m_nonInheritedData->surroundData->border.isEquivalentForPainting(other.m_nonInheritedData->surroundData->border, currentColorDiffers))
                return true;
        }
    }

    if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr()) {
        if (m_nonInheritedData->miscData.ptr() != other.m_nonInheritedData->miscData.ptr()
            && miscDataChangeRequiresRepaint(*m_nonInheritedData->miscData, *other.m_nonInheritedData->miscData, changedContextSensitiveProperties))
            return true;

        if (m_nonInheritedData->rareData.ptr() != other.m_nonInheritedData->rareData.ptr()
            && rareDataChangeRequiresRepaint(*m_nonInheritedData->rareData, *other.m_nonInheritedData->rareData, changedContextSensitiveProperties))
            return true;
    }

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()
        && rareInheritedDataChangeRequiresRepaint(*m_rareInheritedData, *other.m_rareInheritedData))
        return true;

#if ENABLE(CSS_PAINTING_API)
    if (changedCustomPaintWatchedProperty(*this, *m_nonInheritedData->rareData, other, *other.m_nonInheritedData->rareData))
        return true;
#endif

    return false;
}

bool RenderStyle::changeRequiresRepaintIfText(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>&) const
{
    if (m_inheritedData->color != other.m_inheritedData->color
        || m_inheritedFlags.textDecorationLines != other.m_inheritedFlags.textDecorationLines
        || m_nonInheritedFlags.textDecorationLine != other.m_nonInheritedFlags.textDecorationLine
        || m_nonInheritedData->rareData->textDecorationStyle != other.m_nonInheritedData->rareData->textDecorationStyle
        || m_nonInheritedData->rareData->textDecorationColor != other.m_nonInheritedData->rareData->textDecorationColor
        || m_nonInheritedData->rareData->textDecorationThickness != other.m_nonInheritedData->rareData->textDecorationThickness
        || m_rareInheritedData->textDecorationSkipInk != other.m_rareInheritedData->textDecorationSkipInk
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
    if (m_inheritedFlags.pointerEvents != other.m_inheritedFlags.pointerEvents)
        return true;

    if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr() && m_nonInheritedData->rareData.ptr() != other.m_nonInheritedData->rareData.ptr()) {
        if (usedTransformStyle3D() != other.usedTransformStyle3D()
            || m_nonInheritedData->rareData->backfaceVisibility != other.m_nonInheritedData->rareData->backfaceVisibility
            || m_nonInheritedData->rareData->perspective != other.m_nonInheritedData->rareData->perspective
            || m_nonInheritedData->rareData->perspectiveOriginX != other.m_nonInheritedData->rareData->perspectiveOriginX
            || m_nonInheritedData->rareData->perspectiveOriginY != other.m_nonInheritedData->rareData->perspectiveOriginY
            || m_nonInheritedData->rareData->overscrollBehaviorX != other.m_nonInheritedData->rareData->overscrollBehaviorX
            || m_nonInheritedData->rareData->overscrollBehaviorY != other.m_nonInheritedData->rareData->overscrollBehaviorY)
            return true;
    }

    return false;
}

StyleDifference RenderStyle::diff(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    changedContextSensitiveProperties = OptionSet<StyleDifferenceContextSensitiveProperty>();

    if (changeRequiresLayout(other, changedContextSensitiveProperties))
        return StyleDifference::Layout;

    if (changeRequiresPositionedLayoutOnly(other, changedContextSensitiveProperties))
        return StyleDifference::LayoutPositionedMovementOnly;

    if (changeRequiresLayerRepaint(other, changedContextSensitiveProperties))
        return StyleDifference::RepaintLayer;

    if (changeRequiresRepaint(other, changedContextSensitiveProperties))
        return StyleDifference::Repaint;

    if (changeRequiresRepaintIfText(other, changedContextSensitiveProperties))
        return StyleDifference::RepaintIfText;

    // FIXME: RecompositeLayer should also behave as a priority bit (e.g when the style change requires layout, we know that
    // the content also needs repaint and it will eventually get repainted,
    // but a repaint type of change (e.g. color change) does not necessarily trigger recomposition). 
    if (changeRequiresRecompositeLayer(other, changedContextSensitiveProperties))
        return StyleDifference::RecompositeLayer;

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
    auto& data = m_nonInheritedData.access().rareData.access();
    data.clip.top() = WTFMove(top);
    data.clip.right() = WTFMove(right);
    data.clip.bottom() = WTFMove(bottom);
    data.clip.left() = WTFMove(left);
}

void RenderStyle::addCursor(RefPtr<StyleImage>&& image, const std::optional<IntPoint>& hotSpot)
{
    auto& cursorData = m_rareInheritedData.access().cursorData;
    if (!cursorData)
        cursorData = CursorList::create();
    // Point outside the image is how we tell the cursor machinery there is no hot spot, and it should generate one (done in the Cursor class).
    // FIXME: Would it be better to extend the concept of "no hot spot" deeper, into CursorData and beyond, rather than using -1/-1 for it?
    cursorData->append(CursorData(WTFMove(image), hotSpot.value_or(IntPoint { -1, -1 })));
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
    if (arePointingToEqualData(m_nonInheritedData->rareData->willChange.get(), willChangeData.get()))
        return;

    m_nonInheritedData.access().rareData.access().willChange = WTFMove(willChangeData);
}

void RenderStyle::setScale(RefPtr<ScaleTransformOperation>&& t)
{
    m_nonInheritedData.access().rareData.access().scale = WTFMove(t);
}

void RenderStyle::setRotate(RefPtr<RotateTransformOperation>&& t)
{
    m_nonInheritedData.access().rareData.access().rotate = WTFMove(t);
}

void RenderStyle::setTranslate(RefPtr<TranslateTransformOperation>&& t)
{
    m_nonInheritedData.access().rareData.access().translate = WTFMove(t);
}

void RenderStyle::clearCursorList()
{
    if (m_rareInheritedData->cursorData)
        m_rareInheritedData.access().cursorData = nullptr;
}

void RenderStyle::clearContent()
{
    if (m_nonInheritedData->miscData->content)
        m_nonInheritedData.access().miscData.access().content = nullptr;
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
    auto& data = m_nonInheritedData.access().miscData.access();
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
    auto& data = m_nonInheritedData.access().miscData.access();
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
    auto& data = m_nonInheritedData.access().miscData.access();
    data.altText = string;
    if (data.content)
        data.content->setAltText(string);
}

const String& RenderStyle::contentAltText() const
{
    return m_nonInheritedData->miscData->altText;
}

void RenderStyle::setHasAttrContent()
{
    setUnique();
    SET_NESTED_VAR(m_nonInheritedData, miscData, hasAttrContent, true);
}

bool RenderStyle::affectedByTransformOrigin() const
{
    if (m_nonInheritedData->rareData->rotate && !m_nonInheritedData->rareData->rotate->isIdentity())
        return true;

    if (m_nonInheritedData->rareData->scale && !m_nonInheritedData->rareData->scale->isIdentity())
        return true;

    auto& transformOperations = m_nonInheritedData->miscData->transform->operations;
    if (transformOperations.affectedByTransformOrigin())
        return true;

    if (offsetPath())
        return true;

    return false;
}

FloatPoint RenderStyle::computePerspectiveOrigin(const FloatRect& boundingBox) const
{
    return boundingBox.location() + floatPointForLengthPoint(perspectiveOrigin(), boundingBox.size());
}

void RenderStyle::applyPerspective(TransformationMatrix& transform, const FloatPoint& originTranslate) const
{
    // https://www.w3.org/TR/css-transforms-2/#perspective
    // The perspective matrix is computed as follows:
    // 1. Start with the identity matrix.

    // 2. Translate by the computed X and Y values of perspective-origin
    transform.translate(originTranslate.x(), originTranslate.y());

    // 3. Multiply by the matrix that would be obtained from the perspective() transform function, where the length is provided by the value of the perspective property
    transform.applyPerspective(usedPerspective());

    // 4. Translate by the negated computed X and Y values of perspective-origin
    transform.translate(-originTranslate.x(), -originTranslate.y());
}

FloatPoint3D RenderStyle::computeTransformOrigin(const FloatRect& boundingBox) const
{
    FloatPoint3D originTranslate;
    originTranslate.setXY(boundingBox.location() + floatPointForLengthPoint(transformOriginXY(), boundingBox.size()));
    originTranslate.setZ(transformOriginZ());
    return originTranslate;
}

void RenderStyle::applyTransformOrigin(TransformationMatrix& transform, const FloatPoint3D& originTranslate) const
{
    if (!originTranslate.isZero())
        transform.translate3d(originTranslate.x(), originTranslate.y(), originTranslate.z());
}

void RenderStyle::unapplyTransformOrigin(TransformationMatrix& transform, const FloatPoint3D& originTranslate) const
{
    if (!originTranslate.isZero())
        transform.translate3d(-originTranslate.x(), -originTranslate.y(), -originTranslate.z());
}

void RenderStyle::applyTransform(TransformationMatrix& transform, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    if (!options.contains(RenderStyle::TransformOperationOption::TransformOrigin) || !affectedByTransformOrigin()) {
        applyCSSTransform(transform, boundingBox, options);
        return;
    }

    auto originTranslate = computeTransformOrigin(boundingBox);
    applyTransformOrigin(transform, originTranslate);
    applyCSSTransform(transform, boundingBox, options);
    unapplyTransformOrigin(transform, originTranslate);
}

void RenderStyle::applyTransform(TransformationMatrix& transform, const FloatRect& boundingBox) const
{
    applyTransform(transform, boundingBox, allTransformOperations());
}

void RenderStyle::applyCSSTransform(TransformationMatrix& transform, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    // https://www.w3.org/TR/css-transforms-2/#ctm
    // The transformation matrix is computed from the transform, transform-origin, translate, rotate, scale, and offset properties as follows:
    // 1. Start with the identity matrix.

    // 2. Translate by the computed X, Y, and Z values of transform-origin.
    // (implemented in applyTransformOrigin)

    // 3. Translate by the computed X, Y, and Z values of translate.
    if (options.contains(RenderStyle::TransformOperationOption::Translate)) {
        if (TransformOperation* translate = m_nonInheritedData->rareData->translate.get())
            translate->apply(transform, boundingBox.size());
    }

    // 4. Rotate by the computed <angle> about the specified axis of rotate.
    if (options.contains(RenderStyle::TransformOperationOption::Rotate)) {
        if (TransformOperation* rotate = m_nonInheritedData->rareData->rotate.get())
            rotate->apply(transform, boundingBox.size());
    }

    // 5. Scale by the computed X, Y, and Z values of scale.
    if (options.contains(RenderStyle::TransformOperationOption::Scale)) {
        if (TransformOperation* scale = m_nonInheritedData->rareData->scale.get())
            scale->apply(transform, boundingBox.size());
    }

    // 6. Translate and rotate by the transform specified by offset.
    if (options.contains(RenderStyle::TransformOperationOption::Offset))
        applyMotionPathTransform(transform, boundingBox);

    // 7. Multiply by each of the transform functions in transform from left to right.
    auto& transformOperations = m_nonInheritedData->miscData->transform->operations;
    for (auto& operation : transformOperations.operations())
        operation->apply(transform, boundingBox.size());

    // 8. Translate by the negated computed X, Y and Z values of transform-origin.
    // (implemented in unapplyTransformOrigin)
}

static PathTraversalState getTraversalStateAtDistance(const Path& path, const Length& distance)
{
    auto pathLength = path.length();
    auto distanceValue = floatValueForLength(distance, pathLength);

    float resolvedLength = 0;
    if (path.isClosed()) {
        if (pathLength) {
            resolvedLength = fmod(distanceValue, pathLength);
            if (resolvedLength < 0)
                resolvedLength += pathLength;
        }
    } else
        resolvedLength = clampTo<float>(distanceValue, 0, pathLength);

    ASSERT(resolvedLength >= 0);
    return path.traversalStateAtLength(resolvedLength);
}

void RenderStyle::applyMotionPathTransform(TransformationMatrix& transform, const FloatRect& boundingBox) const
{
    if (!offsetPath())
        return;

    auto transformOrigin = computeTransformOrigin(boundingBox).xy();
    auto anchor = transformOrigin;
    if (!offsetAnchor().x().isAuto())
        anchor = floatPointForLengthPoint(offsetAnchor(), boundingBox.size()) + boundingBox.location();
    
    // Shift element to the point on path specified by offset-path and offset-distance.
    auto path = offsetPath()->getPath(boundingBox);
    if (!path)
        return;
    auto traversalState = getTraversalStateAtDistance(*path, offsetDistance());
    transform.translate(traversalState.current().x(), traversalState.current().y());

    // Shift element to the anchor specified by offset-anchor.
    transform.translate(-anchor.x(), -anchor.y());

    auto shiftToOrigin = anchor - transformOrigin;
    transform.translate(shiftToOrigin.width(), shiftToOrigin.height());

    // Apply rotation.
    auto rotation = offsetRotate();
    if (rotation.hasAuto())
        transform.rotate(traversalState.normalAngle() + rotation.angle());
    else
        transform.rotate(rotation.angle());

    transform.translate(-shiftToOrigin.width(), -shiftToOrigin.height());
}

void RenderStyle::setPageScaleTransform(float scale)
{
    if (scale == 1)
        return;
    TransformOperations transform;
    transform.operations().append(ScaleTransformOperation::create(scale, scale, TransformOperation::Type::Scale));
    setTransform(transform);
    setTransformOriginX(Length(0, LengthType::Fixed));
    setTransformOriginY(Length(0, LengthType::Fixed));
}

void RenderStyle::setTextShadow(std::unique_ptr<ShadowData> shadowData, bool add)
{
    ASSERT(!shadowData || (shadowData->spread().isZero() && shadowData->style() == ShadowStyle::Normal));

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
    auto& rareData = m_nonInheritedData.access().miscData.access();
    if (!add) {
        rareData.boxShadow = WTFMove(shadowData);
        return;
    }

    shadowData->setNext(WTFMove(rareData.boxShadow));
    rareData.boxShadow = WTFMove(shadowData);
}

static RoundedRect::Radii calcRadiiFor(const BorderData::Radii& radii, const LayoutSize& size)
{
    return {
        sizeForLengthSize(radii.topLeft, size),
        sizeForLengthSize(radii.topRight, size),
        sizeForLengthSize(radii.bottomLeft, size),
        sizeForLengthSize(radii.bottomRight, size)
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
        RoundedRect::Radii radii = calcRadiiFor(m_nonInheritedData->surroundData->border.m_radii, borderRect.size());
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
    auto radii = hasBorderRadius() ? std::make_optional(m_nonInheritedData->surroundData->border.m_radii) : std::nullopt;
    return getRoundedInnerBorderFor(borderRect, topWidth, bottomWidth, leftWidth, rightWidth, radii, isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
}

RoundedRect RenderStyle::getRoundedInnerBorderFor(const LayoutRect& borderRect, LayoutUnit topWidth, LayoutUnit bottomWidth,
    LayoutUnit leftWidth, LayoutUnit rightWidth, std::optional<BorderData::Radii> radii, bool isHorizontalWritingMode, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    auto width = std::max(0_lu, borderRect.width() - leftWidth - rightWidth);
    auto height = std::max(0_lu, borderRect.height() - topWidth - bottomWidth);
    auto roundedRect = RoundedRect {
        borderRect.x() + leftWidth,
        borderRect.y() + topWidth,
        width,
        height
    };
    if (radii) {
        auto adjustedRadii = calcRadiiFor(*radii, borderRect.size());
        adjustedRadii.scale(calcBorderRadiiConstraintScaleFor(borderRect, adjustedRadii));
        adjustedRadii.shrink(topWidth, bottomWidth, leftWidth, rightWidth);
        roundedRect.includeLogicalEdges(adjustedRadii, isHorizontalWritingMode, includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    if (!roundedRect.isRenderable())
        roundedRect.adjustRadii();
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

const CounterDirectiveMap& RenderStyle::counterDirectives() const
{
    return m_nonInheritedData->rareData->counterDirectives;
}

CounterDirectiveMap& RenderStyle::accessCounterDirectives()
{
    return m_nonInheritedData.access().rareData.access().counterDirectives;
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
    auto* animationList = m_nonInheritedData->miscData->animations.get();
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
    auto* transitionList = m_nonInheritedData->miscData->transitions.get();
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
}

AnimationList& RenderStyle::ensureAnimations()
{
    auto& animations = m_nonInheritedData.access().miscData.access().animations;
    if (!animations)
        animations = AnimationList::create();
    return *animations;
}

AnimationList& RenderStyle::ensureTransitions()
{
    auto& transitions = m_nonInheritedData.access().miscData.access().transitions;
    if (!transitions)
        transitions = AnimationList::create();
    return *transitions;
}

const FontCascade& RenderStyle::fontCascade() const
{
    return m_inheritedData->fontCascade;
}

const FontMetrics& RenderStyle::metricsOfPrimaryFont() const
{
    return m_inheritedData->fontCascade.metricsOfPrimaryFont();
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

const Length& RenderStyle::wordSpacing() const
{
    return m_rareInheritedData->wordSpacing;
}

float RenderStyle::letterSpacing() const
{
    return m_inheritedData->fontCascade.letterSpacing();
}

TextSpacingTrim RenderStyle::textSpacingTrim() const
{
    return m_rareInheritedData->textSpacingTrim;
}

TextAutospace RenderStyle::textAutospace() const
{
    return m_rareInheritedData->textAutospace;
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
        return metricsOfPrimaryFont().lineSpacing();

    if (lineHeightLength.isPercentOrCalculated())
        return minimumValueForLength(lineHeightLength, computedFontSize());

    return clampTo<int>(lineHeightLength.value());
}

// FIXME: Remove this after all old calls to whiteSpace() are replaced with appropriate
// calls to whiteSpaceCollapse() and textWrap().
WhiteSpace RenderStyle::whiteSpace() const
{
    auto whiteSpaceCollapse = static_cast<WhiteSpaceCollapse>(m_inheritedFlags.whiteSpaceCollapse);
    auto textWrap = static_cast<TextWrap>(m_inheritedFlags.textWrap);
    if (whiteSpaceCollapse == WhiteSpaceCollapse::BreakSpaces && textWrap == TextWrap::Wrap)
        return WhiteSpace::BreakSpaces;
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse && textWrap == TextWrap::Wrap)
        return WhiteSpace::Normal;
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse && textWrap == TextWrap::NoWrap)
        return WhiteSpace::NoWrap;
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrap == TextWrap::NoWrap)
        return WhiteSpace::Pre;
    if (whiteSpaceCollapse == WhiteSpaceCollapse::PreserveBreaks && textWrap == TextWrap::Wrap)
        return WhiteSpace::PreLine;
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrap == TextWrap::Wrap)
        return WhiteSpace::PreWrap;

    // Reachable for combinations that can't be represented with the white-space syntax.
    // Do nothing for now since this is a temporary function.
    return WhiteSpace::Normal;
}

void RenderStyle::setWordSpacing(Length&& value)
{
    float fontWordSpacing;
    switch (value.type()) {
    case LengthType::Auto:
        fontWordSpacing = 0;
        break;
    case LengthType::Percent:
        fontWordSpacing = value.percent() * fontCascade().widthOfSpaceString() / 100;
        break;
    case LengthType::Fixed:
        fontWordSpacing = value.value();
        break;
    case LengthType::Calculated:
        fontWordSpacing = value.nonNanCalculatedValue(static_cast<float>(maxValueForCssLength));
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
    auto selector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setShouldDisableLigaturesForSpacing(letterSpacing);
    setFontDescription(WTFMove(description));
    fontCascade().update(selector);

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

    auto selector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setSpecifiedSize(size);
    description.setComputedSize(size);

    setFontDescription(WTFMove(description));
    fontCascade().update(selector);
}

void RenderStyle::setFontSizeAdjust(FontSizeAdjust sizeAdjust)
{
    auto selector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setFontSizeAdjust(sizeAdjust);

    setFontDescription(WTFMove(description));
    fontCascade().update(selector);
}

void RenderStyle::setFontVariationSettings(FontVariationSettings settings)
{
    auto selector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setVariationSettings(WTFMove(settings));

    setFontDescription(WTFMove(description));
    fontCascade().update(selector);
}

void RenderStyle::setFontWeight(FontSelectionValue value)
{
    auto selector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setWeight(value);

    setFontDescription(WTFMove(description));
    fontCascade().update(selector);
}

void RenderStyle::setFontStretch(FontSelectionValue value)
{
    auto selector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setStretch(value);

    setFontDescription(WTFMove(description));
    fontCascade().update(selector);
}

void RenderStyle::setFontItalic(std::optional<FontSelectionValue> value)
{
    auto selector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setItalic(value);

    setFontDescription(WTFMove(description));
    fontCascade().update(selector);
}

void RenderStyle::setFontPalette(FontPalette value)
{
    auto selector = fontCascade().fontSelector();
    auto description = fontDescription();
    description.setFontPalette(value);

    setFontDescription(WTFMove(description));
    fontCascade().update(selector);
}

LayoutBoxExtent RenderStyle::shadowExtent(const ShadowData* shadow)
{
    LayoutUnit top;
    LayoutUnit right;
    LayoutUnit bottom;
    LayoutUnit left;

    for ( ; shadow; shadow = shadow->next()) {
        if (shadow->style() == ShadowStyle::Inset)
            continue;

        auto extentAndSpread = shadow->paintingExtent() + LayoutUnit(shadow->spread().value());
        top = std::min<LayoutUnit>(top, LayoutUnit(shadow->y().value()) - extentAndSpread);
        right = std::max<LayoutUnit>(right, LayoutUnit(shadow->x().value()) + extentAndSpread);
        bottom = std::max<LayoutUnit>(bottom, LayoutUnit(shadow->y().value()) + extentAndSpread);
        left = std::min<LayoutUnit>(left, LayoutUnit(shadow->x().value()) - extentAndSpread);
    }
    
    return { top, right, bottom, left };
}

LayoutBoxExtent RenderStyle::shadowInsetExtent(const ShadowData* shadow)
{
    LayoutUnit top;
    LayoutUnit right;
    LayoutUnit bottom;
    LayoutUnit left;

    for ( ; shadow; shadow = shadow->next()) {
        if (shadow->style() == ShadowStyle::Normal)
            continue;

        auto extentAndSpread = shadow->paintingExtent() + LayoutUnit(shadow->spread().value());
        top = std::max<LayoutUnit>(top, LayoutUnit(shadow->y().value()) + extentAndSpread);
        right = std::min<LayoutUnit>(right, LayoutUnit(shadow->x().value()) - extentAndSpread);
        bottom = std::min<LayoutUnit>(bottom, LayoutUnit(shadow->y().value()) - extentAndSpread);
        left = std::max<LayoutUnit>(left, LayoutUnit(shadow->x().value()) + extentAndSpread);
    }

    return { top, right, bottom, left };
}

void RenderStyle::getShadowHorizontalExtent(const ShadowData* shadow, LayoutUnit &left, LayoutUnit &right)
{
    left = 0;
    right = 0;

    for ( ; shadow; shadow = shadow->next()) {
        if (shadow->style() == ShadowStyle::Inset)
            continue;

        auto extentAndSpread = shadow->paintingExtent() + LayoutUnit(shadow->spread().value());
        left = std::min<LayoutUnit>(left, LayoutUnit(shadow->x().value()) - extentAndSpread);
        right = std::max<LayoutUnit>(right, LayoutUnit(shadow->x().value()) + extentAndSpread);
    }
}

void RenderStyle::getShadowVerticalExtent(const ShadowData* shadow, LayoutUnit &top, LayoutUnit &bottom)
{
    top = 0;
    bottom = 0;

    for ( ; shadow; shadow = shadow->next()) {
        if (shadow->style() == ShadowStyle::Inset)
            continue;

        auto extentAndSpread = shadow->paintingExtent() + LayoutUnit(shadow->spread().value());
        top = std::min<LayoutUnit>(top, LayoutUnit(shadow->y().intValue()) - extentAndSpread);
        bottom = std::max<LayoutUnit>(bottom, LayoutUnit(shadow->y().intValue()) + extentAndSpread);
    }
}

StyleColor RenderStyle::unresolvedColorForProperty(CSSPropertyID colorProperty, bool visitedLink) const
{
    switch (colorProperty) {
    case CSSPropertyAccentColor:
        return accentColor();
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
    case CSSPropertyTextEmphasisColor:
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
    auto result = unresolvedColorForProperty(colorProperty, visitedLink);

    if (result.isCurrentColor()) {
        if (colorProperty == CSSPropertyTextDecorationColor) {
            if (hasPositiveStrokeWidth()) {
                // Prefer stroke color if possible but not if it's fully transparent.
                auto strokeColor = colorResolvingCurrentColor(effectiveStrokeColorProperty(), visitedLink);
                if (strokeColor.isVisible())
                    return strokeColor;
            }

            return colorResolvingCurrentColor(CSSPropertyWebkitTextFillColor, visitedLink);
        }

        return visitedLink ? visitedLinkColor() : color();
    }

    return colorResolvingCurrentColor(result);
}

Color RenderStyle::colorResolvingCurrentColor(const StyleColor& color) const
{
    return color.resolveColor(this->color());
}

Color RenderStyle::visitedDependentColor(CSSPropertyID colorProperty, OptionSet<PaintBehavior> paintBehavior) const
{
    Color unvisitedColor = colorResolvingCurrentColor(colorProperty, false);
    if (insideLink() != InsideLink::InsideVisited)
        return unvisitedColor;

    if (paintBehavior.contains(PaintBehavior::DontShowVisitedLinks))
        return unvisitedColor;

#if ENABLE(CSS_COMPOSITING)
    if (isInSubtreeWithBlendMode())
        return unvisitedColor;
#endif

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

Color RenderStyle::visitedDependentColorWithColorFilter(CSSPropertyID colorProperty, OptionSet<PaintBehavior> paintBehavior) const
{
    if (!hasAppleColorFilter())
        return visitedDependentColor(colorProperty, paintBehavior);

    return colorByApplyingColorFilter(visitedDependentColor(colorProperty, paintBehavior));
}

Color RenderStyle::colorByApplyingColorFilter(const Color& color) const
{
    Color transformedColor = color;
    appleColorFilter().transformColor(transformedColor);
    return transformedColor;
}

Color RenderStyle::colorWithColorFilter(const StyleColor& color) const
{
    return colorByApplyingColorFilter(colorResolvingCurrentColor(color));
}

Color RenderStyle::effectiveAccentColor() const
{
    if (hasAutoAccentColor())
        return { };

    if (hasAppleColorFilter())
        return colorByApplyingColorFilter(colorResolvingCurrentColor(accentColor()));

    return colorResolvingCurrentColor(accentColor());
}

const BorderValue& RenderStyle::borderBefore() const
{
    switch (writingMode()) {
    case WritingMode::TopToBottom:
        return borderTop();
    case WritingMode::BottomToTop:
        return borderBottom();
    case WritingMode::LeftToRight:
        return borderLeft();
    case WritingMode::RightToLeft:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

const BorderValue& RenderStyle::borderAfter() const
{
    switch (writingMode()) {
    case WritingMode::TopToBottom:
        return borderBottom();
    case WritingMode::BottomToTop:
        return borderTop();
    case WritingMode::LeftToRight:
        return borderRight();
    case WritingMode::RightToLeft:
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
    case WritingMode::TopToBottom:
        return borderTopWidth();
    case WritingMode::BottomToTop:
        return borderBottomWidth();
    case WritingMode::LeftToRight:
        return borderLeftWidth();
    case WritingMode::RightToLeft:
        return borderRightWidth();
    }
    ASSERT_NOT_REACHED();
    return borderTopWidth();
}

float RenderStyle::borderAfterWidth() const
{
    switch (writingMode()) {
    case WritingMode::TopToBottom:
        return borderBottomWidth();
    case WritingMode::BottomToTop:
        return borderTopWidth();
    case WritingMode::LeftToRight:
        return borderRightWidth();
    case WritingMode::RightToLeft:
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

StyleColor RenderStyle::initialTapHighlightColor()
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
    if (m_nonInheritedData->surroundData->border.m_image.image() == image.get())
        return;
    m_nonInheritedData.access().surroundData.access().border.m_image.setImage(WTFMove(image));
}

void RenderStyle::setBorderImageSliceFill(bool fill)
{
    if (m_nonInheritedData->surroundData->border.m_image.fill() == fill)
        return;
    m_nonInheritedData.access().surroundData.access().border.m_image.setFill(fill);
}

void RenderStyle::setBorderImageSlices(LengthBox&& slices)
{
    if (m_nonInheritedData->surroundData->border.m_image.imageSlices() == slices)
        return;
    m_nonInheritedData.access().surroundData.access().border.m_image.setImageSlices(WTFMove(slices));
}

void RenderStyle::setBorderImageWidth(LengthBox&& slices)
{
    if (m_nonInheritedData->surroundData->border.m_image.borderSlices() == slices)
        return;
    m_nonInheritedData.access().surroundData.access().border.m_image.setBorderSlices(WTFMove(slices));
}

void RenderStyle::setBorderImageWidthOverridesBorderWidths(bool overridesBorderWidths)
{
    if (m_nonInheritedData->surroundData->border.m_image.overridesBorderWidths() == overridesBorderWidths)
        return;
    m_nonInheritedData.access().surroundData.access().border.m_image.setOverridesBorderWidths(overridesBorderWidths);
}

void RenderStyle::setBorderImageOutset(LengthBox&& outset)
{
    if (m_nonInheritedData->surroundData->border.m_image.outset() == outset)
        return;
    m_nonInheritedData.access().surroundData.access().border.m_image.setOutset(WTFMove(outset));
}

void RenderStyle::setBorderImageHorizontalRule(NinePieceImageRule rule)
{
    if (m_nonInheritedData->surroundData->border.m_image.horizontalRule() == rule)
        return;
    m_nonInheritedData.access().surroundData.access().border.m_image.setHorizontalRule(rule);
}

void RenderStyle::setBorderImageVerticalRule(NinePieceImageRule rule)
{
    if (m_nonInheritedData->surroundData->border.m_image.verticalRule() == rule)
        return;
    m_nonInheritedData.access().surroundData.access().border.m_image.setVerticalRule(rule);
}

void RenderStyle::setColumnStylesFromPaginationMode(PaginationMode paginationMode)
{
    if (paginationMode == Unpaginated)
        return;
    
    setColumnFill(ColumnFill::Auto);
    
    switch (paginationMode) {
    case LeftToRightPaginated:
        setColumnAxis(ColumnAxis::Horizontal);
        if (isHorizontalWritingMode())
            setColumnProgression(isLeftToRightDirection() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        else
            setColumnProgression(isFlippedBlocksWritingMode() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        break;
    case RightToLeftPaginated:
        setColumnAxis(ColumnAxis::Horizontal);
        if (isHorizontalWritingMode())
            setColumnProgression(isLeftToRightDirection() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        else
            setColumnProgression(isFlippedBlocksWritingMode() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        break;
    case TopToBottomPaginated:
        setColumnAxis(ColumnAxis::Vertical);
        if (isHorizontalWritingMode())
            setColumnProgression(isFlippedBlocksWritingMode() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        else
            setColumnProgression(isLeftToRightDirection() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        break;
    case BottomToTopPaginated:
        setColumnAxis(ColumnAxis::Vertical);
        if (isHorizontalWritingMode())
            setColumnProgression(isFlippedBlocksWritingMode() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        else
            setColumnProgression(isLeftToRightDirection() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        break;
    case Unpaginated:
        ASSERT_NOT_REACHED();
        break;
    }
}

void RenderStyle::deduplicateCustomProperties(const RenderStyle& other)
{
    auto deduplicate = [&] <typename T> (const DataRef<T>& data, const DataRef<T>& otherData) {
        auto& properties = const_cast<DataRef<StyleCustomPropertyData>&>(data->customProperties);
        auto& otherProperties = otherData->customProperties;
        if (properties.ptr() == otherProperties.ptr() || *properties != *otherProperties)
            return;
        properties = otherProperties;
    };

    deduplicate(m_rareInheritedData, other.m_rareInheritedData);
    deduplicate(m_nonInheritedData->rareData, other.m_nonInheritedData->rareData);
}

void RenderStyle::setCustomPropertyValue(Ref<const CSSCustomPropertyValue>&& value, bool isInherited)
{
    auto& name = value->name();
    if (isInherited) {
        if (auto* existingValue = m_rareInheritedData->customProperties->get(name); !existingValue || !existingValue->equals(value.get()))
            m_rareInheritedData.access().customProperties.access().set(name, WTFMove(value));
    } else {
        if (auto* existingValue = m_nonInheritedData->rareData->customProperties->get(name); !existingValue || !existingValue->equals(value.get()))
            m_nonInheritedData.access().rareData.access().customProperties.access().set(name, WTFMove(value));
    }
}

const CSSCustomPropertyValue* RenderStyle::customPropertyValue(const AtomString& name) const
{
    for (auto* map : { &nonInheritedCustomProperties(), &inheritedCustomProperties() }) {
        if (auto* value = map->get(name))
            return value;
    }
    return nullptr;
}

bool RenderStyle::customPropertiesEqual(const RenderStyle& other) const
{
    return m_nonInheritedData->rareData->customProperties == other.m_nonInheritedData->rareData->customProperties
        && m_rareInheritedData->customProperties == other.m_rareInheritedData->customProperties;
}

const LengthBox& RenderStyle::scrollMargin() const
{
    return m_nonInheritedData->rareData->scrollMargin;
}

const Length& RenderStyle::scrollMarginTop() const
{
    return scrollMargin().top();
}

const Length& RenderStyle::scrollMarginBottom() const
{
    return scrollMargin().bottom();
}

const Length& RenderStyle::scrollMarginLeft() const
{
    return scrollMargin().left();
}

const Length& RenderStyle::scrollMarginRight() const
{
    return scrollMargin().right();
}

void RenderStyle::setScrollMarginTop(Length&& length)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollMargin.top(), WTFMove(length));
}

void RenderStyle::setScrollMarginBottom(Length&& length)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollMargin.bottom(), WTFMove(length));
}

void RenderStyle::setScrollMarginLeft(Length&& length)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollMargin.left(), WTFMove(length));
}

void RenderStyle::setScrollMarginRight(Length&& length)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollMargin.right(), WTFMove(length));
}

const LengthBox& RenderStyle::scrollPadding() const
{
    return m_nonInheritedData->rareData->scrollPadding;
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

void RenderStyle::setScrollPaddingTop(Length&& length)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollPadding.top(), WTFMove(length));
}

void RenderStyle::setScrollPaddingBottom(Length&& length)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollPadding.bottom(), WTFMove(length));
}

void RenderStyle::setScrollPaddingLeft(Length&& length)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollPadding.left(), WTFMove(length));
}

void RenderStyle::setScrollPaddingRight(Length&& length)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollPadding.right(), WTFMove(length));
}

ScrollSnapType RenderStyle::initialScrollSnapType()
{
    return { };
}

ScrollSnapAlign RenderStyle::initialScrollSnapAlign()
{
    return { };
}

ScrollSnapStop RenderStyle::initialScrollSnapStop()
{
    return ScrollSnapStop::Normal;
}

ScrollbarGutter RenderStyle::initialScrollbarGutter()
{
    return { };
}

ScrollbarWidth RenderStyle::initialScrollbarWidth()
{
    return ScrollbarWidth::Auto;
}

const ScrollSnapType RenderStyle::scrollSnapType() const
{
    return m_nonInheritedData->rareData->scrollSnapType;
}

const ScrollSnapAlign& RenderStyle::scrollSnapAlign() const
{
    return m_nonInheritedData->rareData->scrollSnapAlign;
}

ScrollSnapStop RenderStyle::scrollSnapStop() const
{
    return m_nonInheritedData->rareData->scrollSnapStop;
}

const ScrollbarGutter RenderStyle::scrollbarGutter() const
{
    return m_nonInheritedData->rareData->scrollbarGutter;
}

ScrollbarWidth RenderStyle::scrollbarWidth() const
{
    return m_nonInheritedData->rareData->scrollbarWidth;
}

void RenderStyle::setScrollSnapType(const ScrollSnapType type)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollSnapType, type);
}

void RenderStyle::setScrollSnapAlign(const ScrollSnapAlign& alignment)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollSnapAlign, alignment);
}

void RenderStyle::setScrollSnapStop(const ScrollSnapStop stop)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollSnapStop, stop);
}

void RenderStyle::setScrollbarGutter(const ScrollbarGutter gutter)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollbarGutter, gutter);
}

void RenderStyle::setScrollbarWidth(const ScrollbarWidth width)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, scrollbarWidth, width);
}

bool RenderStyle::hasSnapPosition() const
{
    const ScrollSnapAlign& alignment = this->scrollSnapAlign();
    return alignment.blockAlign != ScrollSnapAxisAlignType::None || alignment.inlineAlign != ScrollSnapAxisAlignType::None;
}

TextBoxEdge RenderStyle::textBoxEdge() const
{
    return m_rareInheritedData->textBoxEdge;
}

void RenderStyle::setTextBoxEdge(TextBoxEdge textBoxEdgeValue)
{
    SET_VAR(m_rareInheritedData, textBoxEdge, textBoxEdgeValue);
}

TextBoxEdge RenderStyle::initialTextBoxEdge()
{
    return { TextBoxEdgeType::Leading, TextBoxEdgeType::Leading };
}

bool RenderStyle::hasReferenceFilterOnly() const
{
    if (!hasFilter())
        return false;
    auto& filterOperations = m_nonInheritedData->miscData->filter->operations;
    return filterOperations.size() == 1 && filterOperations.at(0)->type() == FilterOperation::Type::Reference;
}

float RenderStyle::outlineWidth() const
{
    auto& outline = m_nonInheritedData->backgroundData->outline;
    if (outline.style() == BorderStyle::None)
        return 0;
    if (outlineStyleIsAuto() == OutlineIsAuto::On)
        return std::max(outline.width(), RenderTheme::platformFocusRingWidth());
    return outline.width();
}

float RenderStyle::outlineOffset() const
{
    auto& outline = m_nonInheritedData->backgroundData->outline;
    if (outlineStyleIsAuto() == OutlineIsAuto::On)
        return (outline.offset() + RenderTheme::platformFocusRingOffset(outlineWidth()));
    return outline.offset();
}

bool RenderStyle::shouldPlaceVerticalScrollbarOnLeft() const
{
    return (!isLeftToRightDirection() && isHorizontalWritingMode()) || writingMode() == WritingMode::RightToLeft;
}

std::span<const PaintType, 3> RenderStyle::paintTypesForPaintOrder(PaintOrder order)
{
    static constexpr std::array fill { PaintType::Fill, PaintType::Stroke, PaintType::Markers };
    static constexpr std::array fillMarkers { PaintType::Fill, PaintType::Markers, PaintType::Stroke };
    static constexpr std::array stroke { PaintType::Stroke, PaintType::Fill, PaintType::Markers };
    static constexpr std::array strokeMarkers { PaintType::Stroke, PaintType::Markers, PaintType::Fill };
    static constexpr std::array markers { PaintType::Markers, PaintType::Fill, PaintType::Stroke };
    static constexpr std::array markersStroke { PaintType::Markers, PaintType::Stroke, PaintType::Fill };
    switch (order) {
    case PaintOrder::Normal:
    case PaintOrder::Fill:
        return fill;
    case PaintOrder::FillMarkers:
        return fillMarkers;
    case PaintOrder::Stroke:
        return stroke;
    case PaintOrder::StrokeMarkers:
        return strokeMarkers;
    case PaintOrder::Markers:
        return markers;
    case PaintOrder::MarkersStroke:
        return markersStroke;
    };
    ASSERT_NOT_REACHED();
    return fill;
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

UsedClear RenderStyle::usedClear(const RenderObject& renderer)
{
    auto computedValue = renderer.style().clear();
    switch (computedValue) {
    case Clear::None:
        return UsedClear::None;
    case Clear::Left:
        return UsedClear::Left;
    case Clear::Right:
        return UsedClear::Right;
    case Clear::Both:
        return UsedClear::Both;
    case Clear::InlineStart:
    case Clear::InlineEnd:
        auto containingBlockDirection = renderer.containingBlock()->style().direction();
        if (containingBlockDirection == TextDirection::RTL)
            return computedValue == Clear::InlineStart ? UsedClear::Right : UsedClear::Left;
        return computedValue == Clear::InlineStart ? UsedClear::Left : UsedClear::Right;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

UsedFloat RenderStyle::usedFloat(const RenderObject& renderer)
{
    auto computedValue = renderer.style().floating();
    switch (computedValue) {
    case Float::None:
        return UsedFloat::None;
    case Float::Left:
        return UsedFloat::Left;
    case Float::Right:
        return UsedFloat::Right;
    case Float::InlineStart:
    case Float::InlineEnd:
        auto containingBlockDirection = renderer.containingBlock()->style().direction();
        if (containingBlockDirection == TextDirection::RTL)
            return computedValue == Float::InlineStart ? UsedFloat::Right : UsedFloat::Left;
        return computedValue == Float::InlineStart ? UsedFloat::Left : UsedFloat::Right;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

UserSelect RenderStyle::effectiveUserSelect() const
{
    if (effectiveInert())
        return UserSelect::None;

    auto value = userSelect();
    if (userModify() != UserModify::ReadOnly && userDrag() != UserDrag::Element)
        return value == UserSelect::None ? UserSelect::Text : value;

    return value;
}

} // namespace WebCore

#undef SET_VAR
#undef SET_NESTED_VAR
