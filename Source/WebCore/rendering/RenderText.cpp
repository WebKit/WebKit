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
#include "DocumentMarkerController.h"
#include "DocumentView.h"
#include "FloatQuad.h"
#include "FontCascadeFonts.h"
#include "FontCascadeInlines.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorLineBoxInlines.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineIteratorSVGTextBox.h"
#include "InlineIteratorTextBox.h"
#include "InlineRunAndOffset.h"
#include "LayoutInlineTextBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "LogicalSelectionOffsetCachesInlines.h"
#include "LineSelection.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MathVariant.h"
#include "Range.h"
#include "RenderBlockFlowInlines.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderSVGInlineText.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "SVGElementTypeHelpers.h"
#include "SelectionGeometry.h"
#include "Settings.h"
#include "StyleComputedStyle+SettersInlines.h"
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
#include <wtf/text/icu/UnicodeExtras.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(IOS_FAMILY)
#include "Document.h"
#include "EditorClient.h"
#include "Page.h"
#endif

namespace WebCore {

using namespace WTF::Unicode;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderText);

bool isDutchLocale(const AtomString& locale)
{
    return locale.length() >= 2
        && isASCIIAlphaCaselessEqual(locale[0], 'n')
        && isASCIIAlphaCaselessEqual(locale[1], 'l')
        && (locale.length() == 2 || locale[2] == '-');
}

struct SameSizeAsRenderText : public RenderObject, private Layout::InlineTextBox {
    float candidateTextSize;
    String text;
    uint32_t bitfields : 6;
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

NODELETE static SecureTextTimerMap& secureTextTimers()
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

NODELETE inline unsigned SecureTextTimer::takeOffsetAfterLastTypedCharacter()
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

NODELETE static HashMap<SingleThreadWeakRef<const RenderText>, String>& originalTextMap()
{
    static NeverDestroyed<HashMap<SingleThreadWeakRef<const RenderText>, String>> map;
    return map;
}

NODELETE static constexpr char16_t convertNoBreakSpaceToSpace(char16_t character)
{
    return character == noBreakSpace ? ' ' : character;
}

static inline size_t capitalizeCharacter(StringView textContent, unsigned startCharacterOffset, StringBuilder& output)
{
    if (startCharacterOffset >= textContent.length()) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    auto capitalize = [&](const char16_t* contentToCapitalize, size_t length) -> size_t {
        if (length == 1) {
            if ((*contentToCapitalize >= 'A' && *contentToCapitalize <= 'Z') || *contentToCapitalize == ' ')
                return 0;
            if (*contentToCapitalize <= 'z') {
                char32_t lastCharacter = u_totitle(*contentToCapitalize);
                output.append(lastCharacter);
                return 1;
            }
        }

        char16_t capitalizedCharacter;
        UErrorCode status = U_ZERO_ERROR;
        auto realLength = u_strToTitle(&capitalizedCharacter, 1, contentToCapitalize, length, nullptr, "", &status);
        if (U_SUCCESS(status) && realLength == 1) {
            output.append(capitalizedCharacter);
            return realLength;
        }

        // Decomposed ligatures may need more space.
        std::span<char16_t> capitalizedStringData;
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

// Titlecase the first letter of a word using ICU's locale-aware u_strToTitle.
// CSS capitalize only uppercases the first letter; u_strToTitle also lowercases
// the rest. We determine the titlecased prefix and return only that.
static size_t capitalizeWordWithLocale(StringView textContent, unsigned startOffset, unsigned endOffset, const AtomString& locale, StringBuilder& output)
{
    auto localeUTF8 = locale.string().utf8();
    unsigned wordLength = std::min(endOffset, textContent.length()) - startOffset;
    if (!wordLength)
        return 0;

    const char16_t* wordData;
    Vector<char16_t, 32> wordBuffer;
    if (textContent.is8Bit()) {
        wordBuffer.resize(wordLength);
        auto wordSpan = wordBuffer.mutableSpan();
        for (unsigned i = 0; i < wordLength; ++i)
            wordSpan[i] = textContent[startOffset + i];
        wordData = wordSpan.data();
    } else
        wordData = textContent.span16().subspan(startOffset, wordLength).data();

    Vector<char16_t, 32> titlecased(wordLength + 4);
    UErrorCode status = U_ZERO_ERROR;
    auto realLength = uStrToTitle(titlecased.mutableSpan().data(), titlecased.size(), wordData, wordLength, nullptr, localeUTF8, &status);
    if (U_FAILURE(status)) {
        if (status != U_BUFFER_OVERFLOW_ERROR)
            return 0;
        titlecased.grow(realLength);
        status = U_ZERO_ERROR;
        uStrToTitle(titlecased.mutableSpan().data(), titlecased.size(), wordData, wordLength, nullptr, localeUTF8, &status);
        if (U_FAILURE(status))
            return 0;
    }

    // Find the titlecased prefix: the initial run of non-lowercase alphabetic characters.
    // For Dutch "ij" with locale "nl", this is 2 ("IJ"). For most words, this is 1.
    // This assumes the titlecased prefix is BMP-only with no combining marks, which
    // holds for Dutch IJ. If extended to other locales, use U16_NEXT for surrogate
    // pairs and handle combining marks.
    size_t prefixLength = 0;
    for (size_t i = 0; i < static_cast<size_t>(realLength); ++i) {
        ASSERT(!U16_IS_SURROGATE(titlecased[i]));
        auto type = u_charType(titlecased[i]);
        if (type == U_LOWERCASE_LETTER)
            break;
        ASSERT(type != U_NON_SPACING_MARK && type != U_COMBINING_SPACING_MARK);
        if (U_MASK(type) & U_GC_L_MASK)
            prefixLength = i + 1;
    }

    if (!prefixLength)
        return 0;

    for (size_t i = 0; i < prefixLength; ++i)
        output.append(static_cast<UChar>(titlecased[i]));
    return prefixLength;
}

String capitalize(const String& string, const AtomString& locale)
{
    return capitalize(string, ' ', locale);
}

String capitalize(const String& string, char32_t previousCharacter, const AtomString& locale)
{
    int32_t length = string.length();
    Ref stringImpl = *string.impl();

    static_assert(String::MaxLength < std::numeric_limits<unsigned>::max(), "Must be able to add one without overflowing unsigned");

    // Replace NO BREAK SPACE with a normal spaces since ICU does not treat it as a word separator.
    std::array<char16_t, 2> previousCharacterUTF16;
    int32_t previousCharacterLength = 0;
    U16_APPEND_UNSAFE(previousCharacterUTF16, previousCharacterLength, previousCharacter);

    Vector<char16_t> stringWithPrevious(previousCharacterLength + length);
    for (int32_t i = 0; i < previousCharacterLength; ++i)
        stringWithPrevious[i] = convertNoBreakSpaceToSpace(previousCharacterUTF16[i]);
    for (int32_t i = previousCharacterLength; i < length + previousCharacterLength; ++i)
        stringWithPrevious[i] = convertNoBreakSpaceToSpace(stringImpl.get()[i - previousCharacterLength]);

    auto* breakIterator = WTF::wordBreakIterator(stringWithPrevious.span());
    if (!breakIterator)
        return string;

    bool isDutch = isDutchLocale(locale);
    auto needsLocaleAwareTitlecase = [&](char16_t firstCharOfWord) {
        return isDutch && isASCIIAlphaCaselessEqual(firstCharOfWord, 'i');
    };

    StringBuilder result;
    result.reserveCapacity(length);

    int32_t startOfWord = ubrk_first(breakIterator);
    for (int32_t endOfWord = ubrk_next(breakIterator); endOfWord != UBRK_DONE; startOfWord = endOfWord, endOfWord = ubrk_next(breakIterator)) {
        // Do not try to titlecase the previous content.
        if (startOfWord >= previousCharacterLength) {
            auto startOffset = startOfWord - previousCharacterLength;
            auto endOffset = endOfWord - previousCharacterLength;
            size_t capitalizedContentLength;
            if (needsLocaleAwareTitlecase(stringImpl.get()[startOffset]))
                capitalizedContentLength = capitalizeWordWithLocale(string, startOffset, endOffset, locale, result);
            else
                capitalizedContentLength = capitalizeCharacter(string, startOffset, result);
            for (int32_t i = startOfWord + capitalizedContentLength; i < endOfWord; ++i)
                result.append(stringImpl.get()[i - previousCharacterLength]);
        } else {
            // This is previous and continous non-titlecased current content. Only append current content.
            for (int32_t i = startOfWord; i < endOfWord; ++i) {
                if (i < previousCharacterLength)
                    continue;
                result.append(stringImpl.get()[i - previousCharacterLength]);
            }
        }
    }
    return result == string ? string : result.toString();
}

NODELETE static LayoutRect snappedSelectionRect(const LayoutRect& selectionRect, float logicalRight, WritingMode writingMode)
{
    auto snappedSelectionRect = enclosingIntRect(selectionRect);
    LayoutUnit logicalWidth = snappedSelectionRect.width();
    if (snappedSelectionRect.x() > logicalRight)
        logicalWidth = 0;
    else if (snappedSelectionRect.maxX() > logicalRight)
        logicalWidth = logicalRight - snappedSelectionRect.x();

    if (writingMode.isHorizontal()) {
        return {
            snappedSelectionRect.x(), selectionRect.y(),
            logicalWidth, selectionRect.height(),
        };
    }
    return {
        selectionRect.y(), snappedSelectionRect.x(),
        selectionRect.height(), logicalWidth,
    };
}

// Painting uses the line's selection extent, while selection geometry and text quads match the text so they agree with its character rects.
enum class LineExtent : bool { Content, Selection };
static LayoutRect selectionRectForTextBox(const InlineIterator::TextBox& textBox, unsigned rangeStart, unsigned rangeEnd, LineExtent lineExtent = LineExtent::Content)
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
            bool isRangeWithinTextBox = (rangeStart >= textBox.start() && rangeStart < textBox.end());
            // When rangeStart == textBox.end(), the range starts _after_ this text box.
            // However that position is not necessarily at the start of the next line. If there's trimmed content between,
            // we should consider it a trailing content on the currernt line.
            if (!isRangeWithinTextBox && rangeStart == textBox.end())
                isRangeWithinTextBox = textBox.nextTextBox() && textBox.nextTextBox()->start() > textBox.end();
            if (!isRangeWithinTextBox)
                return { };
        }
    }

    auto lineBox = textBox.lineBox();
    auto lineRect = lineExtent == LineExtent::Selection ? LineSelection::logicalRect(lineBox) : lineBox.contentLogicalRectAdjustedForAdjacentLineBoxes();
    auto selectionRect = LayoutRect { 0, lineRect.y(), textBox.logicalWidth(), lineRect.height() };

    auto textRun = textBox.textRun();
    if (clampedStart || clampedEnd != textRun.length())
        textBox.fontCascade().adjustSelectionRectForText(textBox.renderer().canUseSimplifiedTextMeasuring(), textRun, selectionRect, clampedStart, clampedEnd);

    if (!textBox.writingMode().isLogicalLeftLineLeft())
        selectionRect.setX(textBox.logicalWidth() - selectionRect.maxX());
    selectionRect.move(textBox.logicalLeftIgnoringInlineDirection(), 0);

    return snappedSelectionRect(selectionRect, textBox.logicalRightIgnoringInlineDirection(), textBox.writingMode());
}

static unsigned offsetForPositionInRun(const InlineIterator::TextBox& textBox, float x)
{
    if (textBox.isLineBreak())
        return 0;

    auto runPosition = x - textBox.logicalLeftIgnoringInlineDirection();
    if (runPosition > textBox.logicalWidth())
        return textBox.isInlineFlipped() ? 0 : textBox.length();
    if (runPosition < 0)
        return textBox.isInlineFlipped() ? textBox.length() : 0;

    if (!textBox.writingMode().isLogicalLeftLineLeft())
        runPosition = textBox.logicalWidth() - runPosition;
    return textBox.fontCascade().offsetForPosition(textBox.textRun(InlineIterator::TextRunMode::Editing), runPosition, true);
}

static FontCascade::CodePath computeFontCodePath(const String& text, bool containsOnlyASCII)
{
    ASSERT(containsOnlyASCII == text.impl()->containsOnlyASCII());
    return (containsOnlyASCII || text.is8Bit()) ? FontCascade::CodePath::Simple : FontCascade::characterRangeCodePath(text.span16());
}

inline RenderText::RenderText(Type type, Node& node, const String& text)
    : RenderObject(type, node, TypeFlag::IsText, { })
    // The initial values of an anonymous inline box. The inherited ones come on attach, see inheritStyleFromParent().
    , Layout::InlineTextBox(Style::ComputedStyle::create())
    , m_text(text)
    , m_containsOnlyASCII(text.impl()->containsOnlyASCII())
    , m_fontCodePath(computeFontCodePath(m_text, m_containsOnlyASCII))
{
    ASSERT(!m_text.isNull());
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

ASCIILiteral RenderText::renderName() const
{
    return "RenderText"_s;
}

NODELETE Text* RenderText::textNode() const
{
    return downcast<Text>(RenderObject::node());
}

bool RenderText::computeUseBackslashAsYenSymbol() const
{
    const Style::ComputedStyle& style = this->style();
    const auto& fontDescription = style.fontDescription();
    if (style.fontCascade().useBackslashAsYenSymbol())
        return true;
    if (fontDescription.hasAuthorSpecifiedNonGenericPrimaryFont())
        return false;
    const PAL::TextEncoding* encoding = document().decoder() ? &document().decoder()->encoding() : 0;
    if (encoding && encoding->backslashAsCurrencySymbol() != '\\')
        return true;
    return false;
}

void RenderText::inheritStyleFromParent()
{
    if (inheritsStyleFromDisplayContentsParent())
        return;
    CheckedPtr parentRenderer = parent();
    CheckedRef parentStyle = parentRenderer->style();
    auto& style = mutableStyle();
    style.inheritFrom(parentStyle);
    style.inheritUnicodeBidiFrom(parentStyle);
    // Text lengths resolve like the lengths of the content it is in.
    style.setUseSVGZoomRulesForLength(parentStyle->useSVGZoomRulesForLength());
    auto mayHaveFirstLineStyle = parentRenderer->layoutBox().mayHaveFirstLineStyle();
    setMayHaveFirstLineStyle(mayHaveFirstLineStyle);
    // Measuring depends on the first-line font. It can change with the parent style or with a move to another parent,
    // which doesn't send styleDidChange.
    if (mayHaveFirstLineStyle)
        LayoutIntegration::BoxTreeUpdater::updateSimplifiedMeasuring(*this);
}

void RenderText::setStyleInheritedFromDisplayContentsParent(Style::ComputedStyle&& style)
{
    bool didChangeStyleSource = !inheritsStyleFromDisplayContentsParent();
    setInheritsStyleFromDisplayContentsParent(true);
    // The style doesn't come from the parent box, so neither does a first-line style.
    setMayHaveFirstLineStyle(false);
    auto oldStyle = mutableStyle().replace(WTF::move(style));
    // A renderer that is not attached yet gets its first style notification on attach.
    if (!parent())
        return;
    displayContentsParentStyleDidChange(oldStyle, didChangeStyleSource);
}

void RenderText::clearStyleInheritedFromDisplayContentsParent()
{
    if (!inheritsStyleFromDisplayContentsParent())
        return;
    setInheritsStyleFromDisplayContentsParent(false);
    if (!parent())
        return;
    auto oldStyle = Style::ComputedStyle::clone(style());
    inheritStyleFromParent();
    displayContentsParentStyleDidChange(oldStyle, true);
}

void RenderText::displayContentsParentStyleDidChange(const Style::ComputedStyle& oldStyle, bool didChangeStyleSource)
{
    auto diff = Style::difference(oldStyle, style());
    // Inline layout has simpler paths for text that inherits from its parent box.
    bool needsLineLayoutInvalidation = didChangeStyleSource || diff == Style::DifferenceResult::Layout;
    if (needsLineLayoutInvalidation && LayoutIntegration::BoxTreeUpdater::isInLiveFormattingContext(*this)) {
        if (CheckedPtr container = LayoutIntegration::LineLayout::blockContainer(*this))
            container->invalidateLineLayout(RenderBlockFlow::InvalidationReason::ContentChange);
    }
    if (diff == Style::DifferenceResult::Equal)
        return;
    // The text has no parent style change to take care of it.
    repaint();
    if (diff == Style::DifferenceResult::Overflow)
        parent()->setNeedsLayoutForOverflowChange();
    styleDidChange(diff, &oldStyle);
}

void RenderText::layoutBoxContentDidChange()
{
    // Content in a live inline formatting context changes only together with its inline items. LineLayout::updateTextContent
    // updates it, or the line layout goes away and its teardown does.
    if (!parent() || LayoutIntegration::BoxTreeUpdater::isInLiveFormattingContext(*this)) {
        m_layoutBoxContentNeedsUpdate = true;
        return;
    }
    updateLayoutBoxContent();
}

void RenderText::updateLayoutBoxContent()
{
    m_layoutBoxContentNeedsUpdate = false;
    LayoutIntegration::BoxTreeUpdater::updateContent(*this);
}

void RenderText::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    // There is no need to ever schedule repaints from a style change of a text run, since
    // we already did this for the parent of the text run.
    // We do have to schedule layouts, though, since a style change can force us to
    // need to relayout.
    if (diff == Style::DifferenceResult::Layout)
        setNeedsLayoutAndInvalidateContentLogicalWidths();

    const Style::ComputedStyle& newStyle = style();
    if (!oldStyle || oldStyle->fontCascade().useBackslashAsYenSymbol() != newStyle.fontCascade().useBackslashAsYenSymbol())
        m_useBackslashAsYenSymbol = computeUseBackslashAsYenSymbol();

    auto needsRenderedTextUpdateOnly = [&] {
        if (!oldStyle)
            return m_useBackslashAsYenSymbol || !newStyle.textTransform().isNone() || newStyle.textSecurity() != TextSecurity::None;
        if (oldStyle->fontCascade().useBackslashAsYenSymbol() != newStyle.fontCascade().useBackslashAsYenSymbol())
            return true;
        if (oldStyle->textTransform() != newStyle.textTransform())
            return true;
        if (oldStyle->textSecurity() != newStyle.textSecurity())
            return true;
        return !newStyle.textTransform().isNone() && oldStyle->usedLocale() != newStyle.usedLocale();
    };
    if (needsRenderedTextUpdateOnly())
        updateRenderedText();
    else if (!oldStyle) {
        // The box is new, so no inline items refer to its content yet.
        updateLayoutBoxContent();
    } else {
        auto fontChanged = !oldStyle->fontCascadeEqual(newStyle);
        // Measuring depends on the font and white-space collapsing, and on the first-line font, see inheritStyleFromParent(). It
        // follows them even while live content waits for its inline items to change.
        if (fontChanged || oldStyle->collapseWhiteSpace() != newStyle.collapseWhiteSpace())
            LayoutIntegration::BoxTreeUpdater::updateSimplifiedMeasuring(*this);
        // The secure text disc depends on the font.
        if (fontChanged && newStyle.textSecurity() != TextSecurity::None)
            layoutBoxContentDidChange();
        auto combinedTextMayHaveChanged = oldStyle->textCombine() != newStyle.textCombine()
            || (newStyle.textCombine() != TextCombine::None && (fontChanged || oldStyle->writingMode() != newStyle.writingMode()));
        if (combinedTextMayHaveChanged && LayoutIntegration::BoxTreeUpdater::updateCombinedText(*this) && LayoutIntegration::BoxTreeUpdater::isInLiveFormattingContext(*this)) {
            // Combined text is a single inline item.
            if (CheckedPtr container = LayoutIntegration::LineLayout::blockContainer(*this))
                container->invalidateLineLayout(RenderBlockFlow::InvalidationReason::ContentChange);
        }
    }

    // Accessibility reads the non-inherited properties of text from the parent.
    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->onStyleChange(*this, diff, oldStyle, parent()->style());

    setHorizontalWritingMode(newStyle.writingMode().isHorizontal());
}

void RenderText::willBeDestroyed()
{
    if (m_hasSecureTextTimer)
        secureTextTimers().remove(*this);

    if (m_originalTextDiffersFromRendered)
        originalTextMap().remove(this);

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

// This function is similar in spirit to addLineBoxRects, but returns rectangles
// which are annotated with additional state which helps the iPhone draw selections in its unique way.
// Full annotations are added in this class.
void RenderText::collectSelectionGeometries(Vector<SelectionGeometry>& rects, unsigned start, unsigned end)
{
    CheckedPtr lineLayout = LayoutIntegration::LineLayout::containing(*this);
    if (!lineLayout)
        return;
    // Selection extents span the lines of the block that establishes our inline formatting context.
    CheckedRef formattingContextRoot = lineLayout->flow();

    for (auto textBox = InlineIterator::lineLeftmostTextBoxFor(*this); textBox; textBox = textBox.traverseNextTextBox()) {
        LayoutRect rect;
        if (start <= textBox->start() && textBox->end() <= end)
            rect = selectionRectForTextBox(*textBox, start, end);
        else {
            unsigned realEnd = std::min(textBox->end(), end);
            rect = selectionRectForTextBox(*textBox, start, realEnd);
            if (rect.isEmpty())
                continue;
        }

        bool separateLines = false;
        if (textBox->lineBox().isFirstAfterPageBreak()) {
            // The top of the second line in a column aligns with the bottom of the first line in adjacent columns,
            // which can cause the geometries united, extending the highlight box into unselected content.
            // To prevent this, use setSeparateFromPreviousLine for each column to ensure the geometries won't be united.
            separateLines = true;
            if (textBox->isHorizontal())
                rect.shiftYEdgeTo(textBox->lineBox().logicalTop());
            else
                rect.shiftXEdgeTo(textBox->lineBox().logicalTop());
        }

        // Map rect, extended left to leftOffset, and right to rightOffset, through transforms to get minX and maxX.
        LogicalSelectionOffsetCaches cache(formattingContextRoot);
        LayoutUnit leftOffset = formattingContextRoot->logicalLeftSelectionOffset(formattingContextRoot, LayoutUnit(textBox->logicalTop()), cache);
        LayoutUnit rightOffset = formattingContextRoot->logicalRightSelectionOffset(formattingContextRoot, LayoutUnit(textBox->logicalTop()), cache);
        LayoutRect extentsRect = rect;
        if (textBox->isHorizontal()) {
            extentsRect.setX(leftOffset);
            extentsRect.setWidth(rightOffset - leftOffset);
        } else {
            extentsRect.setY(leftOffset);
            extentsRect.setHeight(rightOffset - leftOffset);
        }
        // FIXME: Remove suppression once rdar://189998444 is fixed.
        SUPPRESS_MEMORY_UNSAFE_CAST extentsRect = localToAbsoluteQuad(FloatRect(extentsRect)).enclosingBoundingBox();
        if (!textBox->isHorizontal())
            extentsRect = extentsRect.transposedRect();
        bool isFirstOnLine = !textBox->nextLineLeftwardOnLine();
        bool isLastOnLine = !textBox->nextLineRightwardOnLine();

        bool containsStart = textBox->start() <= start && textBox->end() >= start;
        bool containsEnd = textBox->start() <= end && textBox->end() >= end;

        bool isFixed = false;
        // FIXME: Remove suppression once rdar://189998444 is fixed.
        SUPPRESS_MEMORY_UNSAFE_CAST auto absoluteQuad = localToAbsoluteQuad(FloatRect(rect), MapCoordinatesMode::UseTransforms, &isFixed);
        bool boxIsHorizontal = !textBox->isSVGText() ? textBox->isHorizontal() : !writingMode().isVertical();

        auto selectionGeometry = SelectionGeometry(absoluteQuad, HTMLElement::selectionRenderingBehavior(protect(textNode())), textBox->direction(), extentsRect.x(), extentsRect.maxX(), extentsRect.maxY(), 0, textBox->isLineBreak(), isFirstOnLine, isLastOnLine, containsStart, containsEnd, boxIsHorizontal, isFixed, view().pageNumberForBlockProgressionOffset(absoluteQuad.enclosingBoundingBox().x()));
        selectionGeometry.setSeparateFromPreviousLine(separateLines);
        rects.append(selectionGeometry);
    }
}

static std::optional<IntRect> ellipsisRectForTextBox(const InlineIterator::TextBox& textBox, unsigned start, unsigned end)
{
    auto lineBox = textBox.lineBox();
    if (!lineBox.hasEllipsis())
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

    return IntRect { lineBox.ellipsisVisualRect(InlineIterator::LineBox::AdjustedForSelection::Yes) };
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
        
        quads.append(textRenderer.localToAbsoluteQuad(boundaries, MapCoordinatesMode::UseTransforms, wasFixed));
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

    auto lineRect = run.lineBox().contentLogicalRectAdjustedForAdjacentLineBoxes();
    auto selectionRect = LayoutRect { run.logicalLeftIgnoringInlineDirection(), lineRect.y(), run.logicalWidth(), lineRect.height() };
    return run.fontCascade().characterSelectionRectsForText(run.textRun(), selectionRect, clampedStart, clampedEnd).map([&](auto& characterRect) {
        return snappedSelectionRect(characterRect, run.logicalRightIgnoringInlineDirection(), run.writingMode());
    });
}

Vector<FloatQuad> RenderText::absoluteQuadsForRange(unsigned start, unsigned end, OptionSet<RenderObject::BoundingRectBehavior> behavior, bool* wasFixed) const
{
    bool useSelectionHeight = behavior.contains(RenderObject::BoundingRectBehavior::UseSelectionHeight);
    bool ignoreEmptyTextSelections = behavior.contains(RenderObject::BoundingRectBehavior::IgnoreEmptyTextSelections);
    bool computeIndividualCharacterRects = behavior.contains(RenderObject::BoundingRectBehavior::ComputeIndividualCharacterRects);

    // Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". Text box coordinates are unsigneds, so changing this
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
                    quads.append(localToAbsoluteQuad(localRect, MapCoordinatesMode::UseTransforms, wasFixed));
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
            quads.append(localToAbsoluteQuad(boundaries, MapCoordinatesMode::UseTransforms, wasFixed));
            continue;
        }
        FloatRect rect = localQuadForTextBox(textBox, start, end, useSelectionHeight);
        if (!rect.isZero())
            quads.append(localToAbsoluteQuad(rect, MapCoordinatesMode::UseTransforms, wasFixed));
    }
    return quads;
}

Position RenderText::positionForPoint(const LayoutPoint& point, HitTestSource source)
{
    return positionForPoint(point, source, nullptr).position();
}

enum ShouldAffinityBeDownstream { AlwaysDownstream, AlwaysUpstream, UpstreamIfPositionIsNotAtStart };

static bool lineDirectionPointFitsInBox(int pointLineDirection, const InlineIterator::TextBoxIterator& textRun, ShouldAffinityBeDownstream& shouldAffinityBeDownstream)
{
    shouldAffinityBeDownstream = AlwaysDownstream;

    // the x coordinate is equal to the left edge of this box
    // the affinity must be downstream so the position doesn't jump back to the previous line
    // except when box is the first box in the line
    if (pointLineDirection <= textRun->logicalLeft()) {
        shouldAffinityBeDownstream = !textRun->nextLogicalLeftwardOnLine() ? UpstreamIfPositionIsNotAtStart : AlwaysDownstream;
        return true;
    }

#if !PLATFORM(IOS_FAMILY)
    // and the x coordinate is to the left of the right edge of this box
    // check to see if position goes in this box
    if (pointLineDirection < textRun->logicalRight()) {
        shouldAffinityBeDownstream = UpstreamIfPositionIsNotAtStart;
        return true;
    }
#endif

    // box is first on line
    // and the x coordinate is to the left of the first text box left edge
    if (!textRun->nextLogicalLeftwardOnLineIgnoringLineBreak() && pointLineDirection < textRun->logicalLeft())
        return true;

    if (!textRun->nextLogicalRightwardOnLineIgnoringLineBreak()) {
        // box is last on line
        // and the x coordinate is to the right of the last text box right edge
        // generate VisiblePosition, use Affinity::Upstream affinity if possible
        shouldAffinityBeDownstream = UpstreamIfPositionIsNotAtStart;
        return true;
    }

    return false;
}

static PositionWithAffinity createPositionWithAffinityForBox(const InlineIterator::BoxIterator& run, unsigned offset, ShouldAffinityBeDownstream shouldAffinityBeDownstream)
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
    return run->renderer().createPositionWithAffinity(offset, affinity);
}

static PositionWithAffinity createPositionWithAffinityAfterAdjustingOffsetForBiDi(const InlineIterator::TextBoxIterator& run, unsigned offset, ShouldAffinityBeDownstream shouldAffinityBeDownstream)
{
    if (offset && offset < run->length())
        return createPositionWithAffinityForBox(run, run->start() + offset, shouldAffinityBeDownstream);

    bool positionIsAtStartOfBox = !offset;
    if (positionIsAtStartOfBox == run->isLeftToRightDirection()) {
        // offset is on the left edge

        auto previousRun = run->nextLineLeftwardOnLineIgnoringLineBreak();
        if ((previousRun && previousRun->bidiLevel() == run->bidiLevel())
            || run->formattingContextRoot().writingMode().bidiDirection() == run->direction()) // FIXME: left on 12CBA
            return createPositionWithAffinityForBox(run, run->leftmostCaretOffset(), shouldAffinityBeDownstream);

        if (previousRun && previousRun->bidiLevel() > run->bidiLevel()) {
            // e.g. left of B in aDC12BAb
            auto leftmostRun = previousRun;
            for (; previousRun; previousRun.traverseLineLeftwardOnLineIgnoringLineBreak()) {
                if (previousRun->bidiLevel() <= run->bidiLevel())
                    break;
                leftmostRun = previousRun;
            }
            return createPositionWithAffinityForBox(leftmostRun, leftmostRun->rightmostCaretOffset(), shouldAffinityBeDownstream);
        }

        if (!previousRun || previousRun->bidiLevel() < run->bidiLevel()) {
            // e.g. left of D in aDC12BAb
            InlineIterator::BoxIterator rightmostRun = run;
            for (auto nextRun = run->nextLineRightwardOnLineIgnoringLineBreak(); nextRun; nextRun.traverseLineRightwardOnLineIgnoringLineBreak()) {
                if (nextRun->bidiLevel() < run->bidiLevel())
                    break;
                rightmostRun = nextRun;
            }
            return createPositionWithAffinityForBox(rightmostRun,
                run->isLeftToRightDirection() ? rightmostRun->maximumCaretOffset() : rightmostRun->minimumCaretOffset(), shouldAffinityBeDownstream);
        }

        return createPositionWithAffinityForBox(run, run->rightmostCaretOffset(), shouldAffinityBeDownstream);
    }

    auto nextRun = run->nextLineRightwardOnLineIgnoringLineBreak();
    if ((nextRun && nextRun->bidiLevel() == run->bidiLevel())
        || run->formattingContextRoot().writingMode().bidiDirection() == run->direction())
        return createPositionWithAffinityForBox(run, run->rightmostCaretOffset(), shouldAffinityBeDownstream);

    // offset is on the right edge
    if (nextRun && nextRun->bidiLevel() > run->bidiLevel()) {
        // e.g. right of C in aDC12BAb
        auto rightmostRun = nextRun;
        for (; nextRun; nextRun.traverseLineRightwardOnLineIgnoringLineBreak()) {
            if (nextRun->bidiLevel() <= run->bidiLevel())
                break;
            rightmostRun = nextRun;
        }

        return createPositionWithAffinityForBox(rightmostRun, rightmostRun->leftmostCaretOffset(), shouldAffinityBeDownstream);
    }

    if (!nextRun || nextRun->bidiLevel() < run->bidiLevel()) {
        // e.g. right of A in aDC12BAb
        InlineIterator::BoxIterator leftmostRun = run;
        for (auto previousRun = run->nextLineLeftwardOnLineIgnoringLineBreak(); previousRun; previousRun.traverseLineLeftwardOnLineIgnoringLineBreak()) {
            if (previousRun->bidiLevel() < run->bidiLevel())
                break;
            leftmostRun = previousRun;
        }

        return createPositionWithAffinityForBox(leftmostRun,
            run->isLeftToRightDirection() ? leftmostRun->minimumCaretOffset() : leftmostRun->maximumCaretOffset(), shouldAffinityBeDownstream);
    }

    return createPositionWithAffinityForBox(run, run->leftmostCaretOffset(), shouldAffinityBeDownstream);
}


PositionWithAffinity RenderText::positionForPoint(const LayoutPoint& point, HitTestSource, const RenderFragmentContainer*)
{
    auto firstRun = InlineIterator::lineLeftmostTextBoxFor(*this);

    if (!firstRun || !text().length())
        return createPositionWithAffinity(0, Affinity::Downstream);

    auto logicalPoint = firstRun->isHorizontal()
        ? LayoutPoint { point.x(), point.y() }
        : LayoutPoint { point.y(), point.x() };
    bool blocksAreFlipped = writingMode().isBlockFlipped();

    InlineIterator::TextBoxIterator lastRun;
    for (auto run = firstRun; run; run.traverseNextTextBox()) {
        if (run->isLineBreak() && !run->nextLineLeftwardOnLine() && run->nextLineRightwardOnLine() && !run->nextLineRightwardOnLine()->isLineBreak())
            run.traverseNextTextBox();

        auto lineBox = run->lineBox();
        auto top = LayoutUnit { std::min(previousLineBoxContentBottomOrBorderAndPadding(lineBox), lineBox.contentLogicalTop()) };
        if (logicalPoint.y() > top || (!blocksAreFlipped && logicalPoint.y() == top)) {
            auto bottom = LayoutUnit { lineBox.contentLogicalBottomAdjustedForFollowingLineBox() };
            if (auto nextLineBox = lineBox.next())
                bottom = std::min(bottom, LayoutUnit { nextLineBox->contentLogicalTop() });

            if (logicalPoint.y() < bottom || (blocksAreFlipped && logicalPoint.y() == bottom)) {
                ShouldAffinityBeDownstream shouldAffinityBeDownstream;
#if PLATFORM(IOS_FAMILY)
                if (logicalPoint.x() != run->logicalLeft() && logicalPoint.x() < run->logicalLeft() + run->logicalWidth()) {
                    auto half = LayoutUnit { run->logicalLeft() + run->logicalWidth() / 2.f };
                    shouldAffinityBeDownstream = logicalPoint.x() < half ? AlwaysDownstream : AlwaysUpstream;
                    return createPositionWithAffinityAfterAdjustingOffsetForBiDi(run, offsetForPositionInRun(*run, logicalPoint.x()), shouldAffinityBeDownstream);
                }
#endif
                if (lineDirectionPointFitsInBox(logicalPoint.x(), run, shouldAffinityBeDownstream))
                    return createPositionWithAffinityAfterAdjustingOffsetForBiDi(run, offsetForPositionInRun(*run, logicalPoint.x()), shouldAffinityBeDownstream);
            }
        }
        lastRun = run;
    }

    if (lastRun) {
        ShouldAffinityBeDownstream shouldAffinityBeDownstream;
        lineDirectionPointFitsInBox(logicalPoint.x(), lastRun, shouldAffinityBeDownstream);
        return createPositionWithAffinityAfterAdjustingOffsetForBiDi(lastRun, offsetForPositionInRun(*lastRun, logicalPoint.x()) + lastRun->start(), shouldAffinityBeDownstream);
    }
    return createPositionWithAffinity(0, Affinity::Downstream);
}

template<typename CharacterType> NODELETE static inline bool containsOnlyCollapsibleWhitespace(std::span<const CharacterType> characters, const Style::ComputedStyle& style)
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
template<typename CharacterType> NODELETE static inline bool containsOnlyPossiblyCollapsibleWhitespace(std::span<const CharacterType> characters)
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

    auto markers = markerController->markersFor(protect(*textNode()), type);
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
    auto first = InlineIterator::lineLeftmostTextBoxFor(*this);
    if (!first)
        return { };
    return IntPoint(first->visualRectIgnoringBlockDirection().location());
}

void RenderText::setSelectionState(HighlightState state)
{
    RenderObject::setSelectionState(state);

    // The containing block can be null in case of an orphaned tree.
    RenderBoxModelObject* containingBlock = this->containingBlock();
    if (containingBlock && !containingBlock->isRenderView())
        containingBlock->setSelectionState(state);
}

NODELETE static inline bool isInlineFlowOrEmptyText(const RenderObject& renderer)
{
    if (renderer.isInlineBox())
        return true;
    auto* textRenderer = dynamicDowncast<RenderText>(renderer);
    return textRenderer && textRenderer->text().isEmpty();
}

char32_t RenderText::previousCharacter() const
{
    const RenderObject* previousText = this;
    while ((previousText = previousText->previousInPreOrder())) {
        if (!previousText->isInFlow())
            continue;
        if (!isInlineFlowOrEmptyText(*previousText))
            break;
    }
    auto* renderText = dynamicDowncast<RenderText>(previousText);
    if (!renderText)
        return ' ';
    auto& previousString = renderText->text();
    unsigned length = previousString.length();
    if (!length)
        return ' ';
    return StringView(previousString).codePointBefore(length);
}

static String convertToFullSizeKana(const String& string)
{
    // https://www.w3.org/TR/css-text-3/#small-kana
    static constexpr SortedArrayMap sortedMap { WTF::toArray<std::pair<char32_t, char16_t>>({
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
    }) };

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

// https://w3c.github.io/mathml-core/#math-auto-transform
static String convertToMathAuto(const String& string)
{
#if ENABLE(MATHML)
    StringView view = string;
    if (auto codePoint = view.convertToSingleCodePoint()) {
        char32_t transformedCodePoint = mathVariantMapCodePoint(codePoint.value(), MathVariant::Italic);
        if (transformedCodePoint != codePoint.value())
            return String::fromCodePoint(transformedCodePoint);
    }
#endif
    return string;
}

String applyTextTransform(const Style::ComputedStyle& style, const String& text)
{
    return applyTextTransform(style, text, ' ');
}

String applyTextTransform(const Style::ComputedStyle& style, const String& text, char32_t previousCharacter)
{
    auto transform = style.textTransform();

    if (transform.isNone())
        return text;

    // https://w3c.github.io/csswg-drafts/css-text/#text-transform-order
    auto modified = text;
    if (transform.contains(Style::TextTransformValue::Capitalize))
        modified = capitalize(modified, previousCharacter, Style::toPlatform(style.usedLocale()));
    else if (transform.contains(Style::TextTransformValue::Uppercase))
        modified = modified.convertToUppercaseWithLocale(Style::toPlatform(style.usedLocale()));
    else if (transform.contains(Style::TextTransformValue::Lowercase))
        modified = modified.convertToLowercaseWithLocale(Style::toPlatform(style.usedLocale()));

    if (transform.contains(Style::TextTransformValue::FullWidth))
        modified = transformToFullWidth(modified);

    if (transform.contains(Style::TextTransformValue::FullSizeKana))
        modified = convertToFullSizeKana(modified);

    if (transform.contains(Style::TextTransformValue::MathAuto))
        modified = convertToMathAuto(modified);

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
    if (!style.textTransform().isNone())
        m_text = applyTextTransform(style, m_text, previousCharacter());

    // At rendering time, if certain fonts are used, these characters get swapped out with higher-quality PUA characters.
    // See RenderBlock::updateSecurityDiscCharacters().
    switch (style.textSecurity()) {
    case TextSecurity::None:
        break;
#if !PLATFORM(IOS_FAMILY)
    // We use the same characters here as for list markers.
    // See the listMarkerText function in RenderListOutsideMarker.cpp.
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
    m_fontCodePath = computeFontCodePath(text(), m_containsOnlyASCII);

    if (m_text != originalText) {
        originalTextMap().set(*this, originalText);
        m_originalTextDiffersFromRendered = true;
    } else if (m_originalTextDiffersFromRendered) {
        originalTextMap().remove(*this);
        m_originalTextDiffersFromRendered = false;
    }

    layoutBoxContentDidChange();
}

void RenderText::secureText(char16_t maskingCharacter)
{
    // This hides the text by replacing all the characters with the masking character.
    // Offsets within the hidden text have to match offsets within the original text
    // to handle things like carets and selection, so this won't work right if any
    // of the characters are surrogate pairs or combining marks. Thus, this function
    // does not attempt to handle either of those.

    unsigned length = text().length();
    if (!length)
        return;

    char16_t characterToReveal = 0;
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

    std::span<char16_t> characters;
    m_text = String::createUninitialized(length, characters);

    std::ranges::fill(characters, maskingCharacter);
    if (characterToReveal)
        characters[revealedCharactersOffset] = characterToReveal;
}

static void invalidateLineLayoutPathOnContentChangeIfNeeded(RenderText& renderer, std::optional<size_t> offset, size_t oldLength)
{
    auto* container = LayoutIntegration::LineLayout::blockContainer(renderer);
    if (!container)
        return;

    auto* inlineLayout = container->inlineLayout();
    if (!inlineLayout)
        return;

    if (LayoutIntegration::LineLayout::shouldInvalidateLineLayoutAfterContentChange(*container, renderer, *inlineLayout)) {
        container->invalidateLineLayout(RenderBlockFlow::InvalidationReason::ContentChange);
        return;
    }

    if (!inlineLayout->updateTextContent(renderer, offset, oldLength))
        container->invalidateLineLayout(RenderBlockFlow::InvalidationReason::ContentChange);
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

    updateRenderedText(text);

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferTextChangedIfNeeded(protect(textNode()));
}

void RenderText::updateRenderedText(const String& text)
{
    setRenderedText(text);
    setNeedsLayoutAndInvalidateContentLogicalWidths();
}

void RenderText::updateRenderedText()
{
    updateRenderedText(originalText());
    auto invalidateLineLayoutIfNeeded = [&] {
        CheckedPtr container = LayoutIntegration::LineLayout::blockContainer(*this);
        if (!container || !container->inlineLayout())
            return;
        container->invalidateLineLayout(RenderBlockFlow::InvalidationReason::ContentChange);
    };
    invalidateLineLayoutIfNeeded();

    // Accessibility exposes the rendered text (e.g. the bullets of a password field), so it needs to hear about style driven changes too.
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferTextChangedIfNeeded(protect(textNode()));
}

void RenderText::setText(const String& newContent, bool force)
{
    auto isDifferent = newContent != text();
    setTextInternal(newContent, force);
    if (isDifferent || force)
        invalidateLineLayoutPathOnContentChangeIfNeeded(*this, { }, { });
}

void RenderText::setTextWithOffset(const String& newText, unsigned offset)
{
    if (text() == newText)
        return;

    size_t oldLength = text().length();
    setTextInternal(newText, false);
    invalidateLineLayoutPathOnContentChangeIfNeeded(*this, offset, oldLength);
}

String RenderText::textWithoutConvertingBackslashToYenSymbol() const
{
    if (!m_useBackslashAsYenSymbol || style().textSecurity() != TextSecurity::None)
        return text();

    if (style().textTransform().isNone())
        return originalText();

    return applyTextTransform(style(), originalText(), previousCharacter());
}

IntRect RenderText::linesBoundingBox() const
{
    auto firstTextBox = InlineIterator::lineLeftmostTextBoxFor(*this);
    if (!firstTextBox)
        return { };

    auto boundingBox = firstTextBox->visualRectIgnoringBlockDirection();
    for (auto textBox = firstTextBox; ++textBox;)
        boundingBox.uniteEvenIfEmpty(textBox->visualRectIgnoringBlockDirection());

    return enclosingIntRect(boundingBox);
}

auto RenderText::localRectsForRepaint(RepaintOutlineBounds) const -> RepaintRects
{
    LayoutRect overflowRect = linesBoundingBox();
    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    overflowRect.move(view().frameView().layoutContext().layoutDelta());

    return RepaintRects { overflowRect };
}

LayoutRect RenderText::collectSelectionGeometriesForLineBoxes(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent, Vector<FloatQuad>* quads)
{
    ASSERT(!needsLayout());

    if (selectionState() == HighlightState::None)
        return LayoutRect();
    if (!LayoutIntegration::LineLayout::containing(*this))
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
        rect.unite(selectionRectForTextBox(textBox, startOffset, endOffset, LineExtent::Selection));
        rect.unite(ellipsisRectForTextBox(textBox, startOffset, endOffset).value_or(IntRect { }));
        if (rect.isEmpty())
            continue;

        resultRect.unite(rect);

        if (quads) {
            // FIXME: Remove suppression once rdar://189998444 is fixed.
            SUPPRESS_MEMORY_UNSAFE_CAST quads->append(localToContainerQuad(FloatRect(rect), repaintContainer));
        }
    }

    if (clipToVisibleContent)
        return computeRectForRepaint(resultRect, repaintContainer);
    // FIXME: Remove suppression once rdar://189998444 is fixed.
    SUPPRESS_MEMORY_UNSAFE_CAST return localToContainerQuad(FloatRect(resultRect), repaintContainer).enclosingBoundingBox();
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
    auto first = InlineIterator::lineLeftmostTextBoxFor(*this);
    if (!first)
        return 0;

    int minOffset = first->start();
    for (auto box = first; ++box;)
        minOffset = std::min<int>(minOffset, box->start());

    return minOffset;
}

int RenderText::caretMaxOffset() const
{
    auto first = InlineIterator::lineLeftmostTextBoxFor(*this);
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

} // namespace WebCore
