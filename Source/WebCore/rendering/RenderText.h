/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
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

#pragma once

#include "RenderElement.h"
#include "RenderTextLineBoxes.h"
#include "Text.h"
#include <wtf/Forward.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

class Font;
class LegacyInlineTextBox;
struct GlyphOverflow;
struct WordTrailingSpace;

namespace LayoutIntegration {
class LineLayout;
}

class RenderText : public RenderObject {
    WTF_MAKE_ISO_ALLOCATED(RenderText);
public:
    RenderText(Text&, const String&);
    RenderText(Document&, const String&);

    virtual ~RenderText();

    WEBCORE_EXPORT Text* textNode() const;

    virtual bool isTextFragment() const;

    const RenderStyle& style() const;
    const RenderStyle& firstLineStyle() const;
    const RenderStyle* getCachedPseudoStyle(PseudoId, const RenderStyle* parentStyle = nullptr) const;

    Color selectionBackgroundColor() const;
    Color selectionForegroundColor() const;
    Color selectionEmphasisMarkColor() const;
    std::unique_ptr<RenderStyle> selectionPseudoStyle() const;

    virtual String originalText() const;

    void extractTextBox(LegacyInlineTextBox& box) { m_lineBoxes.extract(box); }
    void attachTextBox(LegacyInlineTextBox& box) { m_lineBoxes.attach(box); }
    void removeTextBox(LegacyInlineTextBox& box) { m_lineBoxes.remove(box); }

    StringImpl& text() const { return *m_text.impl(); } // Since m_text can never be null, returning this type means callers won't null check.
    String textWithoutConvertingBackslashToYenSymbol() const;

    LegacyInlineTextBox* createInlineTextBox() { return m_lineBoxes.createAndAppendLineBox(*this); }
    void dirtyLineBoxes(bool fullLayout);
    void deleteLineBoxes();

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const final;
    Vector<IntRect> absoluteRectsForRange(unsigned startOffset = 0, unsigned endOffset = UINT_MAX, bool useSelectionHeight = false, bool* wasFixed = nullptr) const;
#if PLATFORM(IOS_FAMILY)
    void collectSelectionGeometries(Vector<SelectionGeometry>&, unsigned startOffset = 0, unsigned endOffset = std::numeric_limits<unsigned>::max()) final;
#endif

    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const final;
    Vector<FloatQuad> absoluteQuadsForRange(unsigned startOffset = 0, unsigned endOffset = UINT_MAX, bool useSelectionHeight = false, bool ignoreEmptyTextSelections = false, bool* wasFixed = nullptr) const;

    Vector<FloatQuad> absoluteQuadsClippedToEllipsis() const;

    Position positionForPoint(const LayoutPoint&) final;

    UChar characterAt(unsigned) const;
    unsigned length() const final { return text().length(); }

    void positionLineBox(LegacyInlineTextBox&);

    float width(unsigned from, unsigned length, const FontCascade&, float xPos, HashSet<const Font*>* fallbackFonts = nullptr, GlyphOverflow* = nullptr) const;
    float width(unsigned from, unsigned length, float xPos, bool firstLine = false, HashSet<const Font*>* fallbackFonts = nullptr, GlyphOverflow* = nullptr) const;

    float minLogicalWidth() const;
    float maxLogicalWidth() const;

    struct Widths {
        float min { 0 };
        float max { 0 };
        float beginMin { 0 };
        float endMin { 0 };
        float beginMax { 0 };
        float endMax { 0 };
        bool beginWS { false };
        bool endWS { false };
        bool hasBreakableChar { false };
        bool hasBreak { false };
        bool endsWithBreak { false };
    };
    Widths trimmedPreferredWidths(float leadWidth, bool& stripFrontSpaces);

    float hangablePunctuationStartWidth(unsigned index) const;
    float hangablePunctuationEndWidth(unsigned index) const;
    unsigned firstCharacterIndexStrippingSpaces() const;
    unsigned lastCharacterIndexStrippingSpaces() const;
    static bool isHangableStopOrComma(UChar);
    
    WEBCORE_EXPORT virtual IntRect linesBoundingBox() const;
    WEBCORE_EXPORT IntPoint firstRunLocation() const;

    virtual void setText(const String&, bool force = false);
    void setTextWithOffset(const String&, unsigned offset, unsigned len, bool force = false);

    bool canBeSelectionLeaf() const override { return true; }

    LayoutRect collectSelectionGeometriesForLineBoxes(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent, Vector<FloatQuad>&);

    LayoutUnit marginLeft() const { return minimumValueForLength(style().marginLeft(), 0); }
    LayoutUnit marginRight() const { return minimumValueForLength(style().marginRight(), 0); }

    LegacyInlineTextBox* firstTextBox() const { return m_lineBoxes.first(); }
    LegacyInlineTextBox* lastTextBox() const { return m_lineBoxes.last(); }

    int caretMinOffset() const final;
    int caretMaxOffset() const final;
    unsigned countRenderedCharacterOffsetsUntil(unsigned) const;
    bool containsRenderedCharacterOffset(unsigned) const;
    bool containsCaretOffset(unsigned) const;
    bool hasRenderedText() const;

    // FIXME: These should return unsigneds.
    int previousOffset(int current) const final;
    int previousOffsetForBackwardDeletion(int current) const final;
    int nextOffset(int current) const final;

    bool needsVisualReordering() const { return m_needsVisualReordering; }
    void setNeedsVisualReordering() { m_needsVisualReordering = true; }

    void momentarilyRevealLastTypedCharacter(unsigned offsetAfterLastTypedCharacter);

    LegacyInlineTextBox* findNextInlineTextBox(int offset, int& pos) const { return m_lineBoxes.findNext(offset, pos); }

    bool isAllCollapsibleWhitespace() const;

    bool canUseSimpleFontCodePath() const { return m_canUseSimpleFontCodePath; }

    void removeAndDestroyTextBoxes();

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual std::unique_ptr<LegacyInlineTextBox> createTextBox();

#if ENABLE(TEXT_AUTOSIZING)
    float candidateComputedTextSize() const { return m_candidateComputedTextSize; }
    void setCandidateComputedTextSize(float size) { m_candidateComputedTextSize = size; }
#endif

    bool usesLegacyLineLayoutPath() const;

    StringView stringView(unsigned start = 0, std::optional<unsigned> stop = std::nullopt) const;
    
    bool containsOnlyHTMLWhitespace(unsigned from, unsigned length) const;
    
    bool canUseSimplifiedTextMeasuring() const { return m_canUseSimplifiedTextMeasuring; }

    Vector<std::pair<unsigned, unsigned>> draggedContentRangesBetweenOffsets(unsigned startOffset, unsigned endOffset) const;

    RenderInline* inlineWrapperForDisplayContents();
    void setInlineWrapperForDisplayContents(RenderInline*);

    template <typename MeasureTextCallback>
    static float measureTextConsideringPossibleTrailingSpace(bool currentCharacterIsSpace, unsigned startIndex, unsigned wordLength, WordTrailingSpace&, HashSet<const Font*>& fallbackFonts, MeasureTextCallback&&);

    static std::optional<bool> emphasisMarkExistsAndIsAbove(const RenderText&, const RenderStyle&);

protected:
    virtual void computePreferredLogicalWidths(float leadWidth);
    void willBeDestroyed() override;

    virtual void setRenderedText(const String&);
    virtual UChar previousCharacter() const;

    RenderTextLineBoxes m_lineBoxes;

private:
    RenderText(Node&, const String&);

    ASCIILiteral renderName() const override;

    bool canHaveChildren() const final { return false; }

    VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*) override;

    void setSelectionState(HighlightState) final;
    LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent = true) final;
    LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const final;

    void computePreferredLogicalWidths(float leadWidth, HashSet<const Font*>& fallbackFonts, GlyphOverflow&);

    bool computeCanUseSimpleFontCodePath() const;
    
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction) final { ASSERT_NOT_REACHED(); return false; }

    float widthFromCache(const FontCascade&, unsigned start, unsigned len, float xPos, HashSet<const Font*>* fallbackFonts, GlyphOverflow*, const RenderStyle&) const;
    bool computeUseBackslashAsYenSymbol() const;

    void secureText(UChar mask);

    LayoutRect collectSelectionGeometriesForLineBoxes(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent, Vector<FloatQuad>*);
    bool computeCanUseSimplifiedTextMeasuring() const;

    void node() const = delete;
    void container() const = delete; // Use parent() instead.
    void container(const RenderLayerModelObject&, bool&) const = delete; // Use parent() instead.

    float maxWordFragmentWidth(const RenderStyle&, const FontCascade&, StringView word, unsigned minimumPrefixLength, unsigned minimumSuffixLength, bool currentCharacterIsSpace, unsigned characterIndex, float xPos, float entireWordWidth, WordTrailingSpace&, HashSet<const Font*>& fallbackFonts, GlyphOverflow&);
    float widthFromCacheConsideringPossibleTrailingSpace(const RenderStyle&, const FontCascade&, unsigned startIndex, unsigned wordLen, float xPos, bool currentCharacterIsSpace, WordTrailingSpace&, HashSet<const Font*>& fallbackFonts, GlyphOverflow&) const;

    // We put the bitfield first to minimize padding on 64-bit.
    unsigned m_hasBreakableChar : 1; // Whether or not we can be broken into multiple lines.
    unsigned m_hasBreak : 1; // Whether or not we have a hard break (e.g., <pre> with '\n').
    unsigned m_hasTab : 1; // Whether or not we have a variable width tab character (e.g., <pre> with '\t').
    unsigned m_hasBeginWS : 1; // Whether or not we begin with WS (only true if we aren't pre)
    unsigned m_hasEndWS : 1; // Whether or not we end with WS (only true if we aren't pre)
    unsigned m_linesDirty : 1; // This bit indicates that the text run has already dirtied specific
                           // line boxes, and this hint will enable layoutInlineChildren to avoid
                           // just dirtying everything when character data is modified (e.g., appended/inserted
                           // or removed).
    unsigned m_needsVisualReordering : 1;
    unsigned m_isAllASCII : 1;
    unsigned m_canUseSimpleFontCodePath : 1;
    mutable unsigned m_knownToHaveNoOverflowAndNoFallbackFonts : 1;
    unsigned m_useBackslashAsYenSymbol : 1;
    unsigned m_originalTextDiffersFromRendered : 1;
    unsigned m_hasInlineWrapperForDisplayContents : 1;
    unsigned m_canUseSimplifiedTextMeasuring : 1;

#if ENABLE(TEXT_AUTOSIZING)
    // FIXME: This should probably be part of the text sizing structures in Document instead. That would save some memory.
    float m_candidateComputedTextSize { 0 };
#endif
    std::optional<float> m_minWidth;
    std::optional<float> m_maxWidth;
    float m_beginMinWidth { 0 };
    float m_endMinWidth { 0 };

    String m_text;
};

String applyTextTransform(const RenderStyle&, const String&, UChar previousCharacter);
String capitalize(const String&, UChar previousCharacter);
LineBreakIteratorMode mapLineBreakToIteratorMode(LineBreak);

inline UChar RenderText::characterAt(unsigned i) const
{
    return i >= length() ? 0 : text()[i];
}

inline const RenderStyle& RenderText::style() const
{
    return parent()->style();
}

inline const RenderStyle& RenderText::firstLineStyle() const
{
    return parent()->firstLineStyle();
}

inline const RenderStyle* RenderText::getCachedPseudoStyle(PseudoId pseudoId, const RenderStyle* parentStyle) const
{
    // Pseudostyle is associated with an element, so ascend the tree until we find a non-anonymous ancestor.
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->getCachedPseudoStyle(pseudoId, parentStyle);
    return nullptr;
}

inline Color RenderText::selectionBackgroundColor() const
{
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->selectionBackgroundColor();
    return Color();
}

inline Color RenderText::selectionForegroundColor() const
{
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->selectionForegroundColor();
    return Color();
}

inline Color RenderText::selectionEmphasisMarkColor() const
{
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->selectionEmphasisMarkColor();
    return Color();
}

inline std::unique_ptr<RenderStyle> RenderText::selectionPseudoStyle() const
{
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->selectionPseudoStyle();
    return nullptr;
}

inline RenderText* Text::renderer() const
{
    return downcast<RenderText>(Node::renderer());
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderText, isText())
