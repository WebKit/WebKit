/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "AutosizeStatus.h"
#include "CSSCustomPropertyValue.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSValuePool.h"
#include "ColorBlending.h"
#include "FloatRoundedRect.h"
#include "FontCascade.h"
#include "FontSelector.h"
#include "InlineIteratorTextBox.h"
#include "InlineTextBoxStyle.h"
#include "Logging.h"
#include "MotionPath.h"
#include "Pagination.h"
#include "PathTraversalState.h"
#include "RenderBlock.h"
#include "RenderElement.h"
#include "RenderStyleDifference.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include "SVGRenderStyle.h"
#include "ScaleTransformOperation.h"
#include "ScrollAxis.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleExtractor.h"
#include "StyleImage.h"
#include "StyleInheritedData.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleResolver.h"
#include "StyleScaleTransformFunction.h"
#include "StyleSelfAlignmentData.h"
#include "StyleTextDecorationLine.h"
#include "StyleTextTransform.h"
#include "StyleTreeResolver.h"
#include "StyleWebKitLocale.h"
#include "TransformOperationData.h"
#include <algorithm>
#include <wtf/MathExtras.h>
#include <wtf/PointerComparison.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

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
    Style::Color m_color;
    float m_width;
    int m_restBits;
};

static_assert(sizeof(BorderValue) == sizeof(SameSizeAsBorderValue), "BorderValue should not grow");

struct SameSizeAsRenderStyle : CanMakeCheckedPtr<SameSizeAsRenderStyle> {
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

static_assert(PublicPseudoIDBits == allPublicPseudoElementTypes.size());

static_assert(!(static_cast<unsigned>(Style::maxTextTransformValue) >> TextTransformBits));

// Value zero is used to indicate no pseudo-element.
static_assert(!((enumToUnderlyingType(PseudoElementType::HighestEnumValue) + 1) >> PseudoElementTypeBits));

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PseudoStyleCache);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyle);

RenderStyle& RenderStyle::defaultStyleSingleton()
{
    static NeverDestroyed<RenderStyle> style { CreateDefaultStyle };
    return style;
}

RenderStyle RenderStyle::create()
{
    return clone(defaultStyleSingleton());
}

std::unique_ptr<RenderStyle> RenderStyle::createPtr()
{
    return clonePtr(defaultStyleSingleton());
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
    ASSERT(pseudoStyle.pseudoElementType() == PseudoElementType::Before || pseudoStyle.pseudoElementType() == PseudoElementType::After);

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
    m_inheritedFlags.writingMode = WritingMode(initialWritingMode(), initialDirection(), initialTextOrientation()).toData();
    m_inheritedFlags.emptyCells = static_cast<unsigned>(initialEmptyCells());
    m_inheritedFlags.captionSide = static_cast<unsigned>(initialCaptionSide());
    m_inheritedFlags.listStylePosition = static_cast<unsigned>(initialListStylePosition());
    m_inheritedFlags.visibility = static_cast<unsigned>(initialVisibility());
    m_inheritedFlags.textAlign = static_cast<unsigned>(initialTextAlign());
    m_inheritedFlags.textTransform = initialTextTransform().toRaw();
    m_inheritedFlags.textDecorationLineInEffect = initialTextDecorationLine().toRaw();
    m_inheritedFlags.cursorType = static_cast<unsigned>(initialCursor().predefined);
#if ENABLE(CURSOR_VISIBILITY)
    m_inheritedFlags.cursorVisibility = static_cast<unsigned>(initialCursorVisibility());
#endif
    m_inheritedFlags.whiteSpaceCollapse = static_cast<unsigned>(initialWhiteSpaceCollapse());
    m_inheritedFlags.textWrapMode = static_cast<unsigned>(initialTextWrapMode());
    m_inheritedFlags.textWrapStyle = static_cast<unsigned>(initialTextWrapStyle());
    m_inheritedFlags.borderCollapse = static_cast<unsigned>(initialBorderCollapse());
    m_inheritedFlags.rtlOrdering = static_cast<unsigned>(initialRTLOrdering());
    m_inheritedFlags.boxDirection = static_cast<unsigned>(initialBoxDirection());
    m_inheritedFlags.printColorAdjust = static_cast<unsigned>(initialPrintColorAdjust());
    m_inheritedFlags.pointerEvents = static_cast<unsigned>(initialPointerEvents());
    m_inheritedFlags.insideLink = static_cast<unsigned>(InsideLink::NotInside);
#if ENABLE(TEXT_AUTOSIZING)
    m_inheritedFlags.autosizeStatus = 0;
#endif

    m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(initialDisplay());
    m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(initialDisplay());
    m_nonInheritedFlags.overflowX = static_cast<unsigned>(initialOverflowX());
    m_nonInheritedFlags.overflowY = static_cast<unsigned>(initialOverflowY());
    m_nonInheritedFlags.clear = static_cast<unsigned>(initialClear());
    m_nonInheritedFlags.position = static_cast<unsigned>(initialPosition());
    m_nonInheritedFlags.unicodeBidi = static_cast<unsigned>(initialUnicodeBidi());
    m_nonInheritedFlags.floating = static_cast<unsigned>(initialFloating());
    m_nonInheritedFlags.textDecorationLine = initialTextDecorationLine().toRaw();
    m_nonInheritedFlags.usesViewportUnits = false;
    m_nonInheritedFlags.usesContainerUnits = false;
    m_nonInheritedFlags.useTreeCountingFunctions = false;
    m_nonInheritedFlags.hasExplicitlyInheritedProperties = false;
    m_nonInheritedFlags.disallowsFastPathInheritance = false;
    m_nonInheritedFlags.emptyState = false;
    m_nonInheritedFlags.firstChildState = false;
    m_nonInheritedFlags.lastChildState = false;
    m_nonInheritedFlags.isLink = false;
    m_nonInheritedFlags.pseudoElementType = 0;
    m_nonInheritedFlags.pseudoBits = 0;

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
    m_inheritedFlags.hasExplicitlySetColor = inheritParent.m_inheritedFlags.hasExplicitlySetColor;

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
    effectiveDisplay = other.effectiveDisplay;
    originalDisplay = other.originalDisplay;
    overflowX = other.overflowX;
    overflowY = other.overflowY;
    clear = other.clear;
    position = other.position;
    unicodeBidi = other.unicodeBidi;
    floating = other.floating;
    textDecorationLine = other.textDecorationLine;
    usesViewportUnits = other.usesViewportUnits;
    usesContainerUnits = other.usesContainerUnits;
    useTreeCountingFunctions = other.useTreeCountingFunctions;
    hasExplicitlyInheritedProperties = other.hasExplicitlyInheritedProperties;
    disallowsFastPathInheritance = other.disallowsFastPathInheritance;
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
    if (!other.m_nonInheritedData->miscData->content.isData())
        return;
    m_nonInheritedData.access().miscData.access().content = other.m_nonInheritedData->miscData->content;
}

void RenderStyle::setEvaluationTimeZoomEnabled(bool value)
{
    SET_VAR(m_rareInheritedData, evaluationTimeZoomEnabled, value);
}

void RenderStyle::setDeviceScaleFactor(float value)
{
    SET_VAR(m_rareInheritedData, deviceScaleFactor, value);
}

void RenderStyle::setUseSVGZoomRulesForLength(bool value)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, useSVGZoomRulesForLength, value);
}

void RenderStyle::copyPseudoElementsFrom(const RenderStyle& other)
{
    if (!other.m_cachedPseudoStyles)
        return;

    for (auto& [key, pseudoElementStyle] : other.m_cachedPseudoStyles->styles) {
        if (!pseudoElementStyle) {
            ASSERT_NOT_REACHED();
            continue;
        }
        addCachedPseudoStyle(makeUnique<RenderStyle>(cloneIncludingPseudoElements(*pseudoElementStyle)));
    }
}

void RenderStyle::copyPseudoElementBitsFrom(const RenderStyle& other)
{
    m_nonInheritedFlags.pseudoBits = other.m_nonInheritedFlags.pseudoBits;
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

RenderStyle* RenderStyle::getCachedPseudoStyle(const Style::PseudoElementIdentifier& pseudoElementIdentifier) const
{
    if (!m_cachedPseudoStyles)
        return nullptr;

    return m_cachedPseudoStyles->styles.get(pseudoElementIdentifier);
}

RenderStyle* RenderStyle::addCachedPseudoStyle(std::unique_ptr<RenderStyle> pseudo)
{
    if (!pseudo)
        return nullptr;

    ASSERT(pseudo->pseudoElementType());

    RenderStyle* result = pseudo.get();

    if (!m_cachedPseudoStyles)
        m_cachedPseudoStyles = makeUnique<PseudoStyleCache>();

    m_cachedPseudoStyles->styles.add(*result->pseudoElementIdentifier(), WTFMove(pseudo));

    return result;
}

bool RenderStyle::inheritedEqual(const RenderStyle& other) const
{
    return m_inheritedFlags == other.m_inheritedFlags
        && m_inheritedData == other.m_inheritedData
        && (m_svgStyle.ptr() == other.m_svgStyle.ptr() || m_svgStyle->inheritedEqual(other.m_svgStyle))
        && m_rareInheritedData == other.m_rareInheritedData;
}

bool RenderStyle::nonInheritedEqual(const RenderStyle& other) const
{
    return m_nonInheritedFlags == other.m_nonInheritedFlags
        && m_nonInheritedData == other.m_nonInheritedData
        && (m_svgStyle.ptr() == other.m_svgStyle.ptr() || m_svgStyle->nonInheritedEqual(other.m_svgStyle));
}

bool RenderStyle::fastPathInheritedEqual(const RenderStyle& other) const
{
    if (m_inheritedFlags.visibility != other.m_inheritedFlags.visibility)
        return false;
    if (m_inheritedFlags.hasExplicitlySetColor != other.m_inheritedFlags.hasExplicitlySetColor)
        return false;
    if (m_inheritedData.ptr() == other.m_inheritedData.ptr())
        return true;
    return m_inheritedData->fastPathInheritedEqual(*other.m_inheritedData);
}

bool RenderStyle::nonFastPathInheritedEqual(const RenderStyle& other) const
{
    auto withoutFastPathFlags = [](auto flags) {
        flags.visibility = { };
        flags.hasExplicitlySetColor = { };
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

    if (m_nonInheritedData->miscData->alignItems != other.m_nonInheritedData->miscData->alignItems)
        return false;

    if (m_nonInheritedData->miscData->justifyItems != other.m_nonInheritedData->miscData->justifyItems)
        return false;

    if (m_nonInheritedData->miscData->usedAppearance != other.m_nonInheritedData->miscData->usedAppearance)
        return false;

    return true;
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
    unsigned hash = m_nonInheritedData->miscData->usedAppearance;
    hash ^= m_nonInheritedData->rareData->lineClamp.valueForHash();
    hash ^= m_rareInheritedData->overflowWrap;
    hash ^= m_rareInheritedData->nbspMode;
    hash ^= m_rareInheritedData->lineBreak;
    hash ^= m_inheritedData->specifiedLineHeight.valueForHash();
    hash ^= computeFontHash(m_inheritedData->fontData->fontCascade);
    hash ^= WTF::FloatHash<float>::hash(m_inheritedData->borderHorizontalSpacing.unresolvedValue());
    hash ^= WTF::FloatHash<float>::hash(m_inheritedData->borderVerticalSpacing.unresolvedValue());
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
    return m_nonInheritedData->miscData->usedAppearance == other.m_nonInheritedData->miscData->usedAppearance
        && m_nonInheritedData->rareData->lineClamp == other.m_nonInheritedData->rareData->lineClamp
        && m_rareInheritedData->textSizeAdjust == other.m_rareInheritedData->textSizeAdjust
        && m_rareInheritedData->overflowWrap == other.m_rareInheritedData->overflowWrap
        && m_rareInheritedData->nbspMode == other.m_rareInheritedData->nbspMode
        && m_rareInheritedData->lineBreak == other.m_rareInheritedData->lineBreak
        && m_rareInheritedData->textSecurity == other.m_rareInheritedData->textSecurity
        && m_inheritedData->specifiedLineHeight == other.m_inheritedData->specifiedLineHeight
        && m_inheritedData->fontData->fontCascade.equalForTextAutoSizing(other.m_inheritedData->fontData->fontCascade)
        && m_inheritedData->borderHorizontalSpacing == other.m_inheritedData->borderHorizontalSpacing
        && m_inheritedData->borderVerticalSpacing == other.m_inheritedData->borderVerticalSpacing
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
            if (whiteSpaceCollapse() == WhiteSpaceCollapse::Collapse && textWrapMode() == TextWrapMode::NoWrap) {
                if (width().isFixed())
                    return false;
                if (auto fixedHeight = height().tryFixed(); fixedHeight && specifiedLineHeight().isFixed()) {
                    if (auto fixedSpecifiedLineHeight = specifiedLineHeight().tryFixed()) {
                        float specifiedSize = specifiedFontSize();
                        if (fixedHeight->resolveZoom(usedZoomForLength()) == specifiedSize && fixedSpecifiedLineHeight->resolveZoom(usedZoomForLength()) == specifiedSize)
                            return false;
                    }
                }
                return true;
            }
            if (fields.contains(AutosizeStatus::Fields::Floating)) {
                if (auto fixedHeight = height().tryFixed(); specifiedLineHeight().isFixed() && fixedHeight) {
                    if (auto fixedSpecifiedLineHeight = specifiedLineHeight().tryFixed()) {
                        float specifiedSize = specifiedFontSize();
                        if (fixedSpecifiedLineHeight->resolveZoom(Style::ZoomFactor { 1.0f, deviceScaleFactor() }) - specifiedSize > smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText
                            && fixedHeight->resolveZoom(usedZoomForLength()) - specifiedSize > smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText)
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
        if (auto fixedSpecifiedLineHeight = specifiedLineHeight().tryFixed(); fixedSpecifiedLineHeight && fixedSpecifiedLineHeight->resolveZoom(usedZoomForLength()) - specifiedFontSize() > largeMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText)
            return true;
        return false;
    }

    if (hasBackgroundImage() && backgroundLayers().usedFirst().repeat() == FillRepeat::NoRepeat)
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

static bool positionChangeIsMovementOnly(const Style::InsetBox& a, const Style::InsetBox& b, const Style::PreferredSize& width)
{
    // If any unit types are different, then we can't guarantee
    // that this was just a movement.
    if (!a.left().hasSameType(b.left())
        || !a.right().hasSameType(b.right())
        || !a.top().hasSameType(b.top())
        || !a.bottom().hasSameType(b.bottom()))
        return false;

    // Only one unit can be non-auto in the horizontal direction and
    // in the vertical direction.  Otherwise the adjustment of values
    // is changing the size of the box.
    if (!a.left().isAuto() && !a.right().isAuto())
        return false;
    if (!a.top().isAuto() && !a.bottom().isAuto())
        return false;
    // If our width is auto and left or right is specified then this 
    // is not just a movement - we need to resize to our container.
    if ((!a.left().isAuto() || !a.right().isAuto()) && width.isIntrinsicOrLegacyIntrinsicOrAuto())
        return false;

    // One of the units is fixed or percent in both directions and stayed
    // that way in the new style.  Therefore all we are doing is moving.
    return true;
}

inline bool RenderStyle::changeAffectsVisualOverflow(const RenderStyle& other) const
{
    auto nonInheritedDataChangeAffectsVisualOverflow = [&]() {
        if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr())
            return false;

        if (m_nonInheritedData->miscData.ptr() != other.m_nonInheritedData->miscData.ptr()
            && m_nonInheritedData->miscData->boxShadow != other.m_nonInheritedData->miscData->boxShadow)
            return true;

        if (m_nonInheritedData->backgroundData.ptr() != other.m_nonInheritedData->backgroundData.ptr()) {
            auto hasOutlineInVisualOverflow = this->hasOutlineInVisualOverflow();
            auto otherHasOutlineInVisualOverflow = other.hasOutlineInVisualOverflow();
            if (hasOutlineInVisualOverflow != otherHasOutlineInVisualOverflow
                || (hasOutlineInVisualOverflow && otherHasOutlineInVisualOverflow && outlineSize() != other.outlineSize()))
                return true;
        }

        return false;
    };

    auto textDecorationsDiffer = [&]() {
        if (m_inheritedFlags.textDecorationLineInEffect != other.m_inheritedFlags.textDecorationLineInEffect)
            return true;

        if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr() && m_nonInheritedData->rareData.ptr() != other.m_nonInheritedData->rareData.ptr()) {
            if (m_nonInheritedData->rareData->textDecorationStyle != other.m_nonInheritedData->rareData->textDecorationStyle
                || m_nonInheritedData->rareData->textDecorationThickness != other.m_nonInheritedData->rareData->textDecorationThickness)
                return true;
        }

        if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()) {
            if (m_rareInheritedData->textUnderlineOffset != other.m_rareInheritedData->textUnderlineOffset
                || m_rareInheritedData->textUnderlinePosition != other.m_rareInheritedData->textUnderlinePosition)
                    return true;
        }

        return false;
    };

    if (nonInheritedDataChangeAffectsVisualOverflow())
        return true;

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()
        && m_rareInheritedData->textShadow != other.m_rareInheritedData->textShadow)
        return true;

    if (textDecorationsDiffer()) {
        // Underlines are always drawn outside of their textbox bounds when text-underline-position: under;
        // is specified. We can take an early out here.
        if (isAlignedForUnder(*this) || isAlignedForUnder(other))
            return true;

        if (inkOverflowForDecorations(*this) != inkOverflowForDecorations(other))
            return true;
    }

    return false;
}

static bool miscDataChangeRequiresLayout(const StyleMiscNonInheritedData& first, const StyleMiscNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
    ASSERT(&first != &second);

    if (first.usedAppearance != second.usedAppearance
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

    if (first.opacity.isOpaque() != second.opacity.isOpaque()) {
        // FIXME: We would like to use SimplifiedLayout here, but we can't quite do that yet.
        // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
        // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
        // In addition we need to solve the floating object issue when layers come and go. Right now
        // a full layout is necessary to keep floating object lists sane.
        return true;
    }

    if (first.hasFilters() != second.hasFilters())
        return true;

    if (first.aspectRatio != second.aspectRatio)
        return true;

    return false;
}

static bool rareDataChangeRequiresLayout(const StyleRareNonInheritedData& first, const StyleRareNonInheritedData& second, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
{
    ASSERT(&first != &second);

    if (first.lineClamp != second.lineClamp || first.initialLetter != second.initialLetter)
        return true;

    if (first.shapeMargin != second.shapeMargin)
        return true;

    if (first.columnGap != second.columnGap || first.rowGap != second.rowGap)
        return true;

    if (first.boxReflect != second.boxReflect)
        return true;

    // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
    if (first.counterDirectives != second.counterDirectives)
        return true;

    if (first.scale != second.scale || first.rotate != second.rotate || first.translate != second.translate)
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Transform);

    if (first.offsetPath != second.offsetPath
        || first.offsetPosition != second.offsetPosition
        || first.offsetDistance != second.offsetDistance
        || first.offsetAnchor != second.offsetAnchor
        || first.offsetRotate != second.offsetRotate)
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Transform);

    if (first.grid != second.grid
        || first.gridItem != second.gridItem)
        return true;

    if (first.willChange != second.willChange) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::WillChange);
        // Don't return; keep looking for another change
    }

    if (first.breakBefore != second.breakBefore || first.breakAfter != second.breakAfter || first.breakInside != second.breakInside)
        return true;

    if (first.isolation != second.isolation) {
        // Ideally this would trigger a cheaper layout that just updates layer z-order trees (webkit.org/b/190088).
        return true;
    }

    if (first.hasBackdropFilters() != second.hasBackdropFilters())
        return true;

#if HAVE(CORE_MATERIAL)
    if (first.appleVisualEffect != second.appleVisualEffect)
        return true;
#endif

    if (first.inputSecurity != second.inputSecurity)
        return true;

    if (first.usedContain().contains(Style::ContainValue::Size) != second.usedContain().contains(Style::ContainValue::Size)
        || first.usedContain().contains(Style::ContainValue::InlineSize) != second.usedContain().contains(Style::ContainValue::InlineSize)
        || first.usedContain().contains(Style::ContainValue::Layout) != second.usedContain().contains(Style::ContainValue::Layout))
        return true;

    // content-visibility:hidden turns on contain:size which requires relayout.
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

    if (first.textBoxTrim != second.textBoxTrim)
        return true;

    if (first.maxLines != second.maxLines)
        return true;

    if (first.overflowContinue != second.overflowContinue)
        return true;

    // CSS Anchor Positioning.
    if (first.anchorScope != second.anchorScope || first.positionArea != second.positionArea)
        return true;

    if (first.fieldSizing != second.fieldSizing)
        return true;

    return false;
}

static bool rareInheritedDataChangeRequiresLayout(const StyleRareInheritedData& first, const StyleRareInheritedData& second)
{
    ASSERT(&first != &second);

    if (first.textIndent != second.textIndent
        || first.textAlignLast != second.textAlignLast
        || first.textJustify != second.textJustify
        || first.textBoxEdge != second.textBoxEdge
        || first.lineFitEdge != second.lineFitEdge
        || first.usedZoom != second.usedZoom
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
        || first.hyphenateLimitBefore != second.hyphenateLimitBefore
        || first.hyphenateLimitAfter != second.hyphenateLimitAfter
        || first.hyphenateCharacter != second.hyphenateCharacter
        || first.rubyPosition != second.rubyPosition
        || first.rubyAlign != second.rubyAlign
        || first.rubyOverhang != second.rubyOverhang
        || first.textCombine != second.textCombine
        || first.textEmphasisStyle != second.textEmphasisStyle
        || first.textEmphasisPosition != second.textEmphasisPosition
        || first.tabSize != second.tabSize
        || first.lineBoxContain != second.lineBoxContain
        || first.lineGrid != second.lineGrid
        || first.imageOrientation != second.imageOrientation
        || first.lineSnap != second.lineSnap
        || first.lineAlign != second.lineAlign
        || first.hangingPunctuation != second.hangingPunctuation
        || first.usedContentVisibility != second.usedContentVisibility
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
        || first.overflowScrolling != second.overflowScrolling
#endif
        || first.listStyleType != second.listStyleType
        || first.listStyleImage != second.listStyleImage
        || first.blockEllipsis != second.blockEllipsis)
        return true;

    if (first.textStrokeWidth != second.textStrokeWidth)
        return true;

    // These properties affect the cached stroke bounding box rects.
    if (first.capStyle != second.capStyle
        || first.joinStyle != second.joinStyle
        || first.strokeWidth != second.strokeWidth
        || first.strokeMiterLimit != second.strokeMiterLimit)
        return true;

    if (first.quotes != second.quotes)
        return true;

    return false;
}

bool RenderStyle::changeRequiresLayout(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    if (m_svgStyle.ptr() != other.m_svgStyle.ptr() && m_svgStyle->changeRequiresLayout(other.m_svgStyle))
        return true;

    if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr()) {
        if (m_nonInheritedData->boxData.ptr() != other.m_nonInheritedData->boxData.ptr()) {
            if (m_nonInheritedData->boxData->width != other.m_nonInheritedData->boxData->width
                || m_nonInheritedData->boxData->minWidth != other.m_nonInheritedData->boxData->minWidth
                || m_nonInheritedData->boxData->maxWidth != other.m_nonInheritedData->boxData->maxWidth
                || m_nonInheritedData->boxData->height != other.m_nonInheritedData->boxData->height
                || m_nonInheritedData->boxData->minHeight != other.m_nonInheritedData->boxData->minHeight
                || m_nonInheritedData->boxData->maxHeight != other.m_nonInheritedData->boxData->maxHeight)
                return true;

            if (m_nonInheritedData->boxData->verticalAlign != other.m_nonInheritedData->boxData->verticalAlign)
                return true;

            if (m_nonInheritedData->boxData->boxSizing != other.m_nonInheritedData->boxData->boxSizing)
                return true;

            if (m_nonInheritedData->boxData->hasAutoUsedZIndex != other.m_nonInheritedData->boxData->hasAutoUsedZIndex)
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
                if (m_nonInheritedData->surroundData->inset != other.m_nonInheritedData->surroundData->inset) {
                    // FIXME: We would like to use SimplifiedLayout for relative positioning, but we can't quite do that yet.
                    // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
                    // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
                    if (position() != PositionType::Absolute)
                        return true;

                    // Optimize for the case where a positioned layer is moving but not changing size.
                    if (!positionChangeIsMovementOnly(m_nonInheritedData->surroundData->inset, other.m_nonInheritedData->surroundData->inset, m_nonInheritedData->boxData->width))
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
            || m_inheritedData->borderHorizontalSpacing != other.m_inheritedData->borderHorizontalSpacing
            || m_inheritedData->borderVerticalSpacing != other.m_inheritedData->borderVerticalSpacing)
            return true;

        if (m_inheritedData->fontData != other.m_inheritedData->fontData)
            return true;
    }

    if (m_inheritedFlags.boxDirection != other.m_inheritedFlags.boxDirection
        || m_inheritedFlags.rtlOrdering != other.m_inheritedFlags.rtlOrdering
        || m_nonInheritedFlags.position != other.m_nonInheritedFlags.position
        || m_nonInheritedFlags.floating != other.m_nonInheritedFlags.floating
        || m_nonInheritedFlags.originalDisplay != other.m_nonInheritedFlags.originalDisplay)
        return true;

    if (static_cast<DisplayType>(m_nonInheritedFlags.effectiveDisplay) >= DisplayType::Table) {
        if (m_inheritedFlags.borderCollapse != other.m_inheritedFlags.borderCollapse
            || m_inheritedFlags.emptyCells != other.m_inheritedFlags.emptyCells
            || m_inheritedFlags.captionSide != other.m_inheritedFlags.captionSide
            || tableLayout() != other.tableLayout())
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
        || m_inheritedFlags.whiteSpaceCollapse != other.m_inheritedFlags.whiteSpaceCollapse
        || m_inheritedFlags.textWrapMode != other.m_inheritedFlags.textWrapMode
        || m_inheritedFlags.textWrapStyle != other.m_inheritedFlags.textWrapStyle
        || m_nonInheritedFlags.clear != other.m_nonInheritedFlags.clear
        || m_nonInheritedFlags.unicodeBidi != other.m_nonInheritedFlags.unicodeBidi)
        return true;

    if (writingMode() != other.writingMode())
        return true;

    // Overflow returns a layout hint.
    if (m_nonInheritedFlags.overflowX != other.m_nonInheritedFlags.overflowX
        || m_nonInheritedFlags.overflowY != other.m_nonInheritedFlags.overflowY)
        return true;

    if ((usedVisibility() == Visibility::Collapse) != (other.usedVisibility() == Visibility::Collapse))
        return true;

    bool hasFirstLineStyle = hasPseudoStyle(PseudoElementType::FirstLine);
    if (hasFirstLineStyle != other.hasPseudoStyle(PseudoElementType::FirstLine))
        return true;

    if (hasFirstLineStyle) {
        auto* firstLineStyle = getCachedPseudoStyle({ PseudoElementType::FirstLine });
        if (!firstLineStyle)
            return true;
        auto* otherFirstLineStyle = other.getCachedPseudoStyle({ PseudoElementType::FirstLine });
        if (!otherFirstLineStyle)
            return true;
        // FIXME: Not all first line style changes actually need layout.
        if (*firstLineStyle != *otherFirstLineStyle)
            return true;
    }

    return false;
}

bool RenderStyle::changeRequiresOutOfFlowMovementLayoutOnly(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>&) const
{
    if (position() != PositionType::Absolute)
        return false;

    // Optimize for the case where a out-of-flow box is moving but not changing size.
    return (m_nonInheritedData->surroundData->inset != other.m_nonInheritedData->surroundData->inset) && positionChangeIsMovementOnly(m_nonInheritedData->surroundData->inset, other.m_nonInheritedData->surroundData->inset, m_nonInheritedData->boxData->width);
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
    if (first.effectiveBlendMode != second.effectiveBlendMode)
        return true;

    if (first.backdropFilter != second.backdropFilter) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Filter);
        // Don't return true; keep looking for another change.
    }

    // FIXME: In SVG this needs to trigger a layout.
    if (first.maskBorder != second.maskBorder)
        return true;

    return false;
}

bool RenderStyle::changeRequiresLayerRepaint(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    // Style::Resolver has ensured that zIndex is non-auto only if it's applicable.

    if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr()) {
        if (m_nonInheritedData->boxData.ptr() != other.m_nonInheritedData->boxData.ptr()) {
            if (m_nonInheritedData->boxData->usedZIndex() != other.m_nonInheritedData->boxData->usedZIndex())
                return true;
        }

        if (position() != PositionType::Static) {
            if (m_nonInheritedData->rareData.ptr() != other.m_nonInheritedData->rareData.ptr()) {
                if (m_nonInheritedData->rareData->clip != other.m_nonInheritedData->rareData->clip) {
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

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()
        && m_rareInheritedData->dynamicRangeLimit != other.m_rareInheritedData->dynamicRangeLimit) {
        return true;
    }

#if HAVE(CORE_MATERIAL)
    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()
        && m_rareInheritedData->usedAppleVisualEffectForSubtree != other.m_rareInheritedData->usedAppleVisualEffectForSubtree) {
        changedContextSensitiveProperties.add(StyleDifferenceContextSensitiveProperty::Filter);
    }
#endif

    bool currentColorDiffers = m_inheritedData->color != other.m_inheritedData->color;
    if (currentColorDiffers) {
        if (filter().hasFilterThatRequiresRepaintForCurrentColorChange() || backdropFilter().hasFilterThatRequiresRepaintForCurrentColorChange())
            return true;
    }

    return false;
}

static bool requiresPainting(const RenderStyle& style)
{
    if (style.usedVisibility() == Visibility::Hidden)
        return false;
    if (style.opacity().isTransparent())
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

    if (first.textDecorationStyle != second.textDecorationStyle || first.textDecorationColor != second.textDecorationColor || first.textDecorationThickness != second.textDecorationThickness)
        return true;

    return false;
}

static bool rareInheritedDataChangeRequiresRepaint(const StyleRareInheritedData& first, const StyleRareInheritedData& second)
{
    return first.effectiveInert != second.effectiveInert
        || first.userModify != second.userModify
        || first.userSelect != second.userSelect
        || first.appleColorFilter != second.appleColorFilter
        || first.imageRendering != second.imageRendering
        || first.accentColor != second.accentColor
        || first.insideDefaultButton != second.insideDefaultButton
        || first.insideSubmitButton != second.insideSubmitButton
#if ENABLE(DARK_MODE_CSS)
        || first.colorScheme != second.colorScheme
#endif
    ;
}

void RenderStyle::addCustomPaintWatchProperty(const AtomString& name)
{
    auto& data = m_nonInheritedData.access().rareData.access();
    data.customPaintWatchedProperties.add(name);
}

inline static bool changedCustomPaintWatchedProperty(const RenderStyle& a, const StyleRareNonInheritedData& aData, const RenderStyle& b, const StyleRareNonInheritedData& bData)
{
    auto& propertiesA = aData.customPaintWatchedProperties;
    auto& propertiesB = bData.customPaintWatchedProperties;

    if (!propertiesA.isEmpty() || !propertiesB.isEmpty()) [[unlikely]] {
        // FIXME: We should not need to use Style::Extractor here.
        Style::Extractor extractor((Element*) nullptr);
        auto& pool = CSSValuePool::singleton();

        for (auto& watchPropertiesMap : { propertiesA, propertiesB }) {
            for (auto& name : watchPropertiesMap) {
                if (isCustomPropertyName(name)) {
                    auto valueA = a.customPropertyValue(name);
                    auto valueB = b.customPropertyValue(name);

                    if (valueA != valueB && (!valueA || !valueB || *valueA != *valueB))
                        return true;
                } else if (auto propertyID = cssPropertyID(name)) {
                    auto valueA = extractor.propertyValueInStyle(a, propertyID, pool);
                    auto valueB = extractor.propertyValueInStyle(b, propertyID, pool);

                    if (valueA != valueB && (!valueA || !valueB || *valueA != *valueB))
                        return true;
                }
            }
        }
    }

    return false;
}

bool RenderStyle::changeRequiresRepaint(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    bool currentColorDiffers = m_inheritedData->color != other.m_inheritedData->color;

    if (currentColorDiffers || m_svgStyle.ptr() != other.m_svgStyle.ptr()) {
        if (m_svgStyle->changeRequiresRepaint(other.m_svgStyle, currentColorDiffers))
            return true;
    }

    if (!requiresPainting(*this) && !requiresPainting(other))
        return false;

    if (usedVisibility() != other.usedVisibility()
        || m_inheritedFlags.printColorAdjust != other.m_inheritedFlags.printColorAdjust
        || m_inheritedFlags.insideLink != other.m_inheritedFlags.insideLink)
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

    if (changedCustomPaintWatchedProperty(*this, *m_nonInheritedData->rareData, other, *other.m_nonInheritedData->rareData))
        return true;

    return false;
}

bool RenderStyle::changeRequiresRepaintIfText(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>&) const
{
    // FIXME: Does this code need to consider currentColorDiffers? webkit.org/b/266833
    if (m_inheritedData->color != other.m_inheritedData->color)
        return true;

    // Note that we may reach this function with mutated text-decoration values (e.g. thickness), when visual overflow recompute is not required.
    // see RenderStyle::changeAffectsVisualOverflow
    if (m_inheritedFlags.textDecorationLineInEffect != other.m_inheritedFlags.textDecorationLineInEffect
        || m_nonInheritedFlags.textDecorationLine != other.m_nonInheritedFlags.textDecorationLine)
        return true;

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr()) {
        if (m_rareInheritedData->textDecorationSkipInk != other.m_rareInheritedData->textDecorationSkipInk
            || m_rareInheritedData->textFillColor != other.m_rareInheritedData->textFillColor
            || m_rareInheritedData->textStrokeColor != other.m_rareInheritedData->textStrokeColor
            || m_rareInheritedData->textEmphasisColor != other.m_rareInheritedData->textEmphasisColor
            || m_rareInheritedData->textEmphasisStyle != other.m_rareInheritedData->textEmphasisStyle
            || m_rareInheritedData->strokeColor != other.m_rareInheritedData->strokeColor
            || m_rareInheritedData->caretColor != other.m_rareInheritedData->caretColor
            || m_rareInheritedData->textUnderlineOffset != other.m_rareInheritedData->textUnderlineOffset)
            return true;
    }

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
            || m_nonInheritedData->rareData->perspectiveOrigin != other.m_nonInheritedData->rareData->perspectiveOrigin
            || m_nonInheritedData->rareData->overscrollBehaviorX != other.m_nonInheritedData->rareData->overscrollBehaviorX
            || m_nonInheritedData->rareData->overscrollBehaviorY != other.m_nonInheritedData->rareData->overscrollBehaviorY)
            return true;
    }

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr() && m_rareInheritedData->effectiveInert != other.m_rareInheritedData->effectiveInert)
        return true;

    return false;
}

bool RenderStyle::scrollAnchoringSuppressionStyleDidChange(const RenderStyle* other) const
{
    // https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
    // Determine if there are any style changes that should result in an scroll anchoring suppression
    if (!other)
        return false;

    if (m_nonInheritedData->boxData.ptr() != other->m_nonInheritedData->boxData.ptr()) {
        if (m_nonInheritedData->boxData->width != other->m_nonInheritedData->boxData->width
            || m_nonInheritedData->boxData->minWidth != other->m_nonInheritedData->boxData->minWidth
            || m_nonInheritedData->boxData->maxWidth != other->m_nonInheritedData->boxData->maxWidth
            || m_nonInheritedData->boxData->height != other->m_nonInheritedData->boxData->height
            || m_nonInheritedData->boxData->minHeight != other->m_nonInheritedData->boxData->minHeight
            || m_nonInheritedData->boxData->maxHeight != other->m_nonInheritedData->boxData->maxHeight)
            return true;
    }

    if (overflowAnchor() != other->overflowAnchor() && overflowAnchor() == OverflowAnchor::None)
        return true;

    if (position() != other->position())
        return true;

    if (m_nonInheritedData->surroundData.ptr() && other->m_nonInheritedData->surroundData.ptr() && m_nonInheritedData->surroundData != other->m_nonInheritedData->surroundData) {
        if (m_nonInheritedData->surroundData->margin != other->m_nonInheritedData->surroundData->margin)
            return true;

        if (m_nonInheritedData->surroundData->padding != other->m_nonInheritedData->surroundData->padding)
            return true;
    }

    if (position() != PositionType::Static) {
        if (m_nonInheritedData->surroundData->inset != other->m_nonInheritedData->surroundData->inset)
            return true;
    }

    if (hasTransformRelatedProperty() != other->hasTransformRelatedProperty() || transform() != other->transform())
        return true;

    return false;
}

bool RenderStyle::outOfFlowPositionStyleDidChange(const RenderStyle* other) const
{
    // https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
    // Determine if there is a style change that causes an element to become or stop
    // being absolutely or fixed positioned
    return other && hasOutOfFlowPosition() != other->hasOutOfFlowPosition();
}

StyleDifference RenderStyle::diff(const RenderStyle& other, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const
{
    changedContextSensitiveProperties = OptionSet<StyleDifferenceContextSensitiveProperty>();

    if (changeRequiresLayout(other, changedContextSensitiveProperties))
        return StyleDifference::Layout;

    if (changeRequiresOutOfFlowMovementLayoutOnly(other, changedContextSensitiveProperties))
        return StyleDifference::LayoutOutOfFlowMovementOnly;

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

void RenderStyle::conservativelyCollectChangedAnimatableProperties(const RenderStyle& other, CSSPropertiesBitSet& changingProperties) const
{
    // FIXME: Consider auto-generating this function from CSSProperties.json.

    // This function conservatively answers what CSS properties we should visit for CSS transitions.
    // We do not need to precisely check equivalence before saying "this property needs to be visited".
    // Right now, we are designing this based on Speedometer3.0 data.

    auto conservativelyCollectChangedAnimatablePropertiesViaInheritedFlags = [&](auto& first, auto& second) {
        if (first.emptyCells != second.emptyCells)
            changingProperties.m_properties.set(CSSPropertyEmptyCells);
        if (first.captionSide != second.captionSide)
            changingProperties.m_properties.set(CSSPropertyCaptionSide);
        if (first.listStylePosition != second.listStylePosition)
            changingProperties.m_properties.set(CSSPropertyListStylePosition);
        if (first.visibility != second.visibility)
            changingProperties.m_properties.set(CSSPropertyVisibility);
        if (first.textAlign != second.textAlign)
            changingProperties.m_properties.set(CSSPropertyTextAlign);
        if (first.textTransform != second.textTransform)
            changingProperties.m_properties.set(CSSPropertyTextTransform);
        if (first.textDecorationLineInEffect != second.textDecorationLineInEffect)
            changingProperties.m_properties.set(CSSPropertyTextDecorationLine);
        if (first.cursorType != second.cursorType)
            changingProperties.m_properties.set(CSSPropertyCursor);
        if (first.whiteSpaceCollapse != second.whiteSpaceCollapse)
            changingProperties.m_properties.set(CSSPropertyWhiteSpaceCollapse);
        if (first.textWrapMode != second.textWrapMode)
            changingProperties.m_properties.set(CSSPropertyTextWrapMode);
        if (first.textWrapStyle != second.textWrapStyle)
            changingProperties.m_properties.set(CSSPropertyTextWrapStyle);
        if (first.borderCollapse != second.borderCollapse)
            changingProperties.m_properties.set(CSSPropertyBorderCollapse);
        if (first.printColorAdjust != second.printColorAdjust)
            changingProperties.m_properties.set(CSSPropertyPrintColorAdjust);
        if (first.pointerEvents != second.pointerEvents)
            changingProperties.m_properties.set(CSSPropertyPointerEvents);

        // Writing mode changes conversion of logical -> physical properties.
        // Thus we need to list up all physical properties.
        if (first.writingMode != second.writingMode) {
            changingProperties.m_properties.merge(CSSProperty::physicalProperties);
            if (WritingMode(first.writingMode).isVerticalTypographic() != WritingMode(second.writingMode).isVerticalTypographic())
                changingProperties.m_properties.set(CSSPropertyTextEmphasisStyle);
        }

        // insideLink changes visited / non-visited colors.
        // Thus we need to list up all color properties.
        if (first.insideLink != second.insideLink)
            changingProperties.m_properties.merge(CSSProperty::colorProperties);

        // Non animated styles are followings.
        // cursorVisibility
        // boxDirection
        // rtlOrdering
        // autosizeStatus
        // hasExplicitlySetColor
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaNonInheritedFlags = [&](auto& first, auto& second) {
        if (first.overflowX != second.overflowX)
            changingProperties.m_properties.set(CSSPropertyOverflowX);
        if (first.overflowY != second.overflowY)
            changingProperties.m_properties.set(CSSPropertyOverflowY);
        if (first.clear != second.clear)
            changingProperties.m_properties.set(CSSPropertyClear);
        if (first.position != second.position)
            changingProperties.m_properties.set(CSSPropertyPosition);
        if (first.effectiveDisplay != second.effectiveDisplay)
            changingProperties.m_properties.set(CSSPropertyDisplay);
        if (first.floating != second.floating)
            changingProperties.m_properties.set(CSSPropertyFloat);
        if (first.textDecorationLine != second.textDecorationLine)
            changingProperties.m_properties.set(CSSPropertyTextDecorationLine);

        // Non animated styles are followings.
        // originalDisplay
        // unicodeBidi
        // usesViewportUnits
        // usesContainerUnits
        // useTreeCountingFunctions
        // hasExplicitlyInheritedProperties
        // disallowsFastPathInheritance
        // hasContentNone
        // emptyState
        // firstChildState
        // lastChildState
        // isLink
        // pseudoElementType
        // pseudoBits
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaTransformData = [&](auto& first, auto& second) {
        if (first.origin.x != second.origin.x)
            changingProperties.m_properties.set(CSSPropertyTransformOriginX);
        if (first.origin.y != second.origin.y)
            changingProperties.m_properties.set(CSSPropertyTransformOriginY);
        if (first.origin.z != second.origin.z)
            changingProperties.m_properties.set(CSSPropertyTransformOriginZ);
        if (static_cast<TransformBox>(first.transformBox) != static_cast<TransformBox>(second.transformBox))
            changingProperties.m_properties.set(CSSPropertyTransformBox);
        if (first.transform != second.transform)
            changingProperties.m_properties.set(CSSPropertyTransform);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaNonInheritedBoxData = [&](auto& first, auto& second) {
        if (first.width != second.width)
            changingProperties.m_properties.set(CSSPropertyWidth);
        if (first.height != second.height)
            changingProperties.m_properties.set(CSSPropertyHeight);
        if (first.minWidth != second.minWidth)
            changingProperties.m_properties.set(CSSPropertyMinWidth);
        if (first.maxWidth != second.maxWidth)
            changingProperties.m_properties.set(CSSPropertyMaxWidth);
        if (first.minHeight != second.minHeight)
            changingProperties.m_properties.set(CSSPropertyMinHeight);
        if (first.maxHeight != second.maxHeight)
            changingProperties.m_properties.set(CSSPropertyMaxHeight);
        if (first.verticalAlign != second.verticalAlign)
            changingProperties.m_properties.set(CSSPropertyVerticalAlign);
        if (first.specifiedZIndex() != second.specifiedZIndex())
            changingProperties.m_properties.set(CSSPropertyZIndex);
        if (static_cast<BoxSizing>(first.boxSizing) != static_cast<BoxSizing>(second.boxSizing))
            changingProperties.m_properties.set(CSSPropertyBoxSizing);
        if (static_cast<BoxDecorationBreak>(first.boxDecorationBreak) != static_cast<BoxDecorationBreak>(second.boxDecorationBreak))
            changingProperties.m_properties.set(CSSPropertyWebkitBoxDecorationBreak);
        // Non animated styles are followings.
        // usedZIndex
        // hasAutoUsedZIndex
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaNonInheritedBackgroundData = [&](auto& first, auto& second) {
        if (first.background != second.background) {
            changingProperties.m_properties.set(CSSPropertyBackgroundImage);
            changingProperties.m_properties.set(CSSPropertyBackgroundPositionX);
            changingProperties.m_properties.set(CSSPropertyBackgroundPositionY);
            changingProperties.m_properties.set(CSSPropertyBackgroundSize);
            changingProperties.m_properties.set(CSSPropertyBackgroundAttachment);
            changingProperties.m_properties.set(CSSPropertyBackgroundClip);
            changingProperties.m_properties.set(CSSPropertyBackgroundOrigin);
            changingProperties.m_properties.set(CSSPropertyBackgroundRepeat);
            changingProperties.m_properties.set(CSSPropertyBackgroundBlendMode);
        }
        if (first.backgroundColor != second.backgroundColor)
            changingProperties.m_properties.set(CSSPropertyBackgroundColor);
        if (first.outline != second.outline) {
            changingProperties.m_properties.set(CSSPropertyOutlineColor);
            changingProperties.m_properties.set(CSSPropertyOutlineStyle);
            changingProperties.m_properties.set(CSSPropertyOutlineWidth);
            changingProperties.m_properties.set(CSSPropertyOutlineOffset);
        }
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaNonInheritedSurroundData = [&](auto& first, auto& second) {
        if (first.inset.top() != second.inset.top())
            changingProperties.m_properties.set(CSSPropertyTop);
        if (first.inset.left() != second.inset.left())
            changingProperties.m_properties.set(CSSPropertyLeft);
        if (first.inset.bottom() != second.inset.bottom())
            changingProperties.m_properties.set(CSSPropertyBottom);
        if (first.inset.right() != second.inset.right())
            changingProperties.m_properties.set(CSSPropertyRight);

        if (first.margin.top() != second.margin.top())
            changingProperties.m_properties.set(CSSPropertyMarginTop);
        if (first.margin.left() != second.margin.left())
            changingProperties.m_properties.set(CSSPropertyMarginLeft);
        if (first.margin.bottom() != second.margin.bottom())
            changingProperties.m_properties.set(CSSPropertyMarginBottom);
        if (first.margin.right() != second.margin.right())
            changingProperties.m_properties.set(CSSPropertyMarginRight);

        if (first.padding.top() != second.padding.top())
            changingProperties.m_properties.set(CSSPropertyPaddingTop);
        if (first.padding.left() != second.padding.left())
            changingProperties.m_properties.set(CSSPropertyPaddingLeft);
        if (first.padding.bottom() != second.padding.bottom())
            changingProperties.m_properties.set(CSSPropertyPaddingBottom);
        if (first.padding.right() != second.padding.right())
            changingProperties.m_properties.set(CSSPropertyPaddingRight);

        if (first.border != second.border) {
            if (first.border.top() != second.border.top()) {
                changingProperties.m_properties.set(CSSPropertyBorderTopWidth);
                changingProperties.m_properties.set(CSSPropertyBorderTopColor);
                changingProperties.m_properties.set(CSSPropertyBorderTopStyle);
            }
            if (first.border.left() != second.border.left()) {
                changingProperties.m_properties.set(CSSPropertyBorderLeftWidth);
                changingProperties.m_properties.set(CSSPropertyBorderLeftColor);
                changingProperties.m_properties.set(CSSPropertyBorderLeftStyle);
            }
            if (first.border.bottom() != second.border.bottom()) {
                changingProperties.m_properties.set(CSSPropertyBorderBottomWidth);
                changingProperties.m_properties.set(CSSPropertyBorderBottomColor);
                changingProperties.m_properties.set(CSSPropertyBorderBottomStyle);
            }
            if (first.border.right() != second.border.right()) {
                changingProperties.m_properties.set(CSSPropertyBorderRightWidth);
                changingProperties.m_properties.set(CSSPropertyBorderRightColor);
                changingProperties.m_properties.set(CSSPropertyBorderRightStyle);
            }
            if (first.border.image() != second.border.image()) {
                changingProperties.m_properties.set(CSSPropertyBorderImageSlice);
                changingProperties.m_properties.set(CSSPropertyBorderImageWidth);
                changingProperties.m_properties.set(CSSPropertyBorderImageRepeat);
                changingProperties.m_properties.set(CSSPropertyBorderImageSource);
                changingProperties.m_properties.set(CSSPropertyBorderImageOutset);
            }
            if (first.border.topLeftRadius() != second.border.topLeftRadius())
                changingProperties.m_properties.set(CSSPropertyBorderTopLeftRadius);
            if (first.border.topRightRadius() != second.border.topRightRadius())
                changingProperties.m_properties.set(CSSPropertyBorderTopRightRadius);
            if (first.border.bottomLeftRadius() != second.border.bottomLeftRadius())
                changingProperties.m_properties.set(CSSPropertyBorderBottomLeftRadius);
            if (first.border.bottomRightRadius() != second.border.bottomRightRadius())
                changingProperties.m_properties.set(CSSPropertyBorderBottomRightRadius);

            if (first.border.topLeftCornerShape() != second.border.topLeftCornerShape())
                changingProperties.m_properties.set(CSSPropertyCornerTopLeftShape);
            if (first.border.topRightCornerShape() != second.border.topRightCornerShape())
                changingProperties.m_properties.set(CSSPropertyCornerTopRightShape);
            if (first.border.bottomLeftCornerShape() != second.border.bottomLeftCornerShape())
                changingProperties.m_properties.set(CSSPropertyCornerBottomLeftShape);
            if (first.border.bottomRightCornerShape() != second.border.bottomRightCornerShape())
                changingProperties.m_properties.set(CSSPropertyCornerBottomRightShape);
        }

        // Non animated styles are followings.
        // hasExplicitlySetBorderBottomLeftRadius
        // hasExplicitlySetBorderBottomRightRadius
        // hasExplicitlySetBorderTopLeftRadius
        // hasExplicitlySetBorderTopRightRadius
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaNonInheritedMiscData = [&](auto& first, auto& second) {
        if (first.opacity != second.opacity)
            changingProperties.m_properties.set(CSSPropertyOpacity);

        if (first.flexibleBox != second.flexibleBox) {
            changingProperties.m_properties.set(CSSPropertyFlexBasis);
            changingProperties.m_properties.set(CSSPropertyFlexDirection);
            changingProperties.m_properties.set(CSSPropertyFlexGrow);
            changingProperties.m_properties.set(CSSPropertyFlexShrink);
            changingProperties.m_properties.set(CSSPropertyFlexWrap);
        }

        if (first.multiCol != second.multiCol) {
            changingProperties.m_properties.set(CSSPropertyColumnCount);
            changingProperties.m_properties.set(CSSPropertyColumnFill);
            changingProperties.m_properties.set(CSSPropertyColumnSpan);
            changingProperties.m_properties.set(CSSPropertyColumnWidth);
            changingProperties.m_properties.set(CSSPropertyColumnRuleColor);
            changingProperties.m_properties.set(CSSPropertyColumnRuleStyle);
            changingProperties.m_properties.set(CSSPropertyColumnRuleWidth);
        }

        if (first.filter != second.filter)
            changingProperties.m_properties.set(CSSPropertyFilter);

        if (first.mask != second.mask) {
            changingProperties.m_properties.set(CSSPropertyMaskImage);
            changingProperties.m_properties.set(CSSPropertyMaskClip);
            changingProperties.m_properties.set(CSSPropertyMaskComposite);
            changingProperties.m_properties.set(CSSPropertyMaskMode);
            changingProperties.m_properties.set(CSSPropertyMaskOrigin);
            changingProperties.m_properties.set(CSSPropertyWebkitMaskPositionX);
            changingProperties.m_properties.set(CSSPropertyWebkitMaskPositionY);
            changingProperties.m_properties.set(CSSPropertyMaskSize);
            changingProperties.m_properties.set(CSSPropertyMaskRepeat);
        }

        if (first.visitedLinkColor.ptr() != second.visitedLinkColor.ptr()) {
            if (first.visitedLinkColor->visitedLinkBackgroundColor != second.visitedLinkColor->visitedLinkBackgroundColor)
                changingProperties.m_properties.set(CSSPropertyBackgroundColor);
            if (first.visitedLinkColor->visitedLinkBorderColors.left() != second.visitedLinkColor->visitedLinkBorderColors.left())
                changingProperties.m_properties.set(CSSPropertyBorderLeftColor);
            if (first.visitedLinkColor->visitedLinkBorderColors.right() != second.visitedLinkColor->visitedLinkBorderColors.right())
                changingProperties.m_properties.set(CSSPropertyBorderRightColor);
            if (first.visitedLinkColor->visitedLinkBorderColors.top() != second.visitedLinkColor->visitedLinkBorderColors.top())
                changingProperties.m_properties.set(CSSPropertyBorderTopColor);
            if (first.visitedLinkColor->visitedLinkBorderColors.bottom() != second.visitedLinkColor->visitedLinkBorderColors.bottom())
                changingProperties.m_properties.set(CSSPropertyBorderBottomColor);
            if (first.visitedLinkColor->visitedLinkTextDecorationColor != second.visitedLinkColor->visitedLinkTextDecorationColor)
                changingProperties.m_properties.set(CSSPropertyTextDecorationColor);
            if (first.visitedLinkColor->visitedLinkOutlineColor != second.visitedLinkColor->visitedLinkOutlineColor)
                changingProperties.m_properties.set(CSSPropertyOutlineColor);
        }

        if (first.content != second.content)
            changingProperties.m_properties.set(CSSPropertyContent);

        if (first.boxShadow != second.boxShadow) {
            changingProperties.m_properties.set(CSSPropertyBoxShadow);
            changingProperties.m_properties.set(CSSPropertyWebkitBoxShadow);
        }

        if (first.aspectRatio != second.aspectRatio)
            changingProperties.m_properties.set(CSSPropertyAspectRatio);
        if (first.alignContent != second.alignContent)
            changingProperties.m_properties.set(CSSPropertyAlignContent);
        if (first.alignItems != second.alignItems)
            changingProperties.m_properties.set(CSSPropertyAlignItems);
        if (first.alignSelf != second.alignSelf)
            changingProperties.m_properties.set(CSSPropertyAlignSelf);
        if (first.justifyContent != second.justifyContent)
            changingProperties.m_properties.set(CSSPropertyJustifyContent);
        if (first.justifyItems != second.justifyItems)
            changingProperties.m_properties.set(CSSPropertyJustifyItems);
        if (first.justifySelf != second.justifySelf)
            changingProperties.m_properties.set(CSSPropertyJustifySelf);
        if (first.order != second.order)
            changingProperties.m_properties.set(CSSPropertyOrder);
        if (first.objectPosition != second.objectPosition)
            changingProperties.m_properties.set(CSSPropertyObjectPosition);
        if (first.textOverflow != second.textOverflow)
            changingProperties.m_properties.set(CSSPropertyTextOverflow);
        if (first.resize != second.resize)
            changingProperties.m_properties.set(CSSPropertyResize);
        if (first.objectFit != second.objectFit)
            changingProperties.m_properties.set(CSSPropertyObjectFit);
        if (first.appearance != second.appearance)
            changingProperties.m_properties.set(CSSPropertyAppearance);
        if (first.tableLayout != second.tableLayout)
            changingProperties.m_properties.set(CSSPropertyTableLayout);

        if (first.transform.ptr() != second.transform.ptr())
            conservativelyCollectChangedAnimatablePropertiesViaTransformData(*first.transform, *second.transform);

        // Non animated styles are followings.
        // deprecatedFlexibleBox
        // hasAttrContent
        // hasExplicitlySetColorScheme
        // hasExplicitlySetDirection
        // hasExplicitlySetWritingMode
        // usedAppearance
        // userDrag
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaNonInheritedRareData = [&](auto& first, auto& second) {
        if (first.blockStepAlign != second.blockStepAlign)
            changingProperties.m_properties.set(CSSPropertyBlockStepAlign);
        if (first.blockStepInsert != second.blockStepInsert)
            changingProperties.m_properties.set(CSSPropertyBlockStepInsert);
        if (first.blockStepRound != second.blockStepRound)
            changingProperties.m_properties.set(CSSPropertyBlockStepRound);
        if (first.blockStepSize != second.blockStepSize)
            changingProperties.m_properties.set(CSSPropertyBlockStepSize);
        if (first.containIntrinsicWidth != second.containIntrinsicWidth)
            changingProperties.m_properties.set(CSSPropertyContainIntrinsicWidth);
        if (first.containIntrinsicHeight != second.containIntrinsicHeight)
            changingProperties.m_properties.set(CSSPropertyContainIntrinsicHeight);
        if (first.perspectiveOrigin.x != second.perspectiveOrigin.x)
            changingProperties.m_properties.set(CSSPropertyPerspectiveOriginX);
        if (first.perspectiveOrigin.y != second.perspectiveOrigin.y)
            changingProperties.m_properties.set(CSSPropertyPerspectiveOriginY);
        if (first.initialLetter != second.initialLetter)
            changingProperties.m_properties.set(CSSPropertyWebkitInitialLetter);
        if (first.backdropFilter != second.backdropFilter)
            changingProperties.m_properties.set(CSSPropertyWebkitBackdropFilter);
        if (first.grid != second.grid) {
            changingProperties.m_properties.set(CSSPropertyGridAutoColumns);
            changingProperties.m_properties.set(CSSPropertyGridAutoFlow);
            changingProperties.m_properties.set(CSSPropertyGridAutoRows);
            changingProperties.m_properties.set(CSSPropertyGridTemplateColumns);
            changingProperties.m_properties.set(CSSPropertyGridTemplateRows);
            changingProperties.m_properties.set(CSSPropertyGridTemplateAreas);
        }
        if (first.gridItem != second.gridItem) {
            changingProperties.m_properties.set(CSSPropertyGridColumnStart);
            changingProperties.m_properties.set(CSSPropertyGridColumnEnd);
            changingProperties.m_properties.set(CSSPropertyGridRowStart);
            changingProperties.m_properties.set(CSSPropertyGridRowEnd);
        }
        if (first.clip != second.clip)
            changingProperties.m_properties.set(CSSPropertyClip);
        if (first.counterDirectives != second.counterDirectives) {
            changingProperties.m_properties.set(CSSPropertyCounterIncrement);
            changingProperties.m_properties.set(CSSPropertyCounterReset);
            changingProperties.m_properties.set(CSSPropertyCounterSet);
        }
        if (first.maskBorder != second.maskBorder) {
            changingProperties.m_properties.set(CSSPropertyMaskBorderSource);
            changingProperties.m_properties.set(CSSPropertyMaskBorderSlice);
            changingProperties.m_properties.set(CSSPropertyMaskBorderWidth);
            changingProperties.m_properties.set(CSSPropertyMaskBorderOutset);
            changingProperties.m_properties.set(CSSPropertyMaskBorderRepeat);
            changingProperties.m_properties.set(CSSPropertyWebkitMaskBoxImage);
        }
        if (first.shapeOutside != second.shapeOutside)
            changingProperties.m_properties.set(CSSPropertyShapeOutside);
        if (first.shapeMargin != second.shapeMargin)
            changingProperties.m_properties.set(CSSPropertyShapeMargin);
        if (first.shapeImageThreshold != second.shapeImageThreshold)
            changingProperties.m_properties.set(CSSPropertyShapeImageThreshold);
        if (first.perspective != second.perspective)
            changingProperties.m_properties.set(CSSPropertyPerspective);
        if (first.clip != second.clip)
            changingProperties.m_properties.set(CSSPropertyClip);
        if (first.clipPath != second.clipPath)
            changingProperties.m_properties.set(CSSPropertyClipPath);
        if (first.textDecorationColor != second.textDecorationColor)
            changingProperties.m_properties.set(CSSPropertyTextDecorationColor);
        if (first.rotate != second.rotate)
            changingProperties.m_properties.set(CSSPropertyRotate);
        if (first.scale != second.scale)
            changingProperties.m_properties.set(CSSPropertyScale);
        if (first.translate != second.translate)
            changingProperties.m_properties.set(CSSPropertyTranslate);
        if (first.columnGap != second.columnGap)
            changingProperties.m_properties.set(CSSPropertyColumnGap);
        if (first.rowGap != second.rowGap)
            changingProperties.m_properties.set(CSSPropertyRowGap);
        if (first.offsetPath != second.offsetPath)
            changingProperties.m_properties.set(CSSPropertyOffsetPath);
        if (first.offsetDistance != second.offsetDistance)
            changingProperties.m_properties.set(CSSPropertyOffsetDistance);
        if (first.offsetPosition != second.offsetPosition)
            changingProperties.m_properties.set(CSSPropertyOffsetPosition);
        if (first.offsetAnchor != second.offsetAnchor)
            changingProperties.m_properties.set(CSSPropertyOffsetAnchor);
        if (first.offsetRotate != second.offsetRotate)
            changingProperties.m_properties.set(CSSPropertyOffsetRotate);
        if (first.textDecorationThickness != second.textDecorationThickness)
            changingProperties.m_properties.set(CSSPropertyTextDecorationThickness);
        if (first.touchAction != second.touchAction)
            changingProperties.m_properties.set(CSSPropertyTouchAction);
        if (first.marginTrim != second.marginTrim)
            changingProperties.m_properties.set(CSSPropertyMarginTrim);
        if (first.scrollbarGutter != second.scrollbarGutter)
            changingProperties.m_properties.set(CSSPropertyScrollbarGutter);
        if (first.scrollbarWidth != second.scrollbarWidth)
            changingProperties.m_properties.set(CSSPropertyScrollbarWidth);
        if (first.transformStyle3D != second.transformStyle3D)
            changingProperties.m_properties.set(CSSPropertyTransformStyle);
        if (first.backfaceVisibility != second.backfaceVisibility)
            changingProperties.m_properties.set(CSSPropertyBackfaceVisibility);
        if (first.scrollBehavior != second.scrollBehavior)
            changingProperties.m_properties.set(CSSPropertyScrollBehavior);
        if (first.textDecorationStyle != second.textDecorationStyle)
            changingProperties.m_properties.set(CSSPropertyTextDecorationStyle);
        if (first.textGroupAlign != second.textGroupAlign)
            changingProperties.m_properties.set(CSSPropertyTextGroupAlign);
        if (first.effectiveBlendMode != second.effectiveBlendMode)
            changingProperties.m_properties.set(CSSPropertyMixBlendMode);
        if (first.isolation != second.isolation)
            changingProperties.m_properties.set(CSSPropertyIsolation);
        if (first.breakAfter != second.breakAfter)
            changingProperties.m_properties.set(CSSPropertyBreakAfter);
        if (first.breakBefore != second.breakBefore)
            changingProperties.m_properties.set(CSSPropertyBreakBefore);
        if (first.breakInside != second.breakInside)
            changingProperties.m_properties.set(CSSPropertyBreakInside);
        if (first.textBoxTrim != second.textBoxTrim)
            changingProperties.m_properties.set(CSSPropertyTextBoxTrim);
        if (first.overflowAnchor != second.overflowAnchor)
            changingProperties.m_properties.set(CSSPropertyOverflowAnchor);
        if (first.viewTransitionClasses != second.viewTransitionClasses)
            changingProperties.m_properties.set(CSSPropertyViewTransitionClass);
        if (first.viewTransitionName != second.viewTransitionName)
            changingProperties.m_properties.set(CSSPropertyViewTransitionName);
        if (first.contentVisibility != second.contentVisibility)
            changingProperties.m_properties.set(CSSPropertyContentVisibility);
        if (first.anchorNames != second.anchorNames)
            changingProperties.m_properties.set(CSSPropertyAnchorName);
        if (first.anchorScope != second.anchorScope)
            changingProperties.m_properties.set(CSSPropertyAnchorScope);
        if (first.positionAnchor != second.positionAnchor)
            changingProperties.m_properties.set(CSSPropertyPositionAnchor);
        if (first.positionArea != second.positionArea)
            changingProperties.m_properties.set(CSSPropertyPositionArea);
        if (first.positionTryFallbacks != second.positionTryFallbacks)
            changingProperties.m_properties.set(CSSPropertyPositionTryFallbacks);
        if (first.positionTryOrder != second.positionTryOrder)
            changingProperties.m_properties.set(CSSPropertyPositionTryOrder);
        if (first.positionVisibility != second.positionVisibility)
            changingProperties.m_properties.set(CSSPropertyPositionVisibility);
        if (first.scrollSnapAlign != second.scrollSnapAlign)
            changingProperties.m_properties.set(CSSPropertyScrollSnapAlign);
        if (first.scrollSnapStop != second.scrollSnapStop)
            changingProperties.m_properties.set(CSSPropertyScrollSnapStop);
        if (first.scrollSnapType != second.scrollSnapType)
            changingProperties.m_properties.set(CSSPropertyScrollSnapType);
        if (first.maxLines != second.maxLines)
            changingProperties.m_properties.set(CSSPropertyMaxLines);
        if (first.overflowContinue != second.overflowContinue)
            changingProperties.m_properties.set(CSSPropertyContinue);

        // Non animated styles are followings.
        // customProperties
        // customPaintWatchedProperties
        // zoom
        // contain
        // containerNames
        // scrollMargin
        // scrollPadding
        // lineClamp
        // willChange
        // marquee
        // boxReflect
        // pageSize
        // overscrollBehaviorX
        // overscrollBehaviorY
        // applePayButtonStyle
        // applePayButtonType
        // inputSecurity
        // containerType
        // transformStyleForcedToFlat
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaInheritedData = [&](auto& first, auto& second) {
        if (first.lineHeight != second.lineHeight)
            changingProperties.m_properties.set(CSSPropertyLineHeight);

#if ENABLE(TEXT_AUTOSIZING)
        if (first.specifiedLineHeight != second.specifiedLineHeight)
            changingProperties.m_properties.set(CSSPropertyLineHeight);
#endif

        if (first.fontData != second.fontData) {
            changingProperties.m_properties.set(CSSPropertyWordSpacing);
            changingProperties.m_properties.set(CSSPropertyLetterSpacing);
            changingProperties.m_properties.set(CSSPropertyTextRendering);
            changingProperties.m_properties.set(CSSPropertyTextSpacingTrim);
            changingProperties.m_properties.set(CSSPropertyTextAutospace);
            changingProperties.m_properties.set(CSSPropertyFontStyle);
#if ENABLE(VARIATION_FONTS)
            changingProperties.m_properties.set(CSSPropertyFontOpticalSizing);
            changingProperties.m_properties.set(CSSPropertyFontVariationSettings);
#endif
            changingProperties.m_properties.set(CSSPropertyFontWeight);
            changingProperties.m_properties.set(CSSPropertyFontSizeAdjust);
            changingProperties.m_properties.set(CSSPropertyFontFamily);
            changingProperties.m_properties.set(CSSPropertyFontFeatureSettings);
            changingProperties.m_properties.set(CSSPropertyFontVariantEastAsian);
            changingProperties.m_properties.set(CSSPropertyFontVariantLigatures);
            changingProperties.m_properties.set(CSSPropertyFontVariantNumeric);
            changingProperties.m_properties.set(CSSPropertyFontSize);
            changingProperties.m_properties.set(CSSPropertyFontWidth);
            changingProperties.m_properties.set(CSSPropertyFontPalette);
            changingProperties.m_properties.set(CSSPropertyFontKerning);
            changingProperties.m_properties.set(CSSPropertyFontSynthesisWeight);
            changingProperties.m_properties.set(CSSPropertyFontSynthesisStyle);
            changingProperties.m_properties.set(CSSPropertyFontSynthesisSmallCaps);
            changingProperties.m_properties.set(CSSPropertyFontVariantAlternates);
            changingProperties.m_properties.set(CSSPropertyFontVariantPosition);
            changingProperties.m_properties.set(CSSPropertyFontVariantCaps);
            changingProperties.m_properties.set(CSSPropertyFontVariantEmoji);
        }

        if (first.borderHorizontalSpacing != second.borderHorizontalSpacing)
            changingProperties.m_properties.set(CSSPropertyWebkitBorderHorizontalSpacing);

        if (first.borderVerticalSpacing != second.borderVerticalSpacing)
            changingProperties.m_properties.set(CSSPropertyWebkitBorderVerticalSpacing);

        if (first.color != second.color || first.visitedLinkColor != second.visitedLinkColor)
            changingProperties.m_properties.set(CSSPropertyColor);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaRareInheritedData = [&](auto& first, auto& second) {
        if (first.textStrokeColor != second.textStrokeColor || first.visitedLinkTextStrokeColor != second.visitedLinkTextStrokeColor)
            changingProperties.m_properties.set(CSSPropertyWebkitTextStrokeColor);
        if (first.textFillColor != second.textFillColor || first.visitedLinkTextFillColor != second.visitedLinkTextFillColor)
            changingProperties.m_properties.set(CSSPropertyWebkitTextFillColor);
        if (first.textEmphasisColor != second.textEmphasisColor || first.visitedLinkTextEmphasisColor != second.visitedLinkTextEmphasisColor)
            changingProperties.m_properties.set(CSSPropertyTextEmphasisColor);
        if (first.caretColor != second.caretColor || first.visitedLinkCaretColor != second.visitedLinkCaretColor || first.hasAutoCaretColor != second.hasAutoCaretColor || first.hasVisitedLinkAutoCaretColor != second.hasVisitedLinkAutoCaretColor)
            changingProperties.m_properties.set(CSSPropertyCaretColor);
        if (first.accentColor != second.accentColor)
            changingProperties.m_properties.set(CSSPropertyAccentColor);
        if (first.textShadow != second.textShadow)
            changingProperties.m_properties.set(CSSPropertyTextShadow);
        if (first.textIndent != second.textIndent)
            changingProperties.m_properties.set(CSSPropertyTextIndent);
        if (first.textUnderlineOffset != second.textUnderlineOffset)
            changingProperties.m_properties.set(CSSPropertyTextUnderlineOffset);
        if (first.strokeMiterLimit != second.strokeMiterLimit)
            changingProperties.m_properties.set(CSSPropertyStrokeMiterlimit);
        if (first.widows != second.widows)
            changingProperties.m_properties.set(CSSPropertyWidows);
        if (first.orphans != second.orphans)
            changingProperties.m_properties.set(CSSPropertyOrphans);
        if (first.wordBreak != second.wordBreak)
            changingProperties.m_properties.set(CSSPropertyWordBreak);
        if (first.overflowWrap != second.overflowWrap)
            changingProperties.m_properties.set(CSSPropertyOverflowWrap);
        if (first.lineBreak != second.lineBreak)
            changingProperties.m_properties.set(CSSPropertyLineBreak);
        if (first.hangingPunctuation != second.hangingPunctuation)
            changingProperties.m_properties.set(CSSPropertyHangingPunctuation);
        if (first.hyphens != second.hyphens)
            changingProperties.m_properties.set(CSSPropertyHyphens);
        if (first.textEmphasisPosition != second.textEmphasisPosition)
            changingProperties.m_properties.set(CSSPropertyTextEmphasisPosition);
#if ENABLE(DARK_MODE_CSS)
        if (first.colorScheme != second.colorScheme)
            changingProperties.m_properties.set(CSSPropertyColorScheme);
#endif
        if (first.dynamicRangeLimit != second.dynamicRangeLimit)
            changingProperties.m_properties.set(CSSPropertyDynamicRangeLimit);
        if (first.textEmphasisStyle != second.textEmphasisStyle)
            changingProperties.m_properties.set(CSSPropertyTextEmphasisStyle);
        if (first.quotes != second.quotes)
            changingProperties.m_properties.set(CSSPropertyQuotes);
        if (first.appleColorFilter != second.appleColorFilter)
            changingProperties.m_properties.set(CSSPropertyAppleColorFilter);
        if (first.tabSize != second.tabSize)
            changingProperties.m_properties.set(CSSPropertyTabSize);
        if (first.imageOrientation != second.imageOrientation)
            changingProperties.m_properties.set(CSSPropertyImageOrientation);
        if (first.imageRendering != second.imageRendering)
            changingProperties.m_properties.set(CSSPropertyImageRendering);
        if (first.textAlignLast != second.textAlignLast)
            changingProperties.m_properties.set(CSSPropertyTextAlignLast);
        if (first.textBoxEdge != second.textBoxEdge)
            changingProperties.m_properties.set(CSSPropertyTextBoxEdge);
        if (first.lineFitEdge != second.lineFitEdge)
            changingProperties.m_properties.set(CSSPropertyLineFitEdge);
        if (first.textJustify != second.textJustify)
            changingProperties.m_properties.set(CSSPropertyTextJustify);
        if (first.textDecorationSkipInk != second.textDecorationSkipInk)
            changingProperties.m_properties.set(CSSPropertyTextDecorationSkipInk);
        if (first.textUnderlinePosition != second.textUnderlinePosition)
            changingProperties.m_properties.set(CSSPropertyTextUnderlinePosition);
        if (first.rubyPosition != second.rubyPosition)
            changingProperties.m_properties.set(CSSPropertyRubyPosition);
        if (first.rubyAlign != second.rubyAlign)
            changingProperties.m_properties.set(CSSPropertyRubyAlign);
        if (first.rubyOverhang != second.rubyOverhang)
            changingProperties.m_properties.set(CSSPropertyRubyOverhang);
        if (first.strokeColor != second.strokeColor)
            changingProperties.m_properties.set(CSSPropertyStrokeColor);
        if (first.paintOrder != second.paintOrder)
            changingProperties.m_properties.set(CSSPropertyPaintOrder);
        if (first.capStyle != second.capStyle)
            changingProperties.m_properties.set(CSSPropertyStrokeLinecap);
        if (first.joinStyle != second.joinStyle)
            changingProperties.m_properties.set(CSSPropertyStrokeLinejoin);
        if (first.hasExplicitlySetStrokeWidth != second.hasExplicitlySetStrokeWidth || first.strokeWidth != second.strokeWidth)
            changingProperties.m_properties.set(CSSPropertyStrokeWidth);
        if (first.listStyleImage != second.listStyleImage)
            changingProperties.m_properties.set(CSSPropertyListStyleImage);
        if (first.scrollbarColor != second.scrollbarColor)
            changingProperties.m_properties.set(CSSPropertyScrollbarColor);
        if (first.listStyleType != second.listStyleType)
            changingProperties.m_properties.set(CSSPropertyListStyleType);
        if (first.hyphenateCharacter != second.hyphenateCharacter)
            changingProperties.m_properties.set(CSSPropertyHyphenateCharacter);
        if (first.blockEllipsis != second.blockEllipsis)
            changingProperties.m_properties.set(CSSPropertyBlockEllipsis);

        // customProperties is handled separately.
        // Non animated styles are followings.
        //
        // textStrokeWidth
        // mathStyle
        // hyphenateLimitBefore
        // hyphenateLimitAfter
        // hyphenateLimitLines
        // tapHighlightColor
        // nbspMode
        // webkitOverflowScrolling
        // textSizeAdjust
        // userSelect
        // isInSubtreeWithBlendMode
        // usedTouchAction
        // eventListenerRegionTypes
        // effectiveInert
        // usedContentVisibility
        // visitedLinkStrokeColor
        // hasExplicitlySetStrokeColor
        // usedZoom
        // textSecurity
        // userModify
        // speakAs
        // textCombine
        // lineBoxContain
        // webkitTouchCallout
        // lineGrid
        // textZoom
        // lineSnap
        // lineAlign
        // cursorData
        // insideDefaultButton
        // insideDisabledSubmitButton
    };

    if (m_inheritedFlags != other.m_inheritedFlags)
        conservativelyCollectChangedAnimatablePropertiesViaInheritedFlags(m_inheritedFlags, other.m_inheritedFlags);

    if (m_nonInheritedFlags != other.m_nonInheritedFlags)
        conservativelyCollectChangedAnimatablePropertiesViaNonInheritedFlags(m_nonInheritedFlags, other.m_nonInheritedFlags);

    if (m_nonInheritedData.ptr() != other.m_nonInheritedData.ptr()) {
        if (m_nonInheritedData->boxData.ptr() != other.m_nonInheritedData->boxData.ptr())
            conservativelyCollectChangedAnimatablePropertiesViaNonInheritedBoxData(*m_nonInheritedData->boxData, *other.m_nonInheritedData->boxData);

        if (m_nonInheritedData->backgroundData.ptr() != other.m_nonInheritedData->backgroundData.ptr())
            conservativelyCollectChangedAnimatablePropertiesViaNonInheritedBackgroundData(*m_nonInheritedData->backgroundData, *other.m_nonInheritedData->backgroundData);

        if (m_nonInheritedData->surroundData.ptr() != other.m_nonInheritedData->surroundData.ptr())
            conservativelyCollectChangedAnimatablePropertiesViaNonInheritedSurroundData(*m_nonInheritedData->surroundData, *other.m_nonInheritedData->surroundData);

        if (m_nonInheritedData->miscData.ptr() != other.m_nonInheritedData->miscData.ptr())
            conservativelyCollectChangedAnimatablePropertiesViaNonInheritedMiscData(*m_nonInheritedData->miscData, *other.m_nonInheritedData->miscData);

        if (m_nonInheritedData->rareData.ptr() != other.m_nonInheritedData->rareData.ptr())
            conservativelyCollectChangedAnimatablePropertiesViaNonInheritedRareData(*m_nonInheritedData->rareData, *other.m_nonInheritedData->rareData);
    }

    if (m_rareInheritedData.ptr() != other.m_rareInheritedData.ptr())
        conservativelyCollectChangedAnimatablePropertiesViaRareInheritedData(*m_rareInheritedData, *other.m_rareInheritedData);

    if (m_inheritedData.ptr() != other.m_inheritedData.ptr())
        conservativelyCollectChangedAnimatablePropertiesViaInheritedData(*m_inheritedData, *other.m_inheritedData);

    if (m_svgStyle.ptr() != other.m_svgStyle.ptr())
        m_svgStyle->conservativelyCollectChangedAnimatableProperties(*other.m_svgStyle, changingProperties);
}

bool RenderStyle::affectedByTransformOrigin() const
{
    if (rotate().affectedByTransformOrigin())
        return true;

    if (scale().affectedByTransformOrigin())
        return true;

    if (transform().affectedByTransformOrigin())
        return true;

    if (hasOffsetPath())
        return true;

    return false;
}

FloatPoint RenderStyle::computePerspectiveOrigin(const FloatRect& boundingBox) const
{
    return boundingBox.location() + Style::evaluate<FloatPoint>(perspectiveOrigin(), boundingBox.size(), Style::ZoomNeeded { });
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
    originTranslate.setXY(boundingBox.location() + Style::evaluate<FloatPoint>(transformOrigin().xy(), boundingBox.size(), Style::ZoomNeeded { }));
    originTranslate.setZ(transformOriginZ().resolveZoom(Style::ZoomNeeded { }));
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

void RenderStyle::applyTransform(TransformationMatrix& transform, const TransformOperationData& transformData, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    if (!options.contains(RenderStyle::TransformOperationOption::TransformOrigin) || !affectedByTransformOrigin()) {
        applyCSSTransform(transform, transformData, options);
        return;
    }

    auto originTranslate = computeTransformOrigin(transformData.boundingBox);
    applyTransformOrigin(transform, originTranslate);
    applyCSSTransform(transform, transformData, options);
    unapplyTransformOrigin(transform, originTranslate);
}

void RenderStyle::applyTransform(TransformationMatrix& transform, const TransformOperationData& transformData) const
{
    applyTransform(transform, transformData, allTransformOperations());
}

void RenderStyle::applyCSSTransform(TransformationMatrix& transform, const TransformOperationData& operationData, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    // https://www.w3.org/TR/css-transforms-2/#ctm
    // The transformation matrix is computed from the transform, transform-origin, translate, rotate, scale, and offset properties as follows:
    // 1. Start with the identity matrix.

    // 2. Translate by the computed X, Y, and Z values of transform-origin.
    // (implemented in applyTransformOrigin)
    auto& boundingBox = operationData.boundingBox;

    // 3. Translate by the computed X, Y, and Z values of translate.
    if (options.contains(RenderStyle::TransformOperationOption::Translate))
        translate().apply(transform, boundingBox.size());

    // 4. Rotate by the computed <angle> about the specified axis of rotate.
    if (options.contains(RenderStyle::TransformOperationOption::Rotate))
        rotate().apply(transform, boundingBox.size());

    // 5. Scale by the computed X, Y, and Z values of scale.
    if (options.contains(RenderStyle::TransformOperationOption::Scale))
        scale().apply(transform, boundingBox.size());

    // 6. Translate and rotate by the transform specified by offset.
    if (options.contains(RenderStyle::TransformOperationOption::Offset))
        MotionPath::applyMotionPathTransform(transform, operationData, *this);

    // 7. Multiply by each of the transform functions in transform from left to right.
    this->transform().apply(transform, boundingBox.size());

    // 8. Translate by the negated computed X, Y and Z values of transform-origin.
    // (implemented in unapplyTransformOrigin)
}

void RenderStyle::setPageScaleTransform(float scale)
{
    if (scale == 1)
        return;

    setTransform(Style::Transform { Style::TransformFunction { Style::ScaleTransformFunction::create(scale, scale, Style::TransformFunctionType::Scale) } });
    setTransformOriginX(0_css_px);
    setTransformOriginY(0_css_px);
}

const Color& RenderStyle::color() const
{
    return m_inheritedData->color;
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

    return WTF::switchOn(m_rareInheritedData->hyphenateCharacter,
        [&](const CSS::Keyword::Auto&) -> const AtomString& {
            // FIXME: This should depend on locale.
            static MainThreadNeverDestroyed<const AtomString> hyphenMinusString(span(hyphenMinus));
            static MainThreadNeverDestroyed<const AtomString> hyphenString(span(hyphen));

            return fontCascade().primaryFont()->glyphForCharacter(hyphen) ? hyphenString : hyphenMinusString;
        },
        [](const AtomString& string) -> const AtomString& {
            return string;
        }
    );
}

void RenderStyle::adjustAnimations()
{
    if (animations().isInitial())
        return;

    ensureAnimations().prepareForUse();
}

void RenderStyle::adjustTransitions()
{
    if (transitions().isInitial())
        return;

    ensureTransitions().prepareForUse();
}

void RenderStyle::adjustBackgroundLayers()
{
    if (backgroundLayers().isInitial())
        return;

    ensureBackgroundLayers().prepareForUse();
}

void RenderStyle::adjustMaskLayers()
{
    if (maskLayers().isInitial())
        return;

    ensureMaskLayers().prepareForUse();
}

const FontMetrics& RenderStyle::metricsOfPrimaryFont() const
{
    return m_inheritedData->fontData->fontCascade.metricsOfPrimaryFont();
}

const FontCascadeDescription& RenderStyle::fontDescription() const
{
    return m_inheritedData->fontData->fontCascade.fontDescription();
}

FontCascadeDescription& RenderStyle::mutableFontDescriptionWithoutUpdate()
{
    auto& cascade = m_inheritedData.access().fontData.access().fontCascade;
    return cascade.mutableFontDescription();
}

FontCascade& RenderStyle::mutableFontCascadeWithoutUpdate()
{
    return m_inheritedData.access().fontData.access().fontCascade;
}

float RenderStyle::specifiedFontSize() const
{
    return fontDescription().specifiedSize();
}

float RenderStyle::computedFontSize() const
{
    return fontDescription().computedSize();
}

void RenderStyle::setFontCascade(FontCascade&& fontCascade)
{
    if (fontCascade == this->fontCascade())
        return;

    m_inheritedData.access().fontData.access().fontCascade = fontCascade;
}

void RenderStyle::setFontDescription(FontCascadeDescription&& description)
{
    if (fontDescription() == description)
        return;

    auto existingFontCascade = this->fontCascade();
    RefPtr fontSelector = existingFontCascade.fontSelector();

    auto newCascade = FontCascade { WTFMove(description), existingFontCascade };
    newCascade.update(WTFMove(fontSelector));
    setFontCascade(WTFMove(newCascade));
}

bool RenderStyle::setFontDescriptionWithoutUpdate(FontCascadeDescription&& description)
{
    if (fontDescription() == description)
        return false;

    auto& cascade = m_inheritedData.access().fontData.access().fontCascade;
    cascade = { WTFMove(description), cascade };
    return true;
}

const Style::LineHeight& RenderStyle::specifiedLineHeight() const
{
#if ENABLE(TEXT_AUTOSIZING)
    return m_inheritedData->specifiedLineHeight;
#else
    return m_inheritedData->lineHeight;
#endif
}

#if ENABLE(TEXT_AUTOSIZING)

void RenderStyle::setSpecifiedLineHeight(Style::LineHeight&& lineHeight)
{
    SET_VAR(m_inheritedData, specifiedLineHeight, WTFMove(lineHeight));
}

#endif

const Style::LineHeight& RenderStyle::lineHeight() const
{
    return m_inheritedData->lineHeight;
}

void RenderStyle::setLineHeight(Style::LineHeight&& lineHeight)
{
    SET_VAR(m_inheritedData, lineHeight, WTFMove(lineHeight));
}

float RenderStyle::computedLineHeight() const
{
    return computeLineHeight(lineHeight());
}

float RenderStyle::computeLineHeight(const Style::LineHeight& lineHeight) const
{
    return WTF::switchOn(lineHeight,
        [&](const CSS::Keyword::Normal&) -> float {
            return metricsOfPrimaryFont().lineSpacing();
        },
        [&](const Style::LineHeight::Fixed& fixed) -> float {
            return Style::evaluate<LayoutUnit>(fixed, usedZoomForLength()).toFloat();
        },
        [&](const Style::LineHeight::Percentage& percentage) -> float {
            return Style::evaluate<LayoutUnit>(percentage, LayoutUnit { computedFontSize() }).toFloat();
        },
        [&](const Style::LineHeight::Calc& calc) -> float {
            return Style::evaluate<LayoutUnit>(calc, LayoutUnit { computedFontSize() }, usedZoomForLength()).toFloat();
        }
    );
}

void RenderStyle::setTextSpacingTrim(Style::TextSpacingTrim value)
{
    auto description = fontDescription();
    description.setTextSpacingTrim(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setTextAutospace(Style::TextAutospace value)
{
    auto description = fontDescription();
    description.setTextAutospace(Style::toPlatform(value));
    setFontDescription(WTFMove(description));
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

    auto description = fontDescription();
    description.setSpecifiedSize(size);
    description.setComputedSize(size);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontSizeAdjust(Style::FontSizeAdjust sizeAdjust)
{
    auto description = fontDescription();
    description.setFontSizeAdjust(sizeAdjust.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontOpticalSizing(FontOpticalSizing opticalSizing)
{
    auto description = fontDescription();
    description.setOpticalSizing(opticalSizing);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontFamily(Style::FontFamilies&& families)
{
    auto description = fontDescription();
    description.setFamilies(families.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontFeatureSettings(Style::FontFeatureSettings&& settings)
{
    auto description = fontDescription();
    description.setFeatureSettings(settings.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontVariationSettings(Style::FontVariationSettings&& settings)
{
    auto description = fontDescription();
    description.setVariationSettings(settings.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontWeight(Style::FontWeight value)
{
    auto description = fontDescription();
    description.setWeight(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontWidth(Style::FontWidth value)
{
    auto description = fontDescription();
    description.setWidth(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontStyle(Style::FontStyle style)
{
    auto description = fontDescription();
    description.setFontStyleSlope(style.platformSlope());
    description.setFontStyleAxis(style.platformAxis());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontPalette(Style::FontPalette&& value)
{
    auto description = fontDescription();
    description.setFontPalette(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontKerning(Kerning value)
{
    auto description = fontDescription();
    description.setKerning(value);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontSmoothing(FontSmoothingMode value)
{
    auto description = fontDescription();
    description.setFontSmoothing(value);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontSynthesisSmallCaps(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisSmallCaps(value);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontSynthesisStyle(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisStyle(value);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontSynthesisWeight(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisWeight(value);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontVariantAlternates(Style::FontVariantAlternates&& value)
{
    auto description = fontDescription();
    description.setVariantAlternates(value.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontVariantCaps(FontVariantCaps value)
{
    auto description = fontDescription();
    description.setVariantCaps(value);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontVariantEastAsian(Style::FontVariantEastAsian value)
{
    auto description = fontDescription();
    description.setVariantEastAsian(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontVariantEmoji(FontVariantEmoji value)
{
    auto description = fontDescription();
    description.setVariantEmoji(value);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontVariantLigatures(Style::FontVariantLigatures value)
{
    auto description = fontDescription();
    description.setVariantLigatures(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontVariantNumeric(Style::FontVariantNumeric value)
{
    auto description = fontDescription();
    description.setVariantNumeric(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setFontVariantPosition(FontVariantPosition value)
{
    auto description = fontDescription();
    description.setVariantPosition(value);
    setFontDescription(WTFMove(description));
}

void RenderStyle::setLocale(Style::WebkitLocale&& value)
{
    auto description = fontDescription();
    description.setSpecifiedLocale(value.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyle::setTextRendering(TextRenderingMode value)
{
    auto description = fontDescription();
    description.setTextRenderingMode(value);
    setFontDescription(WTFMove(description));
}

const Style::Color& RenderStyle::unresolvedColorForProperty(CSSPropertyID colorProperty, bool visitedLink) const
{
    switch (colorProperty) {
    case CSSPropertyAccentColor:
        return accentColor().colorOrCurrentColor();
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
        return fill().colorDisregardingType();
    case CSSPropertyFloodColor:
        return floodColor();
    case CSSPropertyLightingColor:
        return lightingColor();
    case CSSPropertyOutlineColor:
        return visitedLink ? visitedLinkOutlineColor() : outlineColor();
    case CSSPropertyStopColor:
        return stopColor();
    case CSSPropertyStroke:
        return stroke().colorDisregardingType();
    case CSSPropertyStrokeColor:
        return visitedLink ? visitedLinkStrokeColor() : strokeColor();
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineStartColor:
        return unresolvedColorForProperty(CSSProperty::resolveDirectionAwareProperty(colorProperty, writingMode()));
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

    static NeverDestroyed<Style::Color> defaultColor { };
    return defaultColor;
}

Color RenderStyle::colorResolvingCurrentColor(CSSPropertyID colorProperty, bool visitedLink) const
{
    if (colorProperty == CSSPropertyColor)
        return visitedLink ? visitedLinkColor() : color();

    auto& result = unresolvedColorForProperty(colorProperty, visitedLink);
    if (result.isCurrentColor()) {
        if (colorProperty == CSSPropertyTextDecorationColor) {
            if (hasPositiveStrokeWidth()) {
                // Prefer stroke color if possible but not if it's fully transparent.
                auto strokeColor = colorResolvingCurrentColor(usedStrokeColorProperty(), visitedLink);
                if (strokeColor.isVisible())
                    return strokeColor;
            }

            return colorResolvingCurrentColor(CSSPropertyWebkitTextFillColor, visitedLink);
        }

        return visitedLink ? visitedLinkColor() : color();
    }

    return colorResolvingCurrentColor(result, visitedLink);
}

Color RenderStyle::colorResolvingCurrentColor(const Style::Color& color, bool visitedLink) const
{
    return color.resolveColor(visitedLink ? visitedLinkColor() : this->color());
}

Color RenderStyle::visitedDependentColor(CSSPropertyID colorProperty, OptionSet<PaintBehavior> paintBehavior) const
{
    Color unvisitedColor = colorResolvingCurrentColor(colorProperty, false);
    if (insideLink() != InsideLink::InsideVisited)
        return unvisitedColor;

    if (paintBehavior.contains(PaintBehavior::DontShowVisitedLinks))
        return unvisitedColor;

    if (isInSubtreeWithBlendMode())
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

Color RenderStyle::colorWithColorFilter(const Style::Color& color) const
{
    return colorByApplyingColorFilter(colorResolvingCurrentColor(color));
}

Color RenderStyle::usedAccentColor(OptionSet<StyleColorOptions> styleColorOptions) const
{
    return WTF::switchOn(accentColor(),
        [](const CSS::Keyword::Auto&) -> Color {
            return { };
        },
        [&](const Style::Color& color) -> Color {
            auto resolvedAccentColor = colorResolvingCurrentColor(color);

            if (!resolvedAccentColor.isOpaque()) {
                auto computedCanvasColor = RenderTheme::singleton().systemColor(CSSValueCanvas, styleColorOptions);
                resolvedAccentColor = blendSourceOver(computedCanvasColor, resolvedAccentColor);
            }

            if (hasAppleColorFilter())
                return colorByApplyingColorFilter(resolvedAccentColor);

            return resolvedAccentColor;
        }
    );
}

Color RenderStyle::usedScrollbarThumbColor() const
{
    return WTF::switchOn(scrollbarColor(),
        [&](const CSS::Keyword::Auto&) -> Color {
            return { };
        },
        [&](const auto& parts) -> Color {
            if (hasAppleColorFilter())
                return colorByApplyingColorFilter(colorResolvingCurrentColor(parts.thumb));
            return colorResolvingCurrentColor(parts.thumb);
        }
    );
}

Color RenderStyle::usedScrollbarTrackColor() const
{
    return WTF::switchOn(scrollbarColor(),
        [&](const CSS::Keyword::Auto&) -> Color {
            return { };
        },
        [&](const auto& parts) -> Color {
            if (hasAppleColorFilter())
                return colorByApplyingColorFilter(colorResolvingCurrentColor(parts.track));
            return colorResolvingCurrentColor(parts.track);
        }
    );
}

const BorderValue& RenderStyle::borderBefore(const WritingMode writingMode) const
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        return borderTop();
    case FlowDirection::BottomToTop:
        return borderBottom();
    case FlowDirection::LeftToRight:
        return borderLeft();
    case FlowDirection::RightToLeft:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

const BorderValue& RenderStyle::borderAfter(const WritingMode writingMode) const
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        return borderBottom();
    case FlowDirection::BottomToTop:
        return borderTop();
    case FlowDirection::LeftToRight:
        return borderRight();
    case FlowDirection::RightToLeft:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderBottom();
}

const BorderValue& RenderStyle::borderStart(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderLeft() : borderRight();
    return writingMode.isInlineTopToBottom() ? borderTop() : borderBottom();
}

const BorderValue& RenderStyle::borderEnd(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderRight() : borderLeft();
    return writingMode.isInlineTopToBottom() ? borderBottom() : borderTop();
}

Style::LineWidth RenderStyle::borderBeforeWidth(const WritingMode writingMode) const
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        return borderTopWidth();
    case FlowDirection::BottomToTop:
        return borderBottomWidth();
    case FlowDirection::LeftToRight:
        return borderLeftWidth();
    case FlowDirection::RightToLeft:
        return borderRightWidth();
    }
    ASSERT_NOT_REACHED();
    return borderTopWidth();
}

Style::LineWidth RenderStyle::borderAfterWidth(const WritingMode writingMode) const
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        return borderBottomWidth();
    case FlowDirection::BottomToTop:
        return borderTopWidth();
    case FlowDirection::LeftToRight:
        return borderRightWidth();
    case FlowDirection::RightToLeft:
        return borderLeftWidth();
    }
    ASSERT_NOT_REACHED();
    return borderBottomWidth();
}

Style::LineWidth RenderStyle::borderStartWidth(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderLeftWidth() : borderRightWidth();
    return writingMode.isInlineTopToBottom() ? borderTopWidth() : borderBottomWidth();
}

Style::LineWidth RenderStyle::borderEndWidth(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderRightWidth() : borderLeftWidth();
    return writingMode.isInlineTopToBottom() ? borderBottomWidth() : borderTopWidth();
}

void RenderStyle::setMarginStart(Style::MarginEdge&& margin)
{
    if (writingMode().isHorizontal()) {
        if (writingMode().isInlineLeftToRight())
            setMarginLeft(WTFMove(margin));
        else
            setMarginRight(WTFMove(margin));
    } else {
        if (writingMode().isInlineTopToBottom())
            setMarginTop(WTFMove(margin));
        else
            setMarginBottom(WTFMove(margin));
    }
}

void RenderStyle::setMarginEnd(Style::MarginEdge&& margin)
{
    if (writingMode().isHorizontal()) {
        if (writingMode().isInlineLeftToRight())
            setMarginRight(WTFMove(margin));
        else
            setMarginLeft(WTFMove(margin));
    } else {
        if (writingMode().isInlineTopToBottom())
            setMarginBottom(WTFMove(margin));
        else
            setMarginTop(WTFMove(margin));
    }
}

void RenderStyle::setMarginBefore(Style::MarginEdge&& margin)
{
    switch (writingMode().blockDirection()) {
    case FlowDirection::TopToBottom:
        return setMarginTop(WTFMove(margin));
    case FlowDirection::BottomToTop:
        return setMarginBottom(WTFMove(margin));
    case FlowDirection::LeftToRight:
        return setMarginLeft(WTFMove(margin));
    case FlowDirection::RightToLeft:
        return setMarginRight(WTFMove(margin));
    }
}

void RenderStyle::setMarginAfter(Style::MarginEdge&& margin)
{
    switch (writingMode().blockDirection()) {
    case FlowDirection::TopToBottom:
        return setMarginBottom(WTFMove(margin));
    case FlowDirection::BottomToTop:
        return setMarginTop(WTFMove(margin));
    case FlowDirection::LeftToRight:
        return setMarginRight(WTFMove(margin));
    case FlowDirection::RightToLeft:
        return setMarginLeft(WTFMove(margin));
    }
}

void RenderStyle::setPaddingStart(Style::PaddingEdge&& padding)
{
    if (writingMode().isHorizontal()) {
        if (writingMode().isInlineLeftToRight())
            setPaddingLeft(WTFMove(padding));
        else
            setPaddingRight(WTFMove(padding));
    } else {
        if (writingMode().isInlineTopToBottom())
            setPaddingTop(WTFMove(padding));
        else
            setPaddingBottom(WTFMove(padding));
    }
}

void RenderStyle::setPaddingEnd(Style::PaddingEdge&& padding)
{
    if (writingMode().isHorizontal()) {
        if (writingMode().isInlineLeftToRight())
            setPaddingRight(WTFMove(padding));
        else
            setPaddingLeft(WTFMove(padding));
    } else {
        if (writingMode().isInlineTopToBottom())
            setPaddingBottom(WTFMove(padding));
        else
            setPaddingTop(WTFMove(padding));
    }
}

void RenderStyle::setPaddingBefore(Style::PaddingEdge&& padding)
{
    switch (writingMode().blockDirection()) {
    case FlowDirection::TopToBottom:
        return setPaddingTop(WTFMove(padding));
    case FlowDirection::BottomToTop:
        return setPaddingBottom(WTFMove(padding));
    case FlowDirection::LeftToRight:
        return setPaddingLeft(WTFMove(padding));
    case FlowDirection::RightToLeft:
        return setPaddingRight(WTFMove(padding));
    }
}

void RenderStyle::setPaddingAfter(Style::PaddingEdge&& padding)
{
    switch (writingMode().blockDirection()) {
    case FlowDirection::TopToBottom:
        return setPaddingBottom(WTFMove(padding));
    case FlowDirection::BottomToTop:
        return setPaddingTop(WTFMove(padding));
    case FlowDirection::LeftToRight:
        return setPaddingRight(WTFMove(padding));
    case FlowDirection::RightToLeft:
        return setPaddingLeft(WTFMove(padding));
    }
}

#if ENABLE(TOUCH_EVENTS)

Style::Color RenderStyle::initialTapHighlightColor()
{
    return RenderTheme::tapHighlightColor();
}

#endif

String RenderStyle::altFromContent() const
{
    if (auto* contentData = content().tryData())
        return contentData->altText.value_or(nullString());
    return { };
}

template<typename OutsetValue>
static LayoutUnit computeOutset(const OutsetValue& outsetValue, LayoutUnit borderWidth)
{
    return WTF::switchOn(outsetValue,
        [&](const typename OutsetValue::Number& number) {
            return LayoutUnit(number.value * borderWidth);
        },
        [&](const typename OutsetValue::Length& length) {
            return LayoutUnit(length.resolveZoom(Style::ZoomNeeded { }));
        }
    );
}

LayoutBoxExtent RenderStyle::imageOutsets(const Style::BorderImage& image) const
{
    return {
        computeOutset(image.outset().values.top(), Style::evaluate<LayoutUnit>(borderTopWidth(), usedZoomForLength())),
        computeOutset(image.outset().values.right(), Style::evaluate<LayoutUnit>(borderRightWidth(), usedZoomForLength())),
        computeOutset(image.outset().values.bottom(), Style::evaluate<LayoutUnit>(borderBottomWidth(), usedZoomForLength())),
        computeOutset(image.outset().values.left(), Style::evaluate<LayoutUnit>(borderLeftWidth(), usedZoomForLength())),
    };
}

LayoutBoxExtent RenderStyle::imageOutsets(const Style::MaskBorder& image) const
{
    return {
        computeOutset(image.outset().values.top(), Style::evaluate<LayoutUnit>(borderTopWidth(), usedZoomForLength())),
        computeOutset(image.outset().values.right(), Style::evaluate<LayoutUnit>(borderRightWidth(), usedZoomForLength())),
        computeOutset(image.outset().values.bottom(), Style::evaluate<LayoutUnit>(borderBottomWidth(), usedZoomForLength())),
        computeOutset(image.outset().values.left(), Style::evaluate<LayoutUnit>(borderLeftWidth(), usedZoomForLength())),
    };
}

std::pair<FontOrientation, NonCJKGlyphOrientation> RenderStyle::fontAndGlyphOrientation()
{
    if (!writingMode().isVerticalTypographic())
        return { FontOrientation::Horizontal, NonCJKGlyphOrientation::Mixed };

    switch (writingMode().computedTextOrientation()) {
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

void RenderStyle::setColumnStylesFromPaginationMode(PaginationMode paginationMode)
{
    if (paginationMode == Pagination::Mode::Unpaginated)
        return;
    
    setColumnFill(ColumnFill::Auto);
    
    switch (paginationMode) {
    case Pagination::Mode::LeftToRightPaginated:
        setColumnAxis(ColumnAxis::Horizontal);
        if (writingMode().isHorizontal())
            setColumnProgression(writingMode().isBidiLTR() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        else
            setColumnProgression(writingMode().isBlockFlipped() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        break;
    case Pagination::Mode::RightToLeftPaginated:
        setColumnAxis(ColumnAxis::Horizontal);
        if (writingMode().isHorizontal())
            setColumnProgression(writingMode().isBidiLTR() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        else
            setColumnProgression(writingMode().isBlockFlipped() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        break;
    case Pagination::Mode::TopToBottomPaginated:
        setColumnAxis(ColumnAxis::Vertical);
        if (writingMode().isHorizontal())
            setColumnProgression(writingMode().isBlockFlipped() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        else
            setColumnProgression(writingMode().isBidiLTR() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        break;
    case Pagination::Mode::BottomToTopPaginated:
        setColumnAxis(ColumnAxis::Vertical);
        if (writingMode().isHorizontal())
            setColumnProgression(writingMode().isBlockFlipped() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        else
            setColumnProgression(writingMode().isBidiLTR() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        break;
    case Pagination::Mode::Unpaginated:
        ASSERT_NOT_REACHED();
        break;
    }
}

void RenderStyle::deduplicateCustomProperties(const RenderStyle& other)
{
    auto deduplicate = [&] <typename T> (const DataRef<T>& data, const DataRef<T>& otherData) {
        auto& properties = const_cast<DataRef<Style::CustomPropertyData>&>(data->customProperties);
        auto& otherProperties = otherData->customProperties;
        if (properties.ptr() == otherProperties.ptr() || *properties != *otherProperties)
            return;
        properties = otherProperties;
    };

    deduplicate(m_rareInheritedData, other.m_rareInheritedData);
    deduplicate(m_nonInheritedData->rareData, other.m_nonInheritedData->rareData);
}

void RenderStyle::setCustomPropertyValue(Ref<const Style::CustomProperty>&& value, bool isInherited)
{
    auto& name = value->name();
    if (isInherited) {
        if (auto* existingValue = m_rareInheritedData->customProperties->get(name); !existingValue || *existingValue != value.get())
            m_rareInheritedData.access().customProperties.access().set(name, WTFMove(value));
    } else {
        if (auto* existingValue = m_nonInheritedData->rareData->customProperties->get(name); !existingValue || *existingValue != value.get())
            m_nonInheritedData.access().rareData.access().customProperties.access().set(name, WTFMove(value));
    }
}

const Style::CustomProperty* RenderStyle::customPropertyValue(const AtomString& name) const
{
    for (auto* map : { &nonInheritedCustomProperties(), &inheritedCustomProperties() }) {
        if (auto* value = map->get(name))
            return value;
    }
    return nullptr;
}

bool RenderStyle::customPropertyValueEqual(const RenderStyle& other, const AtomString& name) const
{
    if (&nonInheritedCustomProperties() == &other.nonInheritedCustomProperties() && &inheritedCustomProperties() == &other.inheritedCustomProperties())
        return true;

    auto* value = customPropertyValue(name);
    auto* otherValue = other.customPropertyValue(name);
    if (value == otherValue)
        return true;
    if (!value || !otherValue)
        return false;
    return *value == *otherValue;
}

bool RenderStyle::customPropertiesEqual(const RenderStyle& other) const
{
    return m_nonInheritedData->rareData->customProperties == other.m_nonInheritedData->rareData->customProperties
        && m_rareInheritedData->customProperties == other.m_rareInheritedData->customProperties;
}

bool RenderStyle::scrollSnapDataEquivalent(const RenderStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return m_nonInheritedData->rareData->scrollMargin == other.m_nonInheritedData->rareData->scrollMargin
        && m_nonInheritedData->rareData->scrollSnapAlign == other.m_nonInheritedData->rareData->scrollSnapAlign
        && m_nonInheritedData->rareData->scrollSnapStop == other.m_nonInheritedData->rareData->scrollSnapStop
        && m_nonInheritedData->rareData->scrollSnapAlign == other.m_nonInheritedData->rareData->scrollSnapAlign;
}

Style::LineWidth RenderStyle::outlineWidth() const
{
    auto& outline = m_nonInheritedData->backgroundData->outline;
    if (outline.style() == OutlineStyle::None)
        return 0_css_px;
    if (outlineStyle() == OutlineStyle::Auto)
        return Style::LineWidth { std::max(Style::evaluate<float>(outline.width(), usedZoomForLength()), RenderTheme::platformFocusRingWidth()) };
    return outline.width();
}

Style::Length<> RenderStyle::outlineOffset() const
{
    auto& outline = m_nonInheritedData->backgroundData->outline;
    if (outlineStyle() == OutlineStyle::Auto)
        return Style::Length<> { Style::evaluate<float>(outline.offset(), Style::ZoomNeeded { }) + RenderTheme::platformFocusRingOffset(Style::evaluate<float>(outline.width(), usedZoomForLength())) };
    return outline.offset();
}

float RenderStyle::outlineSize() const
{
    return std::max(0.0f, Style::evaluate<float>(outlineWidth(), usedZoomForLength()) + Style::evaluate<float>(outlineOffset(), Style::ZoomNeeded { }));
}

CheckedRef<const FontCascade> RenderStyle::checkedFontCascade() const
{
    return fontCascade();
}

bool RenderStyle::shouldPlaceVerticalScrollbarOnLeft() const
{
    return !writingMode().isAnyLeftToRight();
}

float RenderStyle::computedStrokeWidth(const IntSize& viewportSize) const
{
    // Use the stroke-width and stroke-color value combination only if stroke-color has been explicitly specified.
    // Since there will be no visible stroke when stroke-color is not specified (transparent by default), we fall
    // back to the legacy Webkit text stroke combination in that case.
    if (!hasExplicitlySetStrokeColor())
        return Style::evaluate<float>(textStrokeWidth(), usedZoomForLength());

    return WTF::switchOn(strokeWidth(),
        [&](const Style::StrokeWidth::Fixed& fixedStrokeWidth) -> float {
            return Style::evaluate<float>(fixedStrokeWidth, Style::ZoomNeeded { });
        },
        [&](const Style::StrokeWidth::Percentage& percentageStrokeWidth) -> float {
            // According to the spec, https://drafts.fxtf.org/paint/#stroke-width, the percentage is relative to the scaled viewport size.
            // The scaled viewport size is the geometric mean of the viewport width and height.
            return percentageStrokeWidth.value * (viewportSize.width() + viewportSize.height()) / 200.0f;
        },
        [&](const Style::StrokeWidth::Calc& calcStrokeWidth) -> float {
            // FIXME: It is almost certainly wrong that calc and percentage are being handled differently - https://bugs.webkit.org/show_bug.cgi?id=296482
            return Style::evaluate<float>(calcStrokeWidth, viewportSize.width(), Style::ZoomNeeded { });
        }
    );
}

bool RenderStyle::hasPositiveStrokeWidth() const
{
    if (!hasExplicitlySetStrokeWidth())
        return textStrokeWidth().isPositive();
    return strokeWidth().isPossiblyPositive();
}

Color RenderStyle::computedStrokeColor() const
{
    return visitedDependentColor(usedStrokeColorProperty());
}

UsedClear RenderStyle::usedClear(const RenderElement& renderer)
{
    auto computedClear = renderer.style().clear();
    auto writingMode = renderer.containingBlock()->writingMode();
    switch (computedClear) {
    case Clear::None:
        return UsedClear::None;
    case Clear::Both:
        return UsedClear::Both;
    case Clear::Left:
        return writingMode.isLogicalLeftLineLeft() ? UsedClear::Left : UsedClear::Right;
    case Clear::Right:
        return writingMode.isLogicalLeftLineLeft() ? UsedClear::Right : UsedClear::Left;
    case Clear::InlineStart:
        return writingMode.isLogicalLeftInlineStart() ? UsedClear::Left : UsedClear::Right;
    case Clear::InlineEnd:
        return writingMode.isLogicalLeftInlineStart() ? UsedClear::Right : UsedClear::Left;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

UsedFloat RenderStyle::usedFloat(const RenderElement& renderer)
{
    auto computedFloat = renderer.style().floating();
    auto writingMode = renderer.containingBlock()->writingMode();
    switch (computedFloat) {
    case Float::None:
        return UsedFloat::None;
    case Float::Left:
        return writingMode.isLogicalLeftLineLeft() ? UsedFloat::Left : UsedFloat::Right;
    case Float::Right:
        return writingMode.isLogicalLeftLineLeft() ? UsedFloat::Right : UsedFloat::Left;
    case Float::InlineStart:
        return writingMode.isLogicalLeftInlineStart() ? UsedFloat::Left : UsedFloat::Right;
    case Float::InlineEnd:
        return writingMode.isLogicalLeftInlineStart() ? UsedFloat::Right : UsedFloat::Left;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

UserSelect RenderStyle::usedUserSelect() const
{
    if (effectiveInert())
        return UserSelect::None;

    auto value = userSelect();
    if (userModify() != UserModify::ReadOnly && userDrag() != UserDrag::Element)
        return value == UserSelect::None ? UserSelect::Text : value;

    return value;
}

std::optional<size_t> RenderStyle::usedPositionOptionIndex() const
{
    return m_nonInheritedData->rareData->usedPositionOptionIndex;
}

void RenderStyle::setUsedPositionOptionIndex(std::optional<size_t> index)
{
    SET_NESTED_VAR(m_nonInheritedData, rareData, usedPositionOptionIndex, WTFMove(index));
}

std::optional<Style::PseudoElementIdentifier> RenderStyle::pseudoElementIdentifier() const
{
    if (!pseudoElementType())
        return { };
    return Style::PseudoElementIdentifier { *pseudoElementType(), pseudoElementNameArgument() };
}

void RenderStyle::adjustScrollTimelines()
{
    auto& names = scrollTimelineNames();
    if (names.isNone() && scrollTimelines().isEmpty())
        return;

    auto& axes = scrollTimelineAxes();
    auto numberOfAxes = axes.size();
    ASSERT(numberOfAxes > 0);

    m_nonInheritedData.access().rareData.access().scrollTimelines = { FixedVector<Ref<ScrollTimeline>>::createWithSizeFromGenerator(names.size(), [&](auto i) {
        return ScrollTimeline::create(names[i].value.value, axes[i % numberOfAxes]);
    }) };
}

void RenderStyle::adjustViewTimelines()
{
    auto& names = viewTimelineNames();
    if (names.isNone() && viewTimelines().isEmpty())
        return;

    auto& axes = viewTimelineAxes();
    auto numberOfAxes = axes.size();
    ASSERT(numberOfAxes > 0);

    auto& insets = viewTimelineInsets();
    auto numberOfInsets = insets.size();
    ASSERT(numberOfInsets > 0);

    m_nonInheritedData.access().rareData.access().viewTimelines = { FixedVector<Ref<ViewTimeline>>::createWithSizeFromGenerator(names.size(), [&](auto i) {
        return ViewTimeline::create(names[i].value.value, axes[i % numberOfAxes], insets[i % numberOfInsets]);
    }) };
}

#if !LOG_DISABLED
void RenderStyle::NonInheritedFlags::dumpDifferences(TextStream& ts, const NonInheritedFlags& other) const
{
    if (*this == other)
        return;

    LOG_IF_DIFFERENT_WITH_CAST(DisplayType, effectiveDisplay);
    LOG_IF_DIFFERENT_WITH_CAST(DisplayType, originalDisplay);
    LOG_IF_DIFFERENT_WITH_CAST(Overflow, overflowX);
    LOG_IF_DIFFERENT_WITH_CAST(Overflow, overflowY);
    LOG_IF_DIFFERENT_WITH_CAST(Clear, clear);
    LOG_IF_DIFFERENT_WITH_CAST(PositionType, position);
    LOG_IF_DIFFERENT_WITH_CAST(UnicodeBidi, unicodeBidi);
    LOG_IF_DIFFERENT_WITH_CAST(Float, floating);

    LOG_IF_DIFFERENT(usesViewportUnits);
    LOG_IF_DIFFERENT(usesContainerUnits);
    LOG_IF_DIFFERENT(useTreeCountingFunctions);

    LOG_IF_DIFFERENT_WITH_FROM_RAW(Style::TextDecorationLine, textDecorationLine);

    LOG_IF_DIFFERENT(hasExplicitlyInheritedProperties);
    LOG_IF_DIFFERENT(disallowsFastPathInheritance);

    LOG_IF_DIFFERENT(emptyState);
    LOG_IF_DIFFERENT(firstChildState);
    LOG_IF_DIFFERENT(lastChildState);
    LOG_IF_DIFFERENT(isLink);

    LOG_IF_DIFFERENT_WITH_CAST(PseudoId, pseudoElementType);
    LOG_IF_DIFFERENT_WITH_CAST(unsigned, pseudoBits);
}

void RenderStyle::InheritedFlags::dumpDifferences(TextStream& ts, const InheritedFlags& other) const
{
    if (*this == other)
        return;

    LOG_IF_DIFFERENT(writingMode);

    LOG_IF_DIFFERENT_WITH_CAST(WhiteSpaceCollapse, whiteSpaceCollapse);
    LOG_IF_DIFFERENT_WITH_CAST(TextWrapMode, textWrapMode);
    LOG_IF_DIFFERENT_WITH_CAST(Style::TextAlign, textAlign);
    LOG_IF_DIFFERENT_WITH_CAST(TextWrapStyle, textWrapStyle);

    LOG_IF_DIFFERENT_WITH_FROM_RAW(Style::TextTransform, textTransform);
    LOG_IF_DIFFERENT_WITH_FROM_RAW(Style::TextDecorationLine, textDecorationLineInEffect);

    LOG_IF_DIFFERENT_WITH_CAST(PointerEvents, pointerEvents);
    LOG_IF_DIFFERENT_WITH_CAST(Visibility, visibility);
    LOG_IF_DIFFERENT_WITH_CAST(CursorType, cursorType);

#if ENABLE(CURSOR_VISIBILITY)
    LOG_IF_DIFFERENT_WITH_CAST(CursorVisibility, cursorVisibility);
#endif

    LOG_IF_DIFFERENT_WITH_CAST(ListStylePosition, listStylePosition);
    LOG_IF_DIFFERENT_WITH_CAST(EmptyCell, emptyCells);
    LOG_IF_DIFFERENT_WITH_CAST(BorderCollapse, borderCollapse);
    LOG_IF_DIFFERENT_WITH_CAST(CaptionSide, captionSide);
    LOG_IF_DIFFERENT_WITH_CAST(BoxDirection, boxDirection);
    LOG_IF_DIFFERENT_WITH_CAST(Order, rtlOrdering);
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasExplicitlySetColor);
    LOG_IF_DIFFERENT_WITH_CAST(PrintColorAdjust, printColorAdjust);
    LOG_IF_DIFFERENT_WITH_CAST(InsideLink, insideLink);

#if ENABLE(TEXT_AUTOSIZING)
    LOG_IF_DIFFERENT_WITH_CAST(unsigned, autosizeStatus);
#endif
}

void RenderStyle::dumpDifferences(TextStream& ts, const RenderStyle& other) const
{
    m_nonInheritedData->dumpDifferences(ts, *other.m_nonInheritedData);
    m_nonInheritedFlags.dumpDifferences(ts, other.m_nonInheritedFlags);

    m_rareInheritedData->dumpDifferences(ts, *other.m_rareInheritedData);
    m_inheritedData->dumpDifferences(ts, *other.m_inheritedData);
    m_inheritedFlags.dumpDifferences(ts, other.m_inheritedFlags);

    m_svgStyle->dumpDifferences(ts, other.m_svgStyle);
}
#endif


} // namespace WebCore

#undef SET_VAR
#undef SET_NESTED_VAR
