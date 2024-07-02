/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "TextExtraction.h"

#include "ComposedTreeIterator.h"
#include "ElementInlines.h"
#include "ExceptionCode.h"
#include "ExceptionOr.h"
#include "FrameSelection.h"
#include "HTMLBodyElement.h"
#include "HTMLButtonElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "ImageOverlay.h"
#include "LocalFrame.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderDescendantIterator.h"
#include "RenderIFrame.h"
#include "RenderLayer.h"
#include "RenderLayerModelObject.h"
#include "RenderLayerScrollableArea.h"
#include "RenderView.h"
#include "SimpleRange.h"
#include "Text.h"
#include "TextIterator.h"
#include "WritingMode.h"
#include <unicode/uchar.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace TextExtraction {

static constexpr auto minOpacityToConsiderVisible = 0.05;

using TextNodesAndText = Vector<std::pair<Ref<Text>, String>>;
using TextAndSelectedRange = std::pair<String, std::optional<CharacterRange>>;
using TextAndSelectedRangeMap = HashMap<RefPtr<Text>, TextAndSelectedRange>;

static inline TextNodesAndText collectText(const SimpleRange& range)
{
    TextNodesAndText nodesAndText;
    RefPtr<Text> lastTextNode;
    StringBuilder textForLastTextNode;

    auto emitTextForLastNode = [&] {
        auto text = makeStringByReplacingAll(textForLastTextNode.toString(), noBreakSpace, ' ');
        if (text.isEmpty())
            return;
        nodesAndText.append({ lastTextNode.releaseNonNull(), WTFMove(text) });
    };

    for (TextIterator iterator { range, TextIteratorBehavior::EntersTextControls }; !iterator.atEnd(); iterator.advance()) {
        if (iterator.text().isEmpty())
            continue;

        RefPtr textNode = dynamicDowncast<Text>(iterator.node());
        if (!textNode) {
            textForLastTextNode.append(iterator.text());
            continue;
        }

        if (!lastTextNode)
            lastTextNode = textNode;

        if (lastTextNode == textNode) {
            textForLastTextNode.append(iterator.text());
            continue;
        }

        emitTextForLastNode();
        textForLastTextNode.clear();
        textForLastTextNode.append(iterator.text());
        lastTextNode = textNode;
    }

    if (lastTextNode)
        emitTextForLastNode();

    return nodesAndText;
}

struct TraversalContext {
    const TextAndSelectedRangeMap visibleText;
    const std::optional<WebCore::FloatRect> rectInRootView;
    unsigned onlyCollectTextAndLinksCount { 0 };

    inline bool shouldIncludeNodeWithRect(const FloatRect& rect) const
    {
        return !rectInRootView || rectInRootView->intersects(rect);
    }
};

static inline TextAndSelectedRangeMap collectText(Document& document)
{
    auto fullRange = makeRangeSelectingNodeContents(*document.body());
    auto selection = document.selection().selection();
    TextNodesAndText textBeforeRangedSelection;
    TextNodesAndText textInRangedSelection;
    TextNodesAndText textAfterRangedSelection;
    [&] {
        if (selection.isRange()) {
            auto selectionStart = selection.start();
            auto selectionEnd = selection.end();
            auto rangeBeforeSelection = makeSimpleRange(fullRange.start, selectionStart);
            auto selectionRange = makeSimpleRange(selectionStart, selectionEnd);
            auto rangeAfterSelection = makeSimpleRange(selectionEnd, fullRange.end);
            if (rangeBeforeSelection && selectionRange && rangeAfterSelection) {
                textBeforeRangedSelection = collectText(*rangeBeforeSelection);
                textInRangedSelection = collectText(*selectionRange);
                textAfterRangedSelection = collectText(*rangeAfterSelection);
                return;
            }
        }
        // Fall back to collecting the full document.
        textBeforeRangedSelection = collectText(fullRange);
    }();

    TextAndSelectedRangeMap result;
    for (auto& [node, text] : textBeforeRangedSelection)
        result.add(node.ptr(), TextAndSelectedRange { text, { } });

    bool isFirstSelectedNode = true;
    for (auto& [node, text] : textInRangedSelection) {
        if (std::exchange(isFirstSelectedNode, false)) {
            if (auto entry = result.find(node.ptr()); entry != result.end() && entry->key == node.ptr()) {
                entry->value = std::make_pair(
                    makeString(entry->value.first, text),
                    CharacterRange { entry->value.first.length(), text.length() }
                );
                continue;
            }
        }
        result.add(node.ptr(), TextAndSelectedRange { text, CharacterRange { 0, text.length() } });
    }

    bool isFirstNodeAfterSelection = true;
    for (auto& [node, text] : textAfterRangedSelection) {
        if (std::exchange(isFirstNodeAfterSelection, false)) {
            if (auto entry = result.find(node.ptr()); entry != result.end() && entry->key == node.ptr()) {
                entry->value.first = makeString(entry->value.first, text);
                continue;
            }
        }
        result.add(node.ptr(), TextAndSelectedRange { text, std::nullopt });
    }

    return result;
}

static inline bool canMerge(const Item& destinationItem, const Item& sourceItem)
{
    if (!destinationItem.children.isEmpty() || !sourceItem.children.isEmpty())
        return false;

    if (!std::holds_alternative<TextItemData>(destinationItem.data) || !std::holds_alternative<TextItemData>(sourceItem.data))
        return false;

    // Don't merge adjacent text runs if they represent two different editable roots.
    auto& destination = std::get<TextItemData>(destinationItem.data);
    auto& source = std::get<TextItemData>(sourceItem.data);
    return !destination.editable && !source.editable;
}

static inline void merge(Item& destinationItem, Item&& sourceItem)
{
    ASSERT(canMerge(destinationItem, sourceItem));

    auto& destination = std::get<TextItemData>(destinationItem.data);
    auto& source = std::get<TextItemData>(sourceItem.data);

    destinationItem.rectInRootView.unite(sourceItem.rectInRootView);

    auto originalContentLength = destination.content.length();
    destination.content = makeString(destination.content, WTFMove(source.content));

    if (source.selectedRange) {
        CharacterRange newSelectedRange;
        if (destination.selectedRange)
            newSelectedRange = { destination.selectedRange->location, destination.selectedRange->length + source.selectedRange->length };
        else
            newSelectedRange = { originalContentLength + source.selectedRange->location, source.selectedRange->length };
        destination.selectedRange = WTFMove(newSelectedRange);
    }

    if (!source.links.isEmpty()) {
        for (auto& [url, range] : source.links)
            range.location += originalContentLength;
        destination.links.appendVector(WTFMove(source.links));
    }
}

static inline FloatRect rootViewBounds(Node& node)
{
    auto view = node.document().protectedView();
    if (UNLIKELY(!view))
        return { };

    if (!node.renderer())
        return { };

    return view->contentsToRootView(node.renderer()->absoluteBoundingBoxRect());
}

static inline String labelText(HTMLElement& element)
{
    auto labels = element.labels();
    if (!labels)
        return { };

    RefPtr<Element> firstRenderedLabel;
    for (unsigned index = 0; index < labels->length(); ++index) {
        if (RefPtr label = dynamicDowncast<Element>(labels->item(index)); label && label->renderer())
            firstRenderedLabel = WTFMove(label);
    }

    if (firstRenderedLabel)
        return firstRenderedLabel->textContent();

    return { };
}

enum class SkipExtraction : bool {
    Self,
    SelfAndSubtree
};

static bool shouldTreatAsPasswordField(const Element* element)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(element);
    return input && input->hasEverBeenPasswordField();
}

static inline std::variant<SkipExtraction, ItemData, URL, Editable> extractItemData(Node& node, TraversalContext& context)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer || renderer->style().opacity() < minOpacityToConsiderVisible)
        return { SkipExtraction::SelfAndSubtree };

    if (renderer->style().usedVisibility() == Visibility::Hidden)
        return { SkipExtraction::Self };

    if (RefPtr textNode = dynamicDowncast<Text>(node)) {
        if (shouldTreatAsPasswordField(textNode->shadowHost()))
            return { SkipExtraction::Self };

        if (auto iterator = context.visibleText.find(textNode); iterator != context.visibleText.end()) {
            auto& [textContent, selectedRange] = iterator->value;
            return { TextItemData { { }, selectedRange, textContent, { } } };
        }
        return { SkipExtraction::Self };
    }

    RefPtr element = dynamicDowncast<Element>(node);
    if (!element)
        return { SkipExtraction::Self };

    if (element->isLink()) {
        if (auto href = element->attributeWithoutSynchronization(HTMLNames::hrefAttr); !href.isEmpty()) {
            if (auto url = element->document().completeURL(href); !url.isEmpty())
                return { url };
        }
    }

    if (context.onlyCollectTextAndLinksCount) {
        // FIXME: This isn't quite right in the case where a richly contenteditable element
        // contains more nested editable containers underneath it (for instance, a textarea
        // element inside of a Mail compose draft).
        return { SkipExtraction::Self };
    }

    if (!element->isInUserAgentShadowTree() && element->isRootEditableElement())
        return { Editable { } };

    if (RefPtr image = dynamicDowncast<HTMLImageElement>(element))
        return { ImageItemData { image->src().lastPathComponent().toString(), image->altText() } };

    if (RefPtr control = dynamicDowncast<HTMLTextFormControlElement>(element); control && control->isTextField()) {
        RefPtr input = dynamicDowncast<HTMLInputElement>(control);
        return { Editable {
            labelText(*control),
            input ? input->placeholder() : nullString(),
            shouldTreatAsPasswordField(element.get()),
            element->document().activeElement() == control
        } };
    }

    if (RefPtr button = dynamicDowncast<HTMLButtonElement>(element))
        return { ItemData { ContainerType::Button } };

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
        if (input->isTextButton())
            return { ItemData { ContainerType::Button } };
    }

    if (CheckedPtr box = dynamicDowncast<RenderBox>(node.renderer()); box && box->canBeScrolledAndHasScrollableArea()) {
        if (auto layer = box->checkedLayer(); layer && layer->scrollableArea())
            return { ScrollableItemData { layer->scrollableArea()->totalContentsSize() } };
    }

    if (element->hasTagName(HTMLNames::olTag) || element->hasTagName(HTMLNames::ulTag))
        return { ItemData { ContainerType::List } };

    if (element->hasTagName(HTMLNames::liTag))
        return { ItemData { ContainerType::ListItem } };

    if (element->hasTagName(HTMLNames::blockquoteTag))
        return { ItemData { ContainerType::BlockQuote } };

    if (element->hasTagName(HTMLNames::articleTag))
        return { ItemData { ContainerType::Article } };

    if (element->hasTagName(HTMLNames::sectionTag))
        return { ItemData { ContainerType::Section } };

    if (element->hasTagName(HTMLNames::navTag))
        return { ItemData { ContainerType::Nav } };

    if (renderer->style().hasViewportConstrainedPosition())
        return { ItemData { ContainerType::ViewportConstrained } };

    return { SkipExtraction::Self };
}

static inline void extractRecursive(Node& node, Item& parentItem, TraversalContext& context)
{
    std::optional<Item> item;
    std::optional<Editable> editable;
    std::optional<URL> linkURL;
    bool shouldSkipSubtree = false;

    WTF::switchOn(extractItemData(node, context),
        [&](SkipExtraction skipExtraction) {
            switch (skipExtraction) {
            case SkipExtraction::Self:
                return;
            case SkipExtraction::SelfAndSubtree:
                shouldSkipSubtree = true;
                return;
            }
        },
        [&](URL&& result) { linkURL = WTFMove(result); },
        [&](Editable&& result) { editable = WTFMove(result); },
        [&](ItemData&& result) {
            auto bounds = rootViewBounds(node);
            if (context.shouldIncludeNodeWithRect(bounds))
                item = { { WTFMove(result), WTFMove(bounds), { } } };
        });

    if (shouldSkipSubtree)
        return;

    bool onlyCollectTextAndLinks = linkURL || editable;
    if (onlyCollectTextAndLinks) {
        if (auto bounds = rootViewBounds(node); context.shouldIncludeNodeWithRect(bounds)) {
            item = {
                TextItemData { { }, { }, emptyString(), { } },
                WTFMove(bounds),
                { }
            };
        }
        context.onlyCollectTextAndLinksCount++;
    }

    if (RefPtr container = dynamicDowncast<ContainerNode>(node)) {
        for (auto& child : composedTreeChildren(*container))
            extractRecursive(child, item ? *item : parentItem, context);
    }

    if (onlyCollectTextAndLinks) {
        if (item) {
            if (linkURL) {
                auto& text = std::get<TextItemData>(item->data);
                text.links.append({ WTFMove(*linkURL), CharacterRange { 0, text.content.length() } });
            }
            if (editable) {
                auto& text = std::get<TextItemData>(item->data);
                text.editable = WTFMove(editable);
            }
        }
        context.onlyCollectTextAndLinksCount--;
    }

    if (!item)
        return;

    if (parentItem.children.isEmpty()) {
        if (canMerge(parentItem, *item))
            return merge(parentItem, WTFMove(*item));
    } else if (auto& lastChild = parentItem.children.last(); canMerge(lastChild, *item))
        return merge(lastChild, WTFMove(*item));

    parentItem.children.append(WTFMove(*item));
}

static void pruneRedundantItemsRecursive(Item& item)
{
    item.children.removeAllMatching([](auto& child) {
        if (!child.children.isEmpty() || !std::holds_alternative<TextItemData>(child.data))
            return false;

        auto& text = std::get<TextItemData>(child.data);
        return !text.editable && text.content.template containsOnly<isASCIIWhitespace>();
    });

    for (auto& child : item.children)
        pruneRedundantItemsRecursive(child);
}

Item extractItem(std::optional<WebCore::FloatRect>&& collectionRectInRootView, Page& page)
{
    Item root { ContainerType::Root, { }, { } };
    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
    if (!mainFrame) {
        // FIXME: Propagate text extraction to RemoteFrames.
        return root;
    }

    RefPtr mainDocument = mainFrame->document();
    if (!mainDocument)
        return root;

    RefPtr bodyElement = mainDocument->body();
    if (!bodyElement)
        return root;

    mainDocument->updateLayoutIgnorePendingStylesheets();
    root.rectInRootView = rootViewBounds(*bodyElement);

    {
        TraversalContext context { collectText(*mainDocument), WTFMove(collectionRectInRootView) };
        extractRecursive(*bodyElement, root, context);
    }

    pruneRedundantItemsRecursive(root);

    return root;
}

using Token = std::variant<String, IntSize>;
struct TokenAndBlockOffset {
    Vector<Token> tokens;
    int offset { 0 };
};

static IntSize reducePrecision(FloatSize size)
{
    static constexpr auto resolution = 10;
    return {
        static_cast<int>(std::round(size.width() / resolution)) * resolution,
        static_cast<int>(std::round(size.height() / resolution)) * resolution
    };
}

static void extractRenderedTokens(Vector<TokenAndBlockOffset>& tokensAndOffsets, ContainerNode& node, BlockFlowDirection direction)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return;

    auto appendTokens = [&](Vector<Token>&& tokens, IntRect bounds) mutable {
        static constexpr auto minPixelDistanceForNearbyText = 5;
        if (tokens.isEmpty() || bounds.width() <= minPixelDistanceForNearbyText || bounds.height() <= minPixelDistanceForNearbyText)
            return;

        auto offset = [&] {
            switch (direction) {
            case BlockFlowDirection::TopToBottom:
                return bounds.y();
            case BlockFlowDirection::BottomToTop:
                return bounds.maxY();
            case BlockFlowDirection::LeftToRight:
                return bounds.x();
            case BlockFlowDirection::RightToLeft:
                return bounds.maxX();
            }
            ASSERT_NOT_REACHED();
            return 0;
        }();

        auto foundIndex = tokensAndOffsets.reverseFindIf([&](auto& item) {
            return std::abs(offset - item.offset) <= minPixelDistanceForNearbyText;
        });

        if (foundIndex == notFound) {
            tokensAndOffsets.append({ WTFMove(tokens), offset });
            return;
        }

        tokensAndOffsets[foundIndex].tokens.appendVector(WTFMove(tokens));
    };

    if (CheckedPtr frameRenderer = dynamicDowncast<RenderIFrame>(*renderer)) {
        if (auto contentDocument = frameRenderer->iframeElement().protectedContentDocument())
            extractRenderedTokens(tokensAndOffsets, *contentDocument, direction);
        return;
    }

    auto frameView = renderer->view().protectedFrameView();
    auto appendReplacedContentOrBackgroundImage = [&](const RenderObject& renderer) {
        if (!renderer.style().hasBackgroundImage() && !is<RenderReplaced>(renderer))
            return;

        auto absoluteRect = renderer.absoluteBoundingBoxRect();
        auto roundedSize = reducePrecision(frameView->absoluteToDocumentRect(absoluteRect).size());
        appendTokens({ { roundedSize } }, frameView->contentsToRootView(absoluteRect));
    };

    appendReplacedContentOrBackgroundImage(*renderer);

    for (auto& descendant : descendantsOfType<RenderObject>(*renderer)) {
        if (descendant.style().usedVisibility() == Visibility::Hidden)
            continue;

        if (descendant.style().opacity() < minOpacityToConsiderVisible)
            continue;

        if (RefPtr node = descendant.node(); node && ImageOverlay::isInsideOverlay(*node))
            continue;

        if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(descendant); textRenderer && textRenderer->hasRenderedText()) {
            Vector<Token> tokens;
            for (auto token : textRenderer->text().simplifyWhiteSpace(isASCIIWhitespace).split(' ')) {
                auto candidate = token.removeCharacters([](UChar character) {
                    return !u_isalpha(character) && !u_isdigit(character);
                });
                if (!candidate.isEmpty())
                    tokens.append({ WTFMove(candidate) });
            }
            appendTokens(WTFMove(tokens), frameView->contentsToRootView(descendant.absoluteBoundingBoxRect()));
            continue;
        }

        if (CheckedPtr frameRenderer = dynamicDowncast<RenderIFrame>(descendant)) {
            if (auto contentDocument = frameRenderer->iframeElement().protectedContentDocument())
                extractRenderedTokens(tokensAndOffsets, *contentDocument, direction);
            continue;
        }

        appendReplacedContentOrBackgroundImage(descendant);
    }
}

RenderedText extractRenderedText(Element& element)
{
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return { };

    RefPtr frameView = renderer->view().protectedFrameView();
    auto direction = renderer->style().blockFlowDirection();
    auto elementRectInDocument = frameView->absoluteToDocumentRect(renderer->absoluteBoundingBoxRect());

    Vector<TokenAndBlockOffset> allTokensAndOffsets;
    extractRenderedTokens(allTokensAndOffsets, element, direction);

    bool ascendingOrder = [&] {
        switch (direction) {
        case BlockFlowDirection::TopToBottom:
        case BlockFlowDirection::LeftToRight:
            return true;
        case BlockFlowDirection::BottomToTop:
        case BlockFlowDirection::RightToLeft:
            return false;
        }
        ASSERT_NOT_REACHED();
        return true;
    }();

    std::sort(allTokensAndOffsets.begin(), allTokensAndOffsets.end(), [&](auto& a, auto& b) {
        return ascendingOrder ? a.offset < b.offset : a.offset > b.offset;
    });

    bool hasLargeReplacedDescendant = false;
    StringBuilder textWithReplacedContent;
    StringBuilder textWithoutReplacedContent;
    auto appendText = [](StringBuilder& builder, const String& string) {
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append(string);
    };

    for (auto& [tokens, offset] : allTokensAndOffsets) {
        for (auto& token : tokens) {
            switchOn(token, [&](const String& text) {
                appendText(textWithReplacedContent, text);
                appendText(textWithoutReplacedContent, text);
            }, [&](const IntSize& size) {
                constexpr auto ratioToConsiderLengthAsLarge = 0.9;
                if (size.width() > ratioToConsiderLengthAsLarge * elementRectInDocument.width() && size.height() > ratioToConsiderLengthAsLarge * elementRectInDocument.height())
                    hasLargeReplacedDescendant = true;
                appendText(textWithReplacedContent, makeString('{', size.width(), ',', size.height(), '}'));
            });
        }
    }

    return { textWithReplacedContent.toString(), textWithoutReplacedContent.toString(), hasLargeReplacedDescendant };
}

} // namespace TextExtractor
} // namespace WebCore
