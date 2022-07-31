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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineWalker.h"
#include "LayoutContainerBox.h"
#include "LayoutInlineTextBox.h"
#include "LayoutLineBreakBox.h"
#include "LayoutListMarkerBox.h"
#include "LayoutReplacedBox.h"
#include "RenderBlock.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderCounter.h"
#include "RenderDetailsMarker.h"
#include "RenderFlexibleBox.h"
#include "RenderImage.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderTable.h"
#include "TextUtil.h"

#if ENABLE(TREE_DEBUGGING)
#include "LayoutIntegrationLineLayout.h"
#endif

namespace WebCore {
namespace LayoutIntegration {

static constexpr size_t smallTreeThreshold = 8;

// FIXME: see webkit.org/b/230964
#define CAN_USE_FIRST_LINE_STYLE_RESOLVE 1

static RenderStyle rootBoxStyle(const RenderBlock& rootRenderer)
{
    auto clonedStyle = RenderStyle::clone(rootRenderer.style());
    if (is<RenderBlockFlow>(rootRenderer))
        clonedStyle.setEffectiveDisplay(DisplayType::Block);
    return clonedStyle;
}

static std::unique_ptr<RenderStyle> rootBoxFirstLineStyle(const RenderBlock& rootRenderer)
{
#if CAN_USE_FIRST_LINE_STYLE_RESOLVE
    auto& firstLineStyle = rootRenderer.firstLineStyle();
    if (rootRenderer.style() == firstLineStyle)
        return { };
    auto clonedStyle = RenderStyle::clonePtr(firstLineStyle);
    if (is<RenderBlockFlow>(rootRenderer))
        clonedStyle->setEffectiveDisplay(DisplayType::Block);
    return clonedStyle;
#else
    UNUSED_PARAM(rootRenderer);
    return { };
#endif
}

BoxTree::BoxTree(RenderBlock& rootRenderer)
    : m_rootRenderer(rootRenderer)
    , m_root(Layout::Box::ElementAttributes { Layout::Box::ElementType::IntegrationBlockContainer }, rootBoxStyle(rootRenderer), rootBoxFirstLineStyle(rootRenderer))
{
    if (rootRenderer.isAnonymous())
        m_root.setIsAnonymous();

    if (is<RenderBlockFlow>(rootRenderer))
        buildTreeForInlineContent();
    else if (is<RenderFlexibleBox>(rootRenderer))
        buildTreeForFlexContent();
    else
        ASSERT_NOT_IMPLEMENTED_YET();
}

void BoxTree::buildTreeForInlineContent()
{
    auto createChildBox = [&](RenderObject& childRenderer) -> std::unique_ptr<Layout::Box> {
        std::unique_ptr<RenderStyle> firstLineStyle;
#if CAN_USE_FIRST_LINE_STYLE_RESOLVE
        if (&childRenderer.style() != &childRenderer.firstLineStyle())
            firstLineStyle = RenderStyle::clonePtr(childRenderer.firstLineStyle());
#endif
        if (is<RenderCounter>(childRenderer)) {
            // This ensures that InlineTextBox (see below) always has uptodate counter text (note that RenderCounter is a type of RenderText).
            if (childRenderer.preferredLogicalWidthsDirty()) {
                // Counter content is updated through preferred width computation.
                downcast<RenderCounter>(childRenderer).updateCounter();
            }
        }
        if (is<RenderText>(childRenderer)) {
            auto& textRenderer = downcast<RenderText>(childRenderer);
            auto style = RenderStyle::createAnonymousStyleWithDisplay(textRenderer.style(), DisplayType::Inline);
            auto text = style.textSecurity() == TextSecurity::None ? textRenderer.text() : RenderBlock::updateSecurityDiscCharacters(style, textRenderer.text());
            return makeUnique<Layout::InlineTextBox>(text, textRenderer.canUseSimplifiedTextMeasuring(), textRenderer.canUseSimpleFontCodePath(), WTFMove(style), WTFMove(firstLineStyle));
        }

        auto style = RenderStyle::clone(childRenderer.style());
        if (childRenderer.isLineBreak()) {
            auto adjustStyle = [&] (auto& styleToAdjust) {
                styleToAdjust.setDisplay(DisplayType::Inline);
                styleToAdjust.setFloating(Float::None);
                styleToAdjust.setPosition(PositionType::Static);

                // Clear property should only apply on block elements, however,
                // it appears that browsers seem to ignore it on <br> inline elements.
                // https://drafts.csswg.org/css2/#propdef-clear
                if (downcast<RenderLineBreak>(childRenderer).isWBR())
                    styleToAdjust.setClear(Clear::None);
            };
            adjustStyle(style);
            if (firstLineStyle)
                adjustStyle(*firstLineStyle);

            return makeUnique<Layout::LineBreakBox>(downcast<RenderLineBreak>(childRenderer).isWBR(), WTFMove(style), WTFMove(firstLineStyle));
        }

        if (is<RenderListMarker>(childRenderer)) {
            auto& listMarkerRenderer = downcast<RenderListMarker>(childRenderer);
            return makeUnique<Layout::ListMarkerBox>(listMarkerRenderer.isImage() ? Layout::ListMarkerBox::IsImage::Yes : Layout::ListMarkerBox::IsImage::No
                , listMarkerRenderer.isInside() ? Layout::ListMarkerBox::IsOutside::No : Layout::ListMarkerBox::IsOutside::Yes
                , WTFMove(style)
                , WTFMove(firstLineStyle));
        }

        if (is<RenderReplaced>(childRenderer))
            return makeUnique<Layout::ReplacedBox>(Layout::Box::ElementAttributes { is<RenderImage>(childRenderer) ? Layout::Box::ElementType::Image : Layout::Box::ElementType::GenericElement }, WTFMove(style), WTFMove(firstLineStyle));

        if (is<RenderBlock>(childRenderer)) {
            auto attributes = Layout::Box::ElementAttributes { Layout::Box::ElementType::IntegrationInlineBlock };
            if (is<RenderTable>(childRenderer) || is<RenderDetailsMarker>(childRenderer) || is<RenderListItem>(childRenderer))
                attributes = Layout::Box::ElementAttributes { Layout::Box::ElementType::GenericElement };
            return makeUnique<Layout::ReplacedBox>(attributes, WTFMove(style), WTFMove(firstLineStyle));
        }

        if (is<RenderInline>(childRenderer)) {
            // This looks like continuation renderer.
            auto& renderInline = downcast<RenderInline>(childRenderer);
            auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.isContinuation();
            auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline.isContinuation() && renderInline.inlineContinuation();
            auto adjustStyleForContinuation = [&] (auto& styleToAdjust) {
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
            };
            adjustStyleForContinuation(style);
            if (firstLineStyle)
                adjustStyleForContinuation(*firstLineStyle);
            return makeUnique<Layout::ContainerBox>(Layout::Box::ElementAttributes { Layout::Box::ElementType::GenericElement }, WTFMove(style), WTFMove(firstLineStyle));
        }

        ASSERT_NOT_REACHED();
        return nullptr;
    };

    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(m_rootRenderer)); !walker.atEnd(); walker.advance()) {
        auto& childRenderer = *walker.current();
        auto childBox = createChildBox(childRenderer);
        appendChild(makeUniqueRefFromNonNullUniquePtr(WTFMove(childBox)), childRenderer);
    }
}

void BoxTree::buildTreeForFlexContent()
{
    for (auto& flexItemRenderer : childrenOfType<RenderObject>(m_rootRenderer)) {
        auto style = RenderStyle::clone(flexItemRenderer.style());
        auto flexItem = makeUnique<Layout::ContainerBox>(Layout::Box::ElementAttributes { Layout::Box::ElementType::IntegrationBlockContainer }, WTFMove(style));
        appendChild(makeUniqueRefFromNonNullUniquePtr(WTFMove(flexItem)), flexItemRenderer);
    }
}

void BoxTree::appendChild(UniqueRef<Layout::Box> childBox, RenderObject& childRenderer)
{
    auto& parentBox = downcast<Layout::ContainerBox>(layoutBoxForRenderer(*childRenderer.parent()));

    m_boxes.append({ childBox.get(), &childRenderer });

    parentBox.appendChild(WTFMove(childBox));

    if (m_boxes.size() > smallTreeThreshold) {
        if (m_rendererToBoxMap.isEmpty()) {
            for (auto& entry : m_boxes)
                m_rendererToBoxMap.add(entry.renderer, entry.box.get());
        } else
            m_rendererToBoxMap.add(&childRenderer, m_boxes.last().box.get());
    }
}

void BoxTree::updateStyle(const RenderBoxModelObject& renderer)
{
    auto& layoutBox = layoutBoxForRenderer(renderer);
    auto& style = renderer.style();
    auto firstLineStyle = [&] () -> std::unique_ptr<RenderStyle> {
#if CAN_USE_FIRST_LINE_STYLE_RESOLVE
        if (&renderer.style() != &renderer.firstLineStyle())
            return RenderStyle::clonePtr(renderer.firstLineStyle());
        return nullptr;
#else
        return nullptr;
#endif
    };

    if (&layoutBox == &rootLayoutBox())
        layoutBox.updateStyle(rootBoxStyle(downcast<RenderBlock>(renderer)), rootBoxFirstLineStyle(downcast<RenderBlock>(renderer)));
    else
        layoutBox.updateStyle(style, firstLineStyle());

    if (is<Layout::ContainerBox>(layoutBox)) {
        for (auto* child = downcast<Layout::ContainerBox>(layoutBox).firstChild(); child; child = child->nextSibling()) {
            if (child->isInlineTextBox())
                child->updateStyle(RenderStyle::createAnonymousStyleWithDisplay(style, DisplayType::Inline), firstLineStyle());
        }
    }
}

Layout::Box& BoxTree::layoutBoxForRenderer(const RenderObject& renderer)
{
    if (&renderer == &m_rootRenderer)
        return m_root;

    if (m_boxes.size() <= smallTreeThreshold) {
        auto index = m_boxes.findIf([&](auto& entry) {
            return entry.renderer == &renderer;
        });
        RELEASE_ASSERT(index != notFound);
        return m_boxes[index].box;
    }

    return *m_rendererToBoxMap.get(&renderer);
}

const Layout::Box& BoxTree::layoutBoxForRenderer(const RenderObject& renderer) const
{
    return const_cast<BoxTree&>(*this).layoutBoxForRenderer(renderer);
}

RenderObject& BoxTree::rendererForLayoutBox(const Layout::Box& box)
{
    if (&box == &m_root)
        return m_rootRenderer;

    if (m_boxes.size() <= smallTreeThreshold) {
        auto index = m_boxes.findIf([&](auto& entry) {
            return entry.box.ptr() == &box;
        });
        RELEASE_ASSERT(index != notFound);
        return *m_boxes[index].renderer;
    }

    if (m_boxToRendererMap.isEmpty()) {
        for (auto& entry : m_boxes)
            m_boxToRendererMap.add(entry.box.get(), entry.renderer);
    }
    return *m_boxToRendererMap.get(&box);
}

const RenderObject& BoxTree::rendererForLayoutBox(const Layout::Box& box) const
{
    return const_cast<BoxTree&>(*this).rendererForLayoutBox(box);
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

#endif


