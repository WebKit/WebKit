/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include <WebCore/HTTPParsers.h>
#include <WebCore/RFC8941.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(HTTPParsers, ParseCrossOriginResourcePolicyHeader)
{
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(""_s) == CrossOriginResourcePolicy::None);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(" "_s) == CrossOriginResourcePolicy::None);

    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("same-origin"_s) == CrossOriginResourcePolicy::SameOrigin);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("Same-Origin"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("SAME-ORIGIN"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(" same-orIGIN "_s) == CrossOriginResourcePolicy::Invalid);

    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("same-site"_s) == CrossOriginResourcePolicy::SameSite);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("Same-Site"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("SAME-SITE"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(" same-site "_s) == CrossOriginResourcePolicy::SameSite);

    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("SameOrigin"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("zameorigin"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("samesite"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("same site"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(StringView::fromLatin1("same–site")) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("SAMESITE"_s) == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(StringView::fromLatin1("")) == CrossOriginResourcePolicy::Invalid);
}

#if USE(GLIB)
TEST(HTTPParsers, ValidateUserAgentValues)
{
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari WebKit"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari/10.0"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari WebKit/163"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari/10.0 WebKit"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari/10.0 WebKit/163"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari/10.0 WebKit/163 (Mozilla; like Gecko)"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari (comment (nested comment))"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari () (<- Empty comment)"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari (left paren \\( as quoted pair)"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("!#$%&'*+-.^_`|~ (non-alphanumeric token characters)"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("0123456789 (numeric token characters)"_s));
    EXPECT_TRUE(isValidUserAgentHeaderValue("a (single character token)"_s));

    EXPECT_FALSE(isValidUserAgentHeaderValue(" "_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue(" Safari (leading whitespace)"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari (trailing whitespace) "_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("\nSafari (leading newline)"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari (trailing newline)\n"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari/ (no version token after slash)"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari (unterminated comment"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari unopened commanent)"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("\x1B (contains control character)"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari/\n10.0 (embeded newline)"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("WPE\\ WebKit (quoted pair in token)"_s));
    EXPECT_FALSE(isValidUserAgentHeaderValue("/123 (missing product token)"_s));
}
#endif

TEST(RFC8941, ParseItemStructuredFieldValue)
{
    // Simple Token BareItem.
    auto result = RFC8941::parseItemStructuredFieldValue("unsafe-none"_s);
    EXPECT_TRUE(!!result);
    auto* itemString = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!itemString);
    EXPECT_STREQ("unsafe-none", itemString->string().utf8().data());

    // Invalid Token BareItem.
    result = RFC8941::parseItemStructuredFieldValue("same-site unsafe-allow-outgoing"_s);
    EXPECT_TRUE(!result);

    // String parameter value.
    result = RFC8941::parseItemStructuredFieldValue("same-origin-allow-popups; report-to=\"http://example.com\""_s);
    EXPECT_TRUE(!!result);
    itemString = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!itemString);
    EXPECT_STREQ("same-origin-allow-popups", itemString->string().utf8().data());
    EXPECT_EQ(result->second.map().size(), 1U);
    auto* parameterValueString = result->second.getIf<String>("report-to"_s);
    EXPECT_TRUE(!!parameterValueString);
    EXPECT_STREQ("http://example.com", parameterValueString->utf8().data());

    // Token parameter value.
    result = RFC8941::parseItemStructuredFieldValue("same-origin-allow-popups; report-to=*"_s);
    EXPECT_TRUE(!!result);
    itemString = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!itemString);
    EXPECT_STREQ("same-origin-allow-popups", itemString->string().utf8().data());
    EXPECT_EQ(result->second.map().size(), 1U);
    auto* parameterValueToken = result->second.getIf<RFC8941::Token>("report-to"_s);
    EXPECT_TRUE(!!parameterValueToken);
    EXPECT_STREQ("*", parameterValueToken->string().utf8().data());

    // True boolean parameter value.
    result = RFC8941::parseItemStructuredFieldValue("same-origin-allow-popups; should-report=?1"_s);
    EXPECT_TRUE(!!result);
    itemString = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!itemString);
    EXPECT_STREQ("same-origin-allow-popups", itemString->string().utf8().data());
    EXPECT_EQ(result->second.map().size(), 1U);
    auto* parameterValueBoolean = result->second.getIf<bool>("should-report"_s);
    EXPECT_TRUE(!!parameterValueToken);
    EXPECT_TRUE(*parameterValueBoolean);

    // False boolean parameter value.
    result = RFC8941::parseItemStructuredFieldValue("same-origin-allow-popups; should-report=?0"_s);
    EXPECT_TRUE(!!result);
    itemString = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!itemString);
    EXPECT_STREQ("same-origin-allow-popups", itemString->string().utf8().data());
    EXPECT_EQ(result->second.map().size(), 1U);
    parameterValueBoolean = result->second.getIf<bool>("should-report"_s);
    EXPECT_TRUE(!!parameterValueToken);
    EXPECT_FALSE(*parameterValueBoolean);

    // Invalid boolean parameter value.
    result = RFC8941::parseItemStructuredFieldValue("same-origin-allow-popups; should-report=?3"_s);
    EXPECT_TRUE(!result);

    // Multiple parameters.
    result = RFC8941::parseItemStructuredFieldValue("same-origin-allow-popups; should-report=?1; report-to=\"http://example.com\""_s);
    EXPECT_TRUE(!!result);
    itemString = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!itemString);
    EXPECT_STREQ("same-origin-allow-popups", itemString->string().utf8().data());
    EXPECT_EQ(result->second.map().size(), 2U);
    parameterValueBoolean = result->second.getIf<bool>("should-report"_s);
    EXPECT_TRUE(!!parameterValueToken);
    EXPECT_TRUE(*parameterValueBoolean);
    parameterValueString = result->second.getIf<String>("report-to"_s);
    EXPECT_TRUE(!!parameterValueString);
    EXPECT_STREQ("http://example.com", parameterValueString->utf8().data());

    // Integer BareItem tests.
    result = RFC8941::parseItemStructuredFieldValue("42"_s);
    EXPECT_TRUE(!!result);
    auto* intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 42);

    result = RFC8941::parseItemStructuredFieldValue("0"_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 0);

    result = RFC8941::parseItemStructuredFieldValue("-42"_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, -42);

    result = RFC8941::parseItemStructuredFieldValue("-0"_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 0);

    result = RFC8941::parseItemStructuredFieldValue("042"_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 42);

    result = RFC8941::parseItemStructuredFieldValue("-042"_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, -42);

    result = RFC8941::parseItemStructuredFieldValue("999999999999999"_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 999999999999999LL);

    result = RFC8941::parseItemStructuredFieldValue("-999999999999999"_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, -999999999999999LL);

    result = RFC8941::parseItemStructuredFieldValue("000000000000000"_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 0);

    // Invalid Integer BareItem.
    result = RFC8941::parseItemStructuredFieldValue("0000000000000000"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("9999999999999999"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("-9999999999999999"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("-"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("-."_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("--0"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("- 42"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("2,3"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("-a23"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("4-2"_s);
    EXPECT_FALSE(!!result);

    // Decimal BareItem tests.
    result = RFC8941::parseItemStructuredFieldValue("1.5"_s);
    EXPECT_TRUE(!!result);
    auto* decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, 1.5);

    result = RFC8941::parseItemStructuredFieldValue("-1.5"_s);
    EXPECT_TRUE(!!result);
    decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, -1.5);

    result = RFC8941::parseItemStructuredFieldValue("0.0"_s);
    EXPECT_TRUE(!!result);
    decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, 0.0);

    result = RFC8941::parseItemStructuredFieldValue("1.123"_s);
    EXPECT_TRUE(!!result);
    decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, 1.123);

    result = RFC8941::parseItemStructuredFieldValue("-1.123"_s);
    EXPECT_TRUE(!!result);
    decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, -1.123);

    result = RFC8941::parseItemStructuredFieldValue("1.000"_s);
    EXPECT_TRUE(!!result);
    decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, 1.0);

    result = RFC8941::parseItemStructuredFieldValue("123456789012.123"_s);
    EXPECT_TRUE(!!result);
    decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, 123456789012.123);

    result = RFC8941::parseItemStructuredFieldValue("000000000100.123"_s);
    EXPECT_TRUE(!!result);
    decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, 100.123);

    result = RFC8941::parseItemStructuredFieldValue("-999999999999.999"_s);
    EXPECT_TRUE(!!result);
    decimalValue = std::get_if<double>(&result->first);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, -999999999999.999);

    // Invalid Decimal BareItem.
    result = RFC8941::parseItemStructuredFieldValue("0000000000000.1"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("1.0000"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("-1.1234"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("1."_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("-1."_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("1.1234"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("1234567890123.0"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("-1234567890123.0"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("1. 23"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("1 .23"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("1..4"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("1.5.4"_s);
    EXPECT_FALSE(!!result);

    // Byte sequence BareItem tests.
    result = RFC8941::parseItemStructuredFieldValue("::"_s);
    EXPECT_TRUE(!!result);
    auto* bytes = std::get_if<Vector<uint8_t>>(&result->first);
    EXPECT_TRUE(!!bytes);
    EXPECT_TRUE(bytes->isEmpty());

    result = RFC8941::parseItemStructuredFieldValue(":aGVsbG8=:"_s);
    EXPECT_TRUE(!!result);
    bytes = std::get_if<Vector<uint8_t>>(&result->first);
    EXPECT_TRUE(!!bytes);
    EXPECT_EQ(bytes->size(), 5u);
    EXPECT_EQ((*bytes)[0], 'h');
    EXPECT_EQ((*bytes)[1], 'e');
    EXPECT_EQ((*bytes)[2], 'l');
    EXPECT_EQ((*bytes)[3], 'l');
    EXPECT_EQ((*bytes)[4], 'o');

    result = RFC8941::parseItemStructuredFieldValue(":cHJldGVuZCB0aGlzIGlzIGJpbmFyeSBjb250ZW50Lg==:"_s);
    EXPECT_TRUE(!!result);
    bytes = std::get_if<Vector<uint8_t>>(&result->first);
    EXPECT_TRUE(!!bytes);
    EXPECT_EQ(bytes->size(), 31u);

    result = RFC8941::parseItemStructuredFieldValue(":/+Ah:"_s);
    EXPECT_TRUE(!!result);
    bytes = std::get_if<Vector<uint8_t>>(&result->first);
    EXPECT_TRUE(!!bytes);
    EXPECT_EQ(bytes->size(), 3u);

    // Invalid Byte sequence BareItem.
    result = RFC8941::parseItemStructuredFieldValue("aGVsbG8=:"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue(":aGVsbG8="_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue("aGVsbG8="_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue(":=aGVsbG8=:"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue(":a=GVsbG8=:"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue(":aGVsbG8.:"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue(":aGVsb G8=:"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue(":aGVsbG!8=:"_s);
    EXPECT_FALSE(!!result);

    result = RFC8941::parseItemStructuredFieldValue(":_-Ah:"_s);
    EXPECT_FALSE(!!result);

    // Integer parameter tests.
    result = RFC8941::parseItemStructuredFieldValue("token;count=42"_s);
    EXPECT_TRUE(!!result);
    auto* token = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!token);
    EXPECT_STREQ("token", token->string().utf8().data());
    EXPECT_EQ(result->second.map().size(), 1U);
    auto* paramIntValue = result->second.getIf<int64_t>("count"_s);
    EXPECT_TRUE(!!paramIntValue);
    EXPECT_EQ(*paramIntValue, 42);

    // Negative integer parameter tests.
    result = RFC8941::parseItemStructuredFieldValue("token;offset=-10"_s);
    EXPECT_TRUE(!!result);
    token = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!token);
    paramIntValue = result->second.getIf<int64_t>("offset"_s);
    EXPECT_TRUE(!!paramIntValue);
    EXPECT_EQ(*paramIntValue, -10);

    // Decimal parameter tests.
    result = RFC8941::parseItemStructuredFieldValue("token;ratio=1.5"_s);
    EXPECT_TRUE(!!result);
    token = std::get_if<RFC8941::Token>(&result->first);
    EXPECT_TRUE(!!token);
    auto* paramDecimal = result->second.getIf<double>("ratio"_s);
    EXPECT_TRUE(!!paramDecimal);
    EXPECT_DOUBLE_EQ(*paramDecimal, 1.5);

    // String with multiple parameters (token and integer values).
    result = RFC8941::parseItemStructuredFieldValue("\"b\"; a=c; c=2"_s);
    EXPECT_TRUE(!!result);
    auto* stringValue = std::get_if<String>(&result->first);
    EXPECT_TRUE(!!stringValue);
    EXPECT_STREQ("b", stringValue->utf8().data());
    EXPECT_EQ(result->second.map().size(), 2U);
    auto* paramTokenValue = result->second.getIf<RFC8941::Token>("a"_s);
    EXPECT_TRUE(!!paramTokenValue);
    EXPECT_STREQ("c", paramTokenValue->string().utf8().data());
    auto* paramIntValue2 = result->second.getIf<int64_t>("c"_s);
    EXPECT_TRUE(!!paramIntValue2);
    EXPECT_EQ(*paramIntValue2, 2);

    // Empty item.
    result = RFC8941::parseItemStructuredFieldValue(""_s);
    EXPECT_FALSE(!!result);

    // Leading and trailing whitespace
    result = RFC8941::parseItemStructuredFieldValue("     1  "_s);
    EXPECT_TRUE(!!result);
    intValue = std::get_if<int64_t>(&result->first);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 1);
}

TEST(RFC8941, ParseDictionaryStructuredFieldValue)
{
    auto result = RFC8941::parseDictionaryStructuredFieldValue("default=\"https://www.example.com/reporting/report.py?reportID=46ecac28-6d27-4763-a692-bcc588054716\""_s);
    EXPECT_TRUE(!!result);
    EXPECT_EQ(result->size(), 1U);
    EXPECT_TRUE(result->contains("default"_s));
    auto valueAndParameters = result->get("default"_s);
    auto* bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
    EXPECT_TRUE(!!bareItem);
    auto* endpointURLString = std::get_if<String>(bareItem);
    EXPECT_TRUE(!!endpointURLString);
    EXPECT_STREQ("https://www.example.com/reporting/report.py?reportID=46ecac28-6d27-4763-a692-bcc588054716", endpointURLString->utf8().data());

    result = RFC8941::parseDictionaryStructuredFieldValue("default=\"https://www.example.com/reporting/report.py?reportID=46ecac28-6d27-4763-a692-bcc588054716\", report-only=\"https://www.example.com/reporting/report.py?reportID=46ecac28-6d27-4763-a692-bcc588054717\""_s);
    EXPECT_TRUE(!!result);
    EXPECT_EQ(result->size(), 2U);
    EXPECT_TRUE(result->contains("default"_s));
    valueAndParameters = result->get("default"_s);
    bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
    EXPECT_TRUE(!!bareItem);
    endpointURLString = std::get_if<String>(bareItem);
    EXPECT_TRUE(!!endpointURLString);
    EXPECT_STREQ("https://www.example.com/reporting/report.py?reportID=46ecac28-6d27-4763-a692-bcc588054716", endpointURLString->utf8().data());
    valueAndParameters = result->get("report-only"_s);
    bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
    EXPECT_TRUE(!!bareItem);
    endpointURLString = std::get_if<String>(bareItem);
    EXPECT_TRUE(!!endpointURLString);
    EXPECT_STREQ("https://www.example.com/reporting/report.py?reportID=46ecac28-6d27-4763-a692-bcc588054717", endpointURLString->utf8().data());

    result = RFC8941::parseDictionaryStructuredFieldValue("geolocation=(self \"https://example.com\"), camera=()"_s);
    EXPECT_TRUE(!!result);
    EXPECT_EQ(result->size(), 2U);
    EXPECT_TRUE(result->contains("geolocation"_s));
    valueAndParameters = result->get("geolocation"_s);
    auto* valueList = std::get_if<RFC8941::InnerList>(&valueAndParameters.first);
    EXPECT_TRUE(!!valueList);
    EXPECT_EQ(valueList->size(), 2U);
    EXPECT_TRUE(valueList->at(0).second.map().isEmpty());
    auto* token = std::get_if<RFC8941::Token>(&valueList->at(0).first);
    EXPECT_TRUE(!!token);
    EXPECT_STREQ("self", token->string().utf8().data());
    EXPECT_TRUE(valueList->at(1).second.map().isEmpty());
    auto* urlString = std::get_if<String>(&valueList->at(1).first);
    EXPECT_TRUE(!!urlString);
    EXPECT_STREQ("https://example.com", urlString->utf8().data());
    EXPECT_TRUE(result->contains("camera"_s));
    valueAndParameters = result->get("camera"_s);
    valueList = std::get_if<RFC8941::InnerList>(&valueAndParameters.first);
    EXPECT_TRUE(!!valueList);
    EXPECT_TRUE(valueList->isEmpty());

    result = RFC8941::parseDictionaryStructuredFieldValue("count=42"_s);
    EXPECT_TRUE(!!result);
    EXPECT_EQ(result->size(), 1U);
    EXPECT_TRUE(result->contains("count"_s));
    valueAndParameters = result->get("count"_s);
    bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
    EXPECT_TRUE(!!bareItem);
    auto* intValue = std::get_if<int64_t>(bareItem);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 42);

    result = RFC8941::parseDictionaryStructuredFieldValue("ratio=1.5"_s);
    EXPECT_TRUE(!!result);
    EXPECT_TRUE(result->contains("ratio"_s));
    valueAndParameters = result->get("ratio"_s);
    bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
    EXPECT_TRUE(!!bareItem);
    auto* decimalValue = std::get_if<double>(bareItem);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, 1.5);

    result = RFC8941::parseDictionaryStructuredFieldValue("count=42, ratio=1.5, offset=-10"_s);
    EXPECT_TRUE(!!result);
    EXPECT_EQ(result->size(), 3U);
    EXPECT_TRUE(result->contains("count"_s));
    valueAndParameters = result->get("count"_s);
    bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
    EXPECT_TRUE(!!bareItem);
    intValue = std::get_if<int64_t>(bareItem);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, 42);
    EXPECT_TRUE(result->contains("ratio"_s));
    valueAndParameters = result->get("ratio"_s);
    bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
    EXPECT_TRUE(!!bareItem);
    decimalValue = std::get_if<double>(bareItem);
    EXPECT_TRUE(!!decimalValue);
    EXPECT_DOUBLE_EQ(*decimalValue, 1.5);
    EXPECT_TRUE(result->contains("offset"_s));
    valueAndParameters = result->get("offset"_s);
    bareItem = std::get_if<RFC8941::BareItem>(&valueAndParameters.first);
    EXPECT_TRUE(!!bareItem);
    intValue = std::get_if<int64_t>(bareItem);
    EXPECT_TRUE(!!intValue);
    EXPECT_EQ(*intValue, -10);
}

} // namespace TestWebKitAPI
