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

#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))

#import "SafeBrowsingUtilities.h"
#import "WKWebViewInternal.h"
#import "_WKTextExtractionInternal.h"
#import <WebCore/TextExtraction.h>
#import <wtf/Box.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace WebCore;

inline static WKTextExtractionContainer containerType(TextExtraction::ContainerType type)
{
    switch (type) {
    case TextExtraction::ContainerType::ViewportConstrained:
        return WKTextExtractionContainerViewportConstrained;
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
    case TextExtraction::ContainerType::Button:
        return WKTextExtractionContainerButton;
    case TextExtraction::ContainerType::Canvas:
        return WKTextExtractionContainerCanvas;
    case TextExtraction::ContainerType::Subscript:
        return WKTextExtractionContainerSubscript;
    case TextExtraction::ContainerType::Superscript:
        return WKTextExtractionContainerSuperscript;
    case TextExtraction::ContainerType::Strikethrough:
        return WKTextExtractionContainerStrikethrough;
    case TextExtraction::ContainerType::Generic:
        return WKTextExtractionContainerGeneric;
    }
}

static WKTextExtractionEventListenerTypes eventListenerTypes(OptionSet<TextExtraction::EventListenerCategory> eventListeners)
{
    WKTextExtractionEventListenerTypes result = WKTextExtractionEventListenerTypeNone;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Click))
        result |= WKTextExtractionEventListenerTypeClick;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Hover))
        result |= WKTextExtractionEventListenerTypeHover;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Touch))
        result |= WKTextExtractionEventListenerTypeTouch;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Wheel))
        result |= WKTextExtractionEventListenerTypeWheel;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Keyboard))
        result |= WKTextExtractionEventListenerTypeKeyboard;
    return result;
}

static RetainPtr<WKTextExtractionEditable> createWKEditable(const TextExtraction::Editable& editable)
{
    return adoptNS([[WKTextExtractionEditable alloc]
        initWithLabel:editable.label.createNSString().get()
        placeholder:editable.placeholder.createNSString().get()
        isSecure:static_cast<BOOL>(editable.isSecure)
        isFocused:static_cast<BOOL>(editable.isFocused)]);
}

inline static RetainPtr<WKTextExtractionItem> createItemWithChildren(const TextExtraction::Item& item, const RootViewToWebViewConverter& converter, NSArray<WKTextExtractionItem *> *children)
{
    auto rectInWebView = converter(item.rectInRootView);
    auto eventListeners = eventListenerTypes(item.eventListeners);
    RetainPtr<NSString> nodeIdentifier;
    if (item.nodeIdentifier)
        nodeIdentifier = [NSString stringWithFormat:@"%llu", item.nodeIdentifier->toUInt64()];

    RetainPtr accessibilityRole = item.accessibilityRole.createNSString();
    RetainPtr ariaAttributes = adoptNS([[NSMutableDictionary alloc] initWithCapacity:item.ariaAttributes.size()]);
    for (auto& [attribute, value] : item.ariaAttributes)
        [ariaAttributes setObject:value.createNSString().get() forKey:attribute.createNSString().get()];

    return WTF::switchOn(item.data,
        [&](const TextExtraction::TextItemData& data) -> RetainPtr<WKTextExtractionItem> {
            RetainPtr<WKTextExtractionEditable> editable;
            if (data.editable)
                editable = createWKEditable(*data.editable);

            auto selectedRange = NSMakeRange(NSNotFound, 0);
            if (auto range = data.selectedRange) {
                if (range->location + range->length <= data.content.length()) [[likely]]
                    selectedRange = NSMakeRange(range->location, range->length);
            }

            auto links = createNSArray(data.links, [&](auto& linkAndRange) -> RetainPtr<WKTextExtractionLink> {
                auto& [url, range] = linkAndRange;
                if (range.location + range.length > data.content.length()) [[unlikely]]
                    return { };

                RetainPtr nsURL = url.createNSURL();
                if (!nsURL)
                    return { };

                return adoptNS([[WKTextExtractionLink alloc] initWithURL:nsURL.get() range:NSMakeRange(range.location, range.length)]);
            });

            return adoptNS([[WKTextExtractionTextItem alloc]
                initWithContent:data.content.createNSString().get()
                selectedRange:selectedRange
                links:links.get()
                editable:editable.get()
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::ScrollableItemData& data) -> RetainPtr<WKTextExtractionItem> {
            if (data.isRoot) {
                return adoptNS([[WKTextExtractionContainerItem alloc]
                    initWithContainer:WKTextExtractionContainerRoot
                    rectInWebView:rectInWebView
                    children:children
                    eventListeners:eventListeners
                    ariaAttributes:ariaAttributes.get()
                    accessibilityRole:accessibilityRole.get()
                    nodeIdentifier:nodeIdentifier.get()]);
            }
            return adoptNS([[WKTextExtractionScrollableItem alloc]
                initWithContentSize:data.contentSize
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::SelectData& data) -> RetainPtr<WKTextExtractionItem> {
            auto selectedValues = WTF::compactMap(data.options, [](auto& option) -> std::optional<String> {
                if (option.isSelected)
                    return { option.value };
                return { };
            });
            return adoptNS([[WKTextExtractionSelectItem alloc]
                initWithSelectedValues:createNSArray(selectedValues).get()
                supportsMultiple:data.isMultiple
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::ImageItemData& data) -> RetainPtr<WKTextExtractionItem> {
            RetainPtr name = [data.completedSource.createNSURL() lastPathComponent] ?: @"";
            return adoptNS([[WKTextExtractionImageItem alloc]
                initWithName:name.get()
                altText:data.altText.createNSString().get()
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::ContentEditableData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionContentEditableItem alloc]
                initWithContentEditableType:data.isPlainTextOnly ? WKTextExtractionEditablePlainTextOnly : WKTextExtractionEditableRichText
                isFocused:data.isFocused
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::FormData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionFormItem alloc]
                initWithAutocomplete:data.autocomplete.createNSString().get()
                name:data.name.createNSString().get()
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::TextFormControlData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionTextFormControlItem alloc]
                initWithEditable:createWKEditable(data.editable).get()
                controlType:data.controlType.createNSString().get()
                autocomplete:data.autocomplete.createNSString().get()
                isReadonly:data.isReadonly
                isDisabled:data.isDisabled
                isChecked:data.isChecked
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::LinkItemData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionLinkItem alloc]
                initWithTarget:data.target.createNSString().get()
                url:data.completedURL.createNSURL().get()
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](const TextExtraction::IFrameData& data) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionIFrameItem alloc]
                initWithOrigin:data.origin.createNSString().get()
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }, [&](TextExtraction::ContainerType type) -> RetainPtr<WKTextExtractionItem> {
            return adoptNS([[WKTextExtractionContainerItem alloc]
                initWithContainer:containerType(type)
                rectInWebView:rectInWebView
                children:children
                eventListeners:eventListeners
                ariaAttributes:ariaAttributes.get()
                accessibilityRole:accessibilityRole.get()
                nodeIdentifier:nodeIdentifier.get()]);
        }
    );
}

static RetainPtr<WKTextExtractionItem> createItemRecursive(const TextExtraction::Item& item, const RootViewToWebViewConverter& converter)
{
    return createItemWithChildren(item, converter, createNSArray(item.children, [&](auto& child) {
        return createItemRecursive(child, converter);
    }).get());
}

RetainPtr<WKTextExtractionItem> createItem(const TextExtraction::Item& item, RootViewToWebViewConverter&& converter)
{
    if (auto data = item.dataAs<TextExtraction::ScrollableItemData>(); !data || !data->isRoot) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    return createItemRecursive(item, WTF::move(converter));
}

std::optional<double> computeSimilarity(NSString *stringA, NSString *stringB, unsigned minimumLength)
{
    if (stringA == stringB || [stringA isEqualToString:stringB])
        return 1;

    if (!stringA || !stringB)
        return 0;

    auto lengthA = [stringA length];
    auto lengthB = [stringB length];
    if (lengthA < minimumLength && lengthB < minimumLength)
        return std::nullopt;

    double maxLength = std::max(lengthA, lengthB);
    if (!lengthA || !lengthB)
        return 0;

    Vector<Vector<size_t>> matrix(lengthA + 1, Vector<size_t>(lengthB + 1, 0));

    for (size_t i = 0; i <= lengthA; i++)
        matrix[i][0] = i;

    for (size_t j = 0; j <= lengthB; j++)
        matrix[0][j] = j;

    for (size_t i = 1; i <= lengthA; i++) {
        auto characterA = [stringA characterAtIndex:i - 1];
        for (size_t j = 1; j <= lengthB; j++) {
            auto characterB = [stringB characterAtIndex:j - 1];

            auto cost = (characterA == characterB) ? 0 : 1;
            auto deletion = matrix[i - 1][j] + 1;
            auto insertion = matrix[i][j - 1] + 1;
            auto substitution = matrix[i - 1][j - 1] + cost;

            matrix[i][j] = std::min({ deletion, insertion, substitution });
        }
    }

    return 1.0 - (matrix[lengthA][lengthB] / maxLength);
}

void requestTextExtractionFilterRuleData(CompletionHandler<void(Vector<TextExtraction::FilterRuleData>&&)>&& completion)
{
#if HAVE(SAFE_BROWSING)
    using namespace WebKit::SafeBrowsingUtilities;

    listsForNamespace(namespacedCollectionForTextExtraction(), [completion = WTF::move(completion)](NSDictionary<NSString *, NSArray<NSString *> *> *data, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(TextExtraction, "Failed to request filtering rules: %@", error.localizedDescription);
            return completion({ });
        }

        HashMap<String, TextExtraction::FilterRuleData> allData;
        for (NSString *nsKeyIdentifier : data) {
            auto keyIdentifier = String { nsKeyIdentifier };
            auto keyIdentifierComponents = keyIdentifier.split('/');
            if (keyIdentifierComponents.size() != 2)
                continue;

            auto ruleName = keyIdentifierComponents.first();
            if (ruleName.isEmpty())
                continue;

            auto ensureRuleData = [&] -> TextExtraction::FilterRuleData& {
                return allData.ensure(ruleName, [] {
                    return TextExtraction::FilterRuleData { };
                }).iterator->value;
            };

            if (keyIdentifierComponents[1] == "filter"_s) {
                ensureRuleData().scriptSource = makeStringByJoining(makeVector<String>(data[nsKeyIdentifier]), "\n"_s);
                continue;
            }

            if (keyIdentifierComponents[1] == "domains"_s) {
                ensureRuleData().urlPatternString = [&] -> String {
                    auto domainRules = makeVector<String>(data[nsKeyIdentifier]);
                    if (domainRules.size() == 1) {
                        if (domainRules.first() == ".*"_s)
                            return { };

                        return domainRules.first();
                    }

                    return makeStringByJoining(WTF::map(WTF::move(domainRules), [](auto&& group) {
                        return makeString('(', WTF::move(group), ')');
                    }), "|"_s);
                }();
                continue;
            }
        }

        for (auto& [ruleName, ruleData] : allData)
            ruleData.name = ruleName;

        completion(copyToVector(allData.values()));
    });
#else
    completion({ });
#endif
}

} // namespace WebKit

#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
