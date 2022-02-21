/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
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
#include "CharacterProperties.h"
#include "DocumentInlines.h"
#include "DocumentMarkerController.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLParserIdioms.h"
#include "Hyphenation.h"
#include "InlineIteratorLine.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineIteratorTextBox.h"
#include "InlineRunAndOffset.h"
#include "LayoutIntegrationLineLayout.h"
#include "LegacyEllipsisBox.h"
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
#include "Text.h"
#include "TextResourceDecoder.h"
#include "VisiblePosition.h"
#include "WidthIterator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
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

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderText);

struct SameSizeAsRenderText : public RenderObject {
    void* pointers[2];
    uint32_t bitfields : 16;
#if ENABLE(TEXT_AUTOSIZING)
    float candidateTextSize;
#endif
    float widths[4];
    String text;
};

COMPILE_ASSERT(sizeof(RenderText) == sizeof(SameSizeAsRenderText), RenderText_should_stay_small);

class SecureTextTimer final : private TimerBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SecureTextTimer(RenderText&);
    void restart(unsigned offsetAfterLastTypedCharacter);

    unsigned takeOffsetAfterLastTypedCharacter();

private:
    void fired() override;
    RenderText& m_renderer;
    unsigned m_offsetAfterLastTypedCharacter { 0 };
};

typedef HashMap<RenderText*, std::unique_ptr<SecureTextTimer>> SecureTextTimerMap;

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
    ASSERT(secureTextTimers().get(&m_renderer) == this);
    m_offsetAfterLastTypedCharacter = 0;
    m_renderer.setText(m_renderer.text(), true /* forcing setting text as it may be masked later */);
}

static HashMap<const RenderText*, String>& originalTextMap()
{
    static NeverDestroyed<HashMap<const RenderText*, String>> map;
    return map;
}

static HashMap<const RenderText*, WeakPtr<RenderInline>>& inlineWrapperForDisplayContentsMap()
{
    static NeverDestroyed<HashMap<const RenderText*, WeakPtr<RenderInline>>> map;
    return map;
}

static constexpr UChar convertNoBreakSpaceToSpace(UChar character)
{
    return character == noBreakSpace ? ' ' : character;
}

String capitalize(const String& string, UChar previousCharacter)
{
    // FIXME: Change this to use u_strToTitle instead of u_totitle and to consider locale.

    unsigned length = string.length();
    auto& stringImpl = *string.impl();

    static_assert(String::MaxLength < std::numeric_limits<unsigned>::max(), "Must be able to add one without overflowing unsigned");

    // Replace NO BREAK SPACE with a normal spaces since ICU does not treat it as a word separator.
    Vector<UChar> stringWithPrevious(length + 1);
    stringWithPrevious[0] = convertNoBreakSpaceToSpace(previousCharacter);
    for (unsigned i = 1; i < length + 1; i++)
        stringWithPrevious[i] = convertNoBreakSpaceToSpace(stringImpl[i - 1]);

    auto* breakIterator = wordBreakIterator(StringView { stringWithPrevious.data(), length + 1 });
    if (!breakIterator)
        return string;

    StringBuilder result;
    result.reserveCapacity(length);

    int32_t startOfWord = ubrk_first(breakIterator);
    int32_t endOfWord;
    for (endOfWord = ubrk_next(breakIterator); endOfWord != UBRK_DONE; startOfWord = endOfWord, endOfWord = ubrk_next(breakIterator)) {
        if (startOfWord) // Do not append the first character, since it's the previous character, not from this string.
            result.appendCharacter(u_totitle(stringImpl[startOfWord - 1]));
        for (int i = startOfWord + 1; i < endOfWord; i++)
            result.append(stringImpl[i - 1]);
    }

    return result == string ? string : result.toString();
}

inline RenderText::RenderText(Node& node, const String& text)
    : RenderObject(node)
    , m_hasTab(false)
    , m_linesDirty(false)
    , m_needsVisualReordering(false)
    , m_isAllASCII(text.impl()->isAllASCII())
    , m_knownToHaveNoOverflowAndNoFallbackFonts(false)
    , m_useBackslashAsYenSymbol(false)
    , m_originalTextDiffersFromRendered(false)
    , m_hasInlineWrapperForDisplayContents(false)
    , m_text(text)
{
    ASSERT(!m_text.isNull());
    setIsText();
    m_canUseSimpleFontCodePath = computeCanUseSimpleFontCodePath();
}

RenderText::RenderText(Text& textNode, const String& text)
    : RenderText(static_cast<Node&>(textNode), text)
{
}

RenderText::RenderText(Document& document, const String& text)
    : RenderText(static_cast<Node&>(document), text)
{
}

RenderText::~RenderText()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
    ASSERT(!originalTextMap().contains(this));
}

const char* RenderText::renderName() const
{
    return "RenderText";
}

Text* RenderText::textNode() const
{
    return downcast<Text>(RenderObject::node());
}

bool RenderText::isTextFragment() const
{
    return false;
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
    bool needsResetText = false;
    if (!oldStyle) {
        m_useBackslashAsYenSymbol = computeUseBackslashAsYenSymbol();
        needsResetText = m_useBackslashAsYenSymbol;
    } else if (oldStyle->fontCascade().useBackslashAsYenSymbol() != newStyle.fontCascade().useBackslashAsYenSymbol()) {
        m_useBackslashAsYenSymbol = computeUseBackslashAsYenSymbol();
        needsResetText = true;
    }

    if (!oldStyle || oldStyle->fontCascade() != newStyle.fontCascade())
        m_canUseSimplifiedTextMeasuring = computeCanUseSimplifiedTextMeasuring();

    TextTransform oldTransform = oldStyle ? oldStyle->textTransform() : TextTransform::None;
    TextSecurity oldSecurity = oldStyle ? oldStyle->textSecurity() : TextSecurity::None;
    if (needsResetText || oldTransform != newStyle.textTransform() || oldSecurity != newStyle.textSecurity())
        RenderText::setText(originalText(), true);
}

void RenderText::removeAndDestroyTextBoxes()
{
    if (!renderTreeBeingDestroyed())
        m_lineBoxes.removeAllFromParent(*this);
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    else
        m_lineBoxes.invalidateParentChildLists();
#endif
    deleteLineBoxes();
}

void RenderText::willBeDestroyed()
{
    secureTextTimers().remove(this);

    removeAndDestroyTextBoxes();

    if (m_originalTextDiffersFromRendered)
        originalTextMap().remove(this);

    setInlineWrapperForDisplayContents(nullptr);

    RenderObject::willBeDestroyed();
}

String RenderText::originalText() const
{
    return m_originalTextDiffersFromRendered ? originalTextMap().get(this) : m_text;
}

void RenderText::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    for (auto& run : InlineIterator::textBoxesFor(*this)) {
        auto rect = run.rect();
        rects.append(enclosingIntRect(FloatRect(accumulatedOffset + rect.location(), rect.size())));
    }
}

Vector<IntRect> RenderText::absoluteRectsForRange(unsigned start, unsigned end, bool useSelectionHeight, bool* wasFixed) const
{
    return absoluteQuadsForRange(start, end, useSelectionHeight, false /* ignoreEmptyTextSelections */, wasFixed).map([](auto& quad) {
        return quad.enclosingBoundingBox();
    });
}

#if PLATFORM(IOS_FAMILY)
// This function is similar in spirit to addLineBoxRects, but returns rectangles
// which are annotated with additional state which helps the iPhone draw selections in its unique way.
// Full annotations are added in this class.
void RenderText::collectSelectionGeometries(Vector<SelectionGeometry>& rects, unsigned start, unsigned end)
{
    for (auto run = InlineIterator::firstTextBoxFor(*this); run; run = run.traverseNextTextBox()) {
        LayoutRect rect;
        if (start <= run->start() && run->end() <= end)
            rect = run->selectionRect(start, end);
        else {
            unsigned realEnd = std::min(run->end(), end);
            rect = run->selectionRect(start, realEnd);
            if (rect.isEmpty())
                continue;
        }

        if (run->line()->legacyRootInlineBox() && run->line()->legacyRootInlineBox()->isFirstAfterPageBreak()) {
            if (run->isHorizontal())
                rect.shiftYEdgeTo(run->line()->lineBoxTop());
            else
                rect.shiftXEdgeTo(run->line()->lineBoxTop());
        }

        RenderBlock* containingBlock = this->containingBlock();
        // Map rect, extended left to leftOffset, and right to rightOffset, through transforms to get minX and maxX.
        LogicalSelectionOffsetCaches cache(*containingBlock);
        LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, LayoutUnit(run->logicalTop()), cache);
        LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, LayoutUnit(run->logicalTop()), cache);
        LayoutRect extentsRect = rect;
        if (run->isHorizontal()) {
            extentsRect.setX(leftOffset);
            extentsRect.setWidth(rightOffset - leftOffset);
        } else {
            extentsRect.setY(leftOffset);
            extentsRect.setHeight(rightOffset - leftOffset);
        }
        extentsRect = localToAbsoluteQuad(FloatRect(extentsRect)).enclosingBoundingBox();
        if (!run->isHorizontal())
            extentsRect = extentsRect.transposedRect();
        bool isFirstOnLine = !run->previousOnLine();
        bool isLastOnLine = !run->nextOnLine();
        if (containingBlock->isRubyBase() || containingBlock->isRubyText())
            isLastOnLine = !containingBlock->containingBlock()->inlineBoxWrapper()->nextOnLineExists();

        bool containsStart = run->start() <= start && run->end() >= start;
        bool containsEnd = run->start() <= end && run->end() >= end;

        bool isFixed = false;
        auto absoluteQuad = localToAbsoluteQuad(FloatRect(rect), UseTransforms, &isFixed);
        bool boxIsHorizontal = !is<SVGInlineTextBox>(run->legacyInlineBox()) ? run->isHorizontal() : !style().isVerticalWritingMode();
        // If the containing block is an inline element, we want to check the inlineBoxWrapper orientation
        // to determine the orientation of the block. In this case we also use the inlineBoxWrapper to
        // determine if the element is the last on the line.
        if (containingBlock->inlineBoxWrapper()) {
            if (containingBlock->inlineBoxWrapper()->isHorizontal() != boxIsHorizontal) {
                boxIsHorizontal = containingBlock->inlineBoxWrapper()->isHorizontal();
                isLastOnLine = !containingBlock->inlineBoxWrapper()->nextOnLineExists();
            }
        }

        rects.append(SelectionGeometry(absoluteQuad, HTMLElement::selectionRenderingBehavior(textNode()), run->direction(), extentsRect.x(), extentsRect.maxX(), extentsRect.maxY(), 0, run->isLineBreak(), isFirstOnLine, isLastOnLine, containsStart, containsEnd, boxIsHorizontal, isFixed, containingBlock->isRubyText(), view().pageNumberForBlockProgressionOffset(absoluteQuad.enclosingBoundingBox().x())));
    }
}
#endif

static FloatRect boundariesForTextRun(const InlineIterator::TextBox& run)
{
    if (is<SVGInlineTextBox>(run.legacyInlineBox()))
        return downcast<SVGInlineTextBox>(*run.legacyInlineBox()).calculateBoundaries();

    return run.rect();
}

static IntRect ellipsisRectForTextRun(const InlineIterator::TextBox& run, unsigned start, unsigned end)
{
    // FIXME: No ellipsis support in modern path yet.
    if (!run.legacyInlineBox())
        return { };

    auto& box = *run.legacyInlineBox();

    auto truncation = box.truncation();
    if (!truncation)
        return { };

    auto ellipsis = box.root().ellipsisBox();
    if (!ellipsis)
        return { };

    int ellipsisStartPosition = std::max<int>(start - box.start(), 0);
    int ellipsisEndPosition = std::min<int>(end - box.start(), box.len());

    // The ellipsis should be considered to be selected if the end of
    // the selection is past the beginning of the truncation and the
    // beginning of the selection is before or at the beginning of the truncation.
    if (ellipsisEndPosition < *truncation && ellipsisStartPosition > *truncation)
        return { };

    return ellipsis->selectionRect();
}

enum class ClippingOption { NoClipping, ClipToEllipsis };

// FIXME: Unify with absoluteQuadsForRange.
static Vector<FloatQuad> collectAbsoluteQuads(const RenderText& textRenderer, bool* wasFixed, ClippingOption clipping)
{
    Vector<FloatQuad> quads;
    for (auto& run : InlineIterator::textBoxesFor(textRenderer)) {
        auto boundaries = boundariesForTextRun(run);

        // Shorten the width of this text box if it ends in an ellipsis.
        if (clipping == ClippingOption::ClipToEllipsis) {
            auto ellipsisRect = ellipsisRectForTextRun(run, 0, textRenderer.text().length());
            if (!ellipsisRect.isEmpty()) {
                if (textRenderer.style().isHorizontalWritingMode())
                    boundaries.setWidth(ellipsisRect.maxX() - boundaries.x());
                else
                    boundaries.setHeight(ellipsisRect.maxY() - boundaries.y());
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

static FloatRect localQuadForTextRun(const InlineIterator::TextBox& run, unsigned start, unsigned end, bool useSelectionHeight)
{
    unsigned realEnd = std::min(run.end(), end);
    LayoutRect boxSelectionRect = run.selectionRect(start, realEnd);
    if (!boxSelectionRect.height())
        return { };
    if (useSelectionHeight)
        return boxSelectionRect;

    auto rect = run.rect();
    if (run.isHorizontal()) {
        boxSelectionRect.setHeight(rect.height());
        boxSelectionRect.setY(rect.y());
    } else {
        boxSelectionRect.setWidth(rect.width());
        boxSelectionRect.setX(rect.x());
    }
    return boxSelectionRect;
}

Vector<FloatQuad> RenderText::absoluteQuadsForRange(unsigned start, unsigned end, bool useSelectionHeight, bool ignoreEmptyTextSelections, bool* wasFixed) const
{
    // Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". LegacyInlineTextBox coordinates are unsigneds, so changing this
    // function to take ints causes various internal mismatches. But selectionRect takes ints, and
    // passing UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take unsigneds, but
    // that would cause many ripple effects, so for now we'll just clamp our unsigned parameters to INT_MAX.
    ASSERT(end == UINT_MAX || end <= INT_MAX);
    ASSERT(start <= INT_MAX);
    start = std::min(start, static_cast<unsigned>(INT_MAX));
    end = std::min(end, static_cast<unsigned>(INT_MAX));

    Vector<FloatQuad> quads;
    for (auto& run : InlineIterator::textBoxesFor(*this)) {
        if (ignoreEmptyTextSelections && !run.selectableRange().intersects(start, end))
            continue;
        if (start <= run.start() && run.end() <= end) {
            auto boundaries = boundariesForTextRun(run);

            if (useSelectionHeight) {
                LayoutRect selectionRect = run.selectionRect(start, end);
                if (run.isHorizontal()) {
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
        FloatRect rect = localQuadForTextRun(run, start, end, useSelectionHeight);
        if (!rect.isZero())
            quads.append(localToAbsoluteQuad(rect, UseTransforms, wasFixed));
    }
    return quads;
}

Position RenderText::positionForPoint(const LayoutPoint& point)
{
    return positionForPoint(point, nullptr).deepEquivalent();
}

enum ShouldAffinityBeDownstream { AlwaysDownstream, AlwaysUpstream, UpstreamIfPositionIsNotAtStart };

static bool lineDirectionPointFitsInBox(int pointLineDirection, const InlineIterator::TextBoxIterator& textRun, ShouldAffinityBeDownstream& shouldAffinityBeDownstream)
{
    shouldAffinityBeDownstream = AlwaysDownstream;

    // the x coordinate is equal to the left edge of this box
    // the affinity must be downstream so the position doesn't jump back to the previous line
    // except when box is the first box in the line
    if (pointLineDirection <= textRun->logicalLeft()) {
        shouldAffinityBeDownstream = !textRun->previousOnLine() ? UpstreamIfPositionIsNotAtStart : AlwaysDownstream;
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
    if (!textRun->previousOnLineIgnoringLineBreak() && pointLineDirection < textRun->logicalLeft())
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
            || run->renderer().containingBlock()->style().direction() == run->direction()) // FIXME: left on 12CBA
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
        || run->renderer().containingBlock()->style().direction() == run->direction())
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


VisiblePosition RenderText::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer*)
{
    auto firstRun = InlineIterator::firstTextBoxFor(*this);

    if (!firstRun || !text().length())
        return createVisiblePosition(0, Affinity::Downstream);

    LayoutUnit pointLineDirection = firstRun->isHorizontal() ? point.x() : point.y();
    LayoutUnit pointBlockDirection = firstRun->isHorizontal() ? point.y() : point.x();
    bool blocksAreFlipped = style().isFlippedBlocksWritingMode();

    InlineIterator::TextBoxIterator lastRun;
    for (auto run = firstRun; run; run.traverseNextTextBox()) {
        if (run->isLineBreak() && !run->previousOnLine() && run->nextOnLine() && !run->nextOnLine()->isLineBreak())
            run.traverseNextTextBox();

        auto line = run->line();
        LayoutUnit top = std::min(line->selectionTopForHitTesting(), line->top());
        if (pointBlockDirection > top || (!blocksAreFlipped && pointBlockDirection == top)) {
            LayoutUnit bottom = line->selectionBottom();
            if (auto nextLine = line->next())
                bottom = std::min(bottom, nextLine->top());

            if (pointBlockDirection < bottom || (blocksAreFlipped && pointBlockDirection == bottom)) {
                ShouldAffinityBeDownstream shouldAffinityBeDownstream;
#if PLATFORM(IOS_FAMILY)
                if (pointLineDirection != run->logicalLeft() && point.x() < run->rect().x() + run->logicalWidth()) {
                    int half = run->rect().x() + run->logicalWidth() / 2;
                    auto affinity = point.x() < half ? Affinity::Downstream : Affinity::Upstream;
                    return createVisiblePosition(run->offsetForPosition(pointLineDirection) + run->start(), affinity);
                }
#endif
                if (lineDirectionPointFitsInBox(pointLineDirection, run, shouldAffinityBeDownstream))
                    return createVisiblePositionAfterAdjustingOffsetForBiDi(run, run->offsetForPosition(pointLineDirection), shouldAffinityBeDownstream);
            }
        }
        lastRun = run;
    }

    if (lastRun) {
        ShouldAffinityBeDownstream shouldAffinityBeDownstream;
        lineDirectionPointFitsInBox(pointLineDirection, lastRun, shouldAffinityBeDownstream);
        return createVisiblePositionAfterAdjustingOffsetForBiDi(lastRun, lastRun->offsetForPosition(pointLineDirection) + lastRun->start(), shouldAffinityBeDownstream);
    }
    return createVisiblePosition(0, Affinity::Downstream);
}

ALWAYS_INLINE float RenderText::widthFromCache(const FontCascade& f, unsigned start, unsigned len, float xPos, HashSet<const Font*>* fallbackFonts, GlyphOverflow* glyphOverflow, const RenderStyle& style) const
{
    if (style.hasTextCombine() && is<RenderCombineText>(*this)) {
        const RenderCombineText& combineText = downcast<RenderCombineText>(*this);
        if (combineText.isCombined())
            return combineText.combinedTextWidth(f);
    }

    TextRun run = RenderBlock::constructTextRun(*this, start, len, style);
    run.setCharacterScanForCodePath(!canUseSimpleFontCodePath());
    run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
    run.setXPos(xPos);
    return f.width(run, fallbackFonts, glyphOverflow);
}

ALWAYS_INLINE float RenderText::widthFromCacheConsideringPossibleTrailingSpace(const RenderStyle& style, const FontCascade& font, unsigned startIndex, unsigned wordLen, float xPos, bool currentCharacterIsSpace, WordTrailingSpace& wordTrailingSpace, HashSet<const Font*>& fallbackFonts, GlyphOverflow& glyphOverflow) const
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

    if (m_hasTab || preferredLogicalWidthsDirty())
        computePreferredLogicalWidths(leadWidth);

    Widths widths;

    widths.beginWS = !stripFrontSpaces && m_hasBeginWS;
    widths.endWS = m_hasEndWS;

    unsigned length = this->length();

    if (!length || (stripFrontSpaces && text().isAllSpecialCharacters<isHTMLSpace>()))
        return widths;

    widths.min = m_minWidth;
    widths.max = m_maxWidth;

    widths.beginMin = m_beginMinWidth;
    widths.endMin = m_endMinWidth;

    widths.hasBreakableChar = m_hasBreakableChar;
    widths.hasBreak = m_hasBreak;
    widths.endsWithBreak = m_hasBreak && text()[length - 1] == '\n';

    if (text()[0] == ' ' || (text()[0] == '\n' && !style.preserveNewline()) || text()[0] == '\t') {
        auto& font = style.fontCascade(); // FIXME: This ignores first-line.
        if (stripFrontSpaces)
            widths.max -= font.width(RenderBlock::constructTextRun(&space, 1, style));
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
    if (preferredLogicalWidthsDirty())
        const_cast<RenderText*>(this)->computePreferredLogicalWidths(0);
        
    return m_minWidth;
}

float RenderText::maxLogicalWidth() const
{
    if (preferredLogicalWidthsDirty())
        const_cast<RenderText*>(this)->computePreferredLogicalWidths(0);
        
    return m_maxWidth;
}

LineBreakIteratorMode mapLineBreakToIteratorMode(LineBreak lineBreak)
{
    switch (lineBreak) {
    case LineBreak::Auto:
    case LineBreak::AfterWhiteSpace:
    case LineBreak::Anywhere:
        return LineBreakIteratorMode::Default;
    case LineBreak::Loose:
        return LineBreakIteratorMode::Loose;
    case LineBreak::Normal:
        return LineBreakIteratorMode::Normal;
    case LineBreak::Strict:
        return LineBreakIteratorMode::Strict;
    }
    ASSERT_NOT_REACHED();
    return LineBreakIteratorMode::Default;
}

void RenderText::computePreferredLogicalWidths(float leadWidth)
{
    HashSet<const Font*> fallbackFonts;
    GlyphOverflow glyphOverflow;
    computePreferredLogicalWidths(leadWidth, fallbackFonts, glyphOverflow);
    if (fallbackFonts.isEmpty() && !glyphOverflow.left && !glyphOverflow.right && !glyphOverflow.top && !glyphOverflow.bottom)
        m_knownToHaveNoOverflowAndNoFallbackFonts = true;
}

static inline float hyphenWidth(RenderText& renderer, const FontCascade& font)
{
    const RenderStyle& style = renderer.style();
    auto textRun = RenderBlock::constructTextRun(style.hyphenString().string(), style);
    return font.width(textRun);
}

float RenderText::maxWordFragmentWidth(const RenderStyle& style, const FontCascade& font, StringView word, unsigned minimumPrefixLength, unsigned minimumSuffixLength, bool currentCharacterIsSpace, unsigned characterIndex, float xPos, float entireWordWidth, WordTrailingSpace& wordTrailingSpace, HashSet<const Font*>& fallbackFonts, GlyphOverflow& glyphOverflow)
{
    unsigned suffixStart = 0;
    if (word.length() <= minimumSuffixLength)
        return 0;

    Vector<int, 8> hyphenLocations;
    ASSERT(word.length() >= minimumSuffixLength);
    unsigned hyphenLocation = word.length() - minimumSuffixLength;
    while ((hyphenLocation = lastHyphenLocation(word, hyphenLocation, style.computedLocale())) >= std::max(minimumPrefixLength, 1U))
        hyphenLocations.append(hyphenLocation);

    if (hyphenLocations.isEmpty())
        return 0;

    hyphenLocations.reverse();

    // Consider the word "ABC-DEF-GHI" (where the '-' characters are hyphenation opportunities). We want to measure the width
    // of "ABC-" and "DEF-", but not "GHI-". Instead, we should measure "GHI" the same way we measure regular unhyphenated
    // words (by using wordTrailingSpace). Therefore, this function is split up into two parts - one that measures each prefix,
    // and one that measures the single last suffix.

    // FIXME: Breaking the string at these places in the middle of words doesn't work with complex text.
    float minimumFragmentWidthToConsider = font.pixelSize() * 5 / 4 + hyphenWidth(*this, font);
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

void RenderText::computePreferredLogicalWidths(float leadWidth, HashSet<const Font*>& fallbackFonts, GlyphOverflow& glyphOverflow)
{
    ASSERT(m_hasTab || preferredLogicalWidthsDirty() || !m_knownToHaveNoOverflowAndNoFallbackFonts);

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
    LazyLineBreakIterator breakIterator(string, style.computedLocale(), iteratorMode);
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
    bool canUseLineBreakShortcut = iteratorMode == LineBreakIteratorMode::Default;

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

        bool hasBreak = breakAll || isBreakable(breakIterator, i, nextBreakable, breakNBSP, canUseLineBreakShortcut, keepAllWords, breakAnywhere);
        bool betweenWords = true;
        unsigned j = i;
        while (c != '\n' && !isSpaceAccordingToStyle(c, style) && c != '\t' && (c != softHyphen || style.hyphens() == Hyphens::None)) {
            UChar previousCharacter = c;
            j++;
            if (j == length)
                break;
            c = string[j];
            if (U_IS_LEAD(previousCharacter) && U_IS_TRAIL(c))
                continue;
            if (isBreakable(breakIterator, j, nextBreakable, breakNBSP, canUseLineBreakShortcut, keepAllWords, breakAnywhere) && characterAt(j - 1) != softHyphen)
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
            if ((isSpace || isCollapsibleWhiteSpace) && !containsOnlyHTMLWhitespace(j, length - j))
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

            m_minWidth = std::max(currMinWidth, m_minWidth);

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

                if (currMaxWidth > m_maxWidth)
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

    m_maxWidth = std::max(currMaxWidth, m_maxWidth);

    if (!style.autoWrap())
        m_minWidth = m_maxWidth;

    if (style.whiteSpace() == WhiteSpace::Pre) {
        if (firstLine)
            m_beginMinWidth = m_maxWidth;
        m_endMinWidth = currMaxWidth;
    }

    setPreferredLogicalWidthsDirty(false);
}

template<typename CharacterType> static inline bool isAllCollapsibleWhitespace(const CharacterType* characters, unsigned length, const RenderStyle& style)
{
    for (unsigned i = 0; i < length; ++i) {
        if (!style.isCollapsibleWhiteSpace(characters[i]))
            return false;
    }
    return true;
}

bool RenderText::isAllCollapsibleWhitespace() const
{
    if (text().is8Bit())
        return WebCore::isAllCollapsibleWhitespace(text().characters8(), text().length(), style());
    return WebCore::isAllCollapsibleWhitespace(text().characters16(), text().length(), style());
}

template<typename CharacterType> static inline bool isAllPossiblyCollapsibleWhitespace(const CharacterType* characters, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (!(characters[i] == '\n' || characters[i] == ' ' || characters[i] == '\t'))
            return false;
    }
    return true;
}

bool RenderText::containsOnlyHTMLWhitespace(unsigned from, unsigned length) const
{
    ASSERT(from <= text().length());
    ASSERT(length <= text().length());
    ASSERT(from + length <= text().length());
    if (text().is8Bit())
        return isAllPossiblyCollapsibleWhitespace(text().characters8() + from, length);
    return isAllPossiblyCollapsibleWhitespace(text().characters16() + from, length);
}

Vector<std::pair<unsigned, unsigned>> RenderText::draggedContentRangesBetweenOffsets(unsigned startOffset, unsigned endOffset) const
{
    if (!textNode())
        return { };

    auto markers = document().markers().markersFor(*textNode(), DocumentMarker::DraggedContent);
    if (markers.isEmpty())
        return { };

    Vector<std::pair<unsigned, unsigned>> draggedContentRanges;
    for (auto* marker : markers) {
        unsigned markerStart = std::max(marker->startOffset(), startOffset);
        unsigned markerEnd = std::min(marker->endOffset(), endOffset);
        if (markerStart >= markerEnd || markerStart > endOffset || markerEnd < startOffset)
            continue;

        std::pair<unsigned, unsigned> draggedContentRange;
        draggedContentRange.first = markerStart;
        draggedContentRange.second = markerEnd;
        draggedContentRanges.append(draggedContentRange);
    }
    return draggedContentRanges;
}

IntPoint RenderText::firstRunLocation() const
{
    auto first = InlineIterator::firstTextBoxFor(*this);
    if (!first)
        return { };
    return IntPoint(first->rect().location());
}

void RenderText::setSelectionState(HighlightState state)
{
    RenderObject::setSelectionState(state);

    // The containing block can be null in case of an orphaned tree.
    RenderBlock* containingBlock = this->containingBlock();
    if (containingBlock && !containingBlock->isRenderView())
        containingBlock->setSelectionState(state);
}

void RenderText::setTextWithOffset(const String& newText, unsigned offset, unsigned length, bool force)
{
    if (!force && text() == newText)
        return;

    int delta = newText.length() - text().length();
    unsigned end = offset + length;

    m_linesDirty = m_lineBoxes.dirtyRange(*this, offset, end, delta);

    setText(newText, force || m_linesDirty);
}

static inline bool isInlineFlowOrEmptyText(const RenderObject& renderer)
{
    return is<RenderInline>(renderer) || (is<RenderText>(renderer) && downcast<RenderText>(renderer).text().isEmpty());
}

UChar RenderText::previousCharacter() const
{
    // find previous text renderer if one exists
    const RenderObject* previousText = this;
    while ((previousText = previousText->previousInPreOrder())) {
        if (!isInlineFlowOrEmptyText(*previousText))
            break;
    }
    if (!is<RenderText>(previousText))
        return ' ';
    auto& previousString = downcast<RenderText>(*previousText).text();
    return previousString[previousString.length() - 1];
}

String applyTextTransform(const RenderStyle& style, const String& text, UChar previousCharacter)
{
    switch (style.textTransform()) {
    case TextTransform::None:
        return text;
    case TextTransform::Capitalize:
        return capitalize(text, previousCharacter); // FIXME: Need to take locale into account.
    case TextTransform::Uppercase:
        return text.convertToUppercaseWithLocale(style.computedLocale());
    case TextTransform::Lowercase:
        return text.convertToLowercaseWithLocale(style.computedLocale());
    }
    ASSERT_NOT_REACHED();
    return text;
}

void RenderText::setRenderedText(const String& newText)
{
    ASSERT(!newText.isNull());

    String originalText = this->originalText();

    m_text = newText;

    if (m_useBackslashAsYenSymbol)
        m_text.replace('\\', yenSign);
    
    const auto& style = this->style();
    if (style.textTransform() != TextTransform::None)
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

    m_isAllASCII = text().isAllASCII();
    m_canUseSimpleFontCodePath = computeCanUseSimpleFontCodePath();
    m_canUseSimplifiedTextMeasuring = computeCanUseSimplifiedTextMeasuring();
    
    if (m_text != originalText) {
        originalTextMap().set(this, originalText);
        m_originalTextDiffersFromRendered = true;
    } else if (m_originalTextDiffersFromRendered) {
        originalTextMap().remove(this);
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

    if (SecureTextTimer* timer = secureTextTimers().get(this)) {
        // We take the offset out of the timer to make this one-shot. We count on this being called only once.
        // If it's called a second time we assume the text is different and a character should not be revealed.
        revealedCharactersOffset = timer->takeOffsetAfterLastTypedCharacter();
        if (revealedCharactersOffset && revealedCharactersOffset <= length)
            characterToReveal = text()[--revealedCharactersOffset];
    }

    UChar* characters;
    m_text = String::createUninitialized(length, characters);

    for (unsigned i = 0; i < length; ++i)
        characters[i] = maskingCharacter;
    if (characterToReveal)
        characters[revealedCharactersOffset] = characterToReveal;
}

bool RenderText::computeCanUseSimplifiedTextMeasuring() const
{
    if (!m_canUseSimpleFontCodePath)
        return false;
    
    // FIXME: All these checks should be more fine-grained at the inline item level.
    auto& style = this->style();
    auto& fontCascade = style.fontCascade();
    if (fontCascade.wordSpacing() || fontCascade.letterSpacing())
        return false;

    // Additional check on the font codepath.
    TextRun run(m_text);
    run.setCharacterScanForCodePath(false);
    if (fontCascade.codePath(run) != FontCascade::CodePath::Simple)
        return false;

    auto& primaryFont = fontCascade.primaryFont();
    if (primaryFont.syntheticBoldOffset())
        return false;

    auto whitespaceIsCollapsed = style.collapseWhiteSpace();
    for (unsigned i = 0; i < text().length(); ++i) {
        auto character = text()[i];
        if (!WidthIterator::characterCanUseSimplifiedTextMeasuring(character, whitespaceIsCollapsed))
            return false;
        auto glyphData = fontCascade.glyphDataForCharacter(character, false);
        if (!glyphData.isValid() || glyphData.font != &primaryFont)
            return false;
    }
    return true;
}

void RenderText::setText(const String& text, bool force)
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* container = LayoutIntegration::LineLayout::blockContainer(*this))
        container->invalidateLineLayoutPath();
#endif

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferTextChangedIfNeeded(textNode());
}

String RenderText::textWithoutConvertingBackslashToYenSymbol() const
{
    if (!m_useBackslashAsYenSymbol || style().textSecurity() != TextSecurity::None)
        return text();

    if (style().textTransform() == TextTransform::None)
        return originalText();

    return applyTextTransform(style(), originalText(), previousCharacter());
}

void RenderText::dirtyLineBoxes(bool fullLayout)
{
    if (fullLayout)
        deleteLineBoxes();
    else if (!m_linesDirty)
        m_lineBoxes.dirtyAll();
    m_linesDirty = false;
}

void RenderText::deleteLineBoxes()
{
    m_lineBoxes.deleteAll();
}

std::unique_ptr<LegacyInlineTextBox> RenderText::createTextBox()
{
    return makeUnique<LegacyInlineTextBox>(*this);
}

void RenderText::positionLineBox(LegacyInlineTextBox& textBox)
{
    if (!textBox.hasTextContent())
        return;
    m_needsVisualReordering |= !textBox.isLeftToRightDirection();
}

bool RenderText::usesLegacyLineLayoutPath() const
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    return !LayoutIntegration::LineLayout::containing(*this);
#else
    return true;
#endif
}

float RenderText::width(unsigned from, unsigned len, float xPos, bool firstLine, HashSet<const Font*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    if (from >= text().length())
        return 0;

    if (from + len > text().length())
        len = text().length() - from;

    const RenderStyle& lineStyle = firstLine ? firstLineStyle() : style();
    return width(from, len, lineStyle.fontCascade(), xPos, fallbackFonts, glyphOverflow);
}

float RenderText::width(unsigned from, unsigned len, const FontCascade& f, float xPos, HashSet<const Font*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    ASSERT(from + len <= text().length());
    if (!text().length())
        return 0;

    const RenderStyle& style = this->style();
    float w;
    if (&f == &style.fontCascade()) {
        if (!style.preserveNewline() && !from && len == text().length() && (!glyphOverflow || !glyphOverflow->computeBounds)) {
            if (fallbackFonts) {
                ASSERT(glyphOverflow);
                if (preferredLogicalWidthsDirty() || !m_knownToHaveNoOverflowAndNoFallbackFonts) {
                    const_cast<RenderText*>(this)->computePreferredLogicalWidths(0, *fallbackFonts, *glyphOverflow);
                    if (fallbackFonts->isEmpty() && !glyphOverflow->left && !glyphOverflow->right && !glyphOverflow->top && !glyphOverflow->bottom)
                        m_knownToHaveNoOverflowAndNoFallbackFonts = true;
                }
                w = m_maxWidth;
            } else
                w = maxLogicalWidth();
        } else
            w = widthFromCache(f, from, len, xPos, fallbackFonts, glyphOverflow, style);
    } else {
        TextRun run = RenderBlock::constructTextRun(*this, from, len, style);
        run.setCharacterScanForCodePath(!canUseSimpleFontCodePath());
        run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        run.setXPos(xPos);

        w = f.width(run, fallbackFonts, glyphOverflow);
    }

    return clampTo(w, 0.f);
}

IntRect RenderText::linesBoundingBox() const
{
    auto first = InlineIterator::firstTextBoxFor(*this);
    if (!first)
        return { };

    auto boundingBox = first->rect();
    for (auto box = first; ++box;)
        boundingBox.uniteEvenIfEmpty(box->rect());

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

    for (auto& run : InlineIterator::textBoxesFor(*this)) {
        LayoutRect rect;
        rect.unite(run.selectionRect(startOffset, endOffset));
        rect.unite(ellipsisRectForTextRun(run, startOffset, endOffset));
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
    if (m_isAllASCII || text().is8Bit())
        return current - 1;

    CachedTextBreakIterator iterator(text(), TextBreakIterator::Mode::Caret, nullAtom());
    return iterator.preceding(current).value_or(current - 1);
}

int RenderText::previousOffsetForBackwardDeletion(int current) const
{
    CachedTextBreakIterator iterator(text(), TextBreakIterator::Mode::Delete, nullAtom());
    return iterator.preceding(current).value_or(0);
}

int RenderText::nextOffset(int current) const
{
    if (m_isAllASCII || text().is8Bit())
        return current + 1;

    CachedTextBreakIterator iterator(text(), TextBreakIterator::Mode::Caret, nullAtom());
    return iterator.following(current).value_or(current + 1);
}

bool RenderText::computeCanUseSimpleFontCodePath() const
{
    if (m_isAllASCII || text().is8Bit())
        return true;
    return FontCascade::characterRangeCodePath(text().characters16(), length()) == FontCascade::CodePath::Simple;
}

void RenderText::momentarilyRevealLastTypedCharacter(unsigned offsetAfterLastTypedCharacter)
{
    if (style().textSecurity() == TextSecurity::None)
        return;
    auto& secureTextTimer = secureTextTimers().add(this, nullptr).iterator->value;
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
    if (text().is8Bit())
        return { text().characters8() + start, destination - start };
    return { text().characters16() + start, destination - start };
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
        inlineWrapperForDisplayContentsMap().remove(this);
        m_hasInlineWrapperForDisplayContents = false;
        return;
    }
    inlineWrapperForDisplayContentsMap().add(this, wrapper);
    m_hasInlineWrapperForDisplayContents = true;
}

RenderText* RenderText::findByDisplayContentsInlineWrapperCandidate(RenderElement& renderer)
{
    auto* firstChild = renderer.firstChild();
    if (!is<RenderText>(firstChild))
        return nullptr;
    auto& textRenderer = downcast<RenderText>(*firstChild);
    if (textRenderer.inlineWrapperForDisplayContents() != &renderer)
        return nullptr;
    ASSERT(textRenderer.textNode());
    ASSERT(renderer.firstChild() == renderer.lastChild());
    return &textRenderer;

}

} // namespace WebCore
