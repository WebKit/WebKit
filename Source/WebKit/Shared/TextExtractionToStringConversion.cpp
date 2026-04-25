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
#include <wtf/JSONValues.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <unicode/ubrk.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebKit {

using namespace WebCore;

static String removeZeroWidthCharacters(const String& string)
{
    if (string.is8Bit())
        return string;

    return string.removeCharacters([](char16_t character) {
        switch (character) {
        case zeroWidthSpace:
        case zeroWidthNonJoiner:
        case zeroWidthJoiner:
        case zeroWidthNoBreakSpace:
        case wordJoiner:
        case functionApplication:
        case invisibleTimes:
        case invisibleSeparator:
            return true;
        default:
            return false;
        }
    });
}

static constexpr uint64_t linkContextWords = 5;

static Vector<CharacterRange> characterRangesFromLinks(const Vector<std::pair<URL, CharacterRange>>& links)
{
    return links.map([](auto& pair) {
        return pair.second;
    });
}

static String truncateByWordCount(StringView text, uint64_t wordLimit, const Vector<CharacterRange>& linkCharacterRanges = { })
{
    auto truncateComponent = [wordLimit](auto&& component, const Vector<CharacterRange>& localLinkRanges) -> String {
        if (component.isEmpty())
            return emptyString();

        auto* iterator = WTF::wordBreakIterator(component);
        if (!iterator)
            return component.toString();

        Vector<std::pair<int, int>> wordPositions;
        int position = 0;
        int stringLength = component.length();
        int wordStart = 0;

        while (position < stringLength) {
            wordStart = position;
            position = ubrk_following(iterator, position);
            if (position == UBRK_DONE)
                break;

            if (!position || !u_isalnum(component[position - 1]))
                continue;

            wordPositions.append({ wordStart, position });
        }

        uint64_t totalWords = wordPositions.size();
        if (totalWords <= wordLimit)
            return component.toString();

        int simpleCutPosition = wordPositions[wordLimit - 1].second;

        Vector<CharacterRange> linksBeyondCut;
        for (auto& range : localLinkRanges) {
            if (range.location + range.length > static_cast<uint64_t>(simpleCutPosition))
                linksBeyondCut.append(range);
        }

        if (linksBeyondCut.isEmpty())
            return makeString(component.left(simpleCutPosition), u"…"_str);

        Vector<std::pair<uint64_t, uint64_t>> protectedRanges;
        for (auto& linkRange : linksBeyondCut) {
            uint64_t linkStart = linkRange.location;
            uint64_t linkEnd = linkRange.location + linkRange.length;

            std::optional<uint64_t> firstWordIndex;
            std::optional<uint64_t> lastWordIndex;
            if (!linkRange.length) {
                for (uint64_t i = 0; i < totalWords; ++i) {
                    if (static_cast<uint64_t>(wordPositions[i].first) >= linkStart || static_cast<uint64_t>(wordPositions[i].second) >= linkStart) {
                        firstWordIndex = i;
                        lastWordIndex = i;
                        break;
                    }
                }
                if (!firstWordIndex && totalWords > 0) {
                    firstWordIndex = totalWords - 1;
                    lastWordIndex = totalWords - 1;
                }
            } else {
                for (uint64_t i = 0; i < totalWords; ++i) {
                    auto [wordStart, wordEnd] = wordPositions[i];
                    if (static_cast<uint64_t>(wordEnd) > linkStart && static_cast<uint64_t>(wordStart) < linkEnd) {
                        if (!firstWordIndex)
                            firstWordIndex = i;
                        lastWordIndex = i;
                    }
                }
            }

            if (!firstWordIndex)
                continue;

            uint64_t rangeStart;
            uint64_t rangeEnd;
            if (!linkRange.length) {
                // Zero-length ranges are virtual boundary markers. Protect linkContextWords
                // words ending at the anchor (the link is adjacent, not overlapping).
                rangeStart = *firstWordIndex >= linkContextWords ? *firstWordIndex - linkContextWords + 1 : 0;
                rangeEnd = *lastWordIndex;
            } else {
                rangeStart = *firstWordIndex > linkContextWords ? *firstWordIndex - linkContextWords : 0;
                rangeEnd = std::min(*lastWordIndex + linkContextWords, totalWords - 1);
            }
            protectedRanges.append({ rangeStart, rangeEnd });
        }

        if (protectedRanges.isEmpty())
            return makeString(component.left(simpleCutPosition), u"…"_str);

        std::sort(protectedRanges.begin(), protectedRanges.end());
        Vector<std::pair<uint64_t, uint64_t>> mergedRanges;
        mergedRanges.append(protectedRanges[0]);
        for (size_t i = 1; i < protectedRanges.size(); ++i) {
            auto& last = mergedRanges.last();
            if (protectedRanges[i].first <= last.second + 1)
                last.second = std::max(last.second, protectedRanges[i].second);
            else
                mergedRanges.append(protectedRanges[i]);
        }

        uint64_t totalProtectedWords = 0;
        for (auto& [rangeStart, rangeEnd] : mergedRanges)
            totalProtectedWords += rangeEnd - rangeStart + 1;

        while (totalProtectedWords > wordLimit && !mergedRanges.isEmpty()) {
            auto& [rangeStart, rangeEnd] = mergedRanges.first();
            uint64_t excess = totalProtectedWords - wordLimit;
            uint64_t rangeSize = rangeEnd - rangeStart + 1;
            if (excess >= rangeSize) {
                totalProtectedWords -= rangeSize;
                mergedRanges.removeAt(0);
            } else {
                rangeStart += excess;
                totalProtectedWords -= excess;
            }
        }

        if (mergedRanges.isEmpty())
            return makeString(component.left(simpleCutPosition), u"…"_str);

        uint64_t headWords = wordLimit > totalProtectedWords ? wordLimit - totalProtectedWords : 0;

        StringBuilder builder;
        if (headWords > 0) {
            int headEndPosition = wordPositions[headWords - 1].second;
            builder.append(component.left(headEndPosition));
        }

        std::optional<uint64_t> lastEmittedWordIndex;
        if (headWords > 0)
            lastEmittedWordIndex = headWords - 1;

        for (auto& [rangeStart, rangeEnd] : mergedRanges) {
            if (rangeEnd < headWords)
                continue;

            uint64_t effectiveStart = std::max(rangeStart, headWords);

            if (!lastEmittedWordIndex || effectiveStart > *lastEmittedWordIndex + 1)
                builder.append(u"…"_str);

            int charStart = wordPositions[effectiveStart].first;
            int charEnd = wordPositions[rangeEnd].second;
            builder.append(component.substring(charStart, charEnd - charStart));
            lastEmittedWordIndex = rangeEnd;
        }

        if (!lastEmittedWordIndex || *lastEmittedWordIndex < totalWords - 1)
            builder.append(u"…"_str);

        return builder.toString();
    };

    Vector<String> result;
    uint64_t cumulativeOffset = 0;
    for (auto component : text.splitAllowingEmptyEntries('\n')) {
        uint64_t componentStart = cumulativeOffset;
        uint64_t componentLength = component.length();

        Vector<CharacterRange> localRanges;
        for (auto& range : linkCharacterRanges) {
            uint64_t rangeStart = range.location;
            uint64_t rangeEnd = range.location + range.length;
            bool overlapsComponent = rangeEnd > componentStart && rangeStart < componentStart + componentLength;
            bool isZeroLengthAtEnd = !range.length && rangeStart == componentStart + componentLength;
            if (overlapsComponent || isZeroLengthAtEnd) {
                uint64_t localStart = rangeStart > componentStart ? rangeStart - componentStart : 0;
                uint64_t localEnd = std::min(rangeEnd - componentStart, componentLength);
                localRanges.append({ localStart, localEnd - localStart });
            }
        }

        result.append(truncateComponent(component, localRanges));
        cumulativeOffset += componentLength + 1;
    }

    return makeStringByJoining(WTF::move(result), "\n"_s);
}

static bool isEmptyMarkdownListItem(StringView line)
{
    return line == "-"_s || line == "- "_s;
}

std::optional<ExtractedNodeInfo> parseExtractedNodeInfo(StringView identifierString)
{
    Vector<uint64_t, 3> values;
    for (auto component : identifierString.split('_')) {
        auto rawValue = parseInteger<uint64_t>(component);
        if (!rawValue)
            return { };

        values.append(*rawValue);
    }

    auto validate = []<typename T>(uint64_t rawValue) -> std::optional<T> {
        if (!T::isValidIdentifier(rawValue))
            return { };
        return T { rawValue };
    };

    if (values.size() == 1) {
        if (auto node = validate.operator()<NodeIdentifier>(values.first()))
            return { { { }, WTF::move(*node) } };
    } else if (values.size() == 3) {
        auto frame = validate.operator()<FrameIdentifier>((values[0] << 32) | values[1]);
        auto node = validate.operator()<NodeIdentifier>(values[2]);
        if (frame && node)
            return { { WTF::move(frame), WTF::move(*node) } };
    }

    return { };
}

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
    result = makeStringByReplacingAll(result, "~~"_s, "\\~\\~"_s);
    return result;
}

struct TextExtractionLine {
    unsigned lineIndex { 0 };
    unsigned indentLevel { 0 };
    unsigned enclosingBlockNumber { 0 };
    unsigned superscriptLevel { 0 };
};

static bool shouldEmitFullStopBetweenLines(const TextExtractionLine& previous, const String& previousText, const TextExtractionLine& line, const String& text)
{
    if (previous.enclosingBlockNumber != line.enclosingBlockNumber)
        return false;

    if (previous.superscriptLevel + 1 != line.superscriptLevel)
        return false;

    return parseInteger<unsigned>(previousText) && parseInteger<unsigned>(text);
}

static bool shouldJoinWithPreviousLine(const TextExtractionLine& previous, const String& previousText, const TextExtractionLine& line, const String& text)
{
    if (previous.enclosingBlockNumber != line.enclosingBlockNumber)
        return false;

    ASSERT(!previousText.isEmpty());
    bool textIsNumericValue = false;
    text.toDouble(&textIsNumericValue);
    return isCurrencySymbol(previousText[previousText.length() - 1]) && textIsNumericValue;
}

static bool shouldEmitExtraSpace(char16_t previousCharacter, char16_t nextCharacter)
{
    if (isUnicodeWhitespace(previousCharacter) || isUnicodeWhitespace(nextCharacter))
        return false;

    auto previousCharacterMask = U_GET_GC_MASK(previousCharacter);
    if (isOpeningPunctuation(previousCharacterMask) || (previousCharacterMask & U_GC_PI_MASK))
        return false;

    auto nextCharacterMask = U_GET_GC_MASK(nextCharacter);
    if (isOpeningPunctuation(nextCharacterMask) || (nextCharacterMask & U_GC_PI_MASK))
        return true;

    return !(nextCharacterMask & U_GC_PO_MASK);
}

enum class HasAdjacentLinkAfter : bool { No, Yes };

class TextExtractionAggregator : public RefCounted<TextExtractionAggregator> {
    WTF_MAKE_NONCOPYABLE(TextExtractionAggregator);
    WTF_MAKE_TZONE_ALLOCATED(TextExtractionAggregator);
public:
    TextExtractionAggregator(TextExtractionOptions&& options, CompletionHandler<void(TextExtractionResult&&)>&& completion)
        : m_options(WTF::move(options))
        , m_completion(WTF::move(completion))
    {
        if (version() >= 2)
            m_versionBehaviors.add(TextExtractionVersionBehavior::TagNameForTextFormControls);
    }

    ~TextExtractionAggregator()
    {
        addNativeMenuItemsIfNeeded();
        addVersionNumberIfNeeded();

        m_completion({ takeResults(), m_filteredOutAnyText, std::exchange(m_shortenedURLStrings, { }), std::exchange(m_textToContainerMap, { }) });
    }

    String takeResults()
    {
        if (useJSONOutput()) {
            auto rootObject = WTF::move(m_rootJSONObject);
            if (!rootObject) {
                ASSERT_NOT_REACHED();
                return "{}"_s;
            }

            return rootObject->toJSONString();
        }

        m_lines.removeAllMatching([](auto& line) {
            return line.first.isEmpty();
        });

        if (useMarkdownOutput()) {
            m_lines.removeAllMatching([](auto& line) {
                return isEmptyMarkdownListItem(line.first);
            });
        }

        if (m_lines.size() > 1) {
            Vector<std::pair<String, TextExtractionLine>> unduplicatedLines;
            unduplicatedLines.reserveInitialCapacity(m_lines.size());
            for (auto& line : m_lines) {
                if (unduplicatedLines.isEmpty() || unduplicatedLines.last().first != line.first)
                    unduplicatedLines.append(WTF::move(line));
            }
            m_lines = WTF::move(unduplicatedLines);
        }

        if (useTextTreeOutput() || useHTMLOutput()) {
            return makeStringByJoining(m_lines.map([](auto& stringAndLine) {
                return stringAndLine.first;
            }), "\n"_s);
        }

        std::optional<TextExtractionLine> previousLine;
        String previousText;
        StringBuilder buffer;
        for (auto&& [text, line] : WTF::move(m_lines)) {
            auto separator = [&] -> std::optional<char> {
                if (!previousLine)
                    return std::nullopt;

                if (shouldJoinWithPreviousLine(*previousLine, previousText, line, text))
                    return std::nullopt;

                if (shouldEmitFullStopBetweenLines(*previousLine, previousText, line, text))
                    return '.';

                if (previousLine->enclosingBlockNumber == line.enclosingBlockNumber) {
                    if (shouldEmitExtraSpace(previousText[previousText.length() - 1], text[0]))
                        return ' ';

                    return std::nullopt;
                }

                return '\n';
            }();

            if (separator)
                buffer.append(*separator);

            previousLine = { WTF::move(line) };
            previousText = { text };
            buffer.append(WTF::move(text));
        }

        return buffer.toString();
    }

    static Ref<TextExtractionAggregator> create(TextExtractionOptions&& options, CompletionHandler<void(TextExtractionResult&&)>&& completion)
    {
        return adoptRef(*new TextExtractionAggregator(WTF::move(options), WTF::move(completion)));
    }

    void addResult(const TextExtractionLine& line, Vector<String>&& components)
    {
        if (components.isEmpty())
            return;

        auto [lineIndex, indentLevel, enclosingBlockNumber, superscriptLevel] = line;
        if (lineIndex >= m_lines.size()) {
            ASSERT_NOT_REACHED();
            return;
        }

        static constexpr auto separator = " "_s;
        auto text = makeStringByJoining(WTF::move(components), separator);

        if (!m_lines[lineIndex].first.isEmpty()) {
            m_lines[lineIndex].first = makeString(m_lines[lineIndex].first, separator, WTF::move(text));
            return;
        }

        if (usePlainTextOutput()) {
            m_lines[lineIndex] = { WTF::move(text), line };
            return;
        }

        StringBuilder indentation;

        if (!useMarkdownOutput()) {
            indentation.reserveCapacity(indentLevel);
            for (unsigned i = 0; i < indentLevel; ++i)
                indentation.append('\t');
        }

        m_lines[lineIndex] = { makeString(indentation.toString(), WTF::move(text)), line };
    }

    unsigned advanceToNextLine()
    {
        auto index = m_nextLineIndex++;
        m_lines.resize(m_nextLineIndex);
        return index;
    }

    bool NODELETE useTagNameForTextFormControls() const
    {
        return m_versionBehaviors.contains(TextExtractionVersionBehavior::TagNameForTextFormControls);
    }

    bool NODELETE includeRects() const
    {
        return !usePlainTextOutput() && m_options.flags.contains(TextExtractionOptionFlag::IncludeRects);
    }

    bool NODELETE includeSelectOptions() const
    {
        return !usePlainTextOutput() && m_options.flags.contains(TextExtractionOptionFlag::IncludeSelectOptions);
    }

    bool NODELETE includeURLs() const
    {
        return !usePlainTextOutput() && m_options.flags.contains(TextExtractionOptionFlag::IncludeURLs);
    }

    bool NODELETE shortenURLs() const
    {
        return m_options.flags.contains(TextExtractionOptionFlag::ShortenURLs);
    }

    bool NODELETE usePlainTextOutput() const
    {
        return m_options.outputFormat == TextExtractionOutputFormat::PlainText;
    }

    bool NODELETE useHTMLOutput() const
    {
        return m_options.outputFormat == TextExtractionOutputFormat::HTMLMarkup;
    }

    bool NODELETE useMarkdownOutput() const
    {
        return m_options.outputFormat == TextExtractionOutputFormat::Markdown;
    }

    bool NODELETE useTextTreeOutput() const
    {
        return m_options.outputFormat == TextExtractionOutputFormat::TextTree;
    }

    bool NODELETE useJSONOutput() const
    {
        return m_options.outputFormat == TextExtractionOutputFormat::MinifiedJSON;
    }

    RefPtr<TextExtractionFilterPromise> filter(const String& text, const std::optional<FrameIdentifier>& frameIdentifier, const std::optional<NodeIdentifier>& identifier)
    {
        if (m_options.filterCallbacks.isEmpty())
            return nullptr;

        TextExtractionFilterPromise::Producer producer;
        Ref promise = producer.promise();

        filterRecursive(text, frameIdentifier, identifier, 0, [producer = WTF::move(producer)](auto&& result) mutable {
            producer.settle(WTF::move(result));
        });

        return promise;
    }

    void applyReplacements(String& text)
    {
        for (auto& [original, replacement] : m_options.replacementStrings)
            text = makeStringByReplacingAll(text, original, replacement);
    }

    void truncateTextByWordLimitIfNeeded(String& text, const Vector<CharacterRange>& linkCharacterRanges = { }, HasAdjacentLinkAfter hasAdjacentLinkAfter = HasAdjacentLinkAfter::No)
    {
        if (!m_options.maxWordsPerParagraph)
            return;

        Vector<CharacterRange> ranges = linkCharacterRanges;
        if (hasAdjacentLinkAfter == HasAdjacentLinkAfter::Yes)
            ranges.append({ text.length(), 0 });

        auto truncated = truncateByWordCount(text, *m_options.maxWordsPerParagraph, ranges);
        if (truncated != text) {
            m_filteredOutAnyText = true;
            text = WTF::move(truncated);
        }
    }

    void appendToLine(unsigned lineIndex, const String& text)
    {
        if (lineIndex >= m_lines.size()) {
            ASSERT_NOT_REACHED();
            return;
        }
        m_lines[lineIndex].first = makeString(m_lines[lineIndex].first, text);
    }

    void pushURLString(String&& urlString)
    {
        m_urlStringStack.append(WTF::move(urlString));
    }

    std::optional<String> NODELETE currentURLString() const
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

    void NODELETE pushSuperscript() { m_superscriptLevel++; }
    bool NODELETE superscriptLevel() const { return m_superscriptLevel; }
    void NODELETE popSuperscript()
    {
        if (!m_superscriptLevel) {
            ASSERT_NOT_REACHED();
            return;
        }
        m_superscriptLevel--;
    }

    void NODELETE pushStrikethrough() { m_strikethroughLevel++; }
    bool NODELETE isInsideStrikethrough() const { return m_strikethroughLevel > 0; }
    void NODELETE popStrikethrough()
    {
        if (!m_strikethroughLevel) {
            ASSERT_NOT_REACHED();
            return;
        }
        m_strikethroughLevel--;
    }

    String stringForURL(const TextExtraction::LinkItemData& data)
    {
        return stringForURL(data.shortenedURLString, data.completedURL, ExtractedURLType::Link);
    }

    String stringForURL(const TextExtraction::ImageItemData& data)
    {
        return stringForURL(data.shortenedName, data.completedSource, ExtractedURLType::Image);
    }

    JSON::Object& rootJSONObject()
    {
        ASSERT(useJSONOutput());
        if (!m_rootJSONObject)
            m_rootJSONObject = JSON::Object::create();
        return *m_rootJSONObject;
    }

    String stringForIdentifiers(const std::optional<FrameIdentifier>& frameIdentifier, const NodeIdentifier& nodeIdentifier) const
    {
        if (!frameIdentifier || m_options.mainFrameIdentifier == frameIdentifier)
            return makeString(nodeIdentifier.toUInt64());

        auto frameIdentifierValue = frameIdentifier->toUInt64();
        return makeString(frameIdentifierValue >> 32, '_', (frameIdentifierValue & 0xFFFFFFFF), '_', nodeIdentifier.toUInt64());
    }

    void collectTextMapping(const String& text, const std::optional<FrameIdentifier>& frameIdentifier, const std::optional<NodeIdentifier>& nodeIdentifier, ExtractedNodeInfo::IsInteractive interactivity = ExtractedNodeInfo::IsInteractive::No)
    {
        auto trimmedText = text.trim(isASCIIWhitespace);
        if (trimmedText.isEmpty() || !nodeIdentifier)
            return;

        auto& containers = m_textToContainerMap.ensure(trimmedText, [&] {
            return Vector<ExtractedNodeInfo> { };
        }).iterator->value;

        if (auto identifiers = ExtractedNodeInfo { frameIdentifier, *nodeIdentifier, interactivity }; containers.isEmpty() || containers.last() != identifiers)
            containers.append(WTF::move(identifiers));
    }

private:
    void filterRecursive(const String& originalText, const std::optional<FrameIdentifier>& frameIdentifier, const std::optional<NodeIdentifier>& identifier, size_t index, CompletionHandler<void(String&&)>&& completion)
    {
        if (index >= m_options.filterCallbacks.size())
            return completion(String { originalText });

        Ref promise = m_options.filterCallbacks[index](originalText, std::optional { frameIdentifier }, std::optional { identifier });
        promise->whenSettled(RunLoop::mainSingleton(), [originalText, completion = WTF::move(completion), protectedThis = Ref { *this }, frameIdentifier, identifier, index](auto&& result) mutable {
            if (originalText != result)
                protectedThis->m_filteredOutAnyText = true;

            if (!result)
                return completion({ });

            protectedThis->filterRecursive(WTF::move(*result), frameIdentifier, identifier, index + 1, WTF::move(completion));
        });
    }

    String stringForURL(const String& shortenedString, const URL& url, ExtractedURLType type)
    {
        auto string = [&] -> String {
            if (!url.isValid())
                return { };

            if (!shortenURLs())
                return url.string();

            auto stringToShorten = shortenedString;
            if (!m_options.topHostName.isEmpty() && stringToShorten.startsWithIgnoringASCIICase(m_options.topHostName)) {
                auto rest = stringToShorten.substring(m_options.topHostName.length());
                stringToShorten = rest.isEmpty() ? "/"_s : rest;
            }

            RefPtr cache = m_options.urlCache;
            if (!cache)
                return stringToShorten;

            auto result = cache->add(stringToShorten, url, type);
            if (!result.isEmpty())
                m_shortenedURLStrings.append(result);

            return result;
        }();

        static constexpr auto maxURLStringLength = 150;
        static constexpr auto halfTruncatedLength = maxURLStringLength / 2 - 1;

        auto stringLength = string.length();
        if (stringLength < maxURLStringLength)
            return string;

        return makeString(string.left(halfTruncatedLength), u"…"_str, string.right(halfTruncatedLength));
    }

    void addNativeMenuItemsIfNeeded()
    {
        if (usePlainTextOutput())
            return;

        if (m_options.nativeMenuItems.isEmpty())
            return;

        if (useJSONOutput()) {
            Ref itemsArray = JSON::Array::create();
            for (auto& itemTitle : m_options.nativeMenuItems)
                itemsArray->pushString(itemTitle);

            Ref menuObject = JSON::Object::create();
            menuObject->setString("type"_s, "nativePopupMenu"_s);
            menuObject->setArray("items"_s, WTF::move(itemsArray));

            if (RefPtr children = protect(rootJSONObject())->getArray("children"_s))
                children->pushObject(WTF::move(menuObject));
            return;
        }

        auto escapedQuotedItemTitles = m_options.nativeMenuItems.map([](auto& itemTitle) {
            return makeString('\'', escapeString(itemTitle), '\'');
        });
        auto itemsDescription = makeString("items=["_s, commaSeparatedString(escapedQuotedItemTitles), ']');
        addResult({ advanceToNextLine(), 0 }, { "nativePopupMenu"_s, WTF::move(itemsDescription) });
    }

    void addVersionNumberIfNeeded()
    {
        if (!useJSONOutput())
            return;

        protect(rootJSONObject())->setInteger("version"_s, version());
    }

    uint32_t NODELETE version() const
    {
        return m_options.version.value_or(currentTextExtractionOutputVersion);
    }

    const TextExtractionOptions m_options;
    Vector<std::pair<String, TextExtractionLine>> m_lines;
    Vector<String, 1> m_urlStringStack;
    unsigned m_superscriptLevel { 0 };
    unsigned m_strikethroughLevel { 0 };
    unsigned m_nextLineIndex { 0 };
    CompletionHandler<void(TextExtractionResult&&)> m_completion;
    TextExtractionVersionBehaviors m_versionBehaviors;
    bool m_filteredOutAnyText { false };
    Vector<String> m_shortenedURLStrings;
    HashMap<String, Vector<ExtractedNodeInfo>> m_textToContainerMap;
    RefPtr<JSON::Object> m_rootJSONObject;
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

static String containerTypeString(TextExtraction::ContainerType containerType)
{
    switch (containerType) {
    case TextExtraction::ContainerType::ViewportConstrained:
        return "overlay"_s;
    case TextExtraction::ContainerType::List:
        return "list"_s;
    case TextExtraction::ContainerType::ListItem:
        return "list-item"_s;
    case TextExtraction::ContainerType::BlockQuote:
        return "block-quote"_s;
    case TextExtraction::ContainerType::Article:
        return "article"_s;
    case TextExtraction::ContainerType::Section:
        return "section"_s;
    case TextExtraction::ContainerType::Nav:
        return "navigation"_s;
    case TextExtraction::ContainerType::Button:
        return "button"_s;
    case TextExtraction::ContainerType::Canvas:
        return "canvas"_s;
    case TextExtraction::ContainerType::Subscript:
        return "subscript"_s;
    case TextExtraction::ContainerType::Superscript:
        return "superscript"_s;
    case TextExtraction::ContainerType::Strikethrough:
        return "strikethrough"_s;
    case TextExtraction::ContainerType::Generic:
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

static String jsonTypeStringForItem(const TextExtraction::Item& item, const TextExtractionAggregator& aggregator)
{
    return WTF::switchOn(item.data,
        [](TextExtraction::ContainerType containerType) -> String {
            auto result = containerTypeString(containerType);
            return result.isEmpty() ? "container"_str : result;
        },
        [](const TextExtraction::TextItemData&) -> String { return "text"_s; },
        [](const TextExtraction::ScrollableItemData& scrollableData) -> String { return scrollableData.isRoot ? "root"_s : "scrollable"_s; },
        [](const TextExtraction::ImageItemData&) -> String { return "image"_s; },
        [](const TextExtraction::SelectData&) -> String { return "select"_s; },
        [](const TextExtraction::ContentEditableData&) -> String { return "contentEditable"_s; },
        [&](const TextExtraction::TextFormControlData&) -> String {
            if (aggregator.useTagNameForTextFormControls())
                return item.nodeName.convertToASCIILowercase();

            return "textFormControl"_s;
        },
        [](const TextExtraction::FormData&) -> String { return "form"_s; },
        [](const TextExtraction::LinkItemData&) -> String { return "link"_s; },
        [](const TextExtraction::IFrameData&) -> String { return "iframe"_s; }
    );
}

static bool shouldIncludeFormControlValue(const TextExtraction::TextFormControlData& controlData, const TextExtraction::Item& item)
{
    if (controlData.value.isEmpty())
        return false;

    if (equalLettersIgnoringASCIICase(controlData.value, "on"_s))
        return false;

    if (!equalLettersIgnoringASCIICase(controlData.controlType, "radio"_s) && !equalLettersIgnoringASCIICase(controlData.controlType, "checkbox"_s))
        return false;

    return !item.children.containsIf([](auto& child) {
        auto text = child.template dataAs<TextExtraction::TextItemData>();
        return text && !text->content.template containsOnly<isASCIIWhitespace>();
    });
}

template<typename T> static Vector<String> sortedKeys(const HashMap<String, T>& dictionary)
{
    auto keys = copyToVector(dictionary.keys());
    std::ranges::sort(keys, codePointCompareLessThan);
    return keys;
}

static Ref<JSON::Array> eventListenerTypesToJSONArray(OptionSet<TextExtraction::EventListenerCategory> eventListeners)
{
    Ref result = JSON::Array::create();
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Click))
        result->pushString("click"_s);
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Hover))
        result->pushString("hover"_s);
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Touch))
        result->pushString("touch"_s);
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Wheel))
        result->pushString("wheel"_s);
    if (eventListeners.contains(TextExtraction::EventListenerCategory::Keyboard))
        result->pushString("keyboard"_s);
    return result;
}

static void setCommonJSONProperties(JSON::Object& jsonObject, const TextExtraction::Item& item, const TextExtractionAggregator& aggregator)
{
    if (!item.nodeName.isEmpty() && !item.hasData<TextExtraction::TextItemData>())
        jsonObject.setString("nodeName"_s, item.nodeName.convertToASCIILowercase());

    if (item.nodeIdentifier)
        jsonObject.setString("uid"_s, aggregator.stringForIdentifiers(item.frameIdentifier, *item.nodeIdentifier));

    if (aggregator.includeRects()) {
        Ref rect = JSON::Object::create();
        rect->setInteger("x"_s, static_cast<int>(item.rectInRootView.x()));
        rect->setInteger("y"_s, static_cast<int>(item.rectInRootView.y()));
        rect->setInteger("width"_s, static_cast<int>(item.rectInRootView.width()));
        rect->setInteger("height"_s, static_cast<int>(item.rectInRootView.height()));
        jsonObject.setObject("rect"_s, WTF::move(rect));
    }

    if (!item.accessibilityRole.isEmpty())
        jsonObject.setString("role"_s, item.accessibilityRole);

    if (!item.title.isEmpty())
        jsonObject.setString("title"_s, item.title);

    if (!item.eventListeners.isEmpty())
        jsonObject.setArray("events"_s, eventListenerTypesToJSONArray(item.eventListeners));

    if (!item.ariaAttributes.isEmpty()) {
        for (auto& [key, value] : item.ariaAttributes)
            jsonObject.setString(key, value);
    }

    if (!item.clientAttributes.isEmpty()) {
        for (auto& [key, value] : item.clientAttributes)
            jsonObject.setString(key, value);
    }
}

static void addJSONTextContent(Ref<JSON::Object>&& jsonObject, const TextExtraction::TextItemData& textData, const std::optional<FrameIdentifier>& frameIdentifier, const std::optional<NodeIdentifier>& identifier, TextExtractionAggregator& aggregator)
{
    CompletionHandler<void(String&&)> completion = [jsonObject = WTF::move(jsonObject), aggregator = Ref { aggregator }, selectedRange = textData.selectedRange, linkRanges = characterRangesFromLinks(textData.links)](String&& filteredText) mutable {
        if (filteredText.isEmpty())
            return;

        auto content = removeZeroWidthCharacters(filteredText.trim(isASCIIWhitespace).simplifyWhiteSpace(isASCIIWhitespace));
        aggregator->applyReplacements(content);
        aggregator->truncateTextByWordLimitIfNeeded(content, linkRanges);

        if (content.isEmpty())
            return;

        jsonObject->setString("content"_s, content);

        if (selectedRange && selectedRange->length > 0) {
            Ref selected = JSON::Object::create();
            selected->setInteger("start"_s, selectedRange->location);
            selected->setInteger("end"_s, selectedRange->location + selectedRange->length);
            jsonObject->setObject("selected"_s, WTF::move(selected));
        }
    };

    auto originalContent = textData.content;
    RefPtr filterPromise = aggregator.filter(originalContent, frameIdentifier, identifier);
    if (!filterPromise)
        return completion(WTF::move(originalContent));

    filterPromise->whenSettled(RunLoop::mainSingleton(), [completion = WTF::move(completion), originalContent](auto&& result) mutable {
        if (result)
            completion(WTF::move(*result));
        else
            completion(WTF::move(originalContent));
    });
}

static void populateJSONForItem(JSON::Object&, const TextExtraction::Item&, std::optional<NodeIdentifier>&&, TextExtractionAggregator&);

static Ref<JSON::Object> createJSONForChildItem(const TextExtraction::Item& item, std::optional<NodeIdentifier>&& enclosingNode, TextExtractionAggregator& aggregator)
{
    Ref jsonObject = JSON::Object::create();
    populateJSONForItem(jsonObject.get(), item, WTF::move(enclosingNode), aggregator);
    return jsonObject;
}

static void populateJSONForItem(JSON::Object& jsonObject, const TextExtraction::Item& item, std::optional<NodeIdentifier>&& enclosingNode, TextExtractionAggregator& aggregator)
{
    jsonObject.setString("type"_s, jsonTypeStringForItem(item, aggregator));

    setCommonJSONProperties(jsonObject, item, aggregator);

    auto identifier = item.nodeIdentifier ? item.nodeIdentifier : enclosingNode;

    WTF::switchOn(item.data,
        [&](const TextExtraction::TextItemData& textData) {
            aggregator.collectTextMapping(textData.content, item.frameIdentifier, identifier, item.nodeIdentifier ? ExtractedNodeInfo::IsInteractive::Yes : ExtractedNodeInfo::IsInteractive::No);
            addJSONTextContent(Ref { jsonObject }, textData, item.frameIdentifier, identifier, aggregator);
        },
        [&](const TextExtraction::ScrollableItemData& scrollableData) {
            Ref contentSize = JSON::Object::create();
            contentSize->setInteger("width"_s, static_cast<int>(scrollableData.contentSize.width()));
            contentSize->setInteger("height"_s, static_cast<int>(scrollableData.contentSize.height()));
            if (scrollableData.hasOverflowItems) {
                jsonObject.setObject("contentSize"_s, WTF::move(contentSize));
                Ref scrollPosition = JSON::Object::create();
                scrollPosition->setInteger("x"_s, scrollableData.scrollPosition.x());
                scrollPosition->setInteger("y"_s, scrollableData.scrollPosition.y());
                jsonObject.setObject("scrollPosition"_s, WTF::move(scrollPosition));
            }
        },
        [&](const TextExtraction::ImageItemData& imageData) {
            if (!imageData.completedSource.isEmpty() && aggregator.includeURLs())
                jsonObject.setString("src"_s, aggregator.stringForURL(imageData));
            if (!imageData.altText.isEmpty())
                jsonObject.setString("alt"_s, imageData.altText);
        },
        [&](const TextExtraction::SelectData& selectData) {
            Ref optionsArray = JSON::Array::create();
            if (aggregator.includeSelectOptions()) {
                for (auto& option : selectData.options) {
                    Ref object = JSON::Object::create();
                    object->setString("value"_s, option.value);
                    object->setString("label"_s, option.label);
                    object->setBoolean("selected"_s, option.isSelected);
                    optionsArray->pushObject(WTF::move(object));
                }
            }
            if (optionsArray->length())
                jsonObject.setArray("options"_s, WTF::move(optionsArray));
            if (selectData.isMultiple)
                jsonObject.setBoolean("multiple"_s, true);
        },
        [&](const TextExtraction::ContentEditableData& editableData) {
            if (editableData.isPlainTextOnly)
                jsonObject.setBoolean("plaintextOnly"_s, true);
            if (editableData.isFocused)
                jsonObject.setBoolean("focused"_s, true);
        },
        [&](const TextExtraction::TextFormControlData& controlData) {
            if (!controlData.controlType.isEmpty())
                jsonObject.setString("controlType"_s, controlData.controlType);
            if (!controlData.autocomplete.isEmpty())
                jsonObject.setString("autocomplete"_s, controlData.autocomplete);
            if (!controlData.editable.label.isEmpty())
                jsonObject.setString("label"_s, controlData.editable.label);
            if (!controlData.editable.placeholder.isEmpty())
                jsonObject.setString("placeholder"_s, controlData.editable.placeholder);
            if (!controlData.pattern.isEmpty())
                jsonObject.setString("pattern"_s, controlData.pattern);
            if (!controlData.name.isEmpty())
                jsonObject.setString("name"_s, controlData.name);
            if (shouldIncludeFormControlValue(controlData, item))
                jsonObject.setString("value"_s, controlData.value);
            if (controlData.minLength)
                jsonObject.setInteger("minLength"_s, *controlData.minLength);
            if (controlData.maxLength)
                jsonObject.setInteger("maxLength"_s, *controlData.maxLength);
            if (controlData.isRequired)
                jsonObject.setBoolean("required"_s, true);
            if (controlData.isReadonly)
                jsonObject.setBoolean("readonly"_s, true);
            if (controlData.isDisabled)
                jsonObject.setBoolean("disabled"_s, true);
            if (controlData.isChecked)
                jsonObject.setBoolean("checked"_s, true);
            if (controlData.editable.isSecure)
                jsonObject.setBoolean("secure"_s, true);
            if (controlData.editable.isFocused)
                jsonObject.setBoolean("focused"_s, true);
            if (controlData.isAutofilled)
                jsonObject.setBoolean("autofilled"_s, true);
        },
        [&](const TextExtraction::FormData& formData) {
            if (!formData.autocomplete.isEmpty())
                jsonObject.setString("autocomplete"_s, formData.autocomplete);
            if (!formData.name.isEmpty())
                jsonObject.setString("name"_s, formData.name);
        },
        [&](const TextExtraction::LinkItemData& linkData) {
            if (!linkData.completedURL.isEmpty() && aggregator.includeURLs())
                jsonObject.setString("url"_s, aggregator.stringForURL(linkData));
            if (!linkData.target.isEmpty())
                jsonObject.setString("target"_s, linkData.target);
        },
        [&](const TextExtraction::IFrameData& iframeData) {
            if (!iframeData.origin.isEmpty())
                jsonObject.setString("origin"_s, iframeData.origin);
        },
        [](auto) { }
    );

    if (!item.children.isEmpty()) {
        Ref children = JSON::Array::create();
        for (auto& child : item.children)
            children->pushObject(createJSONForChildItem(child, std::optional { identifier }, aggregator));
        jsonObject.setArray("children"_s, WTF::move(children));
    }
}

static String quoteValue(const String& value, bool conditionalQuoting)
{
    if (!conditionalQuoting || value.contains(' '))
        return makeString('\'', value, '\'');
    return value;
}

enum class IncludeRectForParentItem : bool { No, Yes };

static Vector<String> partsForItem(const TextExtraction::Item& item, const TextExtractionAggregator& aggregator, IncludeRectForParentItem includeRectForParentItem)
{
    Vector<String> parts;
    bool streamlined = aggregator.useTextTreeOutput();

    if (item.nodeIdentifier)
        parts.append(makeString("uid="_s, aggregator.stringForIdentifiers(item.frameIdentifier, *item.nodeIdentifier)));

    if ((item.children.isEmpty() || includeRectForParentItem == IncludeRectForParentItem::Yes) && aggregator.includeRects() && !aggregator.useHTMLOutput()) {
        auto origin = item.rectInRootView.location();
        auto size = item.rectInRootView.size();
        parts.append(makeString("["_s,
            static_cast<int>(origin.x()), ',', static_cast<int>(origin.y()), ";"_s,
            static_cast<int>(size.width()), u'×', static_cast<int>(size.height()), ']'));
    }

    if (!item.accessibilityRole.isEmpty())
        parts.append(makeString("role="_s, quoteValue(escapeString(item.accessibilityRole), streamlined)));

    if (!item.title.isEmpty())
        parts.append(makeString("title="_s, quoteValue(escapeString(item.title), streamlined)));

    if (!streamlined) {
        auto listeners = eventListenerTypesToStringArray(item.eventListeners);
        if (!listeners.isEmpty() && !aggregator.useHTMLOutput()) {
            if (listeners.size() == 1)
                parts.append(makeString("events="_s, listeners.first()));
            else
                parts.append(makeString("events=["_s, commaSeparatedString(listeners), ']'));
        }
    }

    StringView firstChildText;
    if (streamlined && !item.children.isEmpty()) {
        if (auto* textData = std::get_if<TextExtraction::TextItemData>(&item.children[0].data))
            firstChildText = textData->content;
    }

    for (auto& key : sortedKeys(item.ariaAttributes)) {
        auto value = item.ariaAttributes.get(key);
        auto outputKey = streamlined && key.startsWith("aria-"_s) ? key.substring(5) : key;

        if (streamlined && value == "false"_s)
            continue;

        if (streamlined && outputKey == "label"_s) {
            auto trimmed = firstChildText.trim(isASCIIWhitespace);
            if (!trimmed.isEmpty() && trimmed.contains(value))
                continue;
        }

        parts.append(makeString(outputKey, '=', quoteValue(escapeString(value), streamlined)));
    }

    for (auto& key : sortedKeys(item.clientAttributes))
        parts.append(makeString(key, '=', quoteValue(item.clientAttributes.get(key), streamlined)));

    return parts;
}

enum class HasLineThroughStyle : bool { No, Yes };

static void addPartsForText(const TextExtraction::TextItemData& textItem, Vector<String>&& itemParts, const std::optional<FrameIdentifier>& frameIdentifier, const std::optional<NodeIdentifier>& enclosingNode, const TextExtractionLine& line, Ref<TextExtractionAggregator>&& aggregator, HasLineThroughStyle hasLineThrough = HasLineThroughStyle::No, const String& closingTag = { }, HasAdjacentLinkAfter hasAdjacentLinkAfter = HasAdjacentLinkAfter::No)
{
    auto completion = [
        itemParts = WTF::move(itemParts),
        selectedRange = textItem.selectedRange,
        aggregator,
        line,
        closingTag,
        urlString = aggregator->currentURLString(),
        isStrikethrough = aggregator->isInsideStrikethrough() || hasLineThrough == HasLineThroughStyle::Yes,
        linkRanges = characterRangesFromLinks(textItem.links),
        hasAdjacentLinkAfter
    ](String&& filteredText) mutable {
        Vector<String> textParts;
        auto currentLine = line;
        bool includeSelectionAsAttribute = !aggregator->useHTMLOutput() && !aggregator->useMarkdownOutput();
        if (!filteredText.isEmpty()) {
            // Apply replacements only after filtering, so any filtering steps that rely on comparing DOM text against
            // visual data (e.g. recognized text) won't result in false positives.
            aggregator->applyReplacements(filteredText);
            aggregator->truncateTextByWordLimitIfNeeded(filteredText, linkRanges, hasAdjacentLinkAfter);

            if (aggregator->usePlainTextOutput()) {
                aggregator->addResult(currentLine, { escapeString(removeZeroWidthCharacters(filteredText.trim(isASCIIWhitespace).simplifyWhiteSpace(isASCIIWhitespace))) });
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
                    if (!isASCIIWhitespace(filteredText.codeUnitAt(i - 1))) {
                        endIndex = i - 1;
                        break;
                    }
                }

                auto trimmedContent = removeZeroWidthCharacters(filteredText.substring(startIndex, endIndex - startIndex + 1));
                if (aggregator->useHTMLOutput()) {
                    if (!closingTag.isEmpty()) {
                        aggregator->appendToLine(currentLine.lineIndex, makeString(escapeStringForHTML(trimmedContent), closingTag));
                        return;
                    }
                    textParts.append(escapeStringForHTML(trimmedContent));
                } else if (aggregator->useMarkdownOutput()) {
                    auto escapedText = escapeStringForMarkdown(trimmedContent);
                    if (valueOrDefault(urlString).containsIgnoringASCIICase(escapedText))
                        escapedText = { };
                    escapedText = urlString ? makeString('[', WTF::move(escapedText), "]("_s, WTF::move(*urlString), ')') : escapedText;
                    if (isStrikethrough)
                        escapedText = makeString("~~"_s, WTF::move(escapedText), "~~"_s);
                    textParts.append(WTF::move(escapedText));
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

        textParts.appendVector(WTF::move(itemParts));
        aggregator->addResult(currentLine, WTF::move(textParts));
    };

    RefPtr filterPromise = aggregator->filter(textItem.content, frameIdentifier, enclosingNode);
    if (!filterPromise) {
        completion(String { textItem.content });
        return;
    }

    filterPromise->whenSettled(RunLoop::mainSingleton(), [originalContent = textItem.content, completion = WTF::move(completion)](auto&& result) mutable {
        if (result)
            completion(WTF::move(*result));
        else
            completion(WTF::move(originalContent));
    });
}

static void addPartsForItem(const TextExtraction::Item& item, std::optional<NodeIdentifier>&& enclosingNode, const TextExtractionLine& line, TextExtractionAggregator& aggregator, IncludeRectForParentItem includeRectForParentItem, HasAdjacentLinkAfter hasAdjacentLinkAfter = HasAdjacentLinkAfter::No)
{
    Vector<String> parts;
    bool streamlined = aggregator.useTextTreeOutput();
    WTF::switchOn(item.data,
        [&](const TextExtraction::ContainerType& containerType) {
            auto containerString = containerTypeString(containerType);

            if (aggregator.useHTMLOutput()) {
                String tagName;
                if (!item.nodeName.isEmpty())
                    tagName = item.nodeName.convertToASCIILowercase();

                if (!tagName.isEmpty()) {
                    auto attributes = partsForItem(item, aggregator, includeRectForParentItem);
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
                    parts.append(WTF::move(containerString));

                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));
            }
            aggregator.addResult(line, WTF::move(parts));
        },
        [&](const TextExtraction::TextItemData& textData) {
            auto hasLineThrough = item.hasLineThrough ? HasLineThroughStyle::Yes : HasLineThroughStyle::No;
            addPartsForText(textData, partsForItem(item, aggregator, includeRectForParentItem), item.frameIdentifier, enclosingNode, line, aggregator, hasLineThrough, { }, hasAdjacentLinkAfter);
        },
        [&](const TextExtraction::ContentEditableData& editableData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator, includeRectForParentItem);
                if (editableData.isPlainTextOnly)
                    attributes.append("contenteditable='plaintext-only'"_s);
                else
                    attributes.append("contenteditable"_s);
                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("contentEditable"_s);
                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));

                if (editableData.isFocused)
                    parts.append("focused"_s);

                if (editableData.isPlainTextOnly)
                    parts.append("plaintext"_s);
            }

            aggregator.addResult(line, WTF::move(parts));
        },
        [&](const TextExtraction::FormData& formData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator, includeRectForParentItem);
                if (!formData.autocomplete.isEmpty())
                    attributes.append(makeString("autocomplete='"_s, formData.autocomplete, '\''));

                if (!formData.name.isEmpty())
                    attributes.append(makeString("name='"_s, escapeString(formData.name), '\''));

                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("form"_s);
                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));
                if (!formData.autocomplete.isEmpty())
                    parts.append(makeString("autocomplete="_s, quoteValue(formData.autocomplete, streamlined)));

                if (!formData.name.isEmpty())
                    parts.append(makeString("name="_s, quoteValue(escapeString(formData.name), streamlined)));
            }
            aggregator.addResult(line, WTF::move(parts));
        },
        [&](const TextExtraction::TextFormControlData& controlData) {
            auto tagName = aggregator.useTagNameForTextFormControls() ? item.nodeName.convertToASCIILowercase() : String { "textFormControl"_s };

            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator, includeRectForParentItem);

                if (!controlData.controlType.isEmpty() && !equalIgnoringASCIICase(controlData.controlType, item.nodeName))
                    attributes.insert(0, makeString("type='"_s, controlData.controlType, '\''));

                if (!controlData.autocomplete.isEmpty())
                    attributes.append(makeString("autocomplete='"_s, controlData.autocomplete, '\''));

                if (!controlData.editable.label.isEmpty())
                    attributes.append(makeString("label='"_s, escapeString(controlData.editable.label), '\''));

                if (!controlData.editable.placeholder.isEmpty())
                    attributes.append(makeString("placeholder='"_s, escapeString(controlData.editable.placeholder), '\''));

                if (!controlData.pattern.isEmpty())
                    attributes.append(makeString("pattern='"_s, escapeString(controlData.pattern), '\''));

                if (!controlData.name.isEmpty())
                    attributes.append(makeString("name='"_s, escapeString(controlData.name), '\''));

                if (shouldIncludeFormControlValue(controlData, item))
                    attributes.append(makeString("value='"_s, escapeString(controlData.value), '\''));

                if (auto minLength = controlData.minLength)
                    attributes.append(makeString("minlength="_s, *minLength));

                if (auto maxLength = controlData.maxLength)
                    attributes.append(makeString("maxlength="_s, *maxLength));

                if (controlData.isRequired)
                    attributes.append("required"_s);

                if (attributes.isEmpty())
                    parts.append(makeString('<', tagName, '>'));
                else
                    parts.append(makeString('<', tagName, ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append(WTF::move(tagName));
                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));

                bool shouldIncludeType = [&] {
                    if (controlData.controlType.isEmpty())
                        return false;

                    if (equalIgnoringASCIICase(controlData.controlType, item.nodeName))
                        return false;

                    if (controlData.controlType == "text"_s)
                        return false;

                    if (controlData.editable.label.containsIgnoringASCIICase(controlData.controlType))
                        return false;

                    if (controlData.editable.placeholder.containsIgnoringASCIICase(controlData.controlType))
                        return false;

                    if (controlData.name.containsIgnoringASCIICase(controlData.controlType))
                        return false;

                    return true;
                }();

                if (shouldIncludeType)
                    parts.insert(1, controlData.controlType);

                if (!controlData.autocomplete.isEmpty())
                    parts.append(makeString("autocomplete="_s, quoteValue(controlData.autocomplete, streamlined)));

                if (controlData.isReadonly)
                    parts.append("readonly"_s);

                if (controlData.isDisabled)
                    parts.append("disabled"_s);

                if (controlData.isChecked)
                    parts.append("checked"_s);

                if (!controlData.editable.label.isEmpty()) {
                    bool skipLabel = false;
                    if (streamlined && !item.children.isEmpty()) {
                        if (auto* textData = std::get_if<TextExtraction::TextItemData>(&item.children[0].data)) {
                            auto trimmed = StringView(textData->content).trim(isASCIIWhitespace);
                            if (!trimmed.isEmpty() && trimmed.contains(controlData.editable.label))
                                skipLabel = true;
                        }
                    }
                    if (!skipLabel)
                        parts.append(makeString("label="_s, quoteValue(escapeString(controlData.editable.label), streamlined)));
                }

                if (!controlData.editable.placeholder.isEmpty())
                    parts.append(makeString("placeholder="_s, quoteValue(escapeString(controlData.editable.placeholder), streamlined)));

                if (!controlData.pattern.isEmpty())
                    parts.append(makeString("pattern="_s, quoteValue(escapeString(controlData.pattern), streamlined)));

                if (!controlData.name.isEmpty())
                    parts.append(makeString("name="_s, quoteValue(escapeString(controlData.name), streamlined)));

                if (shouldIncludeFormControlValue(controlData, item))
                    parts.append(makeString("value="_s, quoteValue(escapeString(controlData.value), streamlined)));

                if (auto minLength = controlData.minLength)
                    parts.append(makeString("minlength="_s, *minLength));

                if (auto maxLength = controlData.maxLength)
                    parts.append(makeString("maxlength="_s, *maxLength));

                if (controlData.isRequired)
                    parts.append("required"_s);

                if (controlData.editable.isSecure)
                    parts.append("secure"_s);

                if (controlData.editable.isFocused)
                    parts.append("focused"_s);

                if (controlData.isAutofilled)
                    parts.append("autofilled"_s);
            }

            aggregator.addResult(line, WTF::move(parts));
        },
        [&](const TextExtraction::LinkItemData& linkData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator, includeRectForParentItem);

                if (!linkData.completedURL.isEmpty() && aggregator.includeURLs())
                    attributes.append(makeString("href='"_s, aggregator.stringForURL(linkData), '\''));

                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("link"_s);
                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));

                if (!linkData.completedURL.isEmpty() && aggregator.includeURLs())
                    parts.append(makeString("url="_s, quoteValue(aggregator.stringForURL(linkData), streamlined)));
            }

            aggregator.addResult(line, WTF::move(parts));
        },
        [&](const TextExtraction::IFrameData& iframeData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator, includeRectForParentItem);

                if (!iframeData.origin.isEmpty())
                    attributes.append(makeString("src='"_s, iframeData.origin, '\''));

                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("iframe"_s);
                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));

                if (!iframeData.origin.isEmpty())
                    parts.append(makeString("origin="_s, quoteValue(iframeData.origin, streamlined)));
            }

            aggregator.addResult(line, WTF::move(parts));
        },
        [&](const TextExtraction::ScrollableItemData& scrollableData) {
            if (aggregator.useHTMLOutput()) {
                auto tagName = scrollableData.isRoot ? "body"_s : item.nodeName.convertToASCIILowercase();
                auto attributes = partsForItem(item, aggregator, includeRectForParentItem);
                if (attributes.isEmpty())
                    parts.append(makeString('<', tagName, '>'));
                else
                    parts.append(makeString('<', tagName, ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append(scrollableData.isRoot ? "root"_s : "scrollable"_s);
                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));
                if (scrollableData.hasOverflowItems) {
                    parts.append(makeString("scrollPosition=("_s, scrollableData.scrollPosition.x(), ',', scrollableData.scrollPosition.y(), ')'));
                    parts.append(makeString("contentSize=["_s, scrollableData.contentSize.width(), u'×', scrollableData.contentSize.height(), ']'));
                }
            }
            aggregator.addResult(line, WTF::move(parts));
        },
        [&](const TextExtraction::SelectData& selectData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator, includeRectForParentItem);
                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));

                aggregator.addResult(line, WTF::move(parts));

                if (aggregator.includeSelectOptions()) {
                    for (auto& option : selectData.options) {
                        auto optionLine = TextExtractionLine { aggregator.advanceToNextLine(), line.indentLevel + 1 };
                        if (option.isSelected)
                            aggregator.addResult(optionLine, { makeString("<option value='"_s, escapeStringForHTML(option.value), "' selected>"_s, escapeStringForHTML(option.label), "</option>"_s) });
                        else
                            aggregator.addResult(optionLine, { makeString("<option value='"_s, escapeStringForHTML(option.value), "'>"_s, escapeStringForHTML(option.label), "</option>"_s) });
                    }
                }

                aggregator.addResult({ aggregator.advanceToNextLine(), line.indentLevel }, { makeString("</select>"_s) });
            } else if (!aggregator.useMarkdownOutput()) {
                parts.append("select"_s);
                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));

                if (aggregator.includeSelectOptions()) {
                    for (auto& option : selectData.options) {
                        auto optionLine = TextExtractionLine { aggregator.advanceToNextLine(), line.indentLevel + 1 };
                        Vector<String> optionParts { "option"_s };
                        if (option.isSelected)
                            optionParts.append("selected"_s);
                        if (!option.value.isEmpty())
                            optionParts.append(makeString("value="_s, quoteValue(escapeString(option.value), streamlined)));
                        if (!option.label.isEmpty() && !equalIgnoringASCIICase(option.label, option.value))
                            optionParts.append(makeString('\'', escapeString(option.label), '\''));
                        aggregator.addResult(optionLine, WTF::move(optionParts));
                    }
                }

                if (selectData.isMultiple)
                    parts.append("multiple"_s);

                aggregator.addResult(line, WTF::move(parts));
            }
        },
        [&](const TextExtraction::ImageItemData& imageData) {
            if (aggregator.useHTMLOutput()) {
                auto attributes = partsForItem(item, aggregator, includeRectForParentItem);

                if (!imageData.completedSource.isEmpty() && aggregator.includeURLs())
                    attributes.append(makeString("src='"_s, aggregator.stringForURL(imageData), '\''));

                if (!imageData.altText.isEmpty())
                    attributes.append(makeString("alt='"_s, escapeString(imageData.altText), '\''));

                if (attributes.isEmpty())
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), '>'));
                else
                    parts.append(makeString('<', item.nodeName.convertToASCIILowercase(), ' ', makeStringByJoining(attributes, " "_s), '>'));
            } else if (aggregator.useMarkdownOutput()) {
                String imageSource;
                if (auto attributeFromClient = item.clientAttributes.get("src"_s); !attributeFromClient.isEmpty())
                    imageSource = WTF::move(attributeFromClient);
                else if (aggregator.includeURLs())
                    imageSource = aggregator.stringForURL(imageData);
                auto imageMarkdown = makeString("!["_s, escapeStringForMarkdown(imageData.altText), "]("_s, WTF::move(imageSource), ')');
                if (auto urlString = aggregator.currentURLString(); urlString && !urlString->isEmpty())
                    parts.append(makeString(WTF::move(imageMarkdown), " []("_s, WTF::move(*urlString), ')'));
                else
                    parts.append(WTF::move(imageMarkdown));
            } else {
                parts.append("image"_s);
                parts.appendVector(partsForItem(item, aggregator, includeRectForParentItem));

                if (!imageData.completedSource.isEmpty() && aggregator.includeURLs())
                    parts.append(makeString("src="_s, quoteValue(aggregator.stringForURL(imageData), streamlined)));

                if (!imageData.altText.isEmpty())
                    parts.append(makeString("alt="_s, quoteValue(escapeString(imageData.altText), streamlined)));
            }

            aggregator.addResult(line, WTF::move(parts));
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

static void addTextRepresentationRecursive(const TextExtraction::Item& item, std::optional<NodeIdentifier>&& enclosingNode, unsigned depth, TextExtractionAggregator& aggregator, HasAdjacentLinkAfter hasAdjacentLinkAfter = HasAdjacentLinkAfter::No)
{
    auto identifier = item.nodeIdentifier;
    if (!identifier)
        identifier = enclosingNode;

    if (std::holds_alternative<TextExtraction::TextItemData>(item.data))
        aggregator.collectTextMapping(std::get<TextExtraction::TextItemData>(item.data).content, item.frameIdentifier, identifier, item.nodeIdentifier ? ExtractedNodeInfo::IsInteractive::Yes : ExtractedNodeInfo::IsInteractive::No);

    if (aggregator.usePlainTextOutput()) {
        if (std::holds_alternative<TextExtraction::TextItemData>(item.data))
            addPartsForText(std::get<TextExtraction::TextItemData>(item.data), { }, item.frameIdentifier, identifier, { aggregator.advanceToNextLine(), depth }, aggregator, HasLineThroughStyle::No, { }, hasAdjacentLinkAfter);
        for (size_t i = 0; i < item.children.size(); ++i) {
            auto& child = item.children[i];
            bool childHasLinkAfter = i + 1 < item.children.size() && item.children[i + 1].hasData<TextExtraction::LinkItemData>();
            addTextRepresentationRecursive(child, std::optional { identifier }, depth + 1, aggregator, childHasLinkAfter ? HasAdjacentLinkAfter::Yes : HasAdjacentLinkAfter::No);
        }
        return;
    }

    bool isLink = false;
    if (auto link = item.dataAs<TextExtraction::LinkItemData>()) {
        String linkURLString;
        if (auto attributeFromClient = item.clientAttributes.get("href"_s); !attributeFromClient.isEmpty())
            linkURLString = WTF::move(attributeFromClient);
        else if (aggregator.includeURLs())
            linkURLString = aggregator.stringForURL(*link);
        aggregator.pushURLString(WTF::move(linkURLString));
        isLink = true;
    }

    auto containerType = item.dataAs<TextExtraction::ContainerType>();
    bool isSuperscript = containerType == TextExtraction::ContainerType::Superscript;
    if (isSuperscript)
        aggregator.pushSuperscript();

    bool isStrikethrough = containerType == TextExtraction::ContainerType::Strikethrough;
    if (isStrikethrough)
        aggregator.pushStrikethrough();

    auto popStateScope = makeScopeExit([isLink, isSuperscript, isStrikethrough, &aggregator] {
        if (isLink)
            aggregator.popURLString();

        if (isSuperscript)
            aggregator.popSuperscript();

        if (isStrikethrough)
            aggregator.popStrikethrough();
    });

    if (aggregator.useTextTreeOutput() && containerType == TextExtraction::ContainerType::ListItem && item.children.size() == 1 && !item.nodeIdentifier) {
        addTextRepresentationRecursive(item.children[0], std::optional { identifier }, depth, aggregator, hasAdjacentLinkAfter);
        return;
    }

    bool omitChildTextNode = [&] {
        if (aggregator.useMarkdownOutput())
            return false;

        if (item.children.size() != 1)
            return false;

        auto text = item.children[0].dataAs<TextExtraction::TextItemData>();
        if (!text)
            return false;

        return childTextNodeIsRedundant(aggregator, item, text->content.trim(isASCIIWhitespace));
    }();

    auto includeRectForParentItem = omitChildTextNode ? IncludeRectForParentItem::Yes : IncludeRectForParentItem::No;

    TextExtractionLine line { aggregator.advanceToNextLine(), depth, item.enclosingBlockNumber, aggregator.superscriptLevel() };
    addPartsForItem(item, std::optional { identifier }, line, aggregator, includeRectForParentItem, hasAdjacentLinkAfter);

    auto closingTagName = [&] -> String {
        if (!aggregator.useHTMLOutput())
            return { };

        if (auto scrollableData = item.dataAs<TextExtraction::ScrollableItemData>(); scrollableData && scrollableData->isRoot)
            return "body"_s;

        return item.nodeName.convertToASCIILowercase();
    }();

    if (item.children.size() == 1) {
        if (auto text = item.children[0].dataAs<TextExtraction::TextItemData>()) {
            aggregator.collectTextMapping(text->content.trim(isASCIIWhitespace), item.frameIdentifier, identifier, item.nodeIdentifier ? ExtractedNodeInfo::IsInteractive::Yes : ExtractedNodeInfo::IsInteractive::No);

            if (omitChildTextNode)
                return;

            if (aggregator.useHTMLOutput()) {
                auto hasLineThrough = item.children[0].hasLineThrough ? HasLineThroughStyle::Yes : HasLineThroughStyle::No;
                addPartsForText(*text, partsForItem(item.children[0], aggregator, includeRectForParentItem), item.frameIdentifier, identifier, line, aggregator, hasLineThrough, makeString("</"_s, closingTagName, '>'));
                return;
            }

            // In the case of a single text child, we append that text to the same line.
            addPartsForItem(item.children[0], WTF::move(identifier), line, aggregator, includeRectForParentItem);
            return;
        }
    }

    for (size_t i = 0; i < item.children.size(); ++i) {
        auto& child = item.children[i];
        bool childHasLinkAfter = i + 1 < item.children.size() && item.children[i + 1].hasData<TextExtraction::LinkItemData>();
        addTextRepresentationRecursive(child, std::optional { identifier }, depth + 1, aggregator, childHasLinkAfter ? HasAdjacentLinkAfter::Yes : HasAdjacentLinkAfter::No);
    }

    if (aggregator.useHTMLOutput() && !item.children.isEmpty())
        aggregator.addResult({ aggregator.advanceToNextLine(), depth }, { makeString("</"_s, closingTagName, '>') });
}

void convertToText(TextExtraction::Item&& item, TextExtractionOptions&& options, CompletionHandler<void(TextExtractionResult&&)>&& completion)
{
    Ref aggregator = TextExtractionAggregator::create(WTF::move(options), WTF::move(completion));

    if (aggregator->useJSONOutput()) {
        populateJSONForItem(protect(aggregator->rootJSONObject()), item, { }, aggregator);
        return;
    }

    addTextRepresentationRecursive(item, { }, 0, aggregator);
}

} // namespace WebKit
