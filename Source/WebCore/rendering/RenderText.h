/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include <WebCore/Color.h>
#include <WebCore/FontCascade.h>
#include <WebCore/LayoutInlineTextBox.h>
#include <WebCore/RenderElement.h>
#include <wtf/Forward.h>
#include <wtf/Platform.h>

namespace WebCore {

class Text;
enum class DocumentMarkerType : uint32_t;

namespace LayoutIntegration {
class LineLayout;
}

class RenderText : public RenderObject, private Layout::InlineTextBox {
    WTF_MAKE_TZONE_ALLOCATED(RenderText);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderText);
public:
    RenderText(Type, Text&, const String&);
    RenderText(Type, Document&, const String&);

    virtual ~RenderText();

    // RenderObject functions that the layout box base also has.
    using RenderObject::firstLineUsedStyle;
    using RenderObject::isAnonymous;
    using RenderObject::isDescendantOf;
    using RenderObject::isFloatingOrOutOfFlowPositioned;
    using RenderObject::isImage;
    using RenderObject::isInFlow;
    using RenderObject::isInFlowPositioned;
    using RenderObject::isInlineBox;
    using RenderObject::isOutOfFlowPositioned;
    using RenderObject::isPositioned;
    using RenderObject::isRelativelyPositioned;
    using RenderObject::nextInFlowSibling;
    using RenderObject::nextSibling;
    using RenderObject::parent;
    using RenderObject::previousInFlowSibling;
    using RenderObject::previousSibling;
    using RenderObject::usedStyle;
    using RenderObject::writingMode;

    // CheckedPtr to the renderer uses the RenderObject counter, CheckedPtr to the layout box the box counter. Deletion checks both.
    using RenderObject::checkedPtrCount;
    using RenderObject::decrementCheckedPtrCount;
    using RenderObject::incrementCheckedPtrCount;
    ALWAYS_INLINE uint32_t checkedPtrCountWithoutThreadCheck() const { return RenderObject::checkedPtrCountWithoutThreadCheck() + Layout::InlineTextBox::checkedPtrCountWithoutThreadCheck(); }
    void setDidBeginCheckedPtrDeletion()
    {
        RenderObject::setDidBeginCheckedPtrDeletion();
        Layout::InlineTextBox::setDidBeginCheckedPtrDeletion();
    }

    // The layout box is part of the renderer.
    ALWAYS_INLINE Layout::InlineTextBox& layoutBox() LIFETIME_BOUND { return *this; }
    ALWAYS_INLINE const Layout::InlineTextBox& layoutBox() const LIFETIME_BOUND { return *this; }
    // Every InlineTextBox is the base of a RenderText.
    ALWAYS_INLINE static RenderText& fromLayoutBox(Layout::InlineTextBox& box) { SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<RenderText&>(box); }
    ALWAYS_INLINE static const RenderText& fromLayoutBox(const Layout::InlineTextBox& box) { SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<const RenderText&>(box); }

    NODELETE WEBCORE_EXPORT Text* textNode() const;

    // The style of an anonymous inline box around the text. The inherited properties come from the parent box, or from the
    // DOM parent when it is display: contents and has no box. The other properties have their initial values.
    ALWAYS_INLINE const Style::ComputedStyle& style() const LIFETIME_BOUND { return Layout::InlineTextBox::style(); }
    ALWAYS_INLINE const Style::ComputedStyle& firstLineStyle() const LIFETIME_BOUND { return Layout::InlineTextBox::firstLineStyle(); }
    // On attach and parent style change.
    void inheritStyleFromParent();
    bool inheritsStyleFromDisplayContentsParent() const { return Layout::InlineTextBox::inheritsStyleFromDisplayContentsParent(); }
    void setStyleInheritedFromDisplayContentsParent(Style::ComputedStyle&&);
    void clearStyleInheritedFromDisplayContentsParent();
    const Style::ComputedStyle* lazyPseudoElementStyle(const Style::PseudoElementIdentifier&, const Style::ComputedStyle* parentStyle = nullptr) const LIFETIME_BOUND;

    Color selectionBackgroundColor() const;
    Color selectionForegroundColor() const;
    Color selectionEmphasisMarkColor() const;
    const Style::ComputedStyle* selectionPseudoStyle() const LIFETIME_BOUND;

    const Style::ComputedStyle* spellingErrorPseudoStyle() const LIFETIME_BOUND;
    const Style::ComputedStyle* grammarErrorPseudoStyle() const LIFETIME_BOUND;
    const Style::ComputedStyle* targetTextPseudoStyle() const LIFETIME_BOUND;

    virtual String originalText() const;


    const String& text() const LIFETIME_BOUND { return m_text; }
    String textWithoutConvertingBackslashToYenSymbol() const;

    void boundingRects(Vector<LayoutRect>&, const LayoutPoint& accumulatedOffset) const final;
    Vector<IntRect> absoluteRectsForRange(unsigned startOffset = 0, unsigned endOffset = UINT_MAX, bool useSelectionHeight = false, bool* wasFixed = nullptr) const;

    void collectSelectionGeometries(Vector<SelectionGeometry>&, unsigned startOffset = 0, unsigned endOffset = std::numeric_limits<unsigned>::max()) final;

    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const final;
    Vector<FloatQuad> absoluteQuadsForRange(unsigned startOffset = 0, unsigned endOffset = UINT_MAX, OptionSet<RenderObject::BoundingRectBehavior> = { }, bool* wasFixed = nullptr) const;

    Vector<FloatQuad> absoluteQuadsClippedToEllipsis() const;

    Position positionForPoint(const LayoutPoint&, HitTestSource) final;

    bool hasEmptyText() const { return m_text.isEmpty(); }

    char16_t characterAt(unsigned) const;
    size_t length() const { return text().length(); }

    WEBCORE_EXPORT virtual IntRect linesBoundingBox() const;
    WEBCORE_EXPORT IntPoint firstRunLocation() const;

    void setText(const String&, bool force = false);
    virtual void setTextWithOffset(const String&, unsigned offset);

    bool canBeSelectionLeaf() const override { return true; }

    LayoutRect collectSelectionGeometriesForLineBoxes(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent, Vector<FloatQuad>&);

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

    NODELETE bool containsOnlyCollapsibleWhitespace() const;

    FontCascade::CodePath fontCodePath() const { return m_fontCodePath; }
    bool canUseSimpleFontCodePath() const { return fontCodePath() == FontCascade::CodePath::Simple; }
    bool shouldUseSimpleGlyphOverflowCodePath() const { return fontCodePath() == FontCascade::CodePath::SimpleWithGlyphOverflow; }

    virtual void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle);

    float candidateComputedTextSize() const { return m_candidateComputedTextSize; }
    void setCandidateComputedTextSize(float size) { m_candidateComputedTextSize = size; }

    NODELETE StringView stringView(unsigned start = 0, std::optional<unsigned> stop = std::nullopt) const;
    
    NODELETE bool containsOnlyCSSWhitespace(unsigned from, unsigned length) const;

    Vector<std::pair<unsigned, unsigned>> contentRangesBetweenOffsetsForType(const DocumentMarkerType, unsigned startOffset, unsigned endOffset) const;

    bool canUseSimplifiedTextMeasuring() const { return layoutBox().canUseSimplifiedContentMeasuring(); }

    // The layout box content is the rendered text, with the secure text disc swapped on Cocoa platforms. In a live inline
    // formatting context it changes only together with the inline items.
    void updateLayoutBoxContent();
    bool layoutBoxContentNeedsUpdate() const { return m_layoutBoxContentNeedsUpdate; }
    void updateLayoutBoxContentIfNeeded()
    {
        if (m_layoutBoxContentNeedsUpdate)
            updateLayoutBoxContent();
    }

protected:
    void willBeDestroyed() override;

    virtual void setRenderedText(const String&);
    virtual char32_t previousCharacter() const;

    virtual void setTextInternal(const String&, bool force);

    void layoutBoxContentDidChange();
    void displayContentsParentStyleDidChange(const Style::ComputedStyle& oldStyle, bool didChangeStyleSource);

private:
    void updateRenderedText();
    void updateRenderedText(const String&);

    RenderText(Type, Node&, const String&);

    ASCIILiteral renderName() const override;

    bool canHaveChildren() const final { return false; }

    PositionWithAffinity positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*) override;

    void setSelectionState(HighlightState) final;
    LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent = true) final;
    RepaintRects localRectsForRepaint(RepaintOutlineBounds) const final;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction) final { ASSERT_NOT_REACHED(); return false; }

    bool computeUseBackslashAsYenSymbol() const;

    void secureText(char16_t mask);

    LayoutRect collectSelectionGeometriesForLineBoxes(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent, Vector<FloatQuad>*);

    void node() const = delete;
    void container() const = delete; // Use parent() instead.
    void container(const RenderLayerModelObject&, bool&) const = delete; // Use parent() instead.

    // FIXME: This should probably be part of the text sizing structures in Document instead. That would save some memory.
    float m_candidateComputedTextSize { 0 };

    String m_text;

    unsigned m_needsVisualReordering : 1 { false };
    unsigned m_containsOnlyASCII : 1 { false };
    unsigned m_useBackslashAsYenSymbol : 1 { false };
    unsigned m_originalTextDiffersFromRendered : 1 { false };
    unsigned m_layoutBoxContentNeedsUpdate : 1 { false };
    unsigned m_hasSecureTextTimer : 1 { false };
    FontCascade::CodePath m_fontCodePath : 2;
};

String applyTextTransform(const Style::ComputedStyle&, const String&, char32_t previousCharacter);
String applyTextTransform(const Style::ComputedStyle&, const String&);
String capitalize(const String&, char32_t previousCharacter, const AtomString& locale);
String capitalize(const String&, const AtomString& locale);
NODELETE bool isDutchLocale(const AtomString&);

inline char16_t RenderText::characterAt(unsigned i) const
{
    return i >= length() ? 0 : text()[i];
}

inline const Style::ComputedStyle* RenderText::lazyPseudoElementStyle(const Style::PseudoElementIdentifier& pseudoElementIdentifier, const Style::ComputedStyle* parentStyle) const
{
    // Pseudostyle is associated with an element, so ascend the tree until we find a non-anonymous ancestor.
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->lazyPseudoElementStyle(pseudoElementIdentifier, parentStyle);
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

inline const Style::ComputedStyle* RenderText::selectionPseudoStyle() const
{
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->selectionPseudoStyle();
    return nullptr;
}

inline const Style::ComputedStyle* RenderText::spellingErrorPseudoStyle() const
{
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->spellingErrorPseudoStyle();
    return nullptr;
}

inline const Style::ComputedStyle* RenderText::grammarErrorPseudoStyle() const
{
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->grammarErrorPseudoStyle();
    return nullptr;
}

inline const Style::ComputedStyle* RenderText::targetTextPseudoStyle() const
{
    if (auto* ancestor = firstNonAnonymousAncestor())
        return ancestor->targetTextPseudoStyle();
    return nullptr;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderText, isRenderText())

namespace WebCore {

// Both branches compute the same offset, so the branch folds away in optimized builds.
inline Layout::Box& RenderObject::layoutBox()
{
    if (isRenderText())
        return uncheckedDowncast<RenderText>(*this).layoutBox();
    return uncheckedDowncast<RenderElement>(*this).layoutBox();
}

inline const Layout::Box& RenderObject::layoutBox() const
{
    if (isRenderText())
        return uncheckedDowncast<RenderText>(*this).layoutBox();
    return uncheckedDowncast<RenderElement>(*this).layoutBox();
}

inline RenderObject& RenderObject::fromLayoutBox(Layout::Box& box)
{
    ASSERT(!box.isInitialContainingBlock());
    if (box.isInlineTextBox())
        return RenderText::fromLayoutBox(uncheckedDowncast<Layout::InlineTextBox>(box));
    return RenderElement::fromLayoutBox(uncheckedDowncast<Layout::ElementBox>(box));
}

inline const RenderObject& RenderObject::fromLayoutBox(const Layout::Box& box)
{
    ASSERT(!box.isInitialContainingBlock());
    if (box.isInlineTextBox())
        return RenderText::fromLayoutBox(uncheckedDowncast<Layout::InlineTextBox>(box));
    return RenderElement::fromLayoutBox(uncheckedDowncast<Layout::ElementBox>(box));
}

inline RenderObject* Layout::Box::rendererForIntegration() const
{
    if (isInitialContainingBlock()) [[unlikely]]
        return nullptr;
    return &RenderObject::fromLayoutBox(const_cast<Box&>(*this));
}

namespace Layout {

inline const Box* Box::nextSibling() const
{
    for (SUPPRESS_UNCHECKED_LOCAL auto* sibling = RenderObject::fromLayoutBox(*this).nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (!isExcludedFromLayoutBoxNavigation(*sibling)) [[likely]]
            return &sibling->layoutBox();
    }
    return nullptr;
}

inline const Box* Box::previousSibling() const
{
    for (SUPPRESS_UNCHECKED_LOCAL auto* sibling = RenderObject::fromLayoutBox(*this).previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (!isExcludedFromLayoutBoxNavigation(*sibling)) [[likely]]
            return &sibling->layoutBox();
    }
    return nullptr;
}

}

} // namespace WebCore
