/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayoutIntegrationBoxTree.h"

#include "InlineWalker.h"
#include "LayoutElementBox.h"
#include "LayoutInlineTextBox.h"
#include "RenderBlock.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderCombineText.h"
#include "RenderCounter.h"
#include "RenderDetailsMarker.h"
#include "RenderFlexibleBox.h"
#include "RenderImage.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderTable.h"
#include "RenderView.h"
#include "TextUtil.h"

#if ENABLE(TREE_DEBUGGING)
#include "LayoutIntegrationLineLayout.h"
#endif

namespace WebCore {
namespace LayoutIntegration {

static constexpr size_t smallTreeThreshold = 8;

static std::unique_ptr<RenderStyle> firstLineStyleFor(const RenderObject& renderer)
{
    auto& firstLineStyle = renderer.firstLineStyle();
    if (&renderer.style() == &firstLineStyle)
        return { };
    return RenderStyle::clonePtr(firstLineStyle);
}

static Layout::Box::IsAnonymous isAnonymous(const RenderObject& renderer)
{
    return renderer.isAnonymous() ? Layout::Box::IsAnonymous::Yes : Layout::Box::IsAnonymous::No;
}

static Layout::Box::ElementAttributes elementAttributes(const RenderElement& renderer)
{
    auto nodeType = [&] {
        if (is<RenderListMarker>(renderer))
            return Layout::Box::NodeType::ListMarker;
        if (is<RenderReplaced>(renderer))
            return is<RenderImage>(renderer) ? Layout::Box::NodeType::Image : Layout::Box::NodeType::ReplacedElement;
        if (is<RenderLineBreak>(renderer))
            return downcast<RenderLineBreak>(renderer).isWBR() ? Layout::Box::NodeType::WordBreakOpportunity : Layout::Box::NodeType::LineBreak;
        return Layout::Box::NodeType::GenericElement;
    }();

    return { nodeType, isAnonymous(renderer) };
}

BoxTree::BoxTree(RenderBlock& rootRenderer)
    : m_rootRenderer(rootRenderer)
{
    auto* rootBox = m_rootRenderer.layoutBox();
    if (!rootBox) {
        auto newRootBox = createLayoutBox(rootRenderer);
        rootBox = downcast<Layout::ElementBox>(newRootBox.ptr());
        m_rootRenderer.setLayoutBox(*rootBox);
        initialContainingBlock().appendChild(WTFMove(newRootBox));
    }

    if (is<RenderBlockFlow>(rootRenderer)) {
        rootBox->setIsInlineIntegrationRoot();
        rootBox->setIsFirstChildForIntegration(rootRenderer.parent()->firstChild() == &rootRenderer);
        buildTreeForInlineContent();
    } else if (is<RenderFlexibleBox>(rootRenderer))
        buildTreeForFlexContent();
    else
        ASSERT_NOT_IMPLEMENTED_YET();
}

BoxTree::~BoxTree()
{
    for (auto& renderer : m_renderers) {
        if (!renderer)
            continue;

        bool isLFCInlineBlock = is<RenderBlockFlow>(*renderer) && downcast<RenderBlockFlow>(*renderer).modernLineLayout();
        if (isLFCInlineBlock) {
            auto detachedBox = renderer->layoutBox()->removeFromParent();
            initialContainingBlock().appendChild(WTFMove(detachedBox));
            continue;
        }

        renderer->clearLayoutBox();
    }

    m_boxToRendererMap = { };

    if (&rootLayoutBox().parent() == &initialContainingBlock()) {
        auto toDelete = rootLayoutBox().removeFromParent();
        m_rootRenderer.clearLayoutBox();
    } else
        rootLayoutBox().destroyChildren();
}

void BoxTree::adjustStyleIfNeeded(const RenderElement& renderer, RenderStyle& style, RenderStyle* firstLineStyle)
{
    auto adjustStyle = [&] (auto& styleToAdjust) {
        if (is<RenderBlock>(renderer)) {
            if (styleToAdjust.display() == DisplayType::Inline)
                styleToAdjust.setDisplay(DisplayType::InlineBlock);
            if (renderer.isAnonymousBlock()) {
                auto& anonBlockParentStyle = renderer.parent()->style();
                // overflow and text-overflow property values don't get forwarded to anonymous block boxes.
                // e.g. <div style="overflow: hidden; text-overflow: ellipsis; width: 100px; white-space: pre;">this text should have ellipsis<div></div></div>
                styleToAdjust.setTextOverflow(anonBlockParentStyle.textOverflow());
                styleToAdjust.setOverflowX(anonBlockParentStyle.overflowX());
                styleToAdjust.setOverflowY(anonBlockParentStyle.overflowY());
            }
            return;
        }
        if (is<RenderInline>(renderer)) {
            auto& renderInline = downcast<RenderInline>(renderer);
            auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.isContinuation();
            auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline.isContinuation() && renderInline.inlineContinuation();
            // This looks like continuation renderer.
            if (shouldNotRetainBorderPaddingAndMarginStart) {
                styleToAdjust.setMarginStart(RenderStyle::initialMargin());
                styleToAdjust.resetBorderLeft();
                styleToAdjust.setPaddingLeft(RenderStyle::initialPadding());
            }
            if (shouldNotRetainBorderPaddingAndMarginEnd) {
                styleToAdjust.setMarginEnd(RenderStyle::initialMargin());
                styleToAdjust.resetBorderRight();
                styleToAdjust.setPaddingRight(RenderStyle::initialPadding());
            }
            return;
        }
        if (renderer.isLineBreak()) {
            styleToAdjust.setDisplay(DisplayType::Inline);
            styleToAdjust.setFloating(Float::None);
            styleToAdjust.setPosition(PositionType::Static);
            // Clear property should only apply on block elements, however,
            // it appears that browsers seem to ignore it on <br> inline elements.
            // https://drafts.csswg.org/css2/#propdef-clear
            if (downcast<RenderLineBreak>(renderer).isWBR())
                styleToAdjust.setClear(Clear::None);
            return;
        }
    };
    adjustStyle(style);
    if (firstLineStyle)
        adjustStyle(*firstLineStyle);
}

UniqueRef<Layout::Box> BoxTree::createLayoutBox(RenderObject& renderer)
{
    std::unique_ptr<RenderStyle> firstLineStyle = firstLineStyleFor(renderer);

    if (is<RenderText>(renderer)) {
        auto& textRenderer = downcast<RenderText>(renderer);

        if (is<RenderCounter>(textRenderer) && textRenderer.preferredLogicalWidthsDirty()) {
            // This ensures that InlineTextBox always has up-to-date counter text.
            // Counter content is updated through preferred width computation.
            downcast<RenderCounter>(textRenderer).updateCounter();
        }

        auto style = RenderStyle::createAnonymousStyleWithDisplay(textRenderer.style(), DisplayType::Inline);
        auto isCombinedText = is<RenderCombineText>(textRenderer) && downcast<RenderCombineText>(textRenderer).isCombined();
        auto text = style.textSecurity() == TextSecurity::None
            ? (isCombinedText ? textRenderer.originalText() : textRenderer.text())
            : RenderBlock::updateSecurityDiscCharacters(style, isCombinedText ? textRenderer.originalText() : textRenderer.text());
        return makeUniqueRef<Layout::InlineTextBox>(text, isCombinedText, textRenderer.canUseSimplifiedTextMeasuring(), textRenderer.canUseSimpleFontCodePath(), WTFMove(style), WTFMove(firstLineStyle));
    }

    auto& renderElement = downcast<RenderElement>(renderer);

    auto style = RenderStyle::clone(renderer.style());
    adjustStyleIfNeeded(renderElement, style, firstLineStyle.get());

    if (is<RenderListMarker>(renderElement)) {
        auto& listMarkerRenderer = downcast<RenderListMarker>(renderElement);
        OptionSet<Layout::ElementBox::ListMarkerAttribute> listMarkerAttributes;
        if (listMarkerRenderer.isImage())
            listMarkerAttributes.add(Layout::ElementBox::ListMarkerAttribute::Image);
        if (!listMarkerRenderer.isInside())
            listMarkerAttributes.add(Layout::ElementBox::ListMarkerAttribute::Outside);
        return makeUniqueRef<Layout::ElementBox>(elementAttributes(renderElement), listMarkerAttributes, WTFMove(style), WTFMove(firstLineStyle));
    }

    return makeUniqueRef<Layout::ElementBox>(elementAttributes(renderElement), WTFMove(style), WTFMove(firstLineStyle));
};

void BoxTree::buildTreeForInlineContent()
{
    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(m_rootRenderer)); !walker.atEnd(); walker.advance()) {
        auto& childRenderer = *walker.current();
        auto childLayoutBox = [&] {
            if (auto existingChildBox = childRenderer.layoutBox())
                return existingChildBox->removeFromParent();
            return createLayoutBox(childRenderer);
        };
        appendChild(childLayoutBox(), childRenderer);
    }
}

void BoxTree::buildTreeForFlexContent()
{
    for (auto& flexItemRenderer : childrenOfType<RenderElement>(m_rootRenderer)) {
        auto style = RenderStyle::clone(flexItemRenderer.style());
        auto flexItem = makeUniqueRef<Layout::ElementBox>(elementAttributes(flexItemRenderer), WTFMove(style));
        appendChild(WTFMove(flexItem), flexItemRenderer);
    }
}

void BoxTree::appendChild(UniqueRef<Layout::Box> childBox, RenderObject& childRenderer)
{
    auto& parentBox = layoutBoxForRenderer(*childRenderer.parent());

    m_renderers.append(&childRenderer);

    childRenderer.setLayoutBox(childBox);
    parentBox.appendChild(WTFMove(childBox));
}

void BoxTree::updateStyle(const RenderBoxModelObject& renderer)
{
    auto& layoutBox = layoutBoxForRenderer(renderer);
    auto& rendererStyle = renderer.style();

    auto firstLineNewStyle = firstLineStyleFor(renderer);
    auto newStyle = RenderStyle::clone(rendererStyle);
    adjustStyleIfNeeded(renderer, newStyle, firstLineNewStyle.get());
    layoutBox.updateStyle(WTFMove(newStyle), WTFMove(firstLineNewStyle));

    for (auto* child = layoutBox.firstChild(); child; child = child->nextSibling()) {
        if (child->isInlineTextBox())
            child->updateStyle(RenderStyle::createAnonymousStyleWithDisplay(rendererStyle, DisplayType::Inline), firstLineStyleFor(renderer));
    }
}

const Layout::ElementBox& BoxTree::rootLayoutBox() const
{
    return *m_rootRenderer.layoutBox();
}

Layout::ElementBox& BoxTree::rootLayoutBox()
{
    return *m_rootRenderer.layoutBox();
}

Layout::Box& BoxTree::layoutBoxForRenderer(const RenderObject& renderer)
{
    return *const_cast<RenderObject&>(renderer).layoutBox();
}

const Layout::Box& BoxTree::layoutBoxForRenderer(const RenderObject& renderer) const
{
    return const_cast<BoxTree&>(*this).layoutBoxForRenderer(renderer);
}

const Layout::ElementBox& BoxTree::layoutBoxForRenderer(const RenderElement& renderer) const
{
    return downcast<Layout::ElementBox>(layoutBoxForRenderer(static_cast<const RenderObject&>(renderer)));
}

Layout::ElementBox& BoxTree::layoutBoxForRenderer(const RenderElement& renderer)
{
    return downcast<Layout::ElementBox>(layoutBoxForRenderer(static_cast<const RenderObject&>(renderer)));
}

RenderObject& BoxTree::rendererForLayoutBox(const Layout::Box& box)
{
    if (&box == &rootLayoutBox())
        return m_rootRenderer;

    if (m_renderers.size() <= smallTreeThreshold) {
        auto index = m_renderers.findIf([&](auto& renderer) {
            return renderer->layoutBox() == &box;
        });
        RELEASE_ASSERT(index != notFound);
        return *m_renderers[index];
    }

    if (m_boxToRendererMap.isEmpty()) {
        for (auto& renderer : m_renderers)
            m_boxToRendererMap.add(*renderer->layoutBox(), renderer);
    }
    return *m_boxToRendererMap.get(&box);
}

const RenderObject& BoxTree::rendererForLayoutBox(const Layout::Box& box) const
{
    return const_cast<BoxTree&>(*this).rendererForLayoutBox(box);
}

Layout::InitialContainingBlock& BoxTree::initialContainingBlock()
{
    return m_rootRenderer.view().initialContainingBlock();
}

#if ENABLE(TREE_DEBUGGING)
void showInlineContent(TextStream& stream, const InlineContent& inlineContent, size_t depth)
{
    auto& lines = inlineContent.lines;
    auto& boxes = inlineContent.boxes;

    if (boxes.isEmpty()) {
        // Has to have at least one box, the root inline box.
        return;
    }

    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        auto addSpacing = [&] {
            size_t printedCharacters = 0;
            stream << "-------- --";
            while (++printedCharacters <= depth * 2)
                stream << " ";

        };
        addSpacing();
        auto& line = lines[lineIndex];
        stream << "line at (" << line.lineBoxLeft() << "," << line.lineBoxTop() << ") size (" << line.lineBoxRight() - line.lineBoxLeft() << "x" << line.lineBoxBottom() - line.lineBoxTop() << ") baseline (" << line.baseline() << ") enclosing top (" << line.enclosingContentTop() << ") bottom (" << line.enclosingContentBottom() << ")";
        stream.nextLine();

        addSpacing();

        auto& rootInlineBox = boxes[0];
        auto rootInlineBoxRect = rootInlineBox.visualRectIgnoringBlockDirection();
        stream << "  ";
        stream << "Root inline box at (" << rootInlineBoxRect.x() << "," << rootInlineBoxRect.y() << ")" << " size (" << rootInlineBoxRect.width() << "x" << rootInlineBoxRect.height() << ")";
        stream.nextLine();

        for (auto& box : boxes) {
            if (box.lineIndex() != lineIndex || !box.isNonRootInlineBox())
                continue;

            addSpacing();
            stream << "  ";
            for (auto* ancestor = &box.layoutBox(); ancestor != &rootInlineBox.layoutBox(); ancestor = &ancestor->parent())
                stream << "  ";
            auto rect = box.visualRectIgnoringBlockDirection();
            stream << "Inline box at (" << rect.x() << "," << rect.y() << ") size (" << rect.width() << "x" << rect.height() << ") renderer->(" << &inlineContent.rendererForLayoutBox(box.layoutBox()) << ")";
            stream.nextLine();
        }

        addSpacing();
        stream << "  ";
        stream << "Run(s):";
        stream.nextLine();
        for (auto& box : boxes) {
            if (box.lineIndex() != lineIndex || box.isInlineBox())
                continue;
            addSpacing();
            stream << "    ";

            if (box.isText())
                stream << "Text";
            else if (box.isWordSeparator())
                stream << "Word separator";
            else if (box.isLineBreak())
                stream << "Line break";
            else if (box.isAtomicInlineLevelBox())
                stream << "Atomic box";
            else if (box.isGenericInlineLevelBox())
                stream << "Generic inline level box";
            stream << " at (" << box.left() << "," << box.top() << ") size " << box.width() << "x" << box.height();
            if (box.isText())
                stream << " run(" << box.text()->start() << ", " << box.text()->end() << ")";
            stream << " renderer->(" << &inlineContent.rendererForLayoutBox(box.layoutBox()) << ")";
            stream.nextLine();
        }
    }
}
#endif

}
}


