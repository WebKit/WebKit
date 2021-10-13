/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "LayoutReplacedBox.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderImage.h"
#include "RenderLineBreak.h"
#include "TextUtil.h"

namespace WebCore {
namespace LayoutIntegration {

static constexpr size_t smallTreeThreshold = 8;

// FIXME: see webkit.org/b/230964
#define CAN_USE_FIRST_LINE_STYLE_RESOLVE 1

static RenderStyle rootBoxStyle(const RenderStyle& style)
{
    auto clonedStyle = RenderStyle::clone(style);
    clonedStyle.setEffectiveDisplay(DisplayType::Block);
    return clonedStyle;
}

static std::unique_ptr<RenderStyle> rootBoxFirstLineStyle(const RenderBlockFlow& root)
{
#if CAN_USE_FIRST_LINE_STYLE_RESOLVE
    auto& firstLineStyle = root.firstLineStyle();
    if (root.style() == firstLineStyle)
        return { };
    auto clonedStyle = RenderStyle::clonePtr(firstLineStyle);
    clonedStyle->setEffectiveDisplay(DisplayType::Block);
    return clonedStyle;
#else
    UNUSED_PARAM(root);
    return { };
#endif
}

BoxTree::BoxTree(RenderBlockFlow& flow)
    : m_flow(flow)
    , m_root(Layout::Box::ElementAttributes { }, rootBoxStyle(flow.style()), rootBoxFirstLineStyle(flow))
{
    m_root.setIsIntegrationBlockContainer();

    if (flow.isAnonymous())
        m_root.setIsAnonymous();

    buildTree();
}

void BoxTree::buildTree()
{
    auto createChildBox = [&](RenderObject& childRenderer) -> std::unique_ptr<Layout::Box> {
        std::unique_ptr<RenderStyle> firstLineStyle;
#if CAN_USE_FIRST_LINE_STYLE_RESOLVE
        if (&childRenderer.style() != &childRenderer.firstLineStyle())
            firstLineStyle = RenderStyle::clonePtr(childRenderer.firstLineStyle());
#endif
        if (is<RenderText>(childRenderer)) {
            auto& textRenderer = downcast<RenderText>(childRenderer);
            auto style = RenderStyle::createAnonymousStyleWithDisplay(textRenderer.style(), DisplayType::Inline);
            auto canUseSimplifiedTextMeasuring = [&] {
                if (!textRenderer.canUseSimplifiedTextMeasuring())
                    return false;
                return !firstLineStyle || Layout::TextUtil::canUseSimplifiedTextMeasuringForFirstLine(style, *firstLineStyle);
            }();
            return makeUnique<Layout::InlineTextBox>(
                style.textSecurity() == TextSecurity::None ? textRenderer.text() : RenderBlock::updateSecurityDiscCharacters(style, textRenderer.text())
                , canUseSimplifiedTextMeasuring, WTFMove(style), WTFMove(firstLineStyle));
        }

        auto style = RenderStyle::clone(childRenderer.style());
        if (childRenderer.isLineBreak()) {
            auto adjustStyle = [&] (auto& styleToAdjust) {
                styleToAdjust.setDisplay(DisplayType::Inline);
                styleToAdjust.setFloating(Float::None);
                styleToAdjust.setPosition(PositionType::Static);
            };
            adjustStyle(style);
            if (firstLineStyle)
                adjustStyle(*firstLineStyle);

            return makeUnique<Layout::LineBreakBox>(downcast<RenderLineBreak>(childRenderer).isWBR(), WTFMove(style), WTFMove(firstLineStyle));
        }

        if (is<RenderReplaced>(childRenderer))
            return makeUnique<Layout::ReplacedBox>(Layout::Box::ElementAttributes { is<RenderImage>(childRenderer) ? Layout::Box::ElementType::Image : Layout::Box::ElementType::GenericElement }, WTFMove(style), WTFMove(firstLineStyle));

        if (is<RenderBlock>(childRenderer))
            return makeUnique<Layout::ReplacedBox>(Layout::Box::ElementAttributes { Layout::Box::ElementType::GenericElement }, WTFMove(style), WTFMove(firstLineStyle));

        if (is<RenderInline>(childRenderer)) {
            if (childRenderer.parent()->isAnonymousBlock()) {
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
            }
            return makeUnique<Layout::ContainerBox>(Layout::Box::ElementAttributes { Layout::Box::ElementType::GenericElement }, WTFMove(style), WTFMove(firstLineStyle));
        }

        ASSERT_NOT_REACHED();
        return nullptr;
    };

    for (auto walker = InlineWalker(m_flow); !walker.atEnd(); walker.advance()) {
        auto& childRenderer = *walker.current();
        auto childBox = createChildBox(childRenderer);
        appendChild(makeUniqueRefFromNonNullUniquePtr(WTFMove(childBox)), childRenderer);
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
        layoutBox.updateStyle(rootBoxStyle(style), rootBoxFirstLineStyle(downcast<RenderBlockFlow>(renderer)));
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
    if (&renderer == &m_flow)
        return m_root;

    if (m_boxes.size() <= smallTreeThreshold) {
        auto index = m_boxes.findMatching([&](auto& entry) {
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
        return m_flow;

    if (m_boxes.size() <= smallTreeThreshold) {
        auto index = m_boxes.findMatching([&](auto& entry) {
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

}
}

#endif


