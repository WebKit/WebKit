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
#include "HTMLBodyElement.h"
#include "HTMLButtonElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "RenderLayerModelObject.h"
#include "RenderLayerScrollableArea.h"
#include "SimpleRange.h"
#include "Text.h"
#include "TextIterator.h"

namespace WebCore {
namespace TextExtraction {

static inline HashMap<Ref<Text>, String> collectText(ContainerNode& container)
{
    HashMap<Ref<Text>, String> nodeToTextMap;
    RefPtr<Text> lastTextNode;
    StringBuilder textForLastTextNode;
    for (TextIterator iterator { makeRangeSelectingNodeContents(container) }; !iterator.atEnd(); iterator.advance()) {
        if (iterator.text().isEmpty())
            continue;

        RefPtr textNode = dynamicDowncast<Text>(iterator.node());
        if (!textNode)
            continue;

        if (!lastTextNode)
            lastTextNode = textNode;

        if (lastTextNode == textNode) {
            textForLastTextNode.append(iterator.text());
            continue;
        }

        if (auto text = textForLastTextNode.toString().trim(isASCIIWhitespace<UChar>); !text.isEmpty())
            nodeToTextMap.add(*lastTextNode, WTFMove(text));

        textForLastTextNode.clear();
        textForLastTextNode.append(iterator.text());
        lastTextNode = textNode;
    }

    if (auto text = textForLastTextNode.toString().trim(isASCIIWhitespace<UChar>); lastTextNode && !text.isEmpty())
        nodeToTextMap.add(*lastTextNode, WTFMove(text));

    return nodeToTextMap;
}

static bool shouldIncludeChildren(const ItemData& data)
{
    return WTF::switchOn(data,
        [&](const TextItemData&) { return false; },
        [&](const ScrollableItemData&) { return true; },
        [&](const EditableItemData&) { return true; },
        [&](const ImageItemData&) { return false; },
        [&](const InteractiveItemData&) { return true; },
        [&](ContainerType) { return true; }
    );
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

static inline std::optional<ItemData> extractItemData(Node& node, const HashMap<Ref<Text>, String>& extractedText)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return std::nullopt;

    if (renderer->style().visibility() == Visibility::Hidden)
        return std::nullopt;

    if (RefPtr textNode = dynamicDowncast<Text>(node)) {
        if (auto iterator = extractedText.find(*textNode); iterator != extractedText.end())
            return { { TextItemData { iterator->value } } };
        return std::nullopt;
    }

    RefPtr element = dynamicDowncast<Element>(node);
    if (!element)
        return std::nullopt;

    if (!element->isInUserAgentShadowTree() && element->isRootEditableElement())
        return { { EditableItemData { element == element->document().activeElement() } } };

    if (RefPtr image = dynamicDowncast<HTMLImageElement>(element))
        return { { ImageItemData { image->src().lastPathComponent().toString(), image->altText() } } };

    if (RefPtr button = dynamicDowncast<HTMLButtonElement>(element))
        return { { InteractiveItemData { !button->isDisabledFormControl() } } };

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
        if (input->isTextButton())
            return { { InteractiveItemData { !input->isDisabledFormControl() } } };

        if (input->isTextField())
            return { { EditableItemData { input == input->document().activeElement() } } };
    }

    if (CheckedPtr box = dynamicDowncast<RenderBox>(node.renderer()); box && box->canBeScrolledAndHasScrollableArea()) {
        if (auto layer = box->checkedLayer(); layer && layer->scrollableArea())
            return { { ScrollableItemData { layer->scrollableArea()->totalContentsSize() } } };
    }

    if (element->hasTagName(HTMLNames::olTag) || element->hasTagName(HTMLNames::ulTag))
        return { { ContainerType::List } };

    if (element->hasTagName(HTMLNames::liTag))
        return { { ContainerType::ListItem } };

    if (element->hasTagName(HTMLNames::blockquoteTag))
        return { { ContainerType::BlockQuote } };

    if (element->hasTagName(HTMLNames::articleTag))
        return { { ContainerType::Article } };

    if (element->hasTagName(HTMLNames::sectionTag))
        return { { ContainerType::Section } };

    if (element->hasTagName(HTMLNames::navTag))
        return { { ContainerType::Nav } };

    if (element->isLink())
        return { { ContainerType::Link } };

    if (renderer->style().hasViewportConstrainedPosition())
        return { { ContainerType::ViewportConstrained } };

    return std::nullopt;
}

static inline void extractRecursive(Node& node, Item& parentItem, const HashMap<Ref<Text>, String>& extractedText, const std::optional<WebCore::FloatRect>& rectInRootView)
{
    std::optional<Item> item;
    if (auto itemData = extractItemData(node, extractedText)) {
        auto nodeBoundsInRootView = rootViewBounds(node);
        if (!rectInRootView || rectInRootView->intersects(nodeBoundsInRootView))
            item = { { WTFMove(*itemData), WTFMove(nodeBoundsInRootView), { } } };
    }

    if (!item || shouldIncludeChildren(item->data)) {
        if (RefPtr container = dynamicDowncast<ContainerNode>(node)) {
            for (auto& child : composedTreeChildren(*container))
                extractRecursive(child, item ? *item : parentItem, extractedText, rectInRootView);
        }
    }

    if (item)
        parentItem.children.append(WTFMove(*item));
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
    extractRecursive(*bodyElement, root, collectText(*bodyElement), collectionRectInRootView);
    return root;
}

} // namespace TextExtractor
} // namespace WebCore
