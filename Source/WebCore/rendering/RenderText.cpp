/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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
#include "RenderText.h"

#include "AXObjectCache.h"
#include "BreakLines.h"
#include "BreakingContext.h"
#include "DocumentInlines.h"
#include "DocumentMarkerController.h"
#include "FloatQuad.h"
#include "Hyphenation.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorLineBoxInlines.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineIteratorSVGTextBox.h"
#include "InlineIteratorTextBoxInlines.h"
#include "InlineRunAndOffset.h"
#include "LayoutInlineTextBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "LineSelection.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Range.h"
#include "RenderBlock.h"
#include "RenderCombineText.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderTextInlines.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "SVGElementTypeHelpers.h"
#include "SVGInlineTextBox.h"
#include "Settings.h"
#include "SurrogatePairAwareTextIterator.h"
#include "Text.h"
#include "TextResourceDecoder.h"
#include "TextTransform.h"
#include "TextUtil.h"
#include "VisiblePosition.h"
#include "WidthIterator.h"
#include <wtf/BitSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(IOS_FAMILY)
#include "Document.h"
#include "EditorClient.h"
#include "LogicalSelectionOffsetCaches.h"
#include "Page.h"
#include "SelectionGeometry.h"
#endif

namespace WebCore {

using namespace WTF::Unicode;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderText);

struct SameSizeAsRenderText : public RenderObject {
#if ENABLE(TEXT_AUTOSIZING)
    float candidateTextSize;
#endif
    float widths[4];
    void* pointers[2];
    String text;
    std::optional<bool> canUseSimplifiedTextMeasuring;
    std::optional<bool> hasPositionDependentContentWidth;
    std::optional<bool> m_hasStrongDirectionalityContent;
    uint32_t bitfields : 16;
};

static_assert(sizeof(RenderText) == sizeof(SameSizeAsRenderText), "RenderText should stay small");

class SecureTextTimer final : private TimerBase {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(SecureTextTimer);
public:
    explicit SecureTextTimer(RenderText&);
    void restart(unsigned offsetAfterLastTypedCharacter);

    unsigned takeOffsetAfterLastTypedCharacter();

private:
    void fired() override;
    RenderText& m_renderer;
    unsigned m_offsetAfterLastTypedCharacter { 0 };
};

using SecureTextTimerMap = SingleThreadWeakHashMap<RenderText, std::unique_ptr<SecureTextTimer>>;

static SecureTextTimerMap& secureTextTimers()
{
    static NeverDestroyed<SecureTextTimerMap> map;
    return map.get();
}

inline SecureTextTimer::SecureTextTimer(RenderText& renderer)
    : m_renderer(renderer)
{
}

inline void SecureTextTimer::restart(unsigned offsetAfterLastTypedCharacter)
{
    m_offsetAfterLastTypedCharacter = offsetAfterLastTypedCharacter;
    startOneShot(1_s * m_renderer.settings().passwordEchoDurationInSeconds());
}

inline unsigned SecureTextTimer::takeOffsetAfterLastTypedCharacter()
{
    unsigned offset = m_offsetAfterLastTypedCharacter;
    m_offsetAfterLastTypedCharacter = 0;
    return offset;
}

void SecureTextTimer::fired()
{
    ASSERT(secureTextTimers().get(m_renderer) == this);
    m_offsetAfterLastTypedCharacter = 0;
    m_renderer.setText(m_renderer.text(), true /* forcing setting text as it may be masked later */);
}

static UncheckedKeyHashMap<SingleThreadWeakRef<const RenderText>, String>& originalTextMap()
{
    static NeverDestroyed<UncheckedKeyHashMap<SingleThreadWeakRef<const RenderText>, String>> map;
    return map;
}

static UncheckedKeyHashMap<SingleThreadWeakRef<const RenderText>, SingleThreadWeakPtr<RenderInline>>& inlineWrapperForDisplayContentsMap()
{
    static NeverDestroyed<UncheckedKeyHashMap<SingleThreadWeakRef<const RenderText>, SingleThreadWeakPtr<RenderInline>>> map;
    return map;
}

static constexpr UChar convertNoBreakSpaceToSpace(UChar character)
{
    return character == noBreakSpace ? ' ' : character;
}

static inline size_t capitalizeCharacter(String textContent, unsigned startCharacterOffset, StringBuilder& output)
{
    if (startCharacterOffset >= textContent.length()) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    auto capitalize = [&](const UChar* contentToCapitalize, size_t length) -> size_t {
        if (length == 1) {
            if ((*contentToCapitalize >= 'A' && *contentToCapitalize <= 'Z') || *contentToCapitalize == ' ')
                return 0;
            if (*contentToCapitalize <= 'z') {
                char32_t lastCharacter = u_totitle(*contentToCapitalize);
                output.append(lastCharacter);
                return 1;
            }
        }

        UChar capitalizedCharacter;
        UErrorCode status = U_ZERO_ERROR;
        auto realLength = u_strToTitle(&capitalizedCharacter, 1, contentToCapitalize, length, nullptr, "", &status);
        if (U_SUCCESS(status) && realLength == 1) {
            output.append(capitalizedCharacter);
            return realLength;
        }

        // Decomposed ligatures may need more space.
        std::span<UChar> capitalizedStringData;
        auto capitalizedString = String::createUninitialized(realLength, capitalizedStringData);
        status = U_ZERO_ERROR;
        u_strToTitle(capitalizedStringData.data(), capitalizedStringData.size(), contentToCapitalize, length, nullptr, "", &status);
        if (U_SUCCESS(status)) {
            output.append(capitalizedString);
            return length;
        }
        return 0;
    };

    if (textContent.is8Bit()) {
        auto characterToCapitalize = textContent[startCharacterOffset];
        return capitalize(&characterToCapitalize, 1);
    }

    auto content = textContent.span16().subspan(startCharacterOffset);
    auto contentIterator = SurrogatePairAwareTextIterator { content, startCharacterOffset, textContent.length() };
    unsigned capitalizedContentLength = 0;
    char32_t currentCharacter = 0;
    contentIterator.consume(currentCharacter, capitalizedContentLength);
    if (startCharacterOffset + capitalizedContentLength > textContent.length()) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    return capitalize(content.data(), capitalizedContentLength);
}

String capitalize(const String& string)
{
    Vector<UChar> previousCharacter(1, ' ');
    return capitalize(string, previousCharacter);
}

String capitalize(const String& string, Vector<UChar> previousCharacter)
{
    int32_t length = string.length();
    int32_t previousCharacterLength = previousCharacter.size();
    auto& stringImpl = *string.impl();

    static_assert(String::MaxLength < std::numeric_limits<unsigned>::max(), "Must be able to add one without overflowing unsigned");

    // Replace NO BREAK SPACE with a normal spaces since ICU does not treat it as a word separator.
    Vector<UChar> stringWithPrevious(previousCharacterLength + length);
    for (int32_t i = 0; i < previousCharacterLength; ++i)
        stringWithPrevious[i] = convertNoBreakSpaceToSpace(previousCharacter[i]);
    for (int32_t i = previousCharacterLength; i < length + previousCharacterLength; ++i)
        stringWithPrevious[i] = convertNoBreakSpaceToSpace(stringImpl[i - previousCharacterLength]);

    auto* breakIterator = WTF::wordBreakIterator(stringWithPrevious.span());
    if (!breakIterator)
        return string;

    StringBuilder result;
    result.reserveCapacity(length);

    int32_t startOfWord = ubrk_first(breakIterator);
    for (int32_t endOfWord = ubrk_next(breakIterator); endOfWord != UBRK_DONE; startOfWord = endOfWord, endOfWord = ubrk_next(breakIterator)) {
        // Do not try to titlecase the previous content.
        if (startOfWord >= previousCharacterLength) {
            auto capitalizedContentLength = capitalizeCharacter(string, startOfWord - previousCharacterLength, result);
            for (int32_t i = startOfWord + capitalizedContentLength; i < endOfWord; ++i)
                result.append(stringImpl[i - previousCharacterLength]);
        } else {
            // This is previous and continous non-titlecased current content. Only append current content.
            for (int32_t i = startOfWord; i < endOfWord; ++i) {
                if (i < previousCharacterLength)
                    continue;
                result.append(stringImpl[i - previousCharacterLength]);
            }
        }
    }
    return result == string ? string : result.toString();
}

static LayoutRect selectionRectForTextBox(const InlineIterator::TextBox& textBox, unsigned rangeStart, unsigned rangeEnd)
{
    if (auto* svgTextBox = dynamicDowncast<InlineIterator::SVGTextBox>(textBox))
        return svgTextBox->localSelectionRect(rangeStart, rangeEnd);

    bool isCaretCase = rangeStart == rangeEnd;

    auto [clampedStart, clampedEnd] = textBox.selectableRange().clamp(rangeStart, rangeEnd);

    if (clampedStart >= clampedEnd) {
        if (isCaretCase) {
            // handle unitary range, e.g.: representing caret position
            bool isCaretWithinTextBox = rangeStart >= textBox.start() && rangeStart < textBox.end();
            // For last text box in a InlineTextBox chain, we allow the caret to move to a position 'after' the end of the last text box.
            bool isCaretWithinLastTextBox = rangeStart >= textBox.start() && rangeStart <= textBox.end();

            auto isLastTextBox = !textBox.nextTextBox();

            if ((isLastTextBox && !isCaretWithinLastTextBox) || (!isLastTextBox && !isCaretWithinTextBox))
                return { };
        } else {
            bool isRangeWithinTextBox = (rangeStart >= textBox.start() && rangeStart <= textBox.end());
            if (!isRangeWithinTextBox)
                return { };
        }
    }

    auto lineSelectionRect = LineSelection::logicalRect(*textBox.lineBox());
    auto selectionRect = LayoutRect { textBox.logicalLeftIgnoringInlineDirection(), lineSelectionRect.y(), textBox.logicalWidth(), lineSelectionRect.height() };

    auto textRun = textBox.textRun();
    if (clampedStart || clampedEnd != textRun.length())
        textBox.fontCascade().adjustSelectionRectForText(textBox.renderer().canUseSimplifiedTextMeasuring().value_or(false), textRun, selectionRect, clampedStart, clampedEnd);

    return snappedSelectionRect(selectionRect, textBox.logicalRightIgnoringInlineDirection(), lineSelectionRect.y(), lineSelectionRect.height(), textBox.isHorizontal());
}

static unsigned offsetForPositionInRun(const InlineIterator::TextBox& textBox, float x)
{
    if (textBox.isLineBreak())
        return 0;
    if (x - textBox.logicalLeftIgnoringInlineDirection() > textBox.logicalWidth())
        return textBox.isLeftToRightDirection() ? textBox.length() : 0;
    if (x - textBox.logicalLeftIgnoringInlineDirection() < 0)
        return textBox.isLeftToRightDirection() ? 0 : textBox.length();
    return textBox.fontCascade().offsetForPosition(textBox.textRun(InlineIterator::TextRunMode::Editing), x - textBox.logicalLeftIgnoringInlineDirection(), true);
}

inline RenderText::RenderText(Type type, Node& node, const String& text)
    : RenderObject(type, node, TypeFlag::IsText, { })
    , m_text(text)
    , m_containsOnlyASCII(text.impl()->containsOnlyASCII())
{
    ASSERT(!m_text.isNull());
    m_canUseSimpleFontCodePath = computeCanUseSimpleFontCodePath();
    ASSERT(isRenderText());
}

RenderText::RenderText(Type type, Text& textNode, const String& text)
    : RenderText(type, static_cast<Node&>(textNode), text)
{
}

RenderText::RenderText(Type type, Document& document, const String& text)
    : RenderText(type, static_cast<Node&>(document), text)
{
}

RenderText::~RenderText()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
    ASSERT(!originalTextMap().contains(this));
}

Layout::InlineTextBox* RenderText::layoutBox()
{
    return downcast<Layout::InlineTextBox>(RenderObject::layoutBox());
}

const Layout::InlineTextBox* RenderText::layoutBox() const
{
    return downcast<Layout::InlineTextBox>(RenderObject::layoutBox());
}

ASCIILiteral RenderText::renderName() const
{
    return "RenderText"_s;
}

Text* RenderText::textNode() const
{
    return downcast<Text>(RenderObject::node());
}

bool RenderText::computeUseBackslashAsYenSymbol() const
{
    const RenderStyle& style = this->style();
    const auto& fontDescription = style.fontDescription();
    if (style.fontCascade().useBackslashAsYenSymbol())
        return true;
    if (fontDescription.isSpecifiedFont())
        return false;
    const PAL::TextEncoding* encoding = document().decoder() ? &document().decoder()->encoding() : 0;
    if (encoding && encoding->backslashAsCurrencySymbol() != '\\')
        return true;
    return false;
}

void RenderText::initiateFontLoadingByAccessingGlyphDataAndComputeCanUseSimplifiedTextMeasuring(const String& textContent)
{
    auto& style = this->style();
    auto& fontCascade = style.fontCascade();
    // See webkit.org/b/252668
    auto fontVariant = AutoVariant;
    m_canUseSimplifiedTextMeasuring = canUseSimpleFontCodePath();
#if USE(FONT_VARIANT_VIA_FEATURES)
    auto fontVariantCaps = fontCascade.fontDescription().variantCaps();
    if (fontVariantCaps == FontVariantCaps::Small || fontVariantCaps == FontVariantCaps::AllSmall || fontVariantCaps ==  FontVariantCaps::Petite || fontVariantCaps == FontVariantCaps::AllPetite) {
        // This matches the behavior of ComplexTextController::collectComplexTextRuns(): that function doesn't perform font fallback
        // on the capitalized characters when small caps is enabled, so we shouldn't here either.
        fontVariant = NormalVariant;
        m_canUseSimplifiedTextMeasuring = false;
    }
#endif
    auto whitespaceIsCollapsed = style.collapseWhiteSpace();
    auto& primaryFont = fontCascade.primaryFont();
    m_canUseSimplifiedTextMeasuring = *m_canUseSimplifiedTextMeasuring && !fontCascade.wordSpacing() && !fontCascade.letterSpacing() && !primaryFont.syntheticBoldOffset() && (&firstLineStyle() == &style || &fontCascade == &firstLineStyle().fontCascade());

    if (*m_canUseSimplifiedTextMeasuring) {
        // Additional check on the font codepath.
        auto run = TextRun { textContent };
        run.setCharacterScanForCodePath(false);
        m_canUseSimplifiedTextMeasuring = fontCascade.codePath(run) == FontCascade::CodePath::Simple;
    }

    m_hasPositionDependentContentWidth = false;
    m_hasStrongDirectionalityContent = false;
    auto mayHaveStrongDirectionalityContent = !textContent.is8Bit();
    // FIXME: Pre-warm glyph loading in FontCascade with the most common range.
    WTF::BitSet<256> hasSeen;
    for (char32_t character : StringView(textContent).codePoints()) {
        if (character < 256) {
            if (hasSeen.testAndSet(character))
                continue;
        }
        m_canUseSimplifiedTextMeasuring = *m_canUseSimplifiedTextMeasuring && fontCascade.canUseSimplifiedTextMeasuring(character, fontVariant, whitespaceIsCollapsed, primaryFont);
        m_hasPositionDependentContentWidth = *m_hasPositionDependentContentWidth || character == tabCharacter;
        m_hasStrongDirectionalityContent = *m_hasStrongDirectionalityContent || (mayHaveStrongDirectionalityContent && Layout::TextUtil::isStrongDirectionalityCharacter(character));
    }
}

void RenderText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    // There is no need to ever schedule repaints from a style change of a text run, since
    // we already did this for the parent of the text run.
    // We do have to schedule layouts, though, since a style change can force us to
    // need to relayout.
    if (diff == StyleDifference::Layout) {
        setNeedsLayoutAndPrefWidthsRecalc();
        m_knownToHaveNoOverflowAndNoFallbackFonts = false;
    }

    const RenderStyle& newStyle = style();
    if (!oldStyle)
        initiateFontLoadingByAccessingGlyphDataAndComputeCanUseSimplifiedTextMeasuring(m_text);
    if (oldStyle && !oldStyle->fontCascadeEqual(newStyle))
        m_canUseSimplifiedTextMeasuring = { };

    bool needsResetText = false;
    if (!oldStyle) {
        m_useBackslashAsYenSymbol = computeUseBackslashAsYenSymbol();
        needsResetText = m_useBackslashAsYenSymbol;
    } else if (oldStyle->fontCascade().useBackslashAsYenSymbol() != newStyle.fontCascade().useBackslashAsYenSymbol()) {
        m_useBackslashAsYenSymbol = computeUseBackslashAsYenSymbol();
        needsResetText = true;
    }

    auto oldTransform = oldStyle ? oldStyle->textTransform() : OptionSet<TextTransform> { };
    TextSecurity oldSecurity = oldStyle ? oldStyle->textSecurity() : TextSecurity::None;
    if (needsResetText || oldTransform != newStyle.textTransform() || oldSecurity != newStyle.textSecurity())
        RenderText::setText(originalText(), true);

    // FIXME: First line change on the block comes in as equal on text.
    auto needsLayoutBoxStyleUpdate = layoutBox() && (diff >= StyleDifference::Repaint || (&style() != &firstLineStyle()));
    if (needsLayoutBoxStyleUpdate)
        LayoutIntegration::LineLayout::updateStyle(*this);

    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->onStyleChange(*this, diff, oldStyle, newStyle);

    setHorizontalWritingMode(newStyle.writingMode().isHorizontal());
}

void RenderText::removeAndDestroyLegacyTextBoxes()
{
    if (!renderTreeBeingDestroyed())
        m_legacyLineBoxes.removeAllFromParent(*this);
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    else
        m_legacyLineBoxes.invalidateParentChildLists();
#endif
    deleteLegacyLineBoxes();
}

void RenderText::willBeDestroyed()
{
    if (m_hasSecureTextTimer)
        secureTextTimers().remove(*this);

    removeAndDestroyLegacyTextBoxes();

    if (m_originalTextDiffersFromRendered)
        originalTextMap().remove(this);

    setInlineWrapperForDisplayContents(nullptr);

    RenderObject::willBeDestroyed();
}

String RenderText::originalText() const
{
    return m_originalTextDiffersFromRendered ? originalTextMap().get(this) : m_text;
}

void RenderText::boundingRects(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    for (auto& run : InlineIterator::textBoxesFor(*this)) {
        auto rect = LayoutRect { run.visualRectIgnoringBlockDirection() };
        rect.moveBy(accumulatedOffset);
        rects.append(rect);
    }
}

Vector<IntRect> RenderText::absoluteRectsForRange(unsigned start, unsigned end, bool useSelectionHeight, bool* wasFixed) const
{
    OptionSet<RenderObject::BoundingRectBehavior> behavior;
    if (useSelectionHeight)
        behavior.add(RenderObject::BoundingRectBehavior::UseSelectionHeight);
    return absoluteQuadsForRange(start, end, behavior, wasFixed).map([](auto& quad) {
        return quad.enclosingBoundingBox();
    });
}

#if PLATFORM(IOS_FAMILY)
// This function is similar in spirit to addLineBoxRects, but returns rectangles
// which are annotated with additional state which helps the iPhone draw selections in its unique way.
// Full annotations are added in this class.
void RenderText::collectSelectionGeometries(Vector<SelectionGeometry>& rects, unsigned start, unsigned end)
{
    for (auto textBox = InlineIterator::firstTextBoxFor(*this); textBox; textBox = textBox.traverseNextTextBox()) {
        LayoutRect rect;
        if (start <= textBox->start() && textBox->end() <= end)
            rect = selectionRectForTextBox(*textBox, start, end);
        else {
            unsigned realEnd = std::min(textBox->end(), end);
            rect = selectionRectForTextBox(*textBox, start, realEnd);
            if (rect.isEmpty())
                continue;
        }

        if (textBox->lineBox()->isFirstAfterPageBreak()) {
            if (textBox->isHorizontal())
                rect.shiftYEdgeTo(textBox->lineBox()->logicalTop());
            else
                rect.shiftXEdgeTo(textBox->lineBox()->logicalTop());
        }

        RenderBlock* containingBlock = this->containingBlock();
        // Map rect, extended left to leftOffset, and right to rightOffset, through transforms to get minX and maxX.
        LogicalSelectionOffsetCaches cache(*containingBlock);
        LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, LayoutUnit(textBox->logicalTop()), cache);
        LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, LayoutUnit(textBox->logicalTop()), cache);
        LayoutRect extentsRect = rect;
        if (textBox->isHorizontal()) {
            extentsRect.setX(leftOffset);
            extentsRect.setWidth(rightOffset - leftOffset);
        } else {
            extentsRect.setY(leftOffset);
            extentsRect.setHeight(rightOffset - leftOffset);
        }
        extentsRect = localToAbsoluteQuad(FloatRect(extentsRect)).enclosingBoundingBox();
        if (!textBox->isHorizontal())
            extentsRect = extentsRect.transposedRect();
        bool isFirstOnLine = !textBox->previousOnLine();
        bool isLastOnLine = !textBox->nextOnLine();

        bool containsStart = textBox->start() <= start && textBox->end() >= start;
        bool containsEnd = textBox->start() <= end && textBox->end() >= end;

        bool isFixed = false;
        auto absoluteQuad = localToAbsoluteQuad(FloatRect(rect), UseTransforms, &isFixed);
        bool boxIsHorizontal = !is<InlineIterator::SVGTextBoxIterator>(textBox) ? textBox->isHorizontal() : !writingMode().isVertical();

        rects.append(SelectionGeometry(absoluteQuad, HTMLElement::selectionRenderingBehavior(textNode()), textBox->direction(), extentsRect.x(), extentsRect.maxX(), extentsRect.maxY(), 0, textBox->isLineBreak(), isFirstOnLine, isLastOnLine, containsStart, containsEnd, boxIsHorizontal, isFixed, view().pageNumberForBlockProgressionOffset(absoluteQuad.enclosingBoundingBox().x())));
    }
}
#endif

static std::optional<IntRect> ellipsisRectForTextBox(const InlineIterator::TextBox& textBox, unsigned start, unsigned end)
{
    auto lineBox = textBox.lineBox();
    if (!lineBox->hasEllipsis())
        return { };

    auto selectableRange = textBox.selectableRange();
    if (!selectableRange.truncation)
        return { };

    auto ellipsisStartPosition = std::max<unsigned>(start - selectableRange.start, 0);
    auto ellipsisEndPosition = std::min<unsigned>(end - selectableRange.start, selectableRange.length);
    // The ellipsis should be considered to be selected if the end of
    // the selection is past the beginning of the truncation and the
    // beginning of the selection is before or at the beginning of the truncation.
    if (ellipsisEndPosition < *selectableRange.truncation && ellipsisStartPosition > *selectableRange.truncation)
        return { };

    return IntRect { lineBox->ellipsisVisualRect(InlineIterator::LineBox::AdjustedForSelection::Yes) };
}

enum class ClippingOption { NoClipping, ClipToEllipsis };

// FIXME: Unify with absoluteQuadsForRange.
static Vector<FloatQuad> collectAbsoluteQuads(const RenderText& textRenderer, bool* wasFixed, ClippingOption clipping)
{
    Vector<FloatQuad> quads;
    for (auto& textBox : InlineIterator::textBoxesFor(textRenderer)) {
        auto* svgTextBox = dynamicDowncast<InlineIterator::SVGTextBox>(textBox);
        auto boundaries = svgTextBox ? svgTextBox->calculateBoundariesIncludingSVGTransform() : textBox.visualRectIgnoringBlockDirection();

        // Shorten the width of this text box if it ends in an ellipsis.
        if (clipping == ClippingOption::ClipToEllipsis) {
            if (auto ellipsisRect = ellipsisRectForTextBox(textBox, 0, textRenderer.text().length())) {
                if (textRenderer.writingMode().isHorizontal())
                    boundaries.setWidth(ellipsisRect->maxX() - boundaries.x());
                else
                    boundaries.setHeight(ellipsisRect->maxY() - boundaries.y());
            }
        }
        
        quads.append(textRenderer.localToAbsoluteQuad(boundaries, UseTransforms, wasFixed));
    }
    return quads;
}

Vector<FloatQuad> RenderText::absoluteQuadsClippedToEllipsis() const
{
    return collectAbsoluteQuads(*this, nullptr, ClippingOption::ClipToEllipsis);
}

void RenderText::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    quads.appendVector(collectAbsoluteQuads(*this, wasFixed, ClippingOption::NoClipping));
}

static FloatRect localQuadForTextBox(const InlineIterator::TextBox& textBox, unsigned start, unsigned end, bool useSelectionHeight)
{
    LayoutRect boxSelectionRect = selectionRectForTextBox(textBox, start, end);
    if (!boxSelectionRect.height())
        return { };
    if (useSelectionHeight)
        return boxSelectionRect;

    auto rect = textBox.visualRectIgnoringBlockDirection();
    if (textBox.isHorizontal()) {
        boxSelectionRect.setHeight(rect.height());
        boxSelectionRect.setY(rect.y());
    } else {
        boxSelectionRect.setWidth(rect.width());
        boxSelectionRect.setX(rect.x());
    }
    return boxSelectionRect;
}

static Vector<LayoutRect> characterRects(const InlineIterator::TextBox& run, unsigned rangeStart, unsigned rangeEnd)
{
    auto [clampedStart, clampedEnd] = run.selectableRange().clamp(rangeStart, rangeEnd);
    if (clampedStart >= clampedEnd)
        return { };

    if (auto* svgTextBox = dynamicDowncast<InlineIterator::SVGTextBox>(run)) {
        return Vector<LayoutRect>(clampedEnd - clampedStart, [&, clampedStart = clampedStart](size_t i) {
            size_t index = clampedStart + i;
            return svgTextBox->localSelectionRect(index, index + 1);
        });
    }

    auto lineSelectionRect = LineSelection::logicalRect(*run.lineBox());
    auto selectionRect = LayoutRect { run.logicalLeftIgnoringInlineDirection(), lineSelectionRect.y(), run.logicalWidth(), lineSelectionRect.height() };
    return run.fontCascade().characterSelectionRectsForText(run.textRun(), selectionRect, clampedStart, clampedEnd).map([&](auto& characterRect) {
        return snappedSelectionRect(characterRect, run.logicalRightIgnoringInlineDirection(), lineSelectionRect.y(), lineSelectionRect.height(), run.isHorizontal());
    });
}

Vector<FloatQuad> RenderText::absoluteQuadsForRange(unsigned start, unsigned end, OptionSet<RenderObject::BoundingRectBehavior> behavior, bool* wasFixed) const
{
    bool useSelectionHeight = behavior.contains(RenderObject::BoundingRectBehavior::UseSelectionHeight);
    bool ignoreEmptyTextSelections = behavior.contains(RenderObject::BoundingRectBehavior::IgnoreEmptyTextSelections);
    bool computeIndividualCharacterRects = behavior.contains(RenderObject::BoundingRectBehavior::ComputeIndividualCharacterRects);

    // Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". LegacyInlineTextBox coordinates are unsigneds, so changing this
    // function to take ints causes various internal mismatches. But selectionRect takes ints, and
    // passing UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take unsigneds, but
    // that would cause many ripple effects, so for now we'll just clamp our unsigned parameters to INT_MAX.
    ASSERT(end == UINT_MAX || end <= INT_MAX);
    ASSERT(start <= INT_MAX);
    start = std::min(start, static_cast<unsigned>(INT_MAX));
    end = std::min(end, static_cast<unsigned>(INT_MAX));

    const unsigned caretMinOffset = static_cast<unsigned>(this->caretMinOffset());
    const unsigned caretMaxOffset = static_cast<unsigned>(this->caretMaxOffset());

    // Narrows |start| and |end| into |caretMinOffset| and |caretMaxOffset| to ignore unrendered leading 
    // and trailing whitespaces.
    start = std::min(std::max(caretMinOffset, start), caretMaxOffset);
    end = std::min(std::max(caretMinOffset, end), caretMaxOffset);

    Vector<FloatQuad> quads;
    for (auto& textBox : InlineIterator::textBoxesFor(*this)) {
        if (ignoreEmptyTextSelections && !textBox.selectableRange().intersects(start, end))
            continue;

        if (computeIndividualCharacterRects) {
            auto rects = characterRects(textBox, start, end);
            if (!quads.tryReserveCapacity(quads.size() + rects.size()))
                continue;

            if (!useSelectionHeight) {
                for (auto& rect : rects) {
                    auto visualRect = textBox.visualRectIgnoringBlockDirection();
                    if (textBox.isHorizontal()) {
                        rect.setHeight(visualRect.height());
                        rect.setY(visualRect.y());
                    } else {
                        rect.setWidth(visualRect.width());
                        rect.setX(visualRect.x());
                    }
                }
            }

            for (auto& rect : rects) {
                if (FloatRect localRect { rect }; !localRect.isZero())
                    quads.append(localToAbsoluteQuad(localRect, UseTransforms, wasFixed));
            }
            continue;
        }

        if (start <= textBox.start() && textBox.end() <= end) {
            auto* svgTextBox = dynamicDowncast<InlineIterator::SVGTextBox>(textBox);
            auto boundaries = svgTextBox ? svgTextBox->calculateBoundariesIncludingSVGTransform() : textBox.visualRectIgnoringBlockDirection();

            if (useSelectionHeight) {
                LayoutRect selectionRect = selectionRectForTextBox(textBox, start, end);
                if (textBox.isHorizontal()) {
                    boundaries.setHeight(selectionRect.height());
                    boundaries.setY(selectionRect.y());
                } else {
                    boundaries.setWidth(selectionRect.width());
                    boundaries.setX(selectionRect.x());
                }
            }
            quads.append(localToAbsoluteQuad(boundaries, UseTransforms, wasFixed));
            continue;
        }
        FloatRect rect = localQuadForTextBox(textBox, start, end, useSelectionHeight);
        if (!rect.isZero())
            quads.append(localToAbsoluteQuad(rect, UseTransforms, wasFixed));
    }
    return quads;
}

Position RenderText::positionForPoint(const LayoutPoint& point, HitTestSource source)
{
    return positionForPoint(point, source, nullptr).deepEquivalent();
}

enum ShouldAffinityBeDownstream { AlwaysDownstream, AlwaysUpstream, UpstreamIfPositionIsNotAtStart };

static bool lineDirectionPointFitsInBox(int pointLineDirection, const InlineIterator::TextBoxIterator& textRun, ShouldAffinityBeDownstream& shouldAffinityBeDownstream)
{
    shouldAffinityBeDownstream = AlwaysDownstream;

    // the x coordinate is equal to the left edge of this box
    // the affinity must be downstream so the position doesn't jump back to the previous line
    // except when box is the first box in the line
    if (pointLineDirection <= textRun->logicalLeftIgnoringInlineDirection()) {
        shouldAffinityBeDownstream = !textRun->previousOnLine() ? UpstreamIfPositionIsNotAtStart : AlwaysDownstream;
        return true;
    }

#if !PLATFORM(IOS_FAMILY)
    // and the x coordinate is to the left of the right edge of this box
    // check to see if position goes in this box
    if (pointLineDirection < textRun->logicalRightIgnoringInlineDirection()) {
        shouldAffinityBeDownstream = UpstreamIfPositionIsNotAtStart;
        return true;
    }
#endif

    // box is first on line
    // and the x coordinate is to the left of the first text box left edge
    if (!textRun->previousOnLineIgnoringLineBreak() && pointLineDirection < textRun->logicalLeftIgnoringInlineDirection())
        return true;

    if (!textRun->nextOnLineIgnoringLineBreak()) {
        // box is last on line
        // and the x coordinate is to the right of the last text box right edge
        // generate VisiblePosition, use Affinity::Upstream affinity if possible
        shouldAffinityBeDownstream = UpstreamIfPositionIsNotAtStart;
        return true;
    }

    return false;
}

static VisiblePosition createVisiblePositionForBox(const InlineIterator::BoxIterator& run, unsigned offset, ShouldAffinityBeDownstream shouldAffinityBeDownstream)
{
    auto affinity = VisiblePosition::defaultAffinity;
    switch (shouldAffinityBeDownstream) {
    case AlwaysDownstream:
        affinity = Affinity::Downstream;
        break;
    case AlwaysUpstream:
        affinity = Affinity::Upstream;
        break;
    case UpstreamIfPositionIsNotAtStart:
        affinity = offset > run->minimumCaretOffset() ? Affinity::Upstream : Affinity::Downstream;
        break;
    }
    return run->renderer().createVisiblePosition(offset, affinity);
}

static VisiblePosition createVisiblePositionAfterAdjustingOffsetForBiDi(const InlineIterator::TextBoxIterator& run, unsigned offset, ShouldAffinityBeDownstream shouldAffinityBeDownstream)
{
    if (offset && offset < run->length())
        return createVisiblePositionForBox(run, run->start() + offset, shouldAffinityBeDownstream);

    bool positionIsAtStartOfBox = !offset;
    if (positionIsAtStartOfBox == run->isLeftToRightDirection()) {
        // offset is on the left edge

        auto previousRun = run->previousOnLineIgnoringLineBreak();
        if ((previousRun && previousRun->bidiLevel() == run->bidiLevel())
            || run->renderer().containingBlock()->writingMode().bidiDirection() == run->direction()) // FIXME: left on 12CBA
            return createVisiblePositionForBox(run, run->leftmostCaretOffset(), shouldAffinityBeDownstream);

        if (previousRun && previousRun->bidiLevel() > run->bidiLevel()) {
            // e.g. left of B in aDC12BAb
            auto leftmostRun = previousRun;
            for (; previousRun; previousRun.traversePreviousOnLineIgnoringLineBreak()) {
                if (previousRun->bidiLevel() <= run->bidiLevel())
                    break;
                leftmostRun = previousRun;
            }
            return createVisiblePositionForBox(leftmostRun, leftmostRun->rightmostCaretOffset(), shouldAffinityBeDownstream);
        }

        if (!previousRun || previousRun->bidiLevel() < run->bidiLevel()) {
            // e.g. left of D in aDC12BAb
            InlineIterator::BoxIterator rightmostRun = run;
            for (auto nextRun = run->nextOnLineIgnoringLineBreak(); nextRun; nextRun.traverseNextOnLineIgnoringLineBreak()) {
                if (nextRun->bidiLevel() < run->bidiLevel())
                    break;
                rightmostRun = nextRun;
            }
            return createVisiblePositionForBox(rightmostRun,
                run->isLeftToRightDirection() ? rightmostRun->maximumCaretOffset() : rightmostRun->minimumCaretOffset(), shouldAffinityBeDownstream);
        }

        return createVisiblePositionForBox(run, run->rightmostCaretOffset(), shouldAffinityBeDownstream);
    }

    auto nextRun = run->nextOnLineIgnoringLineBreak();
    if ((nextRun && nextRun->bidiLevel() == run->bidiLevel())
        || run->renderer().containingBlock()->writingMode().bidiDirection() == run->direction())
        return createVisiblePositionForBox(run, run->rightmostCaretOffset(), shouldAffinityBeDownstream);

    // offset is on the right edge
    if (nextRun && nextRun->bidiLevel() > run->bidiLevel()) {
        // e.g. right of C in aDC12BAb
        auto rightmostRun = nextRun;
        for (; nextRun; nextRun.traverseNextOnLineIgnoringLineBreak()) {
            if (nextRun->bidiLevel() <= run->bidiLevel())
                break;
            rightmostRun = nextRun;
        }

        return createVisiblePositionForBox(rightmostRun, rightmostRun->leftmostCaretOffset(), shouldAffinityBeDownstream);
    }

    if (!nextRun || nextRun->bidiLevel() < run->bidiLevel()) {
        // e.g. right of A in aDC12BAb
        InlineIterator::BoxIterator leftmostRun = run;
        for (auto previousRun = run->previousOnLineIgnoringLineBreak(); previousRun; previousRun.traversePreviousOnLineIgnoringLineBreak()) {
            if (previousRun->bidiLevel() < run->bidiLevel())
                break;
            leftmostRun = previousRun;
        }

        return createVisiblePositionForBox(leftmostRun,
            run->isLeftToRightDirection() ? leftmostRun->minimumCaretOffset() : leftmostRun->maximumCaretOffset(), shouldAffinityBeDownstream);
    }

    return createVisiblePositionForBox(run, run->leftmostCaretOffset(), shouldAffinityBeDownstream);
}


VisiblePosition RenderText::positionForPoint(const LayoutPoint& point, HitTestSource, const RenderFragmentContainer*)
{
    auto firstRun = InlineIterator::firstTextBoxFor(*this);

    if (!firstRun || !text().length())
        return createVisiblePosition(0, Affinity::Downstream);

    auto pointLineDirection = firstRun->isHorizontal() ? point.x() : point.y();
    auto pointBlockDirection = firstRun->isHorizontal() ? point.y() : point.x();
    bool blocksAreFlipped = writingMode().isBlockFlipped();

    InlineIterator::TextBoxIterator lastRun;
    for (auto run = firstRun; run; run.traverseNextTextBox()) {
        if (run->isLineBreak() && !run->previousOnLine() && run->nextOnLine() && !run->nextOnLine()->isLineBreak())
            run.traverseNextTextBox();

        auto lineBox = run->lineBox();
        auto top = LayoutUnit { std::min(previousLineBoxContentBottomOrBorderAndPadding(*lineBox), lineBox->contentLogicalTop()) };
        if (pointBlockDirection > top || (!blocksAreFlipped && pointBlockDirection == top)) {
            auto bottom = LayoutUnit { LineSelection::logicalBottom(*lineBox) };
            if (auto nextLineBox = lineBox->next())
                bottom = std::min(bottom, LayoutUnit { nextLineBox->contentLogicalTop() });

            if (pointBlockDirection < bottom || (blocksAreFlipped && pointBlockDirection == bottom)) {
                ShouldAffinityBeDownstream shouldAffinityBeDownstream;
#if PLATFORM(IOS_FAMILY)
                if (pointLineDirection != run->logicalLeftIgnoringInlineDirection() && point.x() < run->visualRectIgnoringBlockDirection().x() + run->logicalWidth()) {
                    auto half = LayoutUnit { run->visualRectIgnoringBlockDirection().x() + run->logicalWidth() / 2.f };
                    shouldAffinityBeDownstream = point.x() < half ? AlwaysDownstream : AlwaysUpstream;
                    return createVisiblePositionAfterAdjustingOffsetForBiDi(run, offsetForPositionInRun(*run, pointLineDirection), shouldAffinityBeDownstream);
                }
#endif
                if (lineDirectionPointFitsInBox(pointLineDirection, run, shouldAffinityBeDownstream))
                    return createVisiblePositionAfterAdjustingOffsetForBiDi(run, offsetForPositionInRun(*run, pointLineDirection), shouldAffinityBeDownstream);
            }
        }
        lastRun = run;
    }

    if (lastRun) {
        ShouldAffinityBeDownstream shouldAffinityBeDownstream;
        lineDirectionPointFitsInBox(pointLineDirection, lastRun, shouldAffinityBeDownstream);
        return createVisiblePositionAfterAdjustingOffsetForBiDi(lastRun, offsetForPositionInRun(*lastRun, pointLineDirection) + lastRun->start(), shouldAffinityBeDownstream);
    }
    return createVisiblePosition(0, Affinity::Downstream);
}

static inline std::optional<float> combineTextWidth(const RenderText& renderer, const FontCascade& fontCascade, const RenderStyle& style)
{
    if (!style.hasTextCombine())
        return { };
    auto* combineTextRenderer = dynamicDowncast<RenderCombineText>(renderer);
    if (!combineTextRenderer)
        return { };
    return combineTextRenderer->isCombined() ? std::make_optional(combineTextRenderer->combinedTextWidth(fontCascade)) : std::nullopt;
}

ALWAYS_INLINE float RenderText::widthFromCache(const FontCascade& fontCascade, unsigned start, unsigned length, float xPos, SingleThreadWeakHashSet<const Font>* fallbackFonts, GlyphOverflow* glyphOverflow, const RenderStyle& style) const
{
    if (auto width = combineTextWidth(*this, fontCascade, style))
        return *width;

    TextRun run = RenderBlock::constructTextRun(*this, start, length, style);
    run.setCharacterScanForCodePath(!canUseSimpleFontCodePath());
    run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
    run.setXPos(xPos);
    return fontCascade.width(run, fallbackFonts, glyphOverflow);
}

ALWAYS_INLINE float RenderText::widthFromCacheConsideringPossibleTrailingSpace(const RenderStyle& style, const FontCascade& font, unsigned startIndex, unsigned wordLen, float xPos, bool currentCharacterIsSpace, WordTrailingSpace& wordTrailingSpace, SingleThreadWeakHashSet<const Font>& fallbackFonts, GlyphOverflow& glyphOverflow) const
{
    return measureTextConsideringPossibleTrailingSpace(currentCharacterIsSpace, startIndex, wordLen, wordTrailingSpace, fallbackFonts, [&] (unsigned from, unsigned len) {
        return widthFromCache(font, from, len, xPos, &fallbackFonts, &glyphOverflow, style);
    });
}

inline bool isHangablePunctuationAtLineStart(UChar c)
{
    return U_GET_GC_MASK(c) & (U_GC_PS_MASK | U_GC_PI_MASK | U_GC_PF_MASK);
}

inline bool isHangablePunctuationAtLineEnd(UChar c)
{
    return U_GET_GC_MASK(c) & (U_GC_PE_MASK | U_GC_PI_MASK | U_GC_PF_MASK);
}

float RenderText::hangablePunctuationStartWidth(unsigned index) const
{
    unsigned length = text().length();
    if (index >= length)
        return 0;

    if (!isHangablePunctuationAtLineStart(text()[index]))
        return 0;

    auto& style = this->style();
    return widthFromCache(style.fontCascade(), index, 1, 0, 0, 0, style);
}

float RenderText::hangablePunctuationEndWidth(unsigned index) const
{
    unsigned length = text().length();
    if (index >= length)
        return 0;

    if (!isHangablePunctuationAtLineEnd(text()[index]))
        return 0;

    auto& style = this->style();
    return widthFromCache(style.fontCascade(), index, 1, 0, 0, 0, style);
}

bool RenderText::isHangableStopOrComma(UChar c)
{
    return c == 0x002C || c == 0x002E || c == 0x060C || c == 0x06D4 || c == 0x3001
        || c == 0x3002 || c == 0xFF0C || c == 0xFF0E || c == 0xFE50 || c == 0xFE51
        || c == 0xFE52 || c == 0xFF61 || c == 0xFF64;
}

unsigned RenderText::firstCharacterIndexStrippingSpaces() const
{
    if (!style().collapseWhiteSpace())
        return 0;

    unsigned i = 0;
    for (unsigned length = text().length() ; i < length; ++i) {
        if (text()[i] != ' ' && (text()[i] != '\n' || style().preserveNewline()) && text()[i] != '\t')
            break;
    }
    return i;
}

unsigned RenderText::lastCharacterIndexStrippingSpaces() const
{
    if (!text().length())
        return 0;

    if (!style().collapseWhiteSpace())
        return text().length() - 1;
    
    int i = text().length() - 1;
    for ( ; i  >= 0; --i) {
        if (text()[i] != ' ' && (text()[i] != '\n' || style().preserveNewline()) && text()[i] != '\t')
            break;
    }
    return i;
}

RenderText::Widths RenderText::trimmedPreferredWidths(float leadWidth, bool& stripFrontSpaces)
{
    auto& style = this->style();
    bool collapseWhiteSpace = style.collapseWhiteSpace();

    if (!collapseWhiteSpace)
        stripFrontSpaces = false;

    if (m_hasTab || preferredLogicalWidthsDirty() || !m_minWidth || !m_maxWidth)
        computePreferredLogicalWidths(leadWidth, !m_minWidth || !m_maxWidth);

    Widths widths;

    widths.beginWS = !stripFrontSpaces && m_hasBeginWS;
    widths.endWS = m_hasEndWS;

    unsigned length = this->length();

    if (!length || (stripFrontSpaces && text().containsOnly<isASCIIWhitespace>()))
        return widths;

    widths.endZeroSpace = text()[length - 1] == zeroWidthSpace;

    widths.min = m_minWidth.value_or(-1);
    widths.max = m_maxWidth.value_or(-1);

    widths.beginMin = m_beginMinWidth;
    widths.endMin = m_endMinWidth;

    widths.hasBreakableChar = m_hasBreakableChar;
    widths.hasBreak = m_hasBreak;
    widths.endsWithBreak = m_hasBreak && text()[length - 1] == '\n';

    if (text()[0] == ' ' || (text()[0] == '\n' && !style.preserveNewline()) || text()[0] == '\t') {
        auto& font = style.fontCascade(); // FIXME: This ignores first-line.
        if (stripFrontSpaces)
            widths.max -= font.width(RenderBlock::constructTextRun(span(space), style));
        else
            widths.max += font.wordSpacing();
    }

    stripFrontSpaces = collapseWhiteSpace && m_hasEndWS;

    if (!style.autoWrap() || widths.min > widths.max)
        widths.min = widths.max;

    // Compute our max widths by scanning the string for newlines.
    if (widths.hasBreak) {
        auto& font = style.fontCascade(); // FIXME: This ignores first-line.
        bool firstLine = true;
        widths.beginMax = widths.max;
        widths.endMax = widths.max;
        for (unsigned i = 0; i < length; i++) {
            unsigned lineLength = 0;
            while (i + lineLength < length && text()[i + lineLength] != '\n')
                lineLength++;

            if (lineLength) {
                widths.endMax = widthFromCache(font, i, lineLength, leadWidth + widths.endMax, 0, 0, style);
                if (firstLine) {
                    firstLine = false;
                    leadWidth = 0;
                    widths.beginMax = widths.endMax;
                }
                i += lineLength;
            } else if (firstLine) {
                widths.beginMax = 0;
                firstLine = false;
                leadWidth = 0;
            }

            if (i == length - 1) {
                // A <pre> run that ends with a newline, as in, e.g.,
                // <pre>Some text\n\n<span>More text</pre>
                widths.endMax = 0;
            }
        }
    }

    return widths;
}

static inline bool isSpaceAccordingToStyle(UChar c, const RenderStyle& style)
{
    return c == ' ' || (c == noBreakSpace && style.nbspMode() == NBSPMode::Space);
}

float RenderText::minLogicalWidth() const
{
    if (preferredLogicalWidthsDirty() || !m_minWidth)
        const_cast<RenderText*>(this)->computePreferredLogicalWidths(0, !preferredLogicalWidthsDirty());

    return *m_minWidth;
}

float RenderText::maxLogicalWidth() const
{
    if (preferredLogicalWidthsDirty() || !m_maxWidth)
        const_cast<RenderText*>(this)->computePreferredLogicalWidths(0, !preferredLogicalWidthsDirty());

    return *m_maxWidth;
}

TextBreakIterator::LineMode::Behavior mapLineBreakToIteratorMode(LineBreak lineBreak)
{
    switch (lineBreak) {
    case LineBreak::Auto:
    case LineBreak::AfterWhiteSpace:
    case LineBreak::Anywhere:
        return TextBreakIterator::LineMode::Behavior::Default;
    case LineBreak::Loose:
        return TextBreakIterator::LineMode::Behavior::Loose;
    case LineBreak::Normal:
        return TextBreakIterator::LineMode::Behavior::Normal;
    case LineBreak::Strict:
        return TextBreakIterator::LineMode::Behavior::Strict;
    }
    ASSERT_NOT_REACHED();
    return TextBreakIterator::LineMode::Behavior::Default;
}

TextBreakIterator::ContentAnalysis mapWordBreakToContentAnalysis(WordBreak wordBreak)
{
    switch (wordBreak) {
    case WordBreak::Normal:
    case WordBreak::BreakAll:
    case WordBreak::KeepAll:
    case WordBreak::BreakWord:
        return TextBreakIterator::ContentAnalysis::Mechanical;
    case WordBreak::AutoPhrase:
        return TextBreakIterator::ContentAnalysis::Linguistic;
    }
    return TextBreakIterator::ContentAnalysis::Mechanical;
}

void RenderText::computePreferredLogicalWidths(float leadWidth, bool forcedMinMaxWidthComputation)
{
    SingleThreadWeakHashSet<const Font> fallbackFonts;
    GlyphOverflow glyphOverflow;
    computePreferredLogicalWidths(leadWidth, fallbackFonts, glyphOverflow, forcedMinMaxWidthComputation);
    if (fallbackFonts.isEmptyIgnoringNullReferences() && !glyphOverflow.left && !glyphOverflow.right && !glyphOverflow.top && !glyphOverflow.bottom)
        m_knownToHaveNoOverflowAndNoFallbackFonts = true;
}

static inline float hyphenWidth(RenderText& renderer, const FontCascade& font)
{
    const RenderStyle& style = renderer.style();
    auto textRun = RenderBlock::constructTextRun(style.hyphenString().string(), style);
    return font.width(textRun);
}

float RenderText::maxWordFragmentWidth(const RenderStyle& style, const FontCascade& font, StringView word, unsigned minimumPrefixLength, unsigned minimumSuffixLength, bool currentCharacterIsSpace, unsigned characterIndex, float xPos, float entireWordWidth, WordTrailingSpace& wordTrailingSpace, SingleThreadWeakHashSet<const Font>& fallbackFonts, GlyphOverflow& glyphOverflow)
{
    unsigned suffixStart = 0;
    if (word.length() <= minimumSuffixLength)
        return entireWordWidth;

    Vector<int, 8> hyphenLocations;
    ASSERT(word.length() >= minimumSuffixLength);
    unsigned hyphenLocation = word.length() - minimumSuffixLength;
    while ((hyphenLocation = lastHyphenLocation(word, hyphenLocation, style.computedLocale())) >= std::max(minimumPrefixLength, 1U))
        hyphenLocations.append(hyphenLocation);

    if (hyphenLocations.isEmpty())
        return entireWordWidth;

    hyphenLocations.reverse();

    // Consider the word "ABC-DEF-GHI" (where the '-' characters are hyphenation opportunities). We want to measure the width
    // of "ABC-" and "DEF-", but not "GHI-". Instead, we should measure "GHI" the same way we measure regular unhyphenated
    // words (by using wordTrailingSpace). Therefore, this function is split up into two parts - one that measures each prefix,
    // and one that measures the single last suffix.

    // FIXME: Breaking the string at these places in the middle of words doesn't work with complex text.
    float minimumFragmentWidthToConsider = font.size() * 5 / 4 + hyphenWidth(*this, font);
    float maxFragmentWidth = 0;
    for (size_t k = 0; k < hyphenLocations.size(); ++k) {
        int fragmentLength = hyphenLocations[k] - suffixStart;
        StringBuilder fragmentWithHyphen;
        fragmentWithHyphen.append(word.substring(suffixStart, fragmentLength));
        fragmentWithHyphen.append(style.hyphenString());

        TextRun run = RenderBlock::constructTextRun(fragmentWithHyphen.toString(), style);
        run.setCharacterScanForCodePath(!canUseSimpleFontCodePath());
        float fragmentWidth = font.width(run, &fallbackFonts, &glyphOverflow);

        // Narrow prefixes are ignored. See tryHyphenating in RenderBlockLineLayout.cpp.
        if (fragmentWidth <= minimumFragmentWidthToConsider)
            continue;

        suffixStart += fragmentLength;
        maxFragmentWidth = std::max(maxFragmentWidth, fragmentWidth);
    }

    if (!suffixStart) {
        // We didn't find any hyphenation opportunities that we're willing to actually use.
        // Therefore, the width of the maximum fragment is just ... the width of the entire word.
        return entireWordWidth;
    }

    auto suffixWidth = widthFromCacheConsideringPossibleTrailingSpace(style, font, characterIndex + suffixStart, word.length() - suffixStart, xPos, currentCharacterIsSpace, wordTrailingSpace, fallbackFonts, glyphOverflow);
    return std::max(maxFragmentWidth, suffixWidth);
}

void RenderText::computePreferredLogicalWidths(float leadWidth, SingleThreadWeakHashSet<const Font>& fallbackFonts, GlyphOverflow& glyphOverflow, bool forcedMinMaxWidthComputation)
{
    ASSERT_UNUSED(forcedMinMaxWidthComputation, m_hasTab || preferredLogicalWidthsDirty() || forcedMinMaxWidthComputation || !m_knownToHaveNoOverflowAndNoFallbackFonts);

    m_minWidth = 0;
    m_beginMinWidth = 0;
    m_endMinWidth = 0;
    m_maxWidth = 0;

    float currMaxWidth = 0;
    m_hasBreakableChar = false;
    m_hasBreak = false;
    m_hasTab = false;
    m_hasBeginWS = false;
    m_hasEndWS = false;

    auto& style = this->style();
    auto& font = style.fontCascade(); // FIXME: This ignores first-line.
    float wordSpacing = font.wordSpacing();
    auto& string = text();
    unsigned length = string.length();
    auto iteratorMode = mapLineBreakToIteratorMode(style.lineBreak());
    auto contentAnalysis = mapWordBreakToContentAnalysis(style.wordBreak());
    CachedLineBreakIteratorFactory lineBreakIteratorFactory(string, style.computedLocale(), iteratorMode, contentAnalysis);
    bool needsWordSpacing = false;
    bool ignoringSpaces = false;
    bool isSpace = false;
    bool firstWord = true;
    bool firstLine = true;
    std::optional<unsigned> nextBreakable;
    unsigned lastWordBoundary = 0;

    WordTrailingSpace wordTrailingSpace(style);
    // If automatic hyphenation is allowed, we keep track of the width of the widest word (or word
    // fragment) encountered so far, and only try hyphenating words that are wider.
    float maxWordWidth = std::numeric_limits<float>::max();
    unsigned minimumPrefixLength = 0;
    unsigned minimumSuffixLength = 0;
    if (style.hyphens() == Hyphens::Auto && canHyphenate(style.computedLocale())) {
        maxWordWidth = 0;

        // Map 'hyphenate-limit-{before,after}: auto;' to 2.
        auto before = style.hyphenationLimitBefore();
        minimumPrefixLength = before < 0 ? 2 : before;

        auto after = style.hyphenationLimitAfter();
        minimumSuffixLength = after < 0 ? 2 : after;
    }

    std::optional<LayoutUnit> firstGlyphLeftOverflow;

    bool breakNBSP = style.autoWrap() && style.nbspMode() == NBSPMode::Space;
    
    bool breakAnywhere = style.lineBreak() == LineBreak::Anywhere && style.autoWrap();
    // Note the deliberate omission of word-wrap/overflow-wrap's break-word value from this breakAll check.
    // Those do not affect minimum preferred sizes. Note that break-word is a non-standard value for
    // word-break, but we support it as though it means break-all.
    bool breakAll = (style.wordBreak() == WordBreak::BreakAll || style.wordBreak() == WordBreak::BreakWord || style.overflowWrap() == OverflowWrap::Anywhere) && style.autoWrap();
    bool keepAllWords = style.wordBreak() == WordBreak::KeepAll;
    bool canUseLineBreakShortcut = iteratorMode == TextBreakIterator::LineMode::Behavior::Default
        && contentAnalysis == TextBreakIterator::ContentAnalysis::Mechanical;

    for (unsigned i = 0; i < length; i++) {
        UChar c = string[i];

        bool previousCharacterIsSpace = isSpace;

        bool isNewline = false;
        if (c == '\n') {
            if (style.preserveNewline()) {
                m_hasBreak = true;
                isNewline = true;
                isSpace = false;
            } else
                isSpace = true;
        } else if (c == '\t') {
            if (!style.collapseWhiteSpace()) {
                m_hasTab = true;
                isSpace = false;
            } else
                isSpace = true;
        } else
            isSpace = c == ' ';

        if ((isSpace || isNewline) && !i)
            m_hasBeginWS = true;
        if ((isSpace || isNewline) && i == length - 1)
            m_hasEndWS = true;

        ignoringSpaces |= style.collapseWhiteSpace() && previousCharacterIsSpace && isSpace;
        ignoringSpaces &= isSpace;

        // Ignore spaces and soft hyphens
        if (ignoringSpaces) {
            ASSERT(lastWordBoundary == i);
            lastWordBoundary++;
            continue;
        } else if (c == softHyphen && style.hyphens() != Hyphens::None) {
            ASSERT(i >= lastWordBoundary);
            currMaxWidth += widthFromCache(font, lastWordBoundary, i - lastWordBoundary, leadWidth + currMaxWidth, &fallbackFonts, &glyphOverflow, style);
            if (!firstGlyphLeftOverflow)
                firstGlyphLeftOverflow = glyphOverflow.left;
            lastWordBoundary = i + 1;
            continue;
        }

        bool hasBreak = breakAll || BreakLines::isBreakable(lineBreakIteratorFactory, i, nextBreakable, breakNBSP, canUseLineBreakShortcut, keepAllWords, breakAnywhere);
        bool betweenWords = true;
        unsigned j = i;
        while (c != '\n' && !isSpaceAccordingToStyle(c, style) && c != '\t' && c != zeroWidthSpace && (c != softHyphen || style.hyphens() == Hyphens::None)) {
            UChar previousCharacter = c;
            j++;
            if (j == length)
                break;
            c = string[j];
            if (U_IS_LEAD(previousCharacter) && U_IS_TRAIL(c))
                continue;
            if (BreakLines::isBreakable(lineBreakIteratorFactory, j, nextBreakable, breakNBSP, canUseLineBreakShortcut, keepAllWords, breakAnywhere) && characterAt(j - 1) != softHyphen)
                break;
            if (breakAll) {
                // FIXME: This code is ultra wrong.
                // The spec says "word-break: break-all: Any typographic letter units are treated as ID(“ideographic characters”) for the purpose of line-breaking."
                // The spec describes how a "typographic letter unit" is a cluster, not a code point: https://drafts.csswg.org/css-text-3/#typographic-character-unit
                betweenWords = false;
                break;
            }
        }

        unsigned wordLen = j - i;
        if (wordLen) {
            float currMinWidth = 0;
            bool isSpace = (j < length) && isSpaceAccordingToStyle(c, style);
            float w = widthFromCacheConsideringPossibleTrailingSpace(style, font, i, wordLen, leadWidth + currMaxWidth, isSpace, wordTrailingSpace, fallbackFonts, glyphOverflow);
            if (c == softHyphen && style.hyphens() != Hyphens::None)
                currMinWidth = hyphenWidth(*this, font);

            if (w > maxWordWidth) {
                auto maxFragmentWidth = maxWordFragmentWidth(style, font, StringView(string).substring(i, wordLen), minimumPrefixLength, minimumSuffixLength, isSpace, i, leadWidth + currMaxWidth, w, wordTrailingSpace, fallbackFonts, glyphOverflow);
                currMinWidth += maxFragmentWidth - w; // This, when combined with "currMinWidth += w" below, has the effect of executing "currMinWidth += maxFragmentWidth" instead.
                maxWordWidth = std::max(maxWordWidth, maxFragmentWidth);
            }

            if (!firstGlyphLeftOverflow)
                firstGlyphLeftOverflow = glyphOverflow.left;
            currMinWidth += w;
            if (betweenWords) {
                if (lastWordBoundary == i)
                    currMaxWidth += w;
                else {
                    ASSERT(j >= lastWordBoundary);
                    currMaxWidth += widthFromCache(font, lastWordBoundary, j - lastWordBoundary, leadWidth + currMaxWidth, &fallbackFonts, &glyphOverflow, style);
                }
                lastWordBoundary = j;
            }

            bool isCollapsibleWhiteSpace = (j < length) && style.isCollapsibleWhiteSpace(c);
            if (j < length && style.autoWrap())
                m_hasBreakableChar = true;

            // Add in wordSpacing to our currMaxWidth, but not if this is the last word on a line or the
            // last word in the run.
            if ((isSpace || isCollapsibleWhiteSpace) && !containsOnlyCSSWhitespace(j, length - j))
                currMaxWidth += wordSpacing;

            if (firstWord) {
                firstWord = false;
                // If the first character in the run is breakable, then we consider ourselves to have a beginning
                // minimum width of 0, since a break could occur right before our run starts, preventing us from ever
                // being appended to a previous text run when considering the total minimum width of the containing block.
                if (hasBreak)
                    m_hasBreakableChar = true;
                m_beginMinWidth = hasBreak ? 0 : currMinWidth;
            }
            m_endMinWidth = currMinWidth;

            m_minWidth = std::max(currMinWidth, *m_minWidth);

            i += wordLen - 1;
        } else {
            // Nowrap can never be broken, so don't bother setting the
            // breakable character boolean. Pre can only be broken if we encounter a newline.
            if (style.autoWrap() || isNewline)
                m_hasBreakableChar = true;

            if (isNewline) { // Only set if preserveNewline was true and we saw a newline.
                if (firstLine) {
                    firstLine = false;
                    leadWidth = 0;
                    if (!style.autoWrap())
                        m_beginMinWidth = currMaxWidth;
                }

                if (currMaxWidth > *m_maxWidth)
                    m_maxWidth = currMaxWidth;
                currMaxWidth = 0;
            } else {
                TextRun run = RenderBlock::constructTextRun(*this, i, 1, style);
                run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
                run.setXPos(leadWidth + currMaxWidth);

                currMaxWidth += font.width(run, &fallbackFonts);
                glyphOverflow.right = 0;
                needsWordSpacing = isSpace && !previousCharacterIsSpace && i == length - 1;
            }
            ASSERT(lastWordBoundary == i);
            lastWordBoundary++;
        }
    }

    glyphOverflow.left = firstGlyphLeftOverflow.value_or(glyphOverflow.left);

    if ((needsWordSpacing && length > 1) || (ignoringSpaces && !firstWord))
        currMaxWidth += wordSpacing;

    m_maxWidth = std::max(currMaxWidth, *m_maxWidth);

    if (!style.autoWrap())
        m_minWidth = m_maxWidth;

    if (style.whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve && style.textWrapMode() == TextWrapMode::NoWrap) {
        if (firstLine)
            m_beginMinWidth = *m_maxWidth;
        m_endMinWidth = currMaxWidth;
    }

    setPreferredLogicalWidthsDirty(false);
}

template<typename CharacterType> static inline bool containsOnlyCollapsibleWhitespace(std::span<const CharacterType> characters, const RenderStyle& style)
{
    for (auto character : characters) {
        if (!style.isCollapsibleWhiteSpace(character))
            return false;
    }
    return true;
}

bool RenderText::containsOnlyCollapsibleWhitespace() const
{
    if (text().is8Bit())
        return WebCore::containsOnlyCollapsibleWhitespace(text().span8(), style());
    return WebCore::containsOnlyCollapsibleWhitespace(text().span16(), style());
}

// FIXME: merge this with isCSSSpace somehow
template<typename CharacterType> static inline bool containsOnlyPossiblyCollapsibleWhitespace(std::span<const CharacterType> characters)
{
    for (auto character : characters) {
        if (!(character == '\n' || character == ' ' || character == '\t'))
            return false;
    }
    return true;
}

bool RenderText::containsOnlyCSSWhitespace(unsigned from, unsigned length) const
{
    ASSERT(from <= text().length());
    ASSERT(length <= text().length());
    ASSERT(from + length <= text().length());
    if (text().is8Bit())
        return containsOnlyPossiblyCollapsibleWhitespace(text().span8().subspan(from, length));
    return containsOnlyPossiblyCollapsibleWhitespace(text().span16().subspan(from, length));
}

Vector<std::pair<unsigned, unsigned>> RenderText::contentRangesBetweenOffsetsForType(DocumentMarkerType type, unsigned startOffset, unsigned endOffset) const
{
    if (!textNode())
        return { };

    CheckedPtr markerController = document().markersIfExists();
    if (!markerController)
        return { };

    auto markers = markerController->markersFor(*textNode(), type);
    if (markers.isEmpty())
        return { };

    Vector<std::pair<unsigned, unsigned>> contentRanges;
    for (auto& marker : markers) {
        unsigned markerStart = std::max(marker->startOffset(), startOffset);
        unsigned markerEnd = std::min(marker->endOffset(), endOffset);
        if (markerStart >= markerEnd || markerStart > endOffset || markerEnd < startOffset)
            continue;

        contentRanges.append({ markerStart, markerEnd });
    }
    return contentRanges;
}

IntPoint RenderText::firstRunLocation() const
{
    auto first = InlineIterator::firstTextBoxFor(*this);
    if (!first)
        return { };
    return IntPoint(first->visualRectIgnoringBlockDirection().location());
}

void RenderText::setSelectionState(HighlightState state)
{
    RenderObject::setSelectionState(state);

    // The containing block can be null in case of an orphaned tree.
    RenderBlock* containingBlock = this->containingBlock();
    if (containingBlock && !containingBlock->isRenderView())
        containingBlock->setSelectionState(state);
}

static inline bool isInlineFlowOrEmptyText(const RenderObject& renderer)
{
    if (is<RenderInline>(renderer))
        return true;
    auto* textRenderer = dynamicDowncast<RenderText>(renderer);
    return textRenderer && textRenderer->text().isEmpty();
}

Vector<UChar> RenderText::previousCharacter() const
{
    const RenderObject* previousText = this;
    while ((previousText = previousText->previousInPreOrder())) {
        if (!previousText->isInFlow())
            continue;
        if (!isInlineFlowOrEmptyText(*previousText))
            break;
    }
    auto* renderText = dynamicDowncast<RenderText>(previousText);
    Vector<UChar> previous;
    if (!renderText)
        previous.append(' ');
    else {
        auto& previousString = renderText->text();
        if (previousString.is8Bit())
            previous.append(previousString[previousString.length() - 1]);
        else {
            auto previousCharacterLength = [&] {
                auto contentIterator = SurrogatePairAwareTextIterator { previousString.span16(), 0, previousString.length() };
                unsigned characterLength = 0;
                char32_t currentCharacter = 0;
                while (contentIterator.consume(currentCharacter, characterLength))
                    contentIterator.advance(characterLength);
                return characterLength;
            }();

            if (previousCharacterLength > previousString.length()) {
                ASSERT_NOT_REACHED();
                return previous;
            }
            for (size_t i = previousString.length() - previousCharacterLength; i < previousString.length(); ++i)
                previous.append(previousString[i]);
        }
    }
    return previous;
}

static String convertToFullSizeKana(const String& string)
{
    // https://www.w3.org/TR/css-text-3/#small-kana
    static constexpr std::pair<char32_t, UChar> kanasMap[] = {
        { 0x3041, 0x3042 },
        { 0x3043, 0x3044 },
        { 0x3045, 0x3046 },
        { 0x3047, 0x3048 },
        { 0x3049, 0x304A },
        { 0x3063, 0x3064 },
        { 0x3083, 0x3084 },
        { 0x3085, 0x3086 },
        { 0x3087, 0x3088 },
        { 0x308E, 0x308F },
        { 0x3095, 0x304B },
        { 0x3096, 0x3051 },
        { 0x30A1, 0x30A2 },
        { 0x30A3, 0x30A4 },
        { 0x30A5, 0x30A6 },
        { 0x30A7, 0x30A8 },
        { 0x30A9, 0x30AA },
        { 0x30C3, 0x30C4 },
        { 0x30E3, 0x30E4 },
        { 0x30E5, 0x30E6 },
        { 0x30E7, 0x30E8 },
        { 0x30EE, 0x30EF },
        { 0x30F5, 0x30AB },
        { 0x30F6, 0x30B1 },
        { 0x31F0, 0x30AF },
        { 0x31F1, 0x30B7 },
        { 0x31F2, 0x30B9 },
        { 0x31F3, 0x30C8 },
        { 0x31F4, 0x30CC },
        { 0x31F5, 0x30CF },
        { 0x31F6, 0x30D2 },
        { 0x31F7, 0x30D5 },
        { 0x31F8, 0x30D8 },
        { 0x31F9, 0x30DB },
        { 0x31FA, 0x30E0 },
        { 0x31FB, 0x30E9 },
        { 0x31FC, 0x30EA },
        { 0x31FD, 0x30EB },
        { 0x31FE, 0x30EC },
        { 0x31FF, 0x30ED },
        { 0xFF67, 0xFF71 },
        { 0xFF68, 0xFF72 },
        { 0xFF69, 0xFF73 },
        { 0xFF6A, 0xFF74 },
        { 0xFF6B, 0xFF75 },
        { 0xFF6C, 0xFF94 },
        { 0xFF6D, 0xFF95 },
        { 0xFF6E, 0xFF96 },
        { 0xFF6F, 0xFF82 },
        { 0x1B132, 0x3053 },
        { 0x1B150, 0x3090 },
        { 0x1B151, 0x3091 },
        { 0x1B152, 0x3092 },
        { 0x1B155, 0x30B3 },
        { 0x1B164, 0x30F0 },
        { 0x1B165, 0x30F1 },
        { 0x1B166, 0x30F2 },
        { 0x1B167, 0x30F3 }
    };

    static constexpr SortedArrayMap sortedMap { kanasMap };

    auto codePoints = StringView { string }.codePoints();

    bool needsConversion = false;
    for (auto character : codePoints) {
        if (sortedMap.contains(character)) {
            needsConversion = true;
            break;
        }
    }
    if (!needsConversion)
        return string;

    StringBuilder result;
    for (auto character : codePoints) {
        if (auto found = sortedMap.tryGet(character))
            result.append(*found);
        else
            result.append(character);
    }
    return result.toString();
}

String applyTextTransform(const RenderStyle& style, const String& text)
{
    Vector<UChar> previousCharacter(1, ' ');
    return applyTextTransform(style, text, previousCharacter);
}

String applyTextTransform(const RenderStyle& style, const String& text, Vector<UChar> previousCharacter)
{
    auto transform = style.textTransform();

    if (transform.isEmpty())
        return text;

    // https://w3c.github.io/csswg-drafts/css-text/#text-transform-order
    auto modified = text;
    if (transform.contains(TextTransform::Capitalize))
        modified = capitalize(modified, previousCharacter); // FIXME: Need to take locale into account.
    else if (transform.contains(TextTransform::Uppercase))
        modified = modified.convertToUppercaseWithLocale(style.computedLocale());
    else if (transform.contains(TextTransform::Lowercase))
        modified = modified.convertToLowercaseWithLocale(style.computedLocale());

    if (transform.contains(TextTransform::FullWidth))
        modified = transformToFullWidth(modified);

    if (transform.contains(TextTransform::FullSizeKana))
        modified = convertToFullSizeKana(modified);

    return modified;
}

void RenderText::setRenderedText(const String& newText)
{
    ASSERT(!newText.isNull());

    String originalText = this->originalText();

    m_text = newText;

    if (m_useBackslashAsYenSymbol)
        m_text = makeStringByReplacingAll(m_text, '\\', yenSign);
    
    const auto& style = this->style();
    if (!style.textTransform().isEmpty())
        m_text = applyTextTransform(style, m_text, previousCharacter());

    // At rendering time, if certain fonts are used, these characters get swapped out with higher-quality PUA characters.
    // See RenderBlock::updateSecurityDiscCharacters().
    switch (style.textSecurity()) {
    case TextSecurity::None:
        break;
#if !PLATFORM(IOS_FAMILY)
    // We use the same characters here as for list markers.
    // See the listMarkerText function in RenderListMarker.cpp.
    case TextSecurity::Circle:
        secureText(whiteBullet);
        break;
    case TextSecurity::Disc:
        secureText(bullet);
        break;
    case TextSecurity::Square:
        secureText(blackSquare);
        break;
#else
    // FIXME: Why this quirk on iOS?
    case TextSecurity::Circle:
    case TextSecurity::Disc:
    case TextSecurity::Square:
        secureText(blackCircle);
        break;
#endif
    }

    m_containsOnlyASCII = text().containsOnlyASCII();
    m_canUseSimpleFontCodePath = computeCanUseSimpleFontCodePath();
    m_canUseSimplifiedTextMeasuring = { };
    m_hasPositionDependentContentWidth = { };
    m_hasStrongDirectionalityContent = { };

    if (m_text != originalText) {
        originalTextMap().set(*this, originalText);
        m_originalTextDiffersFromRendered = true;
    } else if (m_originalTextDiffersFromRendered) {
        originalTextMap().remove(*this);
        m_originalTextDiffersFromRendered = false;
    }
}

void RenderText::secureText(UChar maskingCharacter)
{
    // This hides the text by replacing all the characters with the masking character.
    // Offsets within the hidden text have to match offsets within the original text
    // to handle things like carets and selection, so this won't work right if any
    // of the characters are surrogate pairs or combining marks. Thus, this function
    // does not attempt to handle either of those.

    unsigned length = text().length();
    if (!length)
        return;

    UChar characterToReveal = 0;
    unsigned revealedCharactersOffset = 0;

    if (m_hasSecureTextTimer) {
        if (SecureTextTimer* timer = secureTextTimers().get(*this)) {
            // We take the offset out of the timer to make this one-shot. We count on this being called only once.
            // If it's called a second time we assume the text is different and a character should not be revealed.
            revealedCharactersOffset = timer->takeOffsetAfterLastTypedCharacter();
            if (revealedCharactersOffset && revealedCharactersOffset <= length)
                characterToReveal = text()[--revealedCharactersOffset];
        }
    }

    std::span<UChar> characters;
    m_text = String::createUninitialized(length, characters);

    for (unsigned i = 0; i < length; ++i)
        characters[i] = maskingCharacter;
    if (characterToReveal)
        characters[revealedCharactersOffset] = characterToReveal;
}

static void invalidateLineLayoutPathOnContentChangeIfNeeded(RenderText& renderer, size_t offset, int delta)
{
    auto* container = LayoutIntegration::LineLayout::blockContainer(renderer);
    if (!container)
        return;

    auto* inlineLayout = container->inlineLayout();
    if (!inlineLayout)
        return;

    if (LayoutIntegration::LineLayout::shouldInvalidateLineLayoutPathAfterContentChange(*container, renderer, *inlineLayout)) {
        container->invalidateLineLayoutPath(RenderBlockFlow::InvalidationReason::ContentChange);
        return;
    }
    if (!inlineLayout->updateTextContent(renderer, offset, delta))
        container->invalidateLineLayoutPath(RenderBlockFlow::InvalidationReason::ContentChange);
}

void RenderText::setTextInternal(const String& text, bool force)
{
    ASSERT(!text.isNull());

    if (!force && text == originalText())
        return;

    m_text = text;
    if (m_originalTextDiffersFromRendered) {
        originalTextMap().remove(this);
        m_originalTextDiffersFromRendered = false;
    }

    setRenderedText(text);

    setNeedsLayoutAndPrefWidthsRecalc();
    m_knownToHaveNoOverflowAndNoFallbackFonts = false;

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferTextChangedIfNeeded(textNode());
}

void RenderText::setText(const String& newContent, bool force)
{
    auto isDifferent = newContent != text();
    setTextInternal(newContent, force);
    if (isDifferent || force)
        invalidateLineLayoutPathOnContentChangeIfNeeded(*this, 0, text().length());
}

void RenderText::setTextWithOffset(const String& newText, unsigned offset, unsigned, bool force)
{
    if (!force && text() == newText)
        return;

    int delta = newText.length() - text().length();

    m_linesDirty = m_legacyLineBoxes.dirtyForTextChange(*this);

    setTextInternal(newText, force || m_linesDirty);
    invalidateLineLayoutPathOnContentChangeIfNeeded(*this, offset, delta);
}

String RenderText::textWithoutConvertingBackslashToYenSymbol() const
{
    if (!m_useBackslashAsYenSymbol || style().textSecurity() != TextSecurity::None)
        return text();

    if (style().textTransform().isEmpty())
        return originalText();

    return applyTextTransform(style(), originalText(), previousCharacter());
}

void RenderText::dirtyLegacyLineBoxes(bool fullLayout)
{
    if (fullLayout)
        deleteLegacyLineBoxes();
    else if (!m_linesDirty)
        m_legacyLineBoxes.dirtyAll();
    m_linesDirty = false;
}

void RenderText::deleteLegacyLineBoxes()
{
    m_legacyLineBoxes.deleteAll();
}

std::unique_ptr<LegacyInlineTextBox> RenderText::createTextBox()
{
    return makeUnique<LegacyInlineTextBox>(*this);
}

bool RenderText::usesLegacyLineLayoutPath() const
{
    return !LayoutIntegration::LineLayout::containing(*this);
}

float RenderText::width(unsigned from, unsigned len, float xPos, bool firstLine, SingleThreadWeakHashSet<const Font>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    if (from >= text().length())
        return 0;

    if (from + len > text().length())
        len = text().length() - from;

    const RenderStyle& lineStyle = firstLine ? firstLineStyle() : style();
    return width(from, len, lineStyle.fontCascade(), xPos, fallbackFonts, glyphOverflow);
}

float RenderText::width(unsigned from, unsigned length, const FontCascade& fontCascade, float xPos, SingleThreadWeakHashSet<const Font>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    ASSERT(from + length <= text().length());
    if (!text().length() || !length)
        return 0.f;

    auto& style = this->style();
    if (auto width = combineTextWidth(*this, fontCascade, style))
        return *width;

    if (length == 1 && (characterAt(from) == space))
        return fontCascade.widthOfSpaceString();

    float width = 0.f;
    if (&fontCascade == &style.fontCascade()) {
        if (!style.preserveNewline() && !from && length == text().length() && (!glyphOverflow || !glyphOverflow->computeBounds)) {
            if (fallbackFonts) {
                ASSERT(glyphOverflow);
                if (preferredLogicalWidthsDirty() || !m_knownToHaveNoOverflowAndNoFallbackFonts) {
                    const_cast<RenderText*>(this)->computePreferredLogicalWidths(0, *fallbackFonts, *glyphOverflow);
                    if (fallbackFonts->isEmptyIgnoringNullReferences() && !glyphOverflow->left && !glyphOverflow->right && !glyphOverflow->top && !glyphOverflow->bottom)
                        m_knownToHaveNoOverflowAndNoFallbackFonts = true;
                }
                // The rare case of when we switch between IFC and legacy preferred width computation.
                if (!m_maxWidth)
                    width = maxLogicalWidth();
                else
                    width = *m_maxWidth;
            } else
                width = maxLogicalWidth();
        } else
            width = widthFromCache(fontCascade, from, length, xPos, fallbackFonts, glyphOverflow, style);
    } else {
        TextRun run = RenderBlock::constructTextRun(*this, from, length, style);
        run.setCharacterScanForCodePath(!canUseSimpleFontCodePath());
        run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        run.setXPos(xPos);

        width = fontCascade.width(run, fallbackFonts, glyphOverflow);
    }

    return clampTo(width, 0.f);
}

IntRect RenderText::linesBoundingBox() const
{
    auto firstTextBox = InlineIterator::firstTextBoxFor(*this);
    if (!firstTextBox)
        return { };

    auto boundingBox = firstTextBox->visualRectIgnoringBlockDirection();
    for (auto textBox = firstTextBox; ++textBox;)
        boundingBox.uniteEvenIfEmpty(textBox->visualRectIgnoringBlockDirection());

    return enclosingIntRect(boundingBox);
}

LayoutRect RenderText::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    RenderObject* rendererToRepaint = containingBlock();

    // Do not cross self-painting layer boundaries.
    RenderObject& enclosingLayerRenderer = enclosingLayer()->renderer();
    if (&enclosingLayerRenderer != rendererToRepaint && !rendererToRepaint->isDescendantOf(&enclosingLayerRenderer))
        rendererToRepaint = &enclosingLayerRenderer;

    // The renderer we chose to repaint may be an ancestor of repaintContainer, but we need to do a repaintContainer-relative repaint.
    if (repaintContainer && repaintContainer != rendererToRepaint && !rendererToRepaint->isDescendantOf(repaintContainer))
        return repaintContainer->clippedOverflowRect(repaintContainer, context);

    return rendererToRepaint->clippedOverflowRect(repaintContainer, context);
}

auto RenderText::rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds) const -> RepaintRects
{
    return { clippedOverflowRect(repaintContainer, visibleRectContextForRepaint()) };
}

LayoutRect RenderText::collectSelectionGeometriesForLineBoxes(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent, Vector<FloatQuad>* quads)
{
    ASSERT(!needsLayout());

    if (selectionState() == HighlightState::None)
        return LayoutRect();
    if (!containingBlock())
        return LayoutRect();

    // Now calculate startPos and endPos for painting selection.
    // We include a selection while endPos > 0
    unsigned startOffset;
    unsigned endOffset;
    if (selectionState() == HighlightState::Inside) {
        // We are fully selected.
        startOffset = 0;
        endOffset = text().length();
    } else {
        startOffset = view().selection().startOffset();
        endOffset = view().selection().endOffset();
        if (selectionState() == HighlightState::Start)
            endOffset = text().length();
        else if (selectionState() == HighlightState::End)
            startOffset = 0;
    }

    if (startOffset == endOffset)
        return IntRect();

    LayoutRect resultRect;

    for (auto& textBox : InlineIterator::textBoxesFor(*this)) {
        LayoutRect rect;
        rect.unite(selectionRectForTextBox(textBox, startOffset, endOffset));
        rect.unite(ellipsisRectForTextBox(textBox, startOffset, endOffset).value_or(IntRect { }));
        if (rect.isEmpty())
            continue;

        resultRect.unite(rect);

        if (quads)
            quads->append(localToContainerQuad(FloatRect(rect), repaintContainer));
    }

    if (clipToVisibleContent)
        return computeRectForRepaint(resultRect, repaintContainer);
    return localToContainerQuad(FloatRect(resultRect), repaintContainer).enclosingBoundingBox();
}

LayoutRect RenderText::collectSelectionGeometriesForLineBoxes(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent, Vector<FloatQuad>& quads)
{
    return collectSelectionGeometriesForLineBoxes(repaintContainer, clipToVisibleContent, &quads);
}

LayoutRect RenderText::selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent)
{
    return collectSelectionGeometriesForLineBoxes(repaintContainer, clipToVisibleContent, nullptr);
}

int RenderText::caretMinOffset() const
{
    auto first = InlineIterator::firstTextBoxFor(*this);
    if (!first)
        return 0;

    int minOffset = first->start();
    for (auto box = first; ++box;)
        minOffset = std::min<int>(minOffset, box->start());

    return minOffset;
}

int RenderText::caretMaxOffset() const
{
    auto first = InlineIterator::firstTextBoxFor(*this);
    if (!first)
        return text().length();

    int maxOffset = first->end();
    for (auto box = first; ++box;)
        maxOffset = std::max<int>(maxOffset, box->end());

    return maxOffset;
}

unsigned RenderText::countRenderedCharacterOffsetsUntil(unsigned offset) const
{
    unsigned result = 0;
    for (auto& run : InlineIterator::textBoxesFor(*this)) {
        auto start = run.start();
        auto length = run.length();
        if (offset < start)
            return result;
        if (offset <= start + length) {
            result += offset - start;
            return result;
        }
        result += length;
    }
    return result;
}

enum class OffsetType { Character, Caret };
static bool containsOffset(const RenderText& text, unsigned offset, OffsetType type)
{
    for (auto [box, orderCache] = InlineIterator::firstTextBoxInLogicalOrderFor(text); box; box = InlineIterator::nextTextBoxInLogicalOrder(box, orderCache)) {
        auto start = box->start();
        if (offset < start)
            return false;
        unsigned end = box->end();
        if (offset >= start && offset <= end) {
            if (offset == end && (type == OffsetType::Character || box->isLineBreak()))
                continue;
            if (type == OffsetType::Character)
                return true;
            // Return false for offsets inside composed characters.
            return !offset || offset == static_cast<unsigned>(text.nextOffset(text.previousOffset(offset)));
        }
    }
    return false;
}

bool RenderText::containsRenderedCharacterOffset(unsigned offset) const
{
    return containsOffset(*this, offset, OffsetType::Character);
}

bool RenderText::containsCaretOffset(unsigned offset) const
{
    return containsOffset(*this, offset, OffsetType::Caret);
}

bool RenderText::hasRenderedText() const
{
    for (auto& box : InlineIterator::textBoxesFor(*this)) {
        if (box.length())
            return true;
    }
    return false;
}

int RenderText::previousOffset(int current) const
{
    if (m_containsOnlyASCII || text().is8Bit())
        return current - 1;

    CachedTextBreakIterator iterator(text(), { }, TextBreakIterator::CaretMode { }, nullAtom());
    return iterator.preceding(current).value_or(current - 1);
}

int RenderText::previousOffsetForBackwardDeletion(int current) const
{
    CachedTextBreakIterator iterator(text(), { }, TextBreakIterator::DeleteMode { }, nullAtom());
    return iterator.preceding(current).value_or(0);
}

int RenderText::nextOffset(int current) const
{
    if (m_containsOnlyASCII || text().is8Bit())
        return current + 1;

    CachedTextBreakIterator iterator(text(), { }, TextBreakIterator::CaretMode { }, nullAtom());
    return iterator.following(current).value_or(current + 1);
}

bool RenderText::computeCanUseSimpleFontCodePath() const
{
    if (m_containsOnlyASCII || text().is8Bit())
        return true;
    return FontCascade::characterRangeCodePath(text().span16()) == FontCascade::CodePath::Simple;
}

void RenderText::momentarilyRevealLastTypedCharacter(unsigned offsetAfterLastTypedCharacter)
{
    if (style().textSecurity() == TextSecurity::None)
        return;
    m_hasSecureTextTimer = true;
    auto& secureTextTimer = secureTextTimers().add(*this, nullptr).iterator->value;
    if (!secureTextTimer)
        secureTextTimer = makeUnique<SecureTextTimer>(*this);
    secureTextTimer->restart(offsetAfterLastTypedCharacter);
}

StringView RenderText::stringView(unsigned start, std::optional<unsigned> stop) const
{
    unsigned destination = stop.value_or(text().length());
    ASSERT(start <= length());
    ASSERT(destination <= length());
    ASSERT(start <= destination);
    return StringView { text() }.substring(start, destination - start);
}

RenderInline* RenderText::inlineWrapperForDisplayContents()
{
    ASSERT(m_hasInlineWrapperForDisplayContents == inlineWrapperForDisplayContentsMap().contains(this));

    if (!m_hasInlineWrapperForDisplayContents)
        return nullptr;
    return inlineWrapperForDisplayContentsMap().get(this).get();
}

void RenderText::setInlineWrapperForDisplayContents(RenderInline* wrapper)
{
    ASSERT(m_hasInlineWrapperForDisplayContents == inlineWrapperForDisplayContentsMap().contains(this));

    if (!wrapper) {
        if (!m_hasInlineWrapperForDisplayContents)
            return;
        inlineWrapperForDisplayContentsMap().remove(*this);
        m_hasInlineWrapperForDisplayContents = false;
        return;
    }
    inlineWrapperForDisplayContentsMap().add(*this, wrapper);
    m_hasInlineWrapperForDisplayContents = true;
}

std::optional<bool> RenderText::emphasisMarkExistsAndIsAbove(const RenderText& renderer, const RenderStyle& style)
{
    // This function returns true if there are text emphasis marks and they are suppressed by ruby text.
    if (style.textEmphasisMark() == TextEmphasisMark::None)
        return std::nullopt;

    auto emphasisPosition = style.textEmphasisPosition();
    bool isAbove = !emphasisPosition.contains(TextEmphasisPosition::Under);
    if (style.writingMode().isVerticalTypographic())
        isAbove = !emphasisPosition.contains(TextEmphasisPosition::Left);

    auto findRubyAnnotation = [&]() -> RenderBlockFlow* {
        for (auto* baseCandidate = renderer.parent(); baseCandidate; baseCandidate = baseCandidate->parent()) {
            if (!baseCandidate->isInline())
                return nullptr;
            if (baseCandidate->style().display() == DisplayType::RubyBase) {
                auto* annotationCandidate = baseCandidate->nextSibling();
                if (annotationCandidate && annotationCandidate->style().display() == DisplayType::RubyAnnotation)
                    return dynamicDowncast<RenderBlockFlow>(annotationCandidate);
                return nullptr;
            }
        }
        return nullptr;
    };

    if (auto* annotation = findRubyAnnotation()) {
        // The emphasis marks are suppressed only if there is a ruby annotation box on the same side and it is not empty.
        if (annotation->hasLines() && isAbove == (annotation->style().rubyPosition() == RubyPosition::Over))
            return { };
    }

    return isAbove;
}

} // namespace WebCore
