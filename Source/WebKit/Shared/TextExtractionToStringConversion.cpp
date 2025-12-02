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

#include <WebCore/HTMLNames.h>
#include <WebCore/TextExtractionTypes.h>
#include <wtf/EnumSet.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

using namespace WebCore;

enum class TextExtractionVersionBehavior : uint8_t {
    TagNameForTextFormControls,
};
using TextExtractionVersionBehaviors = EnumSet<TextExtractionVersionBehavior>;
static constexpr auto currentTextExtractionOutputVersion = 2;

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

static String escapeStringForHTML(const String& string)
{
    auto result = string;
    result = makeStringByReplacingAll(result, '&', "&amp;"_s);
    result = makeStringByReplacingAll(result, '\\', "\\\\"_s);
    result = makeStringByReplacingAll(result, '<', "&lt;"_s);
    result = makeStringByReplacingAll(result, '>', "&gt;"_s);
    // FIXME: Consider representing hard line breaks using <br>.
    result = makeStringByReplacingAll(result, '\n', " "_s);
    result = makeStringByReplacingAll(result, '\'', "&#39;"_s);
    result = makeStringByReplacingAll(result, '"', "&quot;"_s);
    result = makeStringByReplacingAll(result, '\0', "\\0"_s);
    result = makeStringByReplacingAll(result, '\b', "\\b"_s);
    result = makeStringByReplacingAll(result, '\f', "\\f"_s);
    result = makeStringByReplacingAll(result, '\v', "\\v"_s);
    return result;
}

static String escapeStringForMarkdown(const String& string)
{
    auto result = string;
    result = makeStringByReplacingAll(result, '\\', "\\\\"_s);
    result = makeStringByReplacingAll(result, '[', "\\["_s);
    result = makeStringByReplacingAll(result, ']', "\\]"_s);
    result = makeStringByReplacingAll(result, '(', "\\("_s);
    result = makeStringByReplacingAll(result, ')', "\\)"_s);
    return result;
}

static String normalizedURLString(const URL& url)
{
    static constexpr auto maxURLStringLength = 150;
    return url.stringCenterEllipsizedToLength(maxURLStringLength);
}

struct TextExtractionLine {
    unsigned lineIndex { 0 };
    unsigned indentLevel { 0 };
};

class TextExtractionAggregator : public RefCounted<TextExtractionAggregator> {
    WTF_MAKE_NONCOPYABLE(TextExtractionAggregator);
    WTF_MAKE_TZONE_ALLOCATED(TextExtractionAggregator);
public:
    TextExtractionAggregator(TextExtractionOptions&& options, CompletionHandler<void(TextExtractionResult&&)>&& completion)
        : m_options(WTFMove(options))
        , m_completion(WTFMove(completion))
    {
        if (version() >= 2)
            m_versionBehaviors.add(TextExtractionVersionBehavior::TagNameForTextFormControls);
    }

    ~TextExtractionAggregator()
    {
        addLineForNativeMenuItemsIfNeeded();
        addLineForVersionNumberIfNeeded();
        m_lines.removeAllMatching([](auto& line) {
            return line.isEmpty();
        });
        m_completion({ makeStringByJoining(WTFMove(m_lines), "\n"_s), m_filteredOutAnyText });
    }

    static Ref<TextExtractionAggregator> create(TextExtractionOptions&& options, CompletionHandler<void(TextExtractionResult&&)>&& completion)
    {
        return adoptRef(*new TextExtractionAggregator(WTFMove(options), WTFMove(completion)));
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

        auto separator = (useMarkdownOutput() || useHTMLOutput()) ? " "_s : ","_s;
        auto text = makeStringByJoining(WTFMove(components), separator);

        if (!m_lines[lineIndex].isEmpty()) {
            m_lines[lineIndex] = makeString(m_lines[lineIndex], separator, WTFMove(text));
            return;
        }

        if (onlyIncludeText()) {
            m_lines[lineIndex] = WTFMove(text);
            return;
        }

        StringBuilder indentation;

        if (!useMarkdownOutput()) {
            indentation.reserveCapacity(indentLevel);
            for (unsigned i = 0; i < indentLevel; ++i)
                indentation.append('\t');
        }

        m_lines[lineIndex] = makeString(indentation.toString(), WTFMove(text));
    }

    unsigned advanceToNextLine()
    {
        auto index = m_nextLineIndex++;
        m_lines.resize(m_nextLineIndex);
        return index;
    }

    bool useTagNameForTextFormControls() const
    {
        return m_versionBehaviors.contains(TextExtractionVersionBehavior::TagNameForTextFormControls);
    }

    bool includeRects() const
    {
        return !onlyIncludeText() && m_options.flags.contains(TextExtractionOptionFlag::IncludeRects);
    }

    bool includeURLs() const
    {
        return !onlyIncludeText() && m_options.flags.contains(TextExtractionOptionFlag::IncludeURLs);
    }

    bool onlyIncludeText() const
    {
        return m_options.flags.contains(TextExtractionOptionFlag::OnlyIncludeText);
    }

    bool useHTMLOutput() const
    {
        return m_options.outputFormat == TextExtractionOutputFormat::HTMLMarkup;
    }

    bool useMarkdownOutput() const
    {
        return m_options.outputFormat == TextExtractionOutputFormat::Markdown;
    }

    RefPtr<TextExtractionFilterPromise> filter(const String& text, const std::optional<WebCore::NodeIdentifier>& identifier)
    {
        if (m_options.filterCallbacks.isEmpty())
            return nullptr;

        TextExtractionFilterPromise::Producer producer;
        Ref promise = producer.promise();

        filterRecursive(text, identifier, 0, [producer = WTFMove(producer)](auto&& result) mutable {
            producer.settle(WTFMove(result));
        });

        return promise;
    }

    void applyReplacements(String& text)
    {
        for (auto& [original, replacement] : m_options.replacementStrings)
            text = makeStringByReplacingAll(text, original, replacement);
    }

    void appendToLine(unsigned lineIndex, const String& text)
    {
        if (lineIndex >= m_lines.size()) {
            ASSERT_NOT_REACHED();
            return;
        }
        m_lines[lineIndex] = makeString(m_lines[lineIndex], text);
    }

    void pushURLString(String&& urlString)
    {
        m_urlStringStack.append(WTFMove(urlString));
    }

    std::optional<String> currentURLString() const
    {
        if (m_urlStringStack.isEmpty())
            return std::nullopt;

        return { m_urlStringStack.last() };
    }

    void popURLString()
    {
        if (m_urlStringStack.isEmpty()) {
            ASSERT_NOT_REACHED();
            return;
        }

        m_urlStringStack.removeLast();
    }

private:
    void filterRecursive(const String& originalText, const std::optional<WebCore::NodeIdentifier>& identifier, size_t index, CompletionHandler<void(String&&)>&& completion)
    {
        if (index >= m_options.filterCallbacks.size())
            return completion(String { originalText });

        Ref promise = m_options.filterCallbacks[index](originalText, std::optional { identifier });
        promise->whenSettled(RunLoop::mainSingleton(), [originalText, completion = WTFMove(completion), protectedThis = Ref { *this }, identifier, index](auto&& result) mutable {
            if (originalText != result)
                protectedThis->m_filteredOutAnyText = true;

            if (!result)
                return completion({ });

            protectedThis->filterRecursive(WTFMove(*result), identifier, index + 1, WTFMove(completion));
        });
    }

    void addLineForNativeMenuItemsIfNeeded()
    {
        if (onlyIncludeText())
            return;

        if (m_options.nativeMenuItems.isEmpty())
            return;

        auto escapedQuotedItemTitles = m_options.nativeMenuItems.map([](auto& itemTitle) {
            return makeString('\'', escapeString(itemTitle), '\'');
        });
        auto itemsDescription = makeString("items=["_s, commaSeparatedString(escapedQuotedItemTitles), ']');
        addResult({ advanceToNextLine(), 0 }, { "nativePopupMenu"_s, WTFMove(itemsDescription) });
    }

    void addLineForVersionNumberIfNeeded()
    {
        if (onlyIncludeText())
            return;

        auto versionText = (useHTMLOutput() || useMarkdownOutput()) ? makeString("<!-- version="_s, version(), " -->"_s) : makeString("version="_s, version());
        addResult({ advanceToNextLine(), 0 }, { WTFMove(versionText) });
    }

    uint32_t version() const
    {
        return m_options.version.value_or(currentTextExtractionOutputVersion);
    }

    const TextExtractionOptions m_options;
    Vector<String> m_lines;
    Vector<String, 1> m_urlStringStack;
    unsigned m_nextLineIndex { 0 };
    CompletionHandler<void(TextExtractionResult&&)> m_completion;
    TextExtractionVersionBehaviors m_versionBehaviors;
    bool m_filteredOutAnyText { false };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextExtractionAggregator);

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

template<typename T> static Vector<String> sortedKeys(const HashMap<String, T>& dictionary)
{
    auto keys = copyToVector(dictionary.keys());
    std::ranges::sort(keys, codePointCompareLessThan);
    return keys;
}

static Vector<String> partsForItem(const TextExtraction::Item& item, const TextExtractionAggregator& aggregator)
{
    Vector<String> parts;

    if (item.nodeIdentifier)
        parts.append(makeString("uid="_s, item.nodeIdentifier->toUInt64()));

    if (item.children.isEmpty() && aggregator.includeRects() && !aggregator.useHTMLOutput()) {
        auto origin = item.rectInRootView.location();
        auto size = item.rectInRootView.size();
        parts.append(makeString("["_s,
            static_cast<int>(origin.x()), ',', static_cast<int>(origin.y()), ";"_s,
            static_cast<int>(size.width()), 'x', static_cast<int>(size.height()), ']'));
    }

    if (!item.accessibilityRole.isEmpty())
        parts.append(makeString("role='"_s, escapeString(item.accessibilityRole), '\''));

    auto listeners = eventListenerTypesToStringArray(item.eventListeners);
    if (!listeners.isEmpty() && !aggregator.useHTMLOutput())
        parts.append(makeString("events=["_s, commaSeparatedString(listeners), ']'));

    for (auto& key : sortedKeys(item.ariaAttributes))
        parts.append(makeString(key, "='"_s, escapeString(item.ariaAttributes.get(key)), '\''));

    for (auto& key : sortedKeys(item.clientAttributes))
        parts.append(makeString(key, "='"_s, item.clientAttributes.get(key), '\''));

    return parts;
}

static void addPartsForText(const TextExtraction::TextItemData& textItem, Vector<String>&& itemParts, std::optional<NodeIdentifier>&& enclosingNode, const TextExtractionLine& line, Ref<TextExtractionAggregator>&& aggregator, const String& closingTag = { })
{
    auto completion = [itemParts = WTFMove(itemParts), selectedRange = textItem.selectedRange, aggregator, line, closingTag, urlString = aggregator->currentURLString()](String&& filteredText) mutable {
        Vector<String> textParts;
        auto currentLine = line;
        bool includeSelectionAsAttribute = !aggregator->useHTMLOutput() && !aggregator->useMarkdownOutput();
        if (!filteredText.isEmpty()) {
            // Apply replacements only after filtering, so any filtering steps that rely on comparing DOM text against
            // visual data (e.g. recognized text) won't result in false positives.
            aggregator->applyReplacements(filteredText);

            if (aggregator->onlyIncludeText()) {
                aggregator->addResult(currentLine, { escapeString(filteredText.trim(isASCIIWhitespace).simplifyWhiteSpace(isASCIIWhitespace)) });
                return;
            }

            auto startIndex = filteredText.find([&](auto character) {
                return !isASCIIWhitespace(character);
            });

            if (startIndex == notFound) {
                if (includeSelectionAsAttribute) {
                    textParts.append("''"_s);
                    textParts.append("selected=[0,0]"_s);
                }
            } else {
                size_t endIndex = filteredText.length() - 1;
                for (size_t i = filteredText.length(); i > 0; --i) {
                    if (!isASCIIWhitespace(filteredText.characterAt(i - 1))) {
                        endIndex = i - 1;
                        break;
                    }
                }

                auto trimmedContent = filteredText.substring(startIndex, endIndex - startIndex + 1);
                if (aggregator->useHTMLOutput()) {
                    if (!closingTag.isEmpty()) {
                        aggregator->appendToLine(currentLine.lineIndex, makeString(escapeStringForHTML(trimmedContent), closingTag));
                        return;
                    }
                    textParts.append(escapeStringForHTML(trimmedContent));
                } else if (aggregator->useMarkdownOutput()) {
                    if (urlString)
                        textParts.append(makeString('[', escapeStringForMarkdown(trimmedContent), "]("_s, WTFMove(*urlString), ')'));
                    else
                        textParts.append(trimmedContent);
                } else
                    textParts.append(makeString('\'', escapeString(trimmedContent), '\''));

                if (includeSelectionAsAttribute && selectedRange && selectedRange->length > 0) {
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
        } else if (includeSelectionAsAttribute && selectedRange)
            textParts.append("selected=[0,0]"_s);

        textParts.appendVector(WTFMove(itemParts));
        aggregator->addResult(currentLine, WTFMove(textParts));
    };

    RefPtr filterPromise = aggregator->filter(textItem.content, WTFMove(enclosingNode));
    if (!filterPromise) {
        completion(String { textItem.content });
        return;
    }

    filterPromise->whenSettled(RunLoop::mainSingleton(), [originalContent = textItem.content, completion = WTFMove(completion)](auto&& result) mutable {
        if (result)
            completion(WTFMove(*result));
        else
            completion(WTFMove(originalContent));
    });
}

static void addPartsForItem(const TextExtraction::Item& item, std::optional<NodeIdentifier>&& enclosingNode, const TextExtractionLine& line, TextExtractionAggregator& aggregator)
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
            case TextExtraction::ContainerType::Subscript:
                containerString = "subscript"_s;
                break;
            case TextExtraction::ContainerType::Superscript:
                containerString = "superscript"_s;
                break;
            case TextExtraction::ContainerType::Generic:
                break;
            }

            if (aggregator.useHTMLOutput()) {
                String tagName;
                if (containerType == TextExtraction::ContainerType::Root)
                    tagName = "body"_s;
                else if (!item.nodeName.isEmpty())
                    tagName = item.nodeName.convertToASCIILowercase();

                if (!tagName.isEmpty()) {
                    auto attributes = partsForItem(item, aggregator);
                    if (attributes.isEmpty())
                        parts.append(makeString('<', tagName, '>'));
                    else
                        parts.append(makeString('<', tagName, ' ', makeStringByJoining(attributes, " "_s), '>'));
                }
            } else if (aggregator.useMarkdownOutput()) {
                if (containerType == TextExtraction::ContainerType::BlockQuote)
                    parts.append(">"_s);
                else if (containerType == TextExtraction::ContainerType::ListItem) {
                    // FIXME: Convert ordered lists into 1., 2., 3. etc.
                    parts.append("-"_s);
                }
            } else {
                if (!containerString.isEmpty())
                    parts.append(WTFMove(containerString));

                parts.appendVector(partsForItem(item, aggregator));
            }
            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::TextItemData& textData) {
            addPartsForText(textData, partsForItem(item, aggregator), WTFMove(enclosingNode), line, aggregator);
        },
        [&](const TextExtraction::ContentEditableData& editableData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator);
                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));

                if (editableData.isPlainTextOnly)
                    parts.append("contenteditable='plaintext-only'"_s);
                else
                    parts.append("contenteditable"_s);
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("contentEditable"_s);
                parts.appendVector(partsForItem(item, aggregator));

                if (editableData.isFocused)
                    parts.append("focused"_s);

                if (editableData.isPlainTextOnly)
                    parts.append("plaintext"_s);
            }

            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::TextFormControlData& controlData) {
            auto tagName = aggregator.useTagNameForTextFormControls() ? item.nodeName.convertToASCIILowercase() : String { "textFormControl"_s };

            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator);

                if (!controlData.controlType.isEmpty() && !equalIgnoringASCIICase(controlData.controlType, item.nodeName))
                    attributes.insert(0, makeString("type='"_s, controlData.controlType, '\''));

                if (!controlData.autocomplete.isEmpty())
                    attributes.append(makeString("autocomplete='"_s, controlData.autocomplete, '\''));

                if (!controlData.editable.label.isEmpty())
                    attributes.append(makeString("label='"_s, escapeString(controlData.editable.label), '\''));

                if (!controlData.editable.placeholder.isEmpty())
                    attributes.append(makeString("placeholder='"_s, escapeString(controlData.editable.placeholder), '\''));

                if (attributes.isEmpty())
                    parts.append(makeString('<', tagName, '>'));
                else
                    parts.append(makeString('<', tagName, ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append(WTFMove(tagName));
                parts.appendVector(partsForItem(item, aggregator));

                if (!controlData.controlType.isEmpty() && !equalIgnoringASCIICase(controlData.controlType, item.nodeName))
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
            }

            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::LinkItemData& linkData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator);

                if (!linkData.completedURL.isEmpty() && aggregator.includeURLs())
                    attributes.append(makeString("href='"_s, normalizedURLString(linkData.completedURL), '\''));

                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("link"_s);
                parts.appendVector(partsForItem(item, aggregator));

                if (!linkData.completedURL.isEmpty() && aggregator.includeURLs())
                    parts.append(makeString("url='"_s, normalizedURLString(linkData.completedURL), '\''));
            }

            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::ScrollableItemData& scrollableData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator);
                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("scrollable"_s);
                parts.appendVector(partsForItem(item, aggregator));
                parts.append(makeString("contentSize=["_s, scrollableData.contentSize.width(), 'x', scrollableData.contentSize.height(), ']'));
            }
            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::SelectData& selectData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator);

                if (!selectData.selectedValues.isEmpty()) {
                    auto escapedValues = selectData.selectedValues.map([](auto& value) {
                        return makeString('\'', escapeString(value), '\'');
                    });
                    attributes.append(makeString("selected=["_s, commaSeparatedString(escapedValues), ']'));
                }

                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("select"_s);
                parts.appendVector(partsForItem(item, aggregator));

                if (!selectData.selectedValues.isEmpty()) {
                    auto escapedValues = selectData.selectedValues.map([](auto& value) {
                        return makeString('\'', escapeString(value), '\'');
                    });
                    parts.append(makeString("selected=["_s, commaSeparatedString(escapedValues), ']'));
                }

                if (selectData.isMultiple)
                    parts.append("multiple"_s);
            }

            aggregator.addResult(line, WTFMove(parts));
        },
        [&](const TextExtraction::ImageItemData& imageData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator);

                if (!imageData.completedSource.isEmpty() && aggregator.includeURLs())
                    attributes.append(makeString("src='"_s, normalizedURLString(imageData.completedSource), '\''));

                if (!imageData.altText.isEmpty())
                    attributes.append(makeString("alt='"_s, escapeString(imageData.altText), '\''));

                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (aggregator.useMarkdownOutput()) {
                String imageSource;
                if (auto attributeFromClient = item.clientAttributes.get("src"_s); !attributeFromClient.isEmpty())
                    imageSource = WTFMove(attributeFromClient);
                else
                    imageSource = normalizedURLString(imageData.completedSource);
                parts.append(makeString("!["_s, escapeStringForMarkdown(imageData.altText), "]("_s, WTFMove(imageSource), ')'));
            } else {
                parts.append("image"_s);
                parts.appendVector(partsForItem(item, aggregator));

                if (!imageData.completedSource.isEmpty() && aggregator.includeURLs())
                    parts.append(makeString("src='"_s, normalizedURLString(imageData.completedSource), '\''));

                if (!imageData.altText.isEmpty())
                    parts.append(makeString("alt='"_s, escapeString(imageData.altText), '\''));
            }

            aggregator.addResult(line, WTFMove(parts));
        }
    );
}

static bool childTextNodeIsRedundant(const TextExtractionAggregator& aggregator, const TextExtraction::Item& parent, const String& childText)
{
    if (parent.hasData<TextExtraction::LinkItemData>()) {
        if (valueOrDefault(aggregator.currentURLString()).containsIgnoringASCIICase(childText))
            return true;
    }

    if (auto formControl = parent.dataAs<TextExtraction::TextFormControlData>()) {
        auto& editable = formControl->editable;
        if (editable.placeholder.containsIgnoringASCIICase(childText))
            return true;

        if (editable.label.containsIgnoringASCIICase(childText))
            return true;

        return std::ranges::any_of(parent.ariaAttributes, [&](auto& entry) {
            return entry.value.containsIgnoringASCIICase(childText);
        });
    }

    return false;
}

static void addTextRepresentationRecursive(const TextExtraction::Item& item, std::optional<NodeIdentifier>&& enclosingNode, unsigned depth, TextExtractionAggregator& aggregator)
{
    auto identifier = item.nodeIdentifier;
    if (!identifier)
        identifier = enclosingNode;

    if (aggregator.onlyIncludeText()) {
        if (std::holds_alternative<TextExtraction::TextItemData>(item.data))
            addPartsForText(std::get<TextExtraction::TextItemData>(item.data), { }, std::optional { identifier }, { aggregator.advanceToNextLine(), depth }, aggregator);
        for (auto& child : item.children)
            addTextRepresentationRecursive(child, std::optional { identifier }, depth + 1, aggregator);
        return;
    }

    bool isLink = false;
    if (auto link = item.dataAs<TextExtraction::LinkItemData>()) {
        String linkURLString;
        if (auto attributeFromClient = item.clientAttributes.get("href"_s); !attributeFromClient.isEmpty())
            linkURLString = WTFMove(attributeFromClient);
        else
            linkURLString = normalizedURLString(link->completedURL);
        aggregator.pushURLString(WTFMove(linkURLString));
        isLink = true;
    }

    auto popURLScope = makeScopeExit([isLink, &aggregator] {
        if (isLink)
            aggregator.popURLString();
    });

    TextExtractionLine line { aggregator.advanceToNextLine(), depth };
    addPartsForItem(item, std::optional { identifier }, line, aggregator);

    auto closingTagName = [&] -> String {
        if (!aggregator.useHTMLOutput())
            return { };

        if (item.dataAs<TextExtraction::ContainerType>() == TextExtraction::ContainerType::Root)
            return "body"_s;

        return item.nodeName.convertToASCIILowercase();
    }();

    if (item.children.size() == 1) {
        if (auto text = item.children[0].dataAs<TextExtraction::TextItemData>()) {
            if (childTextNodeIsRedundant(aggregator, item, text->content.trim(isASCIIWhitespace)))
                return;

            if (aggregator.useHTMLOutput()) {
                addPartsForText(*text, partsForItem(item.children[0], aggregator), std::optional { identifier }, line, aggregator, makeString("</"_s, closingTagName, '>'));
                return;
            }

            // In the case of a single text child, we append that text to the same line.
            addPartsForItem(item.children[0], WTFMove(identifier), line, aggregator);
            return;
        }
    }

    for (auto& child : item.children)
        addTextRepresentationRecursive(child, std::optional { identifier }, depth + 1, aggregator);

    if (aggregator.useHTMLOutput() && !item.children.isEmpty())
        aggregator.addResult({ aggregator.advanceToNextLine(), depth }, { makeString("</"_s, closingTagName, '>') });
}

void convertToText(TextExtraction::Item&& item, TextExtractionOptions&& options, CompletionHandler<void(TextExtractionResult&&)>&& completion)
{
    Ref aggregator = TextExtractionAggregator::create(WTFMove(options), WTFMove(completion));

    addTextRepresentationRecursive(item, { }, 0, aggregator);
}

} // namespace WebKit
