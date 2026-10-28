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

#include "FormattingContextBoxIterator.h"
#include "InlineWalker.h"
#include "LayoutElementBox.h"
#include "LayoutInlineTextBox.h"
#include "RenderBlock.h"
#include "RenderBlockFlow.h"
#include "RenderBlockFlowInlines.h"
#include "RenderButton.h"
#include "RenderChildIterator.h"
#include "RenderCombineText.h"
#include "RenderCounter.h"
#include "RenderElementInlines.h"
#include "RenderGlyph.h"
#include "RenderGrid.h"
#include "RenderImage.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderListOutsideMarker.h"
#include "RenderMenuList.h"
#include "RenderObjectInlines.h"
#include "RenderSVGInline.h"
#include "RenderSlider.h"
#include "RenderTable.h"
#include "RenderTextControl.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "TextUtil.h"

#if ENABLE(TREE_DEBUGGING)
#include "LayoutIntegrationLineLayout.h"
#endif

namespace WebCore {
namespace LayoutIntegration {

static std::unique_ptr<Style::ComputedStyle> firstLineStyleFor(const RenderObject& renderer)
{
    CheckedRef firstLineStyle = renderer.firstLineStyle();
    if (&renderer.style() == firstLineStyle.ptr())
        return { };
    return Style::ComputedStyle::clonePtr(firstLineStyle.get());
}

static Layout::Box::IsAnonymous LEGACY_NODELETE isAnonymous(const RenderObject& renderer)
{
    return renderer.isAnonymous() ? Layout::Box::IsAnonymous::Yes : Layout::Box::IsAnonymous::No;
}

static Layout::Box::ElementAttributes elementAttributes(const RenderElement& renderer)
{
    auto nodeType = [&] {
        if (is<RenderListOutsideMarker>(renderer))
            return Layout::Box::NodeType::ListMarker;
        if (is<RenderReplaced>(renderer))
            return is<RenderImage>(renderer) && !renderer.isRenderMedia() ? Layout::Box::NodeType::Image : Layout::Box::NodeType::ReplacedElement;
        if (isAnyOf<RenderButton, RenderMenuList, RenderTextControlInnerContainer, RenderSlider>(renderer) || renderer.isRenderSliderContainer())
            return Layout::Box::NodeType::ImplicitFlexBox;
        if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer))
            return renderLineBreak->isWBR() ? Layout::Box::NodeType::WordBreakOpportunity : Layout::Box::NodeType::LineBreak;
        if (is<RenderTable>(renderer))
            return Layout::Box::NodeType::TableBox;
        if (renderer.isInlineBox())
            return Layout::Box::NodeType::InlineBox;
        return Layout::Box::NodeType::GenericElement;
    }();

    return { nodeType, isAnonymous(renderer) };
}

BoxTreeUpdater::BoxTreeUpdater(RenderBlock& rootRenderer)
    : m_rootRenderer(rootRenderer)
{
}

BoxTreeUpdater::~BoxTreeUpdater() = default;

Layout::ElementBox& BoxTreeUpdater::rootLayoutBox()
{
    return m_rootRenderer.layoutBox();
}

void BoxTreeUpdater::build()
{
    // Another live formatting context that has the root as content keeps its box current.
    if (!isInLiveFormattingContext(m_rootRenderer))
        initializeLayoutBox(m_rootRenderer);

    auto isAnonymousTextIndentCandidateForIntegration = [&] {
        if (!m_rootRenderer.isAnonymousBlock())
            return false;
        if (m_rootRenderer.isBlockBox())
            return !m_rootRenderer.parent() || m_rootRenderer.parent()->firstChild() == &m_rootRenderer;
        // E.g. flex and grid items are not block boxes (they are block containers though).
        return m_rootRenderer.isBlockContainer();
    };
    rootLayoutBox().setIsAnonymousTextIndentCandidateForIntegration(isAnonymousTextIndentCandidateForIntegration());

    if (is<RenderBlockFlow>(m_rootRenderer))
        initializeInlineContent(m_rootRenderer);
    else if (is<RenderGrid>(m_rootRenderer))
        initializeGridContent();
    else
        ASSERT_NOT_IMPLEMENTED_YET();
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
    }

    if (!isInLiveFormattingContext(m_rootRenderer))
        rootLayoutBox().clearFormattingContextData();
    rootLayoutBox().setIsAnonymousTextIndentCandidateForIntegration(false);
}

static Layout::ElementBox::IsListMarkerImage isListMarkerImage(const RenderListOutsideMarker& listMarkerRenderer)
{
    return listMarkerRenderer.isImage() ? Layout::ElementBox::IsListMarkerImage::Yes : Layout::ElementBox::IsListMarkerImage::No;
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

void BoxTreeUpdater::initializeLayoutBox(RenderObject& renderer)
{
    std::unique_ptr<Style::ComputedStyle> firstLineStyle = firstLineStyleFor(renderer);

    if (auto* textRenderer = dynamicDowncast<RenderText>(renderer)) {
        auto style = Style::ComputedStyle::createAnonymousStyleWithDisplay(textRenderer->style(), Style::DisplayType::InlineFlow);
        auto isCombinedText = [&] {
            auto* combineTextRenderer = dynamicDowncast<RenderCombineText>(*textRenderer);
            return combineTextRenderer && combineTextRenderer->isCombined();
        }();
        auto text = style.textSecurity() == TextSecurity::None
            ? (isCombinedText ? textRenderer->originalText() : textRenderer->text())
            : RenderBlock::updateSecurityDiscCharacters(style, isCombinedText ? textRenderer->originalText() : String { textRenderer->text() });

        auto canUseSimpleFontCodePath = textRenderer->canUseSimpleFontCodePath();
        auto canUseSimplifiedTextMeasuring = textRenderer->canUseSimplifiedTextMeasuring();
        if (!canUseSimplifiedTextMeasuring) {
            canUseSimplifiedTextMeasuring = canUseSimpleFontCodePath && Layout::TextUtil::canUseSimplifiedTextMeasuring(text, style.fontCascade(), style.collapseWhiteSpace(), firstLineStyle.get());
            textRenderer->setCanUseSimplifiedTextMeasuring(*canUseSimplifiedTextMeasuring);
        }

        auto hasPositionDependentContentWidth = textRenderer->hasPositionDependentContentWidth();
        if (!hasPositionDependentContentWidth) {
            hasPositionDependentContentWidth = Layout::TextUtil::hasPositionDependentContentWidth(text);
            textRenderer->setHasPositionDependentContentWidth(*hasPositionDependentContentWidth);
        }

        auto hasStrongDirectionalityContent = textRenderer->hasStrongDirectionalityContent();
        if (!hasStrongDirectionalityContent) {
            hasStrongDirectionalityContent = Layout::TextUtil::containsStrongDirectionalityText(text);
            textRenderer->setHasStrongDirectionalityContent(*hasStrongDirectionalityContent);
        }

        auto contentCharacteristic = EnumSet<Layout::InlineTextBox::ContentCharacteristic> { };
        if (canUseSimpleFontCodePath)
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimpleFontCodepath);
        if (textRenderer->shouldUseSimpleGlyphOverflowCodePath())
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::ShouldUseSimpleGlyphOverflowCodePath);
        if (*canUseSimplifiedTextMeasuring)
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimplifiedContentMeasuring);
        if (*hasPositionDependentContentWidth)
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasPositionDependentContentWidth);
        if (*hasStrongDirectionalityContent)
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasStrongDirectionalityContent);
        if (synthesizesGlyph(*textRenderer))
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasSynthesizedGlyph);

        CheckedRef inlineTextBox = textRenderer->layoutBox();
        inlineTextBox->setContent(text, contentCharacteristic);
        inlineTextBox->setIsCombined(isCombinedText);
        inlineTextBox->updateStyle(WTF::move(style), WTF::move(firstLineStyle));
        return;
    }

    // The element box shares the renderer's style.
    auto& renderElement = downcast<RenderElement>(renderer);
    CheckedRef elementBox = renderElement.layoutBox();
    elementBox->setElementAttributes(elementAttributes(renderElement));
    elementBox->setFirstLineStyle(WTF::move(firstLineStyle));
    if (CheckedPtr listMarkerRenderer = dynamicDowncast<RenderListOutsideMarker>(renderElement))
        elementBox->setIsListMarkerImage(isListMarkerImage(*listMarkerRenderer));
}

void BoxTreeUpdater::initializeInlineContent(const RenderElement& container)
{
    for (auto walker = InlineWalker(container); !walker.atEnd(); walker.advance()) {
        CheckedRef childRenderer = *walker.current();
        if (Layout::isExcludedFromLayoutBoxNavigation(childRenderer) || childRenderer->layoutBox().isIntegrationRoot())
            continue;
        initializeLayoutBox(childRenderer);
    }
}

void BoxTreeUpdater::initializeGridContent()
{
    for (CheckedRef gridItemRenderer : childrenOfType<RenderElement>(m_rootRenderer)) {
        if (gridItemRenderer->layoutBox().isIntegrationRoot())
            continue;
        initializeLayoutBox(gridItemRenderer);
    }
}

static void updateContentCharacteristic(const RenderText& rendererText, Layout::InlineTextBox& inlineTextBox)
{
    CheckedRef rendererStyle = rendererText.style();
    auto shouldUpdateContentCharacteristic = !rendererStyle->fontCascadeEqual(inlineTextBox.style());
    if (!shouldUpdateContentCharacteristic)
        return;

    auto contentCharacteristic = EnumSet<Layout::InlineTextBox::ContentCharacteristic> { };
    // These may only change when content changes.
    if (inlineTextBox.canUseSimpleFontCodePath())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimpleFontCodepath);
    if (inlineTextBox.shouldUseSimpleGlyphOverflowCodePath())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::ShouldUseSimpleGlyphOverflowCodePath);
    if (inlineTextBox.hasPositionDependentContentWidth())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasPositionDependentContentWidth);
    if (inlineTextBox.hasStrongDirectionalityContent())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasStrongDirectionalityContent);
    if (inlineTextBox.hasSynthesizedGlyph())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasSynthesizedGlyph);

    if (inlineTextBox.canUseSimpleFontCodePath() && Layout::TextUtil::canUseSimplifiedTextMeasuring(inlineTextBox.content(), rendererStyle->fontCascade(), rendererStyle->collapseWhiteSpace(), &rendererText.firstLineStyle()))
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimplifiedContentMeasuring);

    inlineTextBox.setContentCharacteristic(contentCharacteristic);
}

void BoxTreeUpdater::updateStyle(const RenderObject& renderer)
{
    if (auto* renderText = dynamicDowncast<RenderText>(renderer)) {
        CheckedRef inlineTextBox = const_cast<Layout::InlineTextBox&>(renderText->layoutBox());
        updateContentCharacteristic(*renderText, inlineTextBox);
        inlineTextBox->updateStyle(Style::ComputedStyle::createAnonymousStyleWithDisplay(renderText->style(), Style::DisplayType::InlineFlow), firstLineStyleFor(*renderText));
        return;
    }

    // The element box shares the renderer's style.
    CheckedRef elementBox = const_cast<Layout::ElementBox&>(downcast<RenderElement>(renderer).layoutBox());
    elementBox->setFirstLineStyle(firstLineStyleFor(renderer));
    if (auto* listMarkerRenderer = dynamicDowncast<RenderListOutsideMarker>(renderer))
        elementBox->setIsListMarkerImage(isListMarkerImage(*listMarkerRenderer));
}

void BoxTreeUpdater::updateContent(const RenderText& textRenderer)
{
    CheckedRef inlineTextBox = const_cast<Layout::InlineTextBox&>(textRenderer.layoutBox());
    CheckedRef style = inlineTextBox->style();
    auto isCombinedText = [&] {
        auto* combineTextRenderer = dynamicDowncast<RenderCombineText>(textRenderer);
        return combineTextRenderer && combineTextRenderer->isCombined();
    }();
    auto text = style->textSecurity() == TextSecurity::None ? (isCombinedText ? textRenderer.originalText() : String { textRenderer.text() }) : RenderBlock::updateSecurityDiscCharacters(style.get(), isCombinedText ? textRenderer.originalText() : String { textRenderer.text() });
    auto contentCharacteristic = EnumSet<Layout::InlineTextBox::ContentCharacteristic> { };
    if (textRenderer.canUseSimpleFontCodePath())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimpleFontCodepath);
    if (textRenderer.canUseSimpleFontCodePath() && Layout::TextUtil::canUseSimplifiedTextMeasuring(text, style->fontCascade(), style->collapseWhiteSpace(), &inlineTextBox->firstLineStyle()))
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimplifiedContentMeasuring);
    if (textRenderer.shouldUseSimpleGlyphOverflowCodePath())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::ShouldUseSimpleGlyphOverflowCodePath);
    auto hasPositionDependentContentWidth = textRenderer.hasPositionDependentContentWidth();
    if (!hasPositionDependentContentWidth) {
        hasPositionDependentContentWidth = Layout::TextUtil::hasPositionDependentContentWidth(text);
        const_cast<RenderText&>(textRenderer).setHasPositionDependentContentWidth(*hasPositionDependentContentWidth);
    }
    if (*hasPositionDependentContentWidth)
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasPositionDependentContentWidth);
    auto hasStrongDirectionalityContent = textRenderer.hasStrongDirectionalityContent();
    if (!hasStrongDirectionalityContent) {
        hasStrongDirectionalityContent = Layout::TextUtil::containsStrongDirectionalityText(text);
        const_cast<RenderText&>(textRenderer).setHasStrongDirectionalityContent(*hasStrongDirectionalityContent);
    }
    if (*hasStrongDirectionalityContent)
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasStrongDirectionalityContent);
    if (synthesizesGlyph(textRenderer))
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasSynthesizedGlyph);

    inlineTextBox->setContent(text, contentCharacteristic);
}

void BoxTreeUpdater::insert(RenderObject& child)
{
    initializeLayoutBox(child);
    // An inserted inline box may come with content.
    if (CheckedPtr inlineBox = dynamicDowncast<RenderElement>(child); inlineBox && inlineBox->isInlineBox())
        initializeInlineContent(*inlineBox);
}

void BoxTreeUpdater::remove(RenderObject& child)
{
    // Only text and line breaks are removed without invalidating the whole formatting context.
    ASSERT(!is<RenderElement>(child) || !downcast<RenderElement>(child).firstChild());
    child.layoutBox().clearFormattingContextData();
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
