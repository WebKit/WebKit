/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContentSearchUtilities.h"

#include "RegularExpression.h"
#include "Yarr.h"
#include "YarrFlags.h"
#include "YarrInterpreter.h"
#include <wtf/BumpPointerAllocator.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextPosition.h>

using namespace JSC::Yarr;

namespace Inspector {
namespace ContentSearchUtilities {

static const char regexSpecialCharacters[] = "[](){}+-*.,?\\^$|";

static String createSearchRegexSource(const String& text)
{
    StringBuilder result;

    for (unsigned i = 0; i < text.length(); i++) {
        UChar character = text[i];
        if (isASCII(character) && strchr(regexSpecialCharacters, character))
            result.append('\\');
        result.append(character);
    }

    return result.toString();
}

static inline size_t sizetExtractor(const size_t* value)
{
    return *value;
}

TextPosition textPositionFromOffset(size_t offset, const Vector<size_t>& lineEndings)
{
    const size_t* foundNextStart = approximateBinarySearch<size_t, size_t>(lineEndings, lineEndings.size(), offset, sizetExtractor);
    size_t lineIndex = foundNextStart - &lineEndings.at(0);
    if (offset >= *foundNextStart)
        ++lineIndex;
    size_t lineStartOffset = lineIndex > 0 ? lineEndings.at(lineIndex - 1) : 0;
    size_t column = offset - lineStartOffset;
    return TextPosition(OrdinalNumber::fromZeroBasedInt(lineIndex), OrdinalNumber::fromZeroBasedInt(column));
}

static Vector<std::pair<size_t, String>> getRegularExpressionMatchesByLines(const RegularExpression& regex, const String& text)
{
    Vector<std::pair<size_t, String>> result;
    if (text.isEmpty())
        return result;

    auto endings = lineEndings(text);
    size_t size = endings.size();
    size_t start = 0;

    for (size_t lineNumber = 0; lineNumber < size; ++lineNumber) {
        size_t nextStart = endings[lineNumber];
        String line = text.substring(start, nextStart - start);

        int matchLength;
        if (regex.match(line, 0, &matchLength) != -1)
            result.append(std::pair<size_t, String>(lineNumber, line));

        start = nextStart;
    }

    return result;
}

Vector<size_t> lineEndings(const String& text)
{
    Vector<size_t> result;

    size_t start = 0;
    while (start < text.length()) {
        size_t nextStart = text.find('\n', start);
        if (nextStart == notFound || nextStart == (text.length() - 1)) {
            result.append(text.length());
            break;
        }

        nextStart += 1;
        result.append(nextStart);
        start = nextStart;
    }

    result.append(text.length());

    return result;
}

static Ref<Protocol::GenericTypes::SearchMatch> buildObjectForSearchMatch(size_t lineNumber, const String& lineContent)
{
    return Protocol::GenericTypes::SearchMatch::create()
        .setLineNumber(lineNumber)
        .setLineContent(lineContent)
        .release();
}

RegularExpression createSearchRegex(const String& query, bool caseSensitive, bool isRegex)
{
    return RegularExpression { isRegex ? query : createSearchRegexSource(query), caseSensitive ? TextCaseSensitive : TextCaseInsensitive };
}

int countRegularExpressionMatches(const RegularExpression& regex, const String& content)
{
    if (content.isEmpty())
        return 0;

    int result = 0;
    int position;
    unsigned start = 0;
    int matchLength;
    while ((position = regex.match(content, start, &matchLength)) != -1) {
        if (start >= content.length())
            break;
        if (matchLength > 0)
            ++result;
        start = position + 1;
    }
    return result;
}

Ref<JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>> searchInTextByLines(const String& text, const String& query, const bool caseSensitive, const bool isRegex)
{
    auto result = JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>::create();
    auto regex = ContentSearchUtilities::createSearchRegex(query, caseSensitive, isRegex);
    for (const auto& match : getRegularExpressionMatchesByLines(regex, text))
        result->addItem(buildObjectForSearchMatch(match.first, match.second));
    return result;
}

static String findMagicComment(const String& content, const String& patternString)
{
    if (content.isEmpty())
        return String();

    JSC::Yarr::ErrorCode error { JSC::Yarr::ErrorCode::NoError };
    YarrPattern pattern(patternString, JSC::Yarr::Flags::Multiline, error);
    ASSERT(!hasError(error));
    BumpPointerAllocator regexAllocator;
    JSC::Yarr::ErrorCode ignoredErrorCode = JSC::Yarr::ErrorCode::NoError;
    auto bytecodePattern = byteCompile(pattern, &regexAllocator, ignoredErrorCode);
    RELEASE_ASSERT(bytecodePattern);

    ASSERT(pattern.m_numSubpatterns == 1);
    std::array<unsigned, 4> matches;
    unsigned result = interpret(bytecodePattern.get(), content, 0, matches.data());
    if (result == offsetNoMatch)
        return String();

    ASSERT(matches[2] > 0 && matches[3] > 0);
    return content.substring(matches[2], matches[3] - matches[2]);
}

String findStylesheetSourceMapURL(const String& content)
{
    // "/*# <name>=<value> */" and deprecated "/*@"
    return findMagicComment(content, "/\\*[#@][\040\t]sourceMappingURL=[\040\t]*([^\\s\'\"]*)[\040\t]*\\*/"_s);
}

} // namespace ContentSearchUtilities
} // namespace Inspector
