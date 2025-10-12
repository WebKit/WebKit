/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "TextExtractionToStringConversion.h"

#include <WebCore/TextExtractionTypes.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

struct TextExtractionLine {
    unsigned lineIndex { 0 };
    unsigned indentLevel { 0 };
};

class TextExtractionAggregator : public RefCounted<TextExtractionAggregator> {
    WTF_MAKE_NONCOPYABLE(TextExtractionAggregator);
    WTF_MAKE_TZONE_ALLOCATED(TextExtractionAggregator);
public:
    TextExtractionAggregator(CompletionHandler<void(String&&)>&& completion)
        : m_completion(WTFMove(completion))
    {
    }

    ~TextExtractionAggregator()
    {
        m_completion(makeStringByJoining(WTFMove(m_lines), "\n"_s));
    }

    static Ref<TextExtractionAggregator> create(CompletionHandler<void(String&&)>&& completion)
    {
        return adoptRef(*new TextExtractionAggregator(WTFMove(completion)));
    }

    void addResult(const TextExtractionLine& line, Vector<String>&& components)
    {
        if (components.isEmpty())
            return;

        auto [lineIndex, indentLevel] = line;
        if (lineIndex >= m_lines.size()) {
            ASSERT_NOT_REACHED();
            return;
        }

        auto text = makeStringByJoining(WTFMove(components), ","_s);

        if (!m_lines[lineIndex].isEmpty()) {
            m_lines[lineIndex] = makeString(m_lines[lineIndex], ',', WTFMove(text));
            return;
        }

        StringBuilder indentation;
        indentation.reserveCapacity(indentLevel);
        for (unsigned i = 0; i < indentLevel; ++i)
            indentation.append('\t');
        m_lines[lineIndex] = makeString(indentation.toString(), WTFMove(text));
    }

    unsigned advanceToNextLine()
    {
        auto index = m_nextLineIndex++;
        m_lines.resize(m_nextLineIndex);
        return index;
    }

private:
    Vector<String> m_lines;
    unsigned m_nextLineIndex { 0 };
    CompletionHandler<void(String&&)> m_completion;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextExtractionAggregator);

static String commaSeparatedString(const Vector<String>& parts)
{
    return makeStringByJoining(parts, ","_s);
}

static String escapeString(const String& string)
{
    auto result = string;
    result = makeStringByReplacingAll(result, '\\', "\\\\"_s);
    result = makeStringByReplacingAll(result, '\n', "\\n"_s);
    result = makeStringByReplacingAll(result, '\r', "\\r"_s);
    result = makeStringByReplacingAll(result, '\t', "\\t"_s);
    result = makeStringByReplacingAll(result, '\'', "\\'"_s);
    result = makeStringByReplacingAll(result, '"', "\\\""_s);
    result = makeStringByReplacingAll(result, '\0', "\\0"_s);
    result = makeStringByReplacingAll(result, '\b', "\\b"_s);
    result = makeStringByReplacingAll(result, '\f', "\\f"_s);
    result = makeStringByReplacingAll(result, '\v', "\\v"_s);
    return result;
}

static Vector<String> eventListenerTypesToStringArray(OptionSet<TextExtraction::EventListenerCategory> eventListeners)
{
    Vector<String> result;
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Click))
        result.append("click"_s);
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Hover))
        result.append("hover"_s);
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Touch))
        result.append("touch"_s);
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Wheel))
        result.append("wheel"_s);
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Keyboard))
        result.append("keyboard"_s);
    return result;
}

static Vector<String> partsForItem(const TextExtraction::Item& item)
{
    Vector<String> parts;

    if (item.nodeIdentifier)
        parts.append(makeString("uid="_s, item.nodeIdentifier->toUInt64()));

    if (item.children.isEmpty()) {
        auto origin = item.rectInRootView.location();
        auto size = item.rectInRootView.size();
        parts.append(makeString("["_s,
            static_cast<int>(origin.x()), ',', static_cast<int>(origin.y()), ";"_s,
            static_cast<int>(size.width()), 'x', static_cast<int>(size.height()), ']'));
    }

    if (!item.accessibilityRole.isEmpty())
        parts.append(makeString("role='"_s, escapeString(item.accessibilityRole), '\''));

    auto listeners = eventListenerTypesToStringArray(item.eventListeners);
    if (!listeners.isEmpty())
        parts.append(makeString("events=["_s, commaSeparatedString(listeners), ']'));

    auto attributeKeys = copyToVector(item.ariaAttributes.keys());
    std::ranges::sort(attributeKeys, codePointCompareLessThan);

    for (auto& key : attributeKeys)
        parts.append(makeString(key, "='"_s, escapeString(item.ariaAttributes.get(key)), '\''));

    return parts;
}

static void addPartsForText(const TextExtraction::TextItemData& textItem, Vector<String>&& itemParts, std::optional<NodeIdentifier>&& enclosingNode, const TextExtractionLine& line, const TextExtractionFilterCallback& filter, Ref<TextExtractionAggregator>&& aggregator)
{
    auto completion = [itemParts = WTFMove(itemParts), selectedRange = textItem.selectedRange, aggregator = WTFMove(aggregator), line](const String& filteredText) mutable {
        Vector<String> textParts;
        if (!filteredText.isEmpty()) {
            auto isNewline = [](UChar character) {
                return character == '\n' || character == '\r';
            };

            auto startIndex = filteredText.find([&](auto character) {
                return !isNewline(character);
            });

            if (startIndex == notFound) {
                textParts.append("''"_s);
                textParts.append("selected=[0,0]"_s);
            } else {
                size_t endIndex = filteredText.length() - 1;
                for (size_t i = filteredText.length(); i > 0; --i) {
                    if (!isNewline(filteredText.characterAt(i - 1))) {
                        endIndex = i - 1;
                        break;
                    }
                }

                auto trimmedContent = filteredText.substring(startIndex, endIndex - startIndex + 1);
                textParts.append(makeString('\'', escapeString(trimmedContent), '\''));

                if (selectedRange && selectedRange->length > 0) {
                    if (!trimmedContent.isEmpty()) {
                        int newLocation = std::max(0, static_cast<int>(selectedRange->location) - static_cast<int>(startIndex));
                        int maxLength = static_cast<int>(trimmedContent.length()) - newLocation;
                        int newLength = std::min(static_cast<int>(selectedRange->length), std::max(0, maxLength));
                        if (newLocation < static_cast<int>(trimmedContent.length()) && newLength > 0)
                            textParts.append(makeString("selected=["_s, newLocation, ',', newLocation + newLength, ']'));
                        else
                            textParts.append("selected=[0,0]"_s);
                    } else
                        textParts.append("selected=[0,0]"_s);
                }
            }
        } else if (selectedRange)
            textParts.append("selected=[0,0]"_s);

        textParts.appendVector(WTFMove(itemParts));
        aggregator->addResult(line, WTFMove(textParts));
    };

    if (!filter) {
        completion(textItem.content);
        return;
    }

    filter(textItem.content, WTFMove(enclosingNode))->whenSettled(RunLoop::mainSingleton(), [originalContent = textItem.content, completion = WTFMove(completion)](auto&& result) mutable {
        if (result)
            completion(WTFMove(*result));
        else
            completion(WTFMove(originalContent));
    });
}

static void addPartsForItem(const TextExtraction::Item& item, std::optional<NodeIdentifier>&& enclosingNode, const TextExtractionLine& line, const TextExtractionFilterCallback& filter, TextExtractionAggregator& aggregator)
{
    Vector<String> parts;
    WTF::switchOn(item.data,
        [&](const TextExtraction::ContainerType& containerType) {
            String containerString;
            switch (containerType) {
            case TextExtraction::ContainerType::Root:
                containerString = "root"_s;
                break;
            case TextExtraction::ContainerType::ViewportConstrained:
                containerString = "overlay"_s;
                break;
            case TextExtraction::ContainerType::List:
                containerString = "list"_s;
                break;
            case TextExtraction::ContainerType::ListItem:
                containerString = "list-item"_s;
                break;
            case TextExtraction::ContainerType::BlockQuote:
                containerString = "block-quote"_s;
                break;
            case TextExtraction::ContainerType::Article:
                containerString = "article"_s;
                break;
            case TextExtraction::ContainerType::Section:
                containerString = "section"_s;
                break;
            case TextExtraction::ContainerType::Nav:
                containerString = "navigation"_s;
                break;
            case TextExtraction::ContainerType::Button:
                containerString = "button"_s;
                break;
            case TextExtraction::ContainerType::Canvas:
                containerString = "canvas"_s;
                break;
            case TextExtraction::ContainerType::Generic:
                break;
            }

            if (!containerString.isEmpty())
                parts.append(WTFMove(containerString));

            parts.appendVector(partsForItem(item));
            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::TextItemData& textData) {
            addPartsForText(textData, partsForItem(item), WTFMove(enclosingNode), line, filter, aggregator);
        },
        [&](const TextExtraction::ContentEditableData& editableData) {
            parts.append("contentEditable"_s);
            parts.appendVector(partsForItem(item));

            if (editableData.isFocused)
                parts.append("focused"_s);

            if (editableData.isPlainTextOnly)
                parts.append("plaintext"_s);

            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::TextFormControlData& controlData) {
            parts.append("textFormControl"_s);
            parts.appendVector(partsForItem(item));

            if (!controlData.controlType.isEmpty())
                parts.insert(1, makeString('\'', controlData.controlType, '\''));

            if (!controlData.autocomplete.isEmpty())
                parts.append(makeString("autocomplete='"_s, controlData.autocomplete, '\''));

            if (controlData.isReadonly)
                parts.append("readonly"_s);

            if (controlData.isDisabled)
                parts.append("disabled"_s);

            if (controlData.isChecked)
                parts.append("checked"_s);

            if (!controlData.editable.label.isEmpty())
                parts.append(makeString("label='"_s, escapeString(controlData.editable.label), '\''));

            if (!controlData.editable.placeholder.isEmpty())
                parts.append(makeString("placeholder='"_s, escapeString(controlData.editable.placeholder), '\''));

            if (controlData.editable.isSecure)
                parts.append("secure"_s);

            if (controlData.editable.isFocused)
                parts.append("focused"_s);

        aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::LinkItemData& linkData) {
            parts.append("link"_s);
            parts.appendVector(partsForItem(item));

            if (!linkData.completedURL.isEmpty())
                parts.append(makeString("url='"_s, escapeString(linkData.completedURL.string()), '\''));

            if (!linkData.target.isEmpty())
                parts.append(makeString("target='"_s, escapeString(linkData.target), '\''));

            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::ScrollableItemData& scrollableData) {
            parts.append("scrollable"_s);
            parts.appendVector(partsForItem(item));
            parts.append(makeString("contentSize=["_s, scrollableData.contentSize.width(), 'x', scrollableData.contentSize.height(), ']'));
            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::SelectData& selectData) {
            parts.append("select"_s);
            parts.appendVector(partsForItem(item));

            if (!selectData.selectedValues.isEmpty()) {
                auto escapedValues = selectData.selectedValues.map([](auto& value) {
                    return makeString('\'', escapeString(value), '\'');
                });
                parts.append(makeString("selected=["_s, commaSeparatedString(escapedValues), ']'));
            }

            if (selectData.isMultiple)
                parts.append("multiple"_s);

            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::ImageItemData& imageData) {
            parts.append("image"_s);
            parts.appendVector(partsForItem(item));

            if (!imageData.name.isEmpty())
                parts.append(makeString("name='"_s, escapeString(imageData.name), '\''));

            if (!imageData.altText.isEmpty())
                parts.append(makeString("alt='"_s, escapeString(imageData.altText), '\''));

            aggregator.addResult(line, WTFMove(parts));
        }
    );
}

static void addTextRepresentationRecursive(const TextExtraction::Item& item, std::optional<NodeIdentifier>&& enclosingNode, unsigned depth, const TextExtractionFilterCallback& filter, TextExtractionAggregator& aggregator)
{
    auto identifier = item.nodeIdentifier;
    if (!identifier)
        identifier = enclosingNode;

    TextExtractionLine line { aggregator.advanceToNextLine(), depth };
    addPartsForItem(item, std::optional { identifier }, line, filter, aggregator);

    if (item.children.size() == 1 && std::holds_alternative<TextExtraction::TextItemData>(item.children[0].data)) {
        // In the case of a single text child, we append that text to the same line.
        addPartsForItem(item.children[0], WTFMove(identifier), line, filter, aggregator);
        return;
    }

    for (auto& child : item.children)
        addTextRepresentationRecursive(child, std::optional { identifier }, depth + 1, filter, aggregator);
}

void convertToText(TextExtraction::Item&& item, CompletionHandler<void(String&&)>&& completion, TextExtractionFilterCallback&& filter, Vector<String>&& nativeMenuItems)
{
    Ref aggregator = TextExtractionAggregator::create(WTFMove(completion));
    addTextRepresentationRecursive(item, { }, 0, WTFMove(filter), aggregator);

    if (nativeMenuItems.isEmpty())
        return;

    auto escapedQuotedItemTitles = nativeMenuItems.map([](auto& itemTitle) {
        return makeString('\'', escapeString(itemTitle), '\'');
    });

    aggregator->addResult({ aggregator->advanceToNextLine(), 0 }, {
        "nativePopupMenu"_s,
        makeString("items=["_s, commaSeparatedString(escapedQuotedItemTitles), ']')
    });
}

} // namespace WebKit
