/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <WebCore/DocumentFragment.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/HTMLBodyElement.h>
#include <WebCore/HTMLDivElement.h>
#include <WebCore/HTMLDocument.h>
#include <WebCore/HTMLDocumentParserFastPath.h>
#include <WebCore/HTMLHtmlElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLParserIdioms.h>
#include <WebCore/NodeInlines.h>
#include <WebCore/ParserContentPolicy.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/ScriptWrappableInlines.h>
#include <WebCore/Settings.h>
#include <WebCore/Text.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace TestWebKitAPI {

static int testParseHTMLInteger(StringView input)
{
    auto optionalResult = parseHTMLInteger(input);
    EXPECT_TRUE(!!optionalResult);
    return optionalResult.value_or(0);
}

static bool parseHTMLIntegerFails(StringView input)
{
    return !parseHTMLInteger(input);
}

TEST(WebCoreHTMLParserIdioms, parseHTMLInteger)
{
    EXPECT_EQ(0, testParseHTMLInteger("0"_s));
    EXPECT_EQ(0, testParseHTMLInteger("-0"_s));
    EXPECT_EQ(0, testParseHTMLInteger("+0"_s));
    EXPECT_EQ(123, testParseHTMLInteger("123"_s));
    EXPECT_EQ(123, testParseHTMLInteger("+123"_s));
    EXPECT_EQ(-123, testParseHTMLInteger("-123"_s));
    EXPECT_EQ(123, testParseHTMLInteger("  123"_s));
    EXPECT_EQ(123, testParseHTMLInteger("123   "_s));
    EXPECT_EQ(123, testParseHTMLInteger("   123   "_s));
    EXPECT_EQ(123, testParseHTMLInteger("123abc"_s));
    EXPECT_EQ(-123, testParseHTMLInteger("-123abc"_s));
    EXPECT_EQ(123, testParseHTMLInteger("  +123"_s));
    EXPECT_EQ(-123, testParseHTMLInteger("  -123"_s));
    EXPECT_EQ(12, testParseHTMLInteger("   12 3"_s));
    EXPECT_EQ(1, testParseHTMLInteger("1.0"_s));
    EXPECT_EQ(1, testParseHTMLInteger("1."_s));
    EXPECT_EQ(1, testParseHTMLInteger("1e1"_s));

    // All HTML whitespaces.
    EXPECT_EQ(123, testParseHTMLInteger(" \t\r\n\f123"_s));

    // Boundaries.
    EXPECT_EQ(-2147483648, testParseHTMLInteger("-2147483648"_s));
    EXPECT_EQ(2147483647, testParseHTMLInteger("2147483647"_s));

    // Failure cases.
    EXPECT_TRUE(parseHTMLIntegerFails("-2147483649"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("2147483648"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("111111111111111111"_s));
    EXPECT_TRUE(parseHTMLIntegerFails(""_s));
    EXPECT_TRUE(parseHTMLIntegerFails(" "_s));
    EXPECT_TRUE(parseHTMLIntegerFails("   "_s));
    EXPECT_TRUE(parseHTMLIntegerFails("+"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("+ 123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("-"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("- 123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("a"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("-a"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("+-123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("-+123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("++123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("--123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("\v123"_s)); // '\v' is an ASCII space but not an HTML whitespace.
    EXPECT_TRUE(parseHTMLIntegerFails("a123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("+a123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("-a123"_s));
    EXPECT_TRUE(parseHTMLIntegerFails(".1"_s));
    EXPECT_TRUE(parseHTMLIntegerFails("infinity"_s));
}

static unsigned testParseHTMLNonNegativeInteger(StringView input)
{
    auto optionalResult = parseHTMLNonNegativeInteger(input);
    EXPECT_TRUE(!!optionalResult);
    return optionalResult.value_or(0);
}

static bool parseHTMLNonNegativeIntegerFails(StringView input)
{
    return !parseHTMLNonNegativeInteger(input);
}

TEST(WebCoreHTMLParserIdioms, parseHTMLNonNegativeInteger)
{
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("123"_s));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("+123"_s));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("  123"_s));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("123   "_s));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("   123   "_s));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("123abc"_s));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("  +123"_s));
    EXPECT_EQ(12u, testParseHTMLNonNegativeInteger("   12 3"_s));
    EXPECT_EQ(1u, testParseHTMLNonNegativeInteger("1.0"_s));
    EXPECT_EQ(1u, testParseHTMLNonNegativeInteger("1."_s));
    EXPECT_EQ(1u, testParseHTMLNonNegativeInteger("1e1"_s));

    // All HTML whitespaces.
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger(" \t\r\n\f123"_s));

    // Boundaries.
    EXPECT_EQ(0u, testParseHTMLNonNegativeInteger("+0"_s));
    EXPECT_EQ(0u, testParseHTMLNonNegativeInteger("0"_s));
    EXPECT_EQ(0u, testParseHTMLNonNegativeInteger("-0"_s));
    EXPECT_EQ(2147483647u, testParseHTMLNonNegativeInteger("2147483647"_s));

    // Failure cases.
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-1"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("2147483648"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("2147483649"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("111111111111111111"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("  -123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-123abc"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails(""_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails(" "_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("   "_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("+"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("+ 123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("- 123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("a"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-a"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("+-123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-+123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("++123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("--123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("\v123"_s)); // '\v' is an ASCII space but not an HTML whitespace.
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("a123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("+a123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-a123"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails(".1"_s));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("infinity"_s));
}

TEST(WebCoreHTMLParserIdioms, parseHTMLDimensionsList)
{
    auto empty = parseHTMLDimensionsList(""_s);
    EXPECT_TRUE(empty.isEmpty());

    auto whitespaceOnly = parseHTMLDimensionsList("   "_s);
    EXPECT_EQ(1u, whitespaceOnly.size());
    EXPECT_FLOAT_EQ(0, whitespaceOnly[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, whitespaceOnly[0].unit);

    auto singleIntegralAbsolute = parseHTMLDimensionsList("15"_s);
    EXPECT_EQ(1u, singleIntegralAbsolute.size());
    EXPECT_FLOAT_EQ(15.0, singleIntegralAbsolute[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Absolute, singleIntegralAbsolute[0].unit);

    auto singleFloatingAbsolute = parseHTMLDimensionsList("15.05"_s);
    EXPECT_EQ(1u, singleFloatingAbsolute.size());
    EXPECT_FLOAT_EQ(15.05, singleFloatingAbsolute[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Absolute, singleFloatingAbsolute[0].unit);

    auto singleIntegralRelative = parseHTMLDimensionsList("15*"_s);
    EXPECT_EQ(1u, singleIntegralRelative.size());
    EXPECT_FLOAT_EQ(15.0, singleIntegralRelative[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, singleIntegralRelative[0].unit);

    auto singleFloatingRelative = parseHTMLDimensionsList("15.05*"_s);
    EXPECT_EQ(1u, singleFloatingRelative.size());
    EXPECT_FLOAT_EQ(15.05, singleFloatingRelative[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, singleFloatingRelative[0].unit);

    auto singleIntegralPercentage = parseHTMLDimensionsList("15%"_s);
    EXPECT_EQ(1u, singleIntegralPercentage.size());
    EXPECT_FLOAT_EQ(15.0, singleIntegralPercentage[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, singleIntegralPercentage[0].unit);

    auto singleFloatingPercentage = parseHTMLDimensionsList("15.05%"_s);
    EXPECT_EQ(1u, singleFloatingPercentage.size());
    EXPECT_FLOAT_EQ(15.05, singleFloatingPercentage[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, singleFloatingPercentage[0].unit);

    auto whitespaceBeforeIntegralRelative = parseHTMLDimensionsList("15.05  *"_s);
    EXPECT_EQ(1u, whitespaceBeforeIntegralRelative.size());
    EXPECT_FLOAT_EQ(15.05, whitespaceBeforeIntegralRelative[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, whitespaceBeforeIntegralRelative[0].unit);

    auto whitespaceBeforeFloatingRelative = parseHTMLDimensionsList("15.05  *"_s);
    EXPECT_EQ(1u, whitespaceBeforeFloatingRelative.size());
    EXPECT_FLOAT_EQ(15.05, whitespaceBeforeFloatingRelative[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, whitespaceBeforeFloatingRelative[0].unit);

    auto whitespaceBeforeIntegralPercentage = parseHTMLDimensionsList("15  %"_s);
    EXPECT_EQ(1u, whitespaceBeforeIntegralPercentage.size());
    EXPECT_FLOAT_EQ(15.0, whitespaceBeforeIntegralPercentage[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, whitespaceBeforeIntegralPercentage[0].unit);

    auto whitespaceBeforeFloatingPercentage = parseHTMLDimensionsList("15.05  %"_s);
    EXPECT_EQ(1u, whitespaceBeforeFloatingPercentage.size());
    EXPECT_FLOAT_EQ(15.05, whitespaceBeforeFloatingPercentage[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, whitespaceBeforeFloatingPercentage[0].unit);

    auto whitespaceInFractionalAbsolute = parseHTMLDimensionsList("15. 0   5  "_s);
    EXPECT_EQ(1u, whitespaceInFractionalAbsolute.size());
    EXPECT_FLOAT_EQ(15.05, whitespaceInFractionalAbsolute[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Absolute, whitespaceInFractionalAbsolute[0].unit);

    auto whitespaceInFractionalRelative = parseHTMLDimensionsList("15. 0   5  *"_s);
    EXPECT_EQ(1u, whitespaceInFractionalRelative.size());
    EXPECT_FLOAT_EQ(15.05, whitespaceInFractionalRelative[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, whitespaceInFractionalRelative[0].unit);

    auto whitespaceInFractionalPercentage = parseHTMLDimensionsList("15. 0   5  %"_s);
    EXPECT_EQ(1u, whitespaceInFractionalPercentage.size());
    EXPECT_FLOAT_EQ(15.05, whitespaceInFractionalPercentage[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, whitespaceInFractionalPercentage[0].unit);

    auto leadingWhitespace = parseHTMLDimensionsList("  15.05 %"_s);
    EXPECT_EQ(1u, leadingWhitespace.size());
    EXPECT_FLOAT_EQ(15.05, leadingWhitespace[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, leadingWhitespace[0].unit);

    auto trailingWhitespace = parseHTMLDimensionsList("15.05 %  "_s);
    EXPECT_EQ(1u, trailingWhitespace.size());
    EXPECT_FLOAT_EQ(15.05, trailingWhitespace[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, trailingWhitespace[0].unit);

    auto largeIntegralPart = parseHTMLDimensionsList("8589934592.05%"_s);
    EXPECT_EQ(1u, largeIntegralPart.size());
    EXPECT_FLOAT_EQ(1.0, largeIntegralPart[0].number); // Falls back to "1*"
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, largeIntegralPart[0].unit);

    auto largeFractionalPart = parseHTMLDimensionsList("1.8589934592%"_s);
    EXPECT_EQ(1u, largeFractionalPart.size());
    EXPECT_FLOAT_EQ(1.0, largeFractionalPart[0].number); // Falls back to "1*"
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, largeFractionalPart[0].unit);

    auto twoValuesNoWhitespace = parseHTMLDimensionsList("15.05%,10*"_s);
    EXPECT_EQ(2u, twoValuesNoWhitespace.size());
    EXPECT_FLOAT_EQ(15.05, twoValuesNoWhitespace[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, twoValuesNoWhitespace[0].unit);
    EXPECT_FLOAT_EQ(10.0, twoValuesNoWhitespace[1].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, twoValuesNoWhitespace[1].unit);

    auto twoValuesWhitespace = parseHTMLDimensionsList("   15.05%  ,  10*  "_s);
    EXPECT_EQ(2u, twoValuesWhitespace.size());
    EXPECT_FLOAT_EQ(15.05, twoValuesWhitespace[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, twoValuesWhitespace[0].unit);
    EXPECT_FLOAT_EQ(10.0, twoValuesWhitespace[1].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, twoValuesWhitespace[1].unit);

    auto lastCharacterComma = parseHTMLDimensionsList("15.05%, 10* ,"_s);
    EXPECT_EQ(2u, lastCharacterComma.size());
    EXPECT_FLOAT_EQ(15.05, lastCharacterComma[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, lastCharacterComma[0].unit);
    EXPECT_FLOAT_EQ(10.0, lastCharacterComma[1].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, lastCharacterComma[1].unit);

    auto trailingGarbage = parseHTMLDimensionsList("15.05 % adfa, 10* +]"_s);
    EXPECT_EQ(2u, trailingGarbage.size());
    EXPECT_FLOAT_EQ(15.05, trailingGarbage[0].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Percentage, trailingGarbage[0].unit);
    EXPECT_FLOAT_EQ(10.0, trailingGarbage[1].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, trailingGarbage[1].unit);

    auto whitespaceBeforeDot = parseHTMLDimensionsList("15 .05 %, 10*"_s);
    EXPECT_EQ(2u, whitespaceBeforeDot.size());
    EXPECT_FLOAT_EQ(15.0, whitespaceBeforeDot[0].number); // Everything from after 15 to the comma is skipped.
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Absolute, whitespaceBeforeDot[0].unit);
    EXPECT_FLOAT_EQ(10.0, whitespaceBeforeDot[1].number);
    EXPECT_EQ(HTMLDimensionsListValue::Unit::Relative, whitespaceBeforeDot[1].unit);
}

TEST(WebCoreHTMLParser, HTMLInputElementCheckedState)
{
    ProcessWarming::initializeNames();

    auto settings = Settings::create(nullptr);
    auto document = HTMLDocument::create(nullptr, settings.get(), aboutBlankURL());
    auto documentElement = HTMLHtmlElement::create(document);
    document->appendChild(documentElement);
    auto body = HTMLBodyElement::create(document);
    documentElement->appendChild(body);

    auto div1 = HTMLDivElement::create(document);
    auto div2 = HTMLDivElement::create(document);
    document->body()->appendChild(div1);
    document->body()->appendChild(div2);

    // Set the state for new controls, which triggers a different code path in
    // HTMLInputElement::parseAttribute.
    div1->setInnerHTML("<select form='ff'></select>"_s);
    auto documentState = document->formController().formElementsState(document.get());
    document->formController().setStateForNewFormElements(documentState);
    EXPECT_TRUE(!document->formController().formElementsState(document.get()).isEmpty());
    div2->setInnerHTML("<input checked='true'>"_s);
    auto inputElement = downcast<HTMLInputElement>(div2->firstChild());
    ASSERT_TRUE(inputElement);
    EXPECT_TRUE(inputElement->checked());
}

TEST(WebCoreHTMLParser, FastPathComplexHTMLEntityParsing)
{
    ProcessWarming::initializeNames();

    auto settings = Settings::create(nullptr);
    auto document = HTMLDocument::create(nullptr, settings.get(), aboutBlankURL());
    auto documentElement = HTMLHtmlElement::create(document);
    document->appendChild(documentElement);
    auto body = HTMLBodyElement::create(document);
    documentElement->appendChild(body);

    auto div = HTMLDivElement::create(document);
    document->body()->appendChild(div);

    auto testFastParser = [&](const String& input) -> String {
        auto fragment = DocumentFragment::create(document);
        bool result = tryFastParsingHTMLFragment(input, document, fragment, div, { ParserContentPolicy::AllowScriptingContent });
        EXPECT_TRUE(result);
        auto textChild = dynamicDowncast<Text>(fragment->firstChild());
        EXPECT_TRUE(textChild);
        return textChild ? textChild->data() : String();
    };

    EXPECT_STREQ(testFastParser("Price: 12&cent; only"_s).utf8().data(), String::fromUTF8("Price: 12¢ only").utf8().data());
    EXPECT_STREQ(testFastParser("Genius Nicer Dicer Plus | 18&nbsp&hellip;"_s).utf8().data(), String::fromUTF8("Genius Nicer Dicer Plus | 18 …").utf8().data());
    EXPECT_STREQ(testFastParser("&nbsp&a"_s).utf8().data(), String::fromUTF8(" &a").utf8().data());
    EXPECT_STREQ(testFastParser("&nbsp&"_s).utf8().data(), String::fromUTF8(" &").utf8().data());
    EXPECT_STREQ(testFastParser("&nbsp-"_s).utf8().data(), String::fromUTF8(" -").utf8().data());
    EXPECT_STREQ(testFastParser("food & water"_s).utf8().data(), String("food & water"_s).utf8().data());
}

TEST(WebCoreHTMLParser, FastPathHandlesLi)
{
    ProcessWarming::initializeNames();

    auto settings = Settings::create(nullptr);
    auto document = HTMLDocument::create(nullptr, settings.get(), aboutBlankURL());
    auto documentElement = HTMLHtmlElement::create(document);
    document->appendChild(documentElement);
    auto body = HTMLBodyElement::create(document);
    documentElement->appendChild(body);

    auto div = HTMLDivElement::create(document);
    document->body()->appendChild(div);

    auto fragment = DocumentFragment::create(document);
    bool result = tryFastParsingHTMLFragment("<div><li></li></div>"_s, document, fragment, div, { ParserContentPolicy::AllowScriptingContent });
    EXPECT_TRUE(result);
    EXPECT_STREQ("DIV", fragment->firstChild()->nodeName().utf8().data());
    EXPECT_STREQ("LI", fragment->firstChild()->firstChild()->nodeName().utf8().data());
}

TEST(WebCoreHTMLParser, FastPathFailsWithNestedLi)
{
    ProcessWarming::initializeNames();

    auto settings = Settings::create(nullptr);
    auto document = HTMLDocument::create(nullptr, settings.get(), aboutBlankURL());
    auto documentElement = HTMLHtmlElement::create(document);
    document->appendChild(documentElement);
    auto body = HTMLBodyElement::create(document);
    documentElement->appendChild(body);

    auto div = HTMLDivElement::create(document);
    document->body()->appendChild(div);

    auto fragment = DocumentFragment::create(document);
    bool result = tryFastParsingHTMLFragment("<li><li></li></li>"_s, document, fragment, div, { ParserContentPolicy::AllowScriptingContent });
    EXPECT_FALSE(result);
}

} // namespace TestWebKitAPI
