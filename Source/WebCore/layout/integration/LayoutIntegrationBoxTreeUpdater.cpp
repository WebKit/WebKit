/*
 * Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationBoxTreeUpdater.h"

#include "FontCascadeFonts.h"
#include "FontCascadeInlines.h"
#include "FontSelector.h"
#include "FormattingContextBoxIterator.h"
#include "LayoutElementBox.h"
#include "RenderBlock.h"
#include "RenderBlockFlow.h"
#include "RenderBlockFlowInlines.h"
#include "RenderCounter.h"
#include "RenderElementInlines.h"
#include "RenderGlyph.h"
#include "RenderListItem.h"
#include "RenderObjectInlines.h"
#include "RenderSVGInline.h"
#include "RenderText.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "TextUtil.h"
#include <wtf/unicode/CharacterNames.h>

#if ENABLE(TREE_DEBUGGING)
#include "LayoutIntegrationLineLayout.h"
#endif

namespace WebCore {
namespace LayoutIntegration {

BoxTreeUpdater::BoxTreeUpdater(RenderBlock& rootRenderer)
    : m_rootRenderer(rootRenderer)
{
}

BoxTreeUpdater::~BoxTreeUpdater() = default;

Layout::ElementBox& BoxTreeUpdater::rootLayoutBox()
{
    return m_rootRenderer.layoutBox();
}

void BoxTreeUpdater::tearDown()
{
    if (m_rootRenderer.renderTreeBeingDestroyed())
        return;

    for (CheckedRef layoutBox : formattingContextBoxes(rootLayoutBox())) {
        // Nested formatting context roots keep what their own live layout needs.
        if (layoutBox->isIntegrationRoot())
            continue;
        const_cast<Layout::Box&>(layoutBox.get()).clearFormattingContextData();
        // The inline items are gone, so content changes that waited for them can go in.
        if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(layoutBox->rendererForIntegration()))
            textRenderer->updateLayoutBoxContentIfNeeded();
    }

    if (!isInLiveFormattingContext(m_rootRenderer))
        rootLayoutBox().clearFormattingContextData();
}

static bool canUseSimplifiedMeasuring(const RenderText& renderer, StringView content)
{
    CheckedRef style = renderer.style();
    return Layout::TextUtil::canUseSimplifiedTextMeasuring(content, style->fontCascade(), style->collapseWhiteSpace(), &renderer.firstLineStyle());
}

static bool synthesizesGlyph(const RenderText& textRenderer)
{
    if (is<RenderGlyph>(textRenderer))
        return true;

    // The marker is this text's parent when it is an inline box, and its grandparent when a marker box holds the text in a content container of its own.
    for (CheckedPtr marker = textRenderer.parent(); marker; marker = marker->parent()) {
        if (marker->style().isListMarkerStyle())
            return listMarkerSynthesizesGlyph(marker->style(), protect(marker->document()));
        if (!marker->isAnonymous())
            return false;
    }
    return false;
}

static constexpr float textCombineMargin = 1.15f; // Allow em + 15% margin.

static std::unique_ptr<Layout::InlineTextBox::CombinedText> computeCombinedText(const RenderText& textRenderer, StringView content)
{
    CheckedRef style = textRenderer.style();
    // text-combine-upright works only in vertical typographic mode.
    if (style->textCombine() == TextCombine::None || !style->writingMode().isVerticalTypographic())
        return nullptr;

    RefPtr fontSelector = style->fontCascade().fontSelector();
    auto fontFor = [&](const FontCascadeDescription& description) {
        auto font = FontCascade { FontCascadeDescription { description }, style->fontCascade() };
        font.update(fontSelector.copyRef());
        font.setLetterSpacing(0);
        return font;
    };

    GlyphOverflow glyphOverflow;
    glyphOverflow.computeBounds = true;
    auto measure = [&](const FontCascade& font) {
        glyphOverflow.left = glyphOverflow.top = glyphOverflow.right = glyphOverflow.bottom = 0;
        if (content.length() == 1 && content[0] == space)
            return font.widthOfSpaceString();
        auto run = RenderBlock::constructTextRun(content, style);
        run.setCharacterScanForCodePath(!textRenderer.canUseSimpleFontCodePath());
        run.setTabSize(!style->collapseWhiteSpace(), Style::toPlatform(style->tabSize(), style->usedZoomForLength()));
        return font.width(run, nullptr, &glyphOverflow);
    };
    auto combinedText = [&](FontCascade&& font, float width) {
        return makeUnique<Layout::InlineTextBox::CombinedText>(Layout::InlineTextBox::CombinedText { WTF::move(font), width, glyphOverflow.top, glyphOverflow.bottom });
    };

    auto description = style->fontCascade().fontDescription();
    float emWidth = description.usedSize() * textCombineMargin;
    // Combined text is drawn horizontally.
    description.setOrientation(FontOrientation::Horizontal);

    auto horizontalFont = fontFor(description);
    auto width = measure(horizontalFont);
    if (width <= emWidth)
        return combinedText(WTF::move(horizontalFont), width);

    // Try compressed glyphs.
    float bestFitDelta = width - emWidth;
    auto bestFitDescription = description;
    static constexpr auto widthVariants = WTF::toArray<FontWidthVariant>({ FontWidthVariant::HalfWidth, FontWidthVariant::ThirdWidth, FontWidthVariant::QuarterWidth });
    for (auto widthVariant : widthVariants) {
        // When modifying this, make sure to keep it in sync with FontPlatformData::isForTextCombine()!
        description.setWidthVariant(widthVariant);
        auto compressedFont = fontFor(description);
        auto runWidth = measure(compressedFont);
        if (runWidth <= emWidth)
            return combinedText(WTF::move(compressedFont), runWidth);
        if (auto widthDelta = runWidth - emWidth; widthDelta < bestFitDelta) {
            bestFitDelta = widthDelta;
            bestFitDescription = description;
        }
    }

    // Then a smaller size.
    float scaleFactor = std::max(0.4f, emWidth / (emWidth + bestFitDelta));
    float originalSize = bestFitDescription.usedSize();
    do {
        bestFitDescription.setUsedSize(originalSize * scaleFactor);
        auto scaledFont = fontFor(bestFitDescription);
        auto runWidth = measure(scaledFont);
        if (runWidth <= emWidth)
            return combinedText(WTF::move(scaledFont), runWidth);
        scaleFactor -= 0.05f;
    } while (scaleFactor >= 0.4f);
    return nullptr;
}

bool BoxTreeUpdater::updateCombinedText(RenderText& textRenderer)
{
    CheckedRef inlineTextBox = textRenderer.layoutBox();
    auto wasCombined = inlineTextBox->isCombined();
    inlineTextBox->setCombinedText(computeCombinedText(textRenderer, inlineTextBox->content()));
    return wasCombined != inlineTextBox->isCombined();
}

void BoxTreeUpdater::updateContent(RenderText& textRenderer)
{
    ASSERT(textRenderer.parent());
    CheckedRef style = textRenderer.style();
    auto content = textRenderer.text();
    if (style->textSecurity() != TextSecurity::None)
        content = RenderBlock::updateSecurityDiscCharacters(style, WTF::move(content));

    using ContentCharacteristic = Layout::InlineTextBox::ContentCharacteristic;
    auto contentCharacteristics = EnumSet<ContentCharacteristic> { };
    if (textRenderer.canUseSimpleFontCodePath()) {
        contentCharacteristics.add(ContentCharacteristic::CanUseSimpleFontCodepath);
        if (canUseSimplifiedMeasuring(textRenderer, content))
            contentCharacteristics.add(ContentCharacteristic::CanUseSimplifiedContentMeasuring);
    }
    if (textRenderer.shouldUseSimpleGlyphOverflowCodePath())
        contentCharacteristics.add(ContentCharacteristic::ShouldUseSimpleGlyphOverflowCodePath);
    if (Layout::TextUtil::hasPositionDependentContentWidth(content))
        contentCharacteristics.add(ContentCharacteristic::HasPositionDependentContentWidth);
    if (Layout::TextUtil::containsStrongDirectionalityText(content))
        contentCharacteristics.add(ContentCharacteristic::HasStrongDirectionalityContent);
    if (synthesizesGlyph(textRenderer))
        contentCharacteristics.add(ContentCharacteristic::HasSynthesizedGlyph);

    CheckedRef inlineTextBox = textRenderer.layoutBox();
    inlineTextBox->setContent(WTF::move(content), contentCharacteristics);
    inlineTextBox->setCombinedText(computeCombinedText(textRenderer, inlineTextBox->content()));
}

void BoxTreeUpdater::updateSimplifiedMeasuring(RenderText& textRenderer)
{
    CheckedRef inlineTextBox = textRenderer.layoutBox();
    inlineTextBox->setCanUseSimplifiedContentMeasuring(inlineTextBox->canUseSimpleFontCodePath() && canUseSimplifiedMeasuring(textRenderer, inlineTextBox->content()));
}

Layout::Box::ElementAttributes BoxTreeUpdater::elementAttributes(const RenderObject& renderer)
{
    auto nodeType = [&] {
        if (renderer.isRenderListOutsideMarker())
            return Layout::Box::NodeType::ListMarker;
        if (renderer.isRenderReplaced())
            return renderer.isRenderImage() && !renderer.isRenderMedia() ? Layout::Box::NodeType::Image : Layout::Box::NodeType::ReplacedElement;
        if (renderer.isRenderButton() || renderer.isRenderMenuList() || renderer.isRenderTextControlInnerContainer() || renderer.isRenderSlider() || renderer.isRenderSliderContainer())
            return Layout::Box::NodeType::ImplicitFlexBox;
        if (renderer.isRenderLineBreak())
            return renderer.isWBR() ? Layout::Box::NodeType::WordBreakOpportunity : Layout::Box::NodeType::LineBreak;
        if (renderer.isRenderTable())
            return Layout::Box::NodeType::TableBox;
        if (renderer.isInlineBox())
            return Layout::Box::NodeType::InlineBox;
        return Layout::Box::NodeType::GenericElement;
    }();
    return { nodeType, renderer.isAnonymous() ? Layout::Box::IsAnonymous::Yes : Layout::Box::IsAnonymous::No };
}

void BoxTreeUpdater::updateFirstLineStyleState(RenderElement& renderer)
{
    // As RenderElement::firstLineStyle() finds it. Anonymous blocks use the parent's, if they are its first in-flow child.
    auto mayHaveFirstLineStyle = [&] {
        if (renderer.isAnonymousBlock()) {
            CheckedPtr parent = renderer.parent();
            return parent && parent->style().pseudoElementStyle({ PseudoElementType::FirstLine });
        }
        return !!renderer.style().pseudoElementStyle({ PseudoElementType::FirstLine });
    };
    renderer.layoutBox().setMayHaveFirstLineStyle(mayHaveFirstLineStyle());
}

void BoxTreeUpdater::remove(RenderObject& child)
{
    // Only text and line breaks are removed without invalidating the whole formatting context.
    ASSERT(!is<RenderElement>(child) || !downcast<RenderElement>(child).firstChild());
    child.layoutBox().clearFormattingContextData();
    // The inline items are gone, so content changes that waited for them can go in.
    if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(child))
        textRenderer->updateLayoutBoxContentIfNeeded();
}

#if ENABLE(TREE_DEBUGGING)
void showInlineContent(TextStream& stream, const InlineContent& inlineContent, size_t depth, bool isDamaged)
{
    auto& lines = inlineContent.displayContent().lines;
    auto& boxes = inlineContent.displayContent().boxes;

    for (size_t lineIndex = 0, boxIndex = 0; lineIndex < lines.size() && boxIndex < boxes.size(); ++lineIndex) {
        auto addSpacing = [&](auto& streamToUse) {
            size_t printedCharacters = 0;
            if (isDamaged)
                streamToUse << "           -+";
            else
                streamToUse << "           --";
            while (++printedCharacters <= depth * 2)
                streamToUse << " ";

        };
        addSpacing(stream);
        auto& line = lines[lineIndex];
        stream << "line at (" << line.lineBoxLeft() << "," << line.lineBoxTop() << ") size (" << line.lineBoxRight() - line.lineBoxLeft() << "x" << line.lineBoxBottom() - line.lineBoxTop() << ") baseline (" << line.baseline() << ") enclosing top (" << line.enclosingContentLogicalTop() << ") bottom (" << line.enclosingContentLogicalBottom() << ") ";
        if (line.hasEllipsis())
            stream << "truncated with ellipsis.";
        stream.nextLine();

        addSpacing(stream);

        auto& rootInlineBox = boxes[boxIndex++];
        auto rootInlineBoxRect = rootInlineBox.visualRectIgnoringBlockDirection();
        stream << "  ";
        stream << "Root inline box at (" << rootInlineBoxRect.x() << "," << rootInlineBoxRect.y() << ")" << " size (" << rootInlineBoxRect.width() << "x" << rootInlineBoxRect.height() << ")";
        stream.nextLine();

        auto inlineBoxStream = TextStream { TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect };
        auto runStream = TextStream { TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect };
        for (; boxIndex < boxes.size(); ++boxIndex) {
            auto& box = boxes[boxIndex];
            if (box.lineIndex() != lineIndex)
                break;

            if (box.isNonRootInlineBox()) {
                addSpacing(inlineBoxStream);
                inlineBoxStream << "  ";

                for (auto* ancestor = &box.layoutBox(); ancestor != &rootInlineBox.layoutBox(); ancestor = &ancestor->parent())
                    inlineBoxStream << "  ";
                auto rect = box.visualRectIgnoringBlockDirection();
                inlineBoxStream << "Inline box at (" << rect.x() << "," << rect.y() << ") size (" << rect.width() << "x" << rect.height() << ")";
                if (isDamaged)
                    inlineBoxStream << " (renderer may be damaged)";
                else
                    inlineBoxStream << " renderer->(" << box.layoutBox().rendererForIntegration() << ")";
                inlineBoxStream.nextLine();
            } else {
                addSpacing(runStream);
                runStream << "    ";

                if (box.isText())
                    runStream << "Text";
                else if (box.isWordSeparator())
                    runStream << "Word separator";
                else if (box.isLineBreak())
                    runStream << "Line break";
                else if (box.isAtomicInlineBox())
                    runStream << "Atomic box";
                else if (box.isBlockLevelBox())
                    runStream << "Block level box";
                else if (box.isGenericInlineLevelBox())
                    runStream << "Generic inline level box";
                runStream << " at (" << box.left() << "," << box.top() << ") size " << box.width() << "x" << box.height();
                if (box.isText())
                    runStream << " run(" << box.text().start() << ", " << box.text().end() << ")";
                if (isDamaged)
                    runStream << " (renderer may be damaged)";
                else
                    runStream << " renderer->(" << box.layoutBox().rendererForIntegration() << ")";
                runStream.nextLine();
            }
        }

        stream << inlineBoxStream.release();

        addSpacing(stream);
        stream << "  ";
        stream << "Run(s):";
        stream.nextLine();

        stream << runStream.release();
    }
}
#endif

}
}
