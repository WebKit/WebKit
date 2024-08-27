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

#include "FormattingContextBoxIterator.h"
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
#include "RenderStyleSetters.h"
#include "RenderTable.h"
#include "RenderView.h"
#include "TextUtil.h"

#if ENABLE(TREE_DEBUGGING)
#include "LayoutIntegrationLineLayout.h"
#endif

namespace WebCore {
namespace LayoutIntegration {

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
        if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer))
            return renderLineBreak->isWBR() ? Layout::Box::NodeType::WordBreakOpportunity : Layout::Box::NodeType::LineBreak;
        if (is<RenderTable>(renderer))
            return Layout::Box::NodeType::TableBox;
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
        rootBox->setIsFirstChildForIntegration(!rootRenderer.parent() || rootRenderer.parent()->firstChild() == &rootRenderer);
        buildTreeForInlineContent();
    } else if (is<RenderFlexibleBox>(rootRenderer))
        buildTreeForFlexContent();
    else
        ASSERT_NOT_IMPLEMENTED_YET();
}

BoxTree::~BoxTree()
{
    Vector<CheckedRef<Layout::Box>> boxesToDetach;
    for (auto& constLayoutBox : formattingContextBoxes(rootLayoutBox())) {
        auto& layoutBox = const_cast<Layout::Box&>(constLayoutBox);
        auto* renderer = layoutBox.rendererForIntegration();
        if (!renderer)
            continue;

        auto* renderBlockFlow = dynamicDowncast<RenderBlockFlow>(*renderer);
        auto isLFCInlineBlock = renderBlockFlow && renderBlockFlow->modernLineLayout();
        if (isLFCInlineBlock)
            boxesToDetach.append(layoutBox);
    }

    for (auto& toDetach : boxesToDetach) {
        auto detachedBox = toDetach->removeFromParent();
        initialContainingBlock().appendChild(WTFMove(detachedBox));
    }

    if (&rootLayoutBox().parent() == &initialContainingBlock())
        rootLayoutBox().removeFromParent();
    else
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
        if (auto* renderInline = dynamicDowncast<RenderInline>(renderer)) {
            auto shouldNotRetainBorderPaddingAndMarginStart = renderInline->isContinuation();
            auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline->isContinuation() && renderInline->inlineContinuation();
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
            if ((styleToAdjust.display() == DisplayType::RubyBase || styleToAdjust.display() == DisplayType::RubyAnnotation) && renderInline->parent()->style().display() != DisplayType::Ruby)
                styleToAdjust.setDisplay(DisplayType::Inline);
            return;
        }
        if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer)) {
            if (!styleToAdjust.hasOutOfFlowPosition()) {
                // Force in-flow display value to inline (see webkit.org/b/223151).
                styleToAdjust.setDisplay(DisplayType::Inline);
            }
            styleToAdjust.setFloating(Float::None);
            // Clear property should only apply on block elements, however,
            // it appears that browsers seem to ignore it on <br> inline elements.
            // https://drafts.csswg.org/css2/#propdef-clear
            if (renderLineBreak->isWBR())
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

    if (auto* textRenderer = dynamicDowncast<RenderText>(renderer)) {
        auto style = RenderStyle::createAnonymousStyleWithDisplay(textRenderer->style(), DisplayType::Inline);
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

        auto contentCharacteristic = OptionSet<Layout::InlineTextBox::ContentCharacteristic> { };
        if (canUseSimpleFontCodePath)
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimpledFontCodepath);
        if (*canUseSimplifiedTextMeasuring)
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimplifiedContentMeasuring);
        if (*hasPositionDependentContentWidth)
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasPositionDependentContentWidth);
        if (*hasStrongDirectionalityContent)
            contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasStrongDirectionalityContent);

        return makeUniqueRef<Layout::InlineTextBox>(text, isCombinedText, contentCharacteristic, WTFMove(style), WTFMove(firstLineStyle));
    }

    auto& renderElement = downcast<RenderElement>(renderer);

    auto style = RenderStyle::clone(renderer.style());
    adjustStyleIfNeeded(renderElement, style, firstLineStyle.get());

    if (auto* listMarkerRenderer = dynamicDowncast<RenderListMarker>(renderElement)) {
        OptionSet<Layout::ElementBox::ListMarkerAttribute> listMarkerAttributes;
        if (listMarkerRenderer->isImage())
            listMarkerAttributes.add(Layout::ElementBox::ListMarkerAttribute::Image);
        if (!listMarkerRenderer->isInside())
            listMarkerAttributes.add(Layout::ElementBox::ListMarkerAttribute::Outside);
        if (listMarkerRenderer->listItem() && !listMarkerRenderer->listItem()->notInList())
            listMarkerAttributes.add(Layout::ElementBox::ListMarkerAttribute::HasListElementAncestor);
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
        insertChild(childLayoutBox(), childRenderer, childRenderer.previousSibling());
    }
}

void BoxTree::buildTreeForFlexContent()
{
    for (auto& flexItemRenderer : childrenOfType<RenderElement>(m_rootRenderer)) {
        auto style = RenderStyle::clone(flexItemRenderer.style());
        auto flexItem = makeUniqueRef<Layout::ElementBox>(elementAttributes(flexItemRenderer), WTFMove(style));
        insertChild(WTFMove(flexItem), flexItemRenderer, flexItemRenderer.previousSibling());
    }
}

void BoxTree::insertChild(UniqueRef<Layout::Box> childBox, RenderObject& childRenderer, const RenderObject* beforeChild)
{
    auto& parentBox = *childRenderer.parent()->layoutBox();
    auto* beforeChildBox = beforeChild ? beforeChild->layoutBox() : nullptr;

    childRenderer.setLayoutBox(childBox);
    parentBox.insertChild(WTFMove(childBox), const_cast<Layout::Box*>(beforeChildBox));
}

static void updateContentCharacteristic(const RenderText& rendererText, Layout::InlineTextBox& inlineTextBox)
{
    auto& rendererStyle = rendererText.style();
    auto shouldUpdateContentCharacteristic = rendererStyle.fontCascade() != inlineTextBox.style().fontCascade();
    if (!shouldUpdateContentCharacteristic)
        return;

    auto contentCharacteristic = OptionSet<Layout::InlineTextBox::ContentCharacteristic> { };
    // These may only change when content changes.
    if (inlineTextBox.canUseSimpleFontCodePath())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimpledFontCodepath);
    if (inlineTextBox.hasPositionDependentContentWidth())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasPositionDependentContentWidth);
    if (inlineTextBox.hasStrongDirectionalityContent())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::HasStrongDirectionalityContent);

    if (inlineTextBox.canUseSimpleFontCodePath() && Layout::TextUtil::canUseSimplifiedTextMeasuring(inlineTextBox.content(), rendererStyle.fontCascade(), rendererStyle.collapseWhiteSpace(), &rendererText.firstLineStyle()))
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimplifiedContentMeasuring);

    inlineTextBox.setContentCharacteristic(contentCharacteristic);
}

static void updateListMarkerAttributes(const RenderListMarker& listMarkerRenderer, Layout::ElementBox& layoutBox)
{
    auto listMarkerAttributes = OptionSet<Layout::ElementBox::ListMarkerAttribute> { };
    if (listMarkerRenderer.isImage())
        listMarkerAttributes.add(Layout::ElementBox::ListMarkerAttribute::Image);
    if (!listMarkerRenderer.isInside())
        listMarkerAttributes.add(Layout::ElementBox::ListMarkerAttribute::Outside);
    if (listMarkerRenderer.listItem() && !listMarkerRenderer.listItem()->notInList())
        listMarkerAttributes.add(Layout::ElementBox::ListMarkerAttribute::HasListElementAncestor);

    layoutBox.setListMarkerAttributes(listMarkerAttributes);
}

void BoxTree::updateStyle(const RenderObject& renderer)
{
    auto& rendererStyle = renderer.style();
    auto* layoutBox = const_cast<Layout::Box*>(renderer.layoutBox());
    if (!layoutBox) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (auto* renderText = dynamicDowncast<RenderText>(renderer)) {
        if (auto* inlineTextBox = dynamicDowncast<Layout::InlineTextBox>(*layoutBox)) {
            updateContentCharacteristic(*renderText, *inlineTextBox);
            inlineTextBox->updateStyle(RenderStyle::createAnonymousStyleWithDisplay(rendererStyle, DisplayType::Inline), firstLineStyleFor(*renderText));
            return;
        }
        ASSERT_NOT_REACHED();
    }

    auto firstLineNewStyle = firstLineStyleFor(renderer);
    auto newStyle = RenderStyle::clone(rendererStyle);
    adjustStyleIfNeeded(downcast<RenderElement>(renderer), newStyle, firstLineNewStyle.get());
    layoutBox->updateStyle(WTFMove(newStyle), WTFMove(firstLineNewStyle));
    if (auto* listMarkerRenderer = dynamicDowncast<RenderListMarker>(renderer); listMarkerRenderer && is<Layout::ElementBox>(*layoutBox))
        updateListMarkerAttributes(*listMarkerRenderer, downcast<Layout::ElementBox>(*layoutBox));
}

void BoxTree::updateContent(const RenderText& textRenderer)
{
    auto& inlineTextBox = const_cast<Layout::InlineTextBox&>(*textRenderer.layoutBox());
    auto& style = inlineTextBox.style();
    auto isCombinedText = [&] {
        auto* combineTextRenderer = dynamicDowncast<RenderCombineText>(textRenderer);
        return combineTextRenderer && combineTextRenderer->isCombined();
    }();
    auto text = style.textSecurity() == TextSecurity::None ? (isCombinedText ? textRenderer.originalText() : String { textRenderer.text() }) : RenderBlock::updateSecurityDiscCharacters(style, isCombinedText ? textRenderer.originalText() : String { textRenderer.text() });
    auto contentCharacteristic = OptionSet<Layout::InlineTextBox::ContentCharacteristic> { };
    if (textRenderer.canUseSimpleFontCodePath())
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimpledFontCodepath);
    if (textRenderer.canUseSimpleFontCodePath() && Layout::TextUtil::canUseSimplifiedTextMeasuring(text, style.fontCascade(), style.collapseWhiteSpace(), &inlineTextBox.firstLineStyle()))
        contentCharacteristic.add(Layout::InlineTextBox::ContentCharacteristic::CanUseSimplifiedContentMeasuring);
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

    inlineTextBox.setContent(text, contentCharacteristic);
}

const Layout::Box& BoxTree::insert(const RenderElement& parent, RenderObject& child, const RenderObject* beforeChild)
{
    UNUSED_PARAM(parent);

    insertChild(createLayoutBox(child), child, beforeChild);
    return *child.layoutBox();
}

UniqueRef<Layout::Box> BoxTree::remove(const RenderElement& parent, RenderObject& child)
{
    UNUSED_PARAM(parent);
    ASSERT(child.layoutBox());

    auto* layoutBox = child.layoutBox();
    child.clearLayoutBox();

    return layoutBox->removeFromParent();
}

const Layout::ElementBox& BoxTree::rootLayoutBox() const
{
    return *m_rootRenderer.layoutBox();
}

Layout::ElementBox& BoxTree::rootLayoutBox()
{
    return *m_rootRenderer.layoutBox();
}

bool BoxTree::contains(const RenderElement& rendererToFind) const
{
    auto* boxToFind = rendererToFind.layoutBox();
    if (!boxToFind)
        return false;

    auto* ancestor = &boxToFind->parent();
    while (true) {
        if (ancestor->establishesFormattingContext())
            break;
        ancestor = &ancestor->parent();
    }
    return ancestor == &rootLayoutBox();
}

Layout::InitialContainingBlock& BoxTree::initialContainingBlock()
{
    return m_rootRenderer.view().initialContainingBlock();
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
                streamToUse << "            -+";
            else
                streamToUse << "            --";
            while (++printedCharacters <= depth * 2)
                streamToUse << " ";

        };
        addSpacing(stream);
        auto& line = lines[lineIndex];
        stream << "line at (" << line.lineBoxLeft() << "," << line.lineBoxTop() << ") size (" << line.lineBoxRight() - line.lineBoxLeft() << "x" << line.lineBoxBottom() - line.lineBoxTop() << ") baseline (" << line.baseline() << ") enclosing top (" << line.enclosingContentLogicalTop() << ") bottom (" << line.enclosingContentLogicalBottom() << ")";
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


