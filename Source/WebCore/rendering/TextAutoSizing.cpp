/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "TextAutoSizing.h"

#if ENABLE(TEXT_AUTOSIZING)

#include "CSSFontSelector.h"
#include "Document.h"
#include "FontCascade.h"
#include "Logging.h"
#include "RenderBlock.h"
#include "RenderListMarker.h"
#include "RenderObjectInlines.h"
#include "RenderStyleSetters.h"
#include "RenderText.h"
#include "RenderTextFragment.h"
#include "RenderTreeBuilder.h"
#include "Settings.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleResolver.h"
#include "StyleTextSizeAdjust.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextAutoSizingValue);
WTF_MAKE_TZONE_ALLOCATED_IMPL(TextAutoSizing);

static RenderStyle cloneRenderStyleWithState(const RenderStyle& currentStyle)
{
    auto newStyle = RenderStyle::clone(currentStyle);

    // FIXME: This should probably handle at least ::first-line too.
    if (auto* firstLetterStyle = currentStyle.getCachedPseudoStyle({ PseudoElementType::FirstLetter }))
        newStyle.addCachedPseudoStyle(makeUnique<RenderStyle>(RenderStyle::clone(*firstLetterStyle)));

    if (currentStyle.lastChildState())
        newStyle.setLastChildState();
    if (currentStyle.firstChildState())
        newStyle.setFirstChildState();
    return newStyle;
}

TextAutoSizingKey::TextAutoSizingKey(DeletedTag)
{
    HashTraits<std::unique_ptr<RenderStyle>>::constructDeletedValue(m_style);
}

TextAutoSizingKey::TextAutoSizingKey(const RenderStyle& style, unsigned hash)
    : m_style(RenderStyle::clonePtr(style)) // FIXME: This seems very inefficient.
    , m_hash(hash)
{
}

void TextAutoSizingValue::addTextNode(Text& node, float size)
{
    node.renderer()->setCandidateComputedTextSize(size);
    m_autoSizedNodes.add(&node);
}

auto TextAutoSizingValue::adjustTextNodeSizes() -> StillHasNodes
{
    // Remove stale nodes. Nodes may have had their renderers detached. We'll also need to remove the style from the documents m_textAutoSizedNodes
    // collection. Return true indicates we need to do that removal.
    Vector<Text*> nodesForRemoval;
    for (auto& textNode : m_autoSizedNodes) {
        auto* renderer = textNode->renderer();
        if (!renderer || !renderer->style().textSizeAdjust().isAuto() || !renderer->candidateComputedTextSize())
            nodesForRemoval.append(textNode.get());
    }

    for (auto& node : nodesForRemoval)
        m_autoSizedNodes.remove(node);

    StillHasNodes stillHasNodes = m_autoSizedNodes.isEmpty() ? StillHasNodes::No : StillHasNodes::Yes;

    // If we only have one piece of text with the style on the page don't adjust it's size.
    if (m_autoSizedNodes.size() <= 1)
        return stillHasNodes;

    // Compute average size.
    float cumulativeSize = 0;
    for (auto& node : m_autoSizedNodes)
        cumulativeSize += node->renderer()->candidateComputedTextSize();

    float averageSize = std::round(cumulativeSize / m_autoSizedNodes.size());

    // FIXME: Figure out how to make this code use RenderTreeUpdater/Builder properly.
    RenderTreeBuilder builder((*m_autoSizedNodes.begin())->renderer()->view());

    // Adjust sizes.
    bool firstPass = true;
    for (auto& node : m_autoSizedNodes) {
        auto& renderer = *node->renderer();
        if (renderer.style().fontDescription().computedSize() == averageSize)
            continue;

        float specifiedSize = renderer.style().fontDescription().specifiedSize();
        float maxScaleIncrease = renderer.settings().maxTextAutosizingScaleIncrease();
        float scaleChange = averageSize / specifiedSize;
        if (scaleChange > maxScaleIncrease && firstPass) {
            firstPass = false;
            averageSize = std::round(specifiedSize * maxScaleIncrease);
            scaleChange = averageSize / specifiedSize;
        }

        LOG(TextAutosizing, "  adjust node size %p firstPass=%d averageSize=%f scaleChange=%f", node.get(), firstPass, averageSize, scaleChange);

        auto* parentRenderer = renderer.parent();

        auto style = cloneRenderStyleWithState(renderer.style());
        auto fontDescription = style.fontDescription();
        fontDescription.setComputedSize(averageSize);
        style.setFontDescription(FontCascadeDescription { fontDescription });
        parentRenderer->setStyle(WTFMove(style));

        if (parentRenderer->isAnonymousBlock())
            parentRenderer = parentRenderer->parent();

        // If we have a list we should resize ListMarkers separately.
        if (auto* listMarkerRenderer = dynamicDowncast<RenderListMarker>(*parentRenderer->firstChild())) {
            auto style = cloneRenderStyleWithState(listMarkerRenderer->style());
            style.setFontDescription(FontCascadeDescription { fontDescription });
            listMarkerRenderer->setStyle(WTFMove(style));
        }

        // Resize the line height of the parent.
        auto& parentStyle = parentRenderer->style();
        auto& lineHeightLength = parentStyle.specifiedLineHeight();

        int specifiedLineHeight = WTF::switchOn(lineHeightLength,
            [&](const CSS::Keyword::Normal&) {
                return 0;
            },
            [&](const Style::LineHeight::Fixed& fixed) {
                return Style::evaluate<LayoutUnit>(fixed, Style::ZoomFactor { 1.0f, parentStyle.deviceScaleFactor() }).toInt();
            },
            [&](const Style::LineHeight::Percentage& percentage) {
                return Style::evaluate<LayoutUnit>(percentage, LayoutUnit { fontDescription.specifiedSize() }).toInt();
            },
            [&](const Style::LineHeight::Calc&) {
                return 0;
            }
        );

        // This calculation matches the line-height computed size calculation in StyleBuilderCustom::applyValueLineHeight().
        int lineHeight = specifiedLineHeight * scaleChange;
        if (auto fixedLineHeight = lineHeightLength.tryFixed(); fixedLineHeight && fixedLineHeight->resolveZoom(Style::ZoomFactor { 1.0f, parentStyle.deviceScaleFactor() }) == lineHeight)
            continue;

        auto newParentStyle = cloneRenderStyleWithState(parentStyle);
        newParentStyle.setLineHeight(lineHeightLength.isNormal() ? Style::LineHeight { lineHeightLength } : Style::LineHeight { Style::LineHeight::Fixed { static_cast<float>(lineHeight) } });
        newParentStyle.setSpecifiedLineHeight(Style::LineHeight { lineHeightLength });
        newParentStyle.setFontDescription(WTFMove(fontDescription));
        parentRenderer->setStyle(WTFMove(newParentStyle));

        builder.updateAfterDescendants(*parentRenderer);
    }

    for (auto& node : m_autoSizedNodes) {
        auto* textRenderer = dynamicDowncast<RenderTextFragment>(*node->renderer());
        if (!textRenderer)
            continue;
        auto* block = textRenderer->blockForAccompanyingFirstLetter();
        if (!block)
            continue;

        auto [firstLetter, firstLetterContainer] = block->firstLetterAndContainer();
        if (firstLetter && firstLetter->parent() && firstLetter->parent()->parent()) {
            auto& parentStyle = firstLetter->parent()->parent()->style();
            auto* firstLetterStyle = parentStyle.getCachedPseudoStyle({ PseudoElementType::FirstLetter });
            if (!firstLetterStyle)
                continue;
            auto fontDescription = firstLetterStyle->fontDescription();
            fontDescription.setComputedSize(averageSize * fontDescription.specifiedSize() / parentStyle.fontDescription().specifiedSize());
            firstLetterStyle->setFontDescription(FontCascadeDescription { fontDescription });
        }

        builder.updateAfterDescendants(*block);
    }

    return stillHasNodes;
}

TextAutoSizingValue::~TextAutoSizingValue()
{
    reset();
}

void TextAutoSizingValue::reset()
{
    for (auto& node : m_autoSizedNodes) {
        auto* renderer = node->renderer();
        if (!renderer)
            continue;

        auto* parentRenderer = renderer->parent();
        if (!parentRenderer)
            continue;

        // Reset the font size back to the original specified size
        auto fontDescription = renderer->style().fontDescription();
        float originalSize = fontDescription.specifiedSize();
        if (fontDescription.computedSize() != originalSize) {
            fontDescription.setComputedSize(originalSize);
            auto style = cloneRenderStyleWithState(renderer->style());
            style.setFontDescription(FontCascadeDescription { fontDescription });
            parentRenderer->setStyle(WTFMove(style));
        }

        // Reset the line height of the parent.
        if (parentRenderer->isAnonymousBlock())
            parentRenderer = parentRenderer->parent();

        auto& parentStyle = parentRenderer->style();
        auto& originalLineHeight = parentStyle.specifiedLineHeight();
        if (originalLineHeight == parentStyle.lineHeight())
            continue;

        auto newParentStyle = cloneRenderStyleWithState(parentStyle);
        newParentStyle.setLineHeight(Style::LineHeight { originalLineHeight });
        newParentStyle.setFontDescription(WTFMove(fontDescription));
        parentRenderer->setStyle(WTFMove(newParentStyle));
    }
}

void TextAutoSizing::addTextNode(Text& node, float candidateSize)
{
    LOG(TextAutosizing, " addAutoSizedNode %p candidateSize=%f", &node, candidateSize);
    auto addResult = m_textNodes.add<TextAutoSizingHashTranslator>(node.renderer()->style(), nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = makeUnique<TextAutoSizingValue>();
    addResult.iterator->value->addTextNode(node, candidateSize);
}

void TextAutoSizing::updateRenderTree()
{
    m_textNodes.removeIf([](auto& keyAndValue) {
        return keyAndValue.value->adjustTextNodeSizes() == TextAutoSizingValue::StillHasNodes::No;
    });
}

void TextAutoSizing::reset()
{
    m_textNodes.clear();
}

} // namespace WebCore

#endif // ENABLE(TEXT_AUTOSIZING)
