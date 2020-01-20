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

#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"

namespace WebCore {
namespace LayoutIntegration {

static constexpr size_t smallTreeThreshold = 8;

static RenderStyle rootBoxStyle(const RenderStyle& style)
{
    auto clonedStyle = RenderStyle::clone(style);
    clonedStyle.setDisplay(DisplayType::Block);
    return clonedStyle;
}

BoxTree::BoxTree(const RenderBlockFlow& flow)
    : m_root({ }, rootBoxStyle(flow.style()))
{
    if (flow.isAnonymous())
        m_root.setIsAnonymous();

    buildTree(flow);
}

void BoxTree::buildTree(const RenderBlockFlow& flow)
{
    for (auto& childRenderer : childrenOfType<RenderObject>(flow)) {
        std::unique_ptr<Layout::Box> childBox;
        if (is<RenderText>(childRenderer)) {
            auto& textRenderer = downcast<RenderText>(childRenderer);
            auto textContent = Layout::TextContext { textRenderer.text(), textRenderer.canUseSimplifiedTextMeasuring() };
            childBox = makeUnique<Layout::Box>(WTFMove(textContent), RenderStyle::createAnonymousStyleWithDisplay(m_root.style(), DisplayType::Inline));
            childBox->setIsAnonymous();
        } else if (childRenderer.isLineBreak()) {
            auto clonedStyle = RenderStyle::clone(childRenderer.style());
            clonedStyle.setDisplay(DisplayType::Inline);
            clonedStyle.setFloating(Float::No);
            childBox = makeUnique<Layout::Box>(Layout::Box::ElementAttributes { Layout::Box::ElementType::HardLineBreak }, WTFMove(clonedStyle));
        }
        ASSERT(childBox);

        m_root.appendChild(*childBox);
        m_boxes.append({ WTFMove(childBox), &childRenderer });
    }
}

const Layout::Box* BoxTree::layoutBoxForRenderer(const RenderObject& renderer) const
{
    if (m_boxes.size() <= smallTreeThreshold) {
        auto index = m_boxes.findMatching([&](auto& entry) {
            return entry.renderer == &renderer;
        });
        if (index == notFound)
            return nullptr;
        return m_boxes[index].box.get();
    }

    if (m_rendererToBoxMap.isEmpty()) {
        for (auto& entry : m_boxes)
            m_rendererToBoxMap.add(entry.renderer, entry.box.get());
    }
    return m_rendererToBoxMap.get(&renderer);
}

const RenderObject* BoxTree::rendererForLayoutBox(const Layout::Box& box) const
{
    if (m_boxes.size() <= smallTreeThreshold) {
        auto index = m_boxes.findMatching([&](auto& entry) {
            return entry.box.get() == &box;
        });
        if (index == notFound)
            return nullptr;
        return m_boxes[index].renderer;
    }

    if (m_boxToRendererMap.isEmpty()) {
        for (auto& entry : m_boxes)
            m_boxToRendererMap.add(entry.box.get(), entry.renderer);
    }
    return m_boxToRendererMap.get(&box);
}

}
}

#endif


