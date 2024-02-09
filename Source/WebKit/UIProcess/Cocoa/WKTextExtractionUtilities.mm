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

#import "config.h"
#import "WKTextExtractionUtilities.h"

#import "WKTextExtractionItem.h"
#import <WebCore/TextExtraction.h>
#import <wtf/cocoa/VectorCocoa.h>

#import "WebKitSwiftSoftLink.h"

namespace WebKit {
using namespace WebCore;

inline static WKTextExtractionContainer containerType(TextExtraction::ContainerType type)
{
    switch (type) {
    case TextExtraction::ContainerType::Root:
        return WKTextExtractionContainerRoot;
    case TextExtraction::ContainerType::ViewportConstrained:
        return WKTextExtractionContainerViewportConstrained;
    case TextExtraction::ContainerType::Link:
        return WKTextExtractionContainerLink;
    case TextExtraction::ContainerType::List:
        return WKTextExtractionContainerList;
    case TextExtraction::ContainerType::ListItem:
        return WKTextExtractionContainerListItem;
    case TextExtraction::ContainerType::BlockQuote:
        return WKTextExtractionContainerBlockQuote;
    case TextExtraction::ContainerType::Article:
        return WKTextExtractionContainerArticle;
    case TextExtraction::ContainerType::Section:
        return WKTextExtractionContainerSection;
    case TextExtraction::ContainerType::Nav:
        return WKTextExtractionContainerNav;
    }
}

inline static RetainPtr<WKTextExtractionItem> createItemIgnoringChildren(const TextExtraction::Item& item)
{
    return WTF::switchOn(item.data,
        [&](const TextExtraction::TextItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([allocWKTextExtractionTextItemInstance() initWithContent:data.content rectInRootView:item.rectInRootView]);
        }, [&](const TextExtraction::ScrollableItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([allocWKTextExtractionScrollableItemInstance() initWithContentSize:data.contentSize rectInRootView:item.rectInRootView]);
        }, [&](const TextExtraction::EditableItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([allocWKTextExtractionEditableItemInstance() initWithIsFocused:data.isFocused rectInRootView:item.rectInRootView]);
        }, [&](const TextExtraction::ImageItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([allocWKTextExtractionImageItemInstance() initWithName:data.name altText:data.altText rectInRootView:item.rectInRootView]);
        }, [&](const TextExtraction::InteractiveItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([allocWKTextExtractionInteractiveItemInstance() initWithIsEnabled:data.isEnabled rectInRootView:item.rectInRootView]);
        }, [&](TextExtraction::ContainerType type) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([allocWKTextExtractionContainerItemInstance() initWithContainer:containerType(type) rectInRootView:item.rectInRootView]);
        }
    );
}

static RetainPtr<WKTextExtractionItem> createItemRecursive(const TextExtraction::Item& item)
{
    auto wkItem = createItemIgnoringChildren(item);
    [wkItem setChildren:createNSArray(item.children, [](auto& child) {
        return createItemRecursive(child);
    }).get()];
    return wkItem.get();
}

RetainPtr<WKTextExtractionItem> createItem(const TextExtraction::Item& item)
{
    if (!std::holds_alternative<TextExtraction::ContainerType>(item.data)) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    if (std::get<TextExtraction::ContainerType>(item.data) != TextExtraction::ContainerType::Root) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    return createItemRecursive(item);
}

} // namespace WebKit
