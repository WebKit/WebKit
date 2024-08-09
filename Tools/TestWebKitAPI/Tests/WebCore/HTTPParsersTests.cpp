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
}

} // namespace TestWebKitAPI
