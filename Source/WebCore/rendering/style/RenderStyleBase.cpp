/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderStyleBase.h"

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

static_assert(PublicPseudoIDBits == allPublicPseudoElementTypes.size());
static_assert(!(static_cast<unsigned>(Style::maxTextTransformValue) >> TextTransformBits));

// Value zero is used to indicate no pseudo-element.
static_assert(!((enumToUnderlyingType(PseudoElementType::HighestEnumValue) + 1) >> PseudoElementTypeBits));

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PseudoStyleCache);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyleBase);

RenderStyleBase::~RenderStyleBase()
{
#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    ASSERT_WITH_SECURITY_IMPLICATION(!m_deletionHasBegun);
    m_deletionHasBegun = true;
#endif
}

// MARK: - Non-property getter/setters.

#if ENABLE(TEXT_AUTOSIZING)

AutosizeStatus RenderStyleBase::autosizeStatus() const
{
    return OptionSet<AutosizeStatus::Fields>::fromRaw(m_inheritedFlags.autosizeStatus);
}

void RenderStyleBase::setAutosizeStatus(AutosizeStatus autosizeStatus)
{
    m_inheritedFlags.autosizeStatus = autosizeStatus.fields().toRaw();
}

#endif // ENABLE(TEXT_AUTOSIZING)

// MARK: - FontCascade support.

CheckedRef<const FontCascade> RenderStyleBase::checkedFontCascade() const
{
    return fontCascade();
}

FontCascade& RenderStyleBase::mutableFontCascadeWithoutUpdate()
{
    return m_inheritedData.access().fontData.access().fontCascade;
}

void RenderStyleBase::setFontCascade(FontCascade&& fontCascade)
{
    if (fontCascade == this->fontCascade())
        return;

    m_inheritedData.access().fontData.access().fontCascade = fontCascade;
}

// MARK: - FontCascadeDescription support.

const FontCascadeDescription& RenderStyleBase::fontDescription() const
{
    return m_inheritedData->fontData->fontCascade.fontDescription();
}

FontCascadeDescription& RenderStyleBase::mutableFontDescriptionWithoutUpdate()
{
    auto& cascade = m_inheritedData.access().fontData.access().fontCascade;
    return cascade.mutableFontDescription();
}

void RenderStyleBase::setFontDescription(FontCascadeDescription&& description)
{
    if (fontDescription() == description)
        return;

    auto existingFontCascade = this->fontCascade();
    RefPtr fontSelector = existingFontCascade.fontSelector();

    auto newCascade = FontCascade { WTFMove(description), existingFontCascade };
    newCascade.update(WTFMove(fontSelector));
    setFontCascade(WTFMove(newCascade));
}

bool RenderStyleBase::setFontDescriptionWithoutUpdate(FontCascadeDescription&& description)
{
    if (fontDescription() == description)
        return false;

    auto& cascade = m_inheritedData.access().fontData.access().fontCascade;
    cascade = { WTFMove(description), cascade };
    return true;
}

const FontMetrics& RenderStyleBase::metricsOfPrimaryFont() const
{
    return m_inheritedData->fontData->fontCascade.metricsOfPrimaryFont();
}


std::pair<FontOrientation, NonCJKGlyphOrientation> RenderStyleBase::fontAndGlyphOrientation()
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

Style::FontSize RenderStyleBase::specifiedFontSize() const
{
    return fontDescription().specifiedSize();
}

const Style::LineHeight& RenderStyleBase::specifiedLineHeight() const
{
#if ENABLE(TEXT_AUTOSIZING)
    return m_inheritedData->specifiedLineHeight;
#else
    return m_inheritedData->lineHeight;
#endif
}

#if ENABLE(TEXT_AUTOSIZING)

void RenderStyleBase::setSpecifiedLineHeight(Style::LineHeight&& lineHeight)
{
    SET_VAR(m_inheritedData, specifiedLineHeight, WTFMove(lineHeight));
}

#endif

// MARK: - Properties/descriptors that are not yet generated

const CounterDirectiveMap& RenderStyleBase::counterDirectives() const
{
    return m_nonInheritedData->rareData->counterDirectives;
}

CounterDirectiveMap& RenderStyleBase::accessCounterDirectives()
{
    return m_nonInheritedData.access().rareData.access().counterDirectives;
}

// MARK: - Flags Diffing

#if !LOG_DISABLED
void RenderStyleBase::NonInheritedFlags::dumpDifferences(TextStream& ts, const NonInheritedFlags& other) const
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

void RenderStyleBase::InheritedFlags::dumpDifferences(TextStream& ts, const InheritedFlags& other) const
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
#endif

} // namespace WebCore

#undef SET_VAR
#undef SET_NESTED_VAR
