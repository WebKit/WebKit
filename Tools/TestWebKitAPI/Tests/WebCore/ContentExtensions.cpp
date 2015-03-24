/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "PlatformUtilities.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/ContentExtensionCompiler.h>
#include <WebCore/ContentExtensionsBackend.h>
#include <WebCore/NFA.h>
#include <WebCore/ResourceLoadInfo.h>
#include <WebCore/URL.h>
#include <WebCore/URLFilterParser.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>

namespace WebCore {
namespace ContentExtensions {
inline std::ostream& operator<<(std::ostream& os, const ActionType& action)
{
    switch (action) {
    case ActionType::BlockLoad:
        return os << "ContentFilterAction::BlockLoad";
    case ActionType::BlockCookies:
        return os << "ContentFilterAction::BlockCookies";
    case ActionType::CSSDisplayNoneSelector:
        return os << "ContentFilterAction::CSSDisplayNone";
    case ActionType::CSSDisplayNoneStyleSheet:
        return os << "ContentFilterAction::CSSDisplayNoneStyleSheet";
    case ActionType::IgnorePreviousRules:
        return os << "ContentFilterAction::IgnorePreviousRules";
    case ActionType::InvalidAction:
        return os << "ContentFilterAction::InvalidAction";
    }
}
}
}

using namespace WebCore;

namespace TestWebKitAPI {

class ContentExtensionTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
        JSC::initializeThreading();
        RunLoop::initializeMainRunLoop();
    }
};

class InMemoryCompiledContentExtension : public ContentExtensions::CompiledContentExtension {
public:
    static RefPtr<InMemoryCompiledContentExtension> create(ContentExtensions::CompiledContentExtensionData&& data)
    {
        return adoptRef(new InMemoryCompiledContentExtension(WTF::move(data)));
    }

    virtual ~InMemoryCompiledContentExtension()
    {
    }

    virtual const ContentExtensions::DFABytecode* bytecode() const override { return m_data.bytecode.data(); }
    virtual unsigned bytecodeLength() const override { return m_data.bytecode.size(); }
    virtual const ContentExtensions::SerializedActionByte* actions() const override { return m_data.actions.data(); }
    virtual unsigned actionsLength() const override { return m_data.actions.size(); }

private:
    InMemoryCompiledContentExtension(ContentExtensions::CompiledContentExtensionData&& data)
        : m_data(WTF::move(data))
    {
    }

    ContentExtensions::CompiledContentExtensionData m_data;
};

void static testRequest(ContentExtensions::ContentExtensionsBackend contentExtensionsBackend, const ResourceLoadInfo& resourceLoadInfo, Vector<ContentExtensions::ActionType> expectedActions)
{
    auto actions = contentExtensionsBackend.actionsForResourceLoad(resourceLoadInfo);
    // The last action is applying the compiled stylesheet.
    EXPECT_EQ(expectedActions.size(), actions.size() ? actions.size() - 1 : 0);
    if (expectedActions.size() != (actions.size() ? actions.size() - 1 : 0))
        return;

    for (unsigned i = 0; i < expectedActions.size(); ++i)
        EXPECT_EQ(expectedActions[i], actions[i].type());
}

ResourceLoadInfo mainDocumentRequest(const char* url, ResourceType resourceType = ResourceType::Document)
{
    return { URL(URL(), url), URL(URL(), url), resourceType };
}

const char* basicFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]";

TEST_F(ContentExtensionTest, Basic)
{
    auto extensionData = ContentExtensions::compileRuleList(basicFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter", extension);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
}

TEST_F(ContentExtensionTest, RangeBasic)
{
    const char* rangeBasicFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"w[0-9]c\", \"url-filter-is-case-sensitive\":true}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"[A-H][a-z]cko\", \"url-filter-is-case-sensitive\":true}}]";
    auto extensionData = ContentExtensions::compileRuleList(rangeBasicFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("PatternNestedGroupsFilter", extension);

    testRequest(backend, mainDocumentRequest("http://w3c.org"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("w2c://whatwg.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/w0c"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/wac"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/wAc"), { });

    // Note: URL parsing and canonicalization lowercase the scheme and hostname.
    testRequest(backend, mainDocumentRequest("Aacko://webkit.org"), { });
    testRequest(backend, mainDocumentRequest("aacko://webkit.org"), { });
    testRequest(backend, mainDocumentRequest("http://gCcko.org/"), { });
    testRequest(backend, mainDocumentRequest("http://gccko.org/"), { });

    testRequest(backend, mainDocumentRequest("http://webkit.org/Gecko"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://webkit.org/gecko"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/GEcko"), { });
}

TEST_F(ContentExtensionTest, RangeExclusionGeneratingUniversalTransition)
{
    // Transition of the type ([^X]X) effictively transition on every input.
    const char* rangeExclusionGeneratingUniversalTransitionFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[^a]+afoobar\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(rangeExclusionGeneratingUniversalTransitionFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("PatternNestedGroupsFilter", extension);

    testRequest(backend, mainDocumentRequest("http://w3c.org"), { });

    testRequest(backend, mainDocumentRequest("http://w3c.org/foobafoobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://w3c.org/foobarfoobar"), { });
    testRequest(backend, mainDocumentRequest("http://w3c.org/FOOBAFOOBAR"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://w3c.org/FOOBARFOOBAR"), { });

    // The character before the "a" prefix cannot be another "a".
    testRequest(backend, mainDocumentRequest("http://w3c.org/aafoobar"), { });
    testRequest(backend, mainDocumentRequest("http://w3c.org/Aafoobar"), { });
    testRequest(backend, mainDocumentRequest("http://w3c.org/aAfoobar"), { });
    testRequest(backend, mainDocumentRequest("http://w3c.org/AAfoobar"), { });
}

TEST_F(ContentExtensionTest, PatternStartingWithGroup)
{
    const char* patternsStartingWithGroupFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(http://whatwg\\\\.org/)?webkit\134\134.org\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(patternsStartingWithGroupFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("PatternNestedGroupsFilter", extension);

    testRequest(backend, mainDocumentRequest("http://whatwg.org/webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://whatwg.org/webkit.org"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("http://whatwg.org/"), { });
    testRequest(backend, mainDocumentRequest("http://whatwg.org"), { });
}

TEST_F(ContentExtensionTest, PatternNestedGroups)
{
    const char* patternNestedGroupsFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/(foo(bar)*)+\"}}]";

    auto extensionData = ContentExtensions::compileRuleList(patternNestedGroupsFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("PatternNestedGroupsFilter", extension);

    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarbar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foofoobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foob"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foor"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bar"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/fobar"), { });
}

TEST_F(ContentExtensionTest, MatchPastEndOfString)
{
    const char* matchPastEndOfStringFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".+\"}}]";

    auto extensionData = ContentExtensions::compileRuleList(matchPastEndOfStringFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("MatchPastEndOfString", extension);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarbar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foofoobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foob"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foor"), { ContentExtensions::ActionType::BlockLoad });
}

const char* startOfLineAssertionFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^foobar\"}}]";

TEST_F(ContentExtensionTest, StartOfLineAssertion)
{
    auto extensionData = ContentExtensions::compileRuleList(startOfLineAssertionFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("StartOfLineAssertion", extension);

    testRequest(backend, mainDocumentRequest("foobar://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("foobars:///foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("foobarfoobar:///foobarfoobarfoobar"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoo"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarf"), { });
    testRequest(backend, mainDocumentRequest("http://foobar.org/"), { });
    testRequest(backend, mainDocumentRequest("http://foobar.org/"), { });
}

TEST_F(ContentExtensionTest, EndOfLineAssertion)
{
    const char* endOfLineAssertionFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foobar$\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(endOfLineAssertionFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("EndOfLineAssertion", extension);

    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("file:///foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("file:///foobarfoobarfoobar"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoo"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarf"), { });
}

TEST_F(ContentExtensionTest, EndOfLineAssertionWithInvertedCharacterSet)
{
    const char* endOfLineAssertionWithInvertedCharacterSetFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[^y]$\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(endOfLineAssertionWithInvertedCharacterSetFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("EndOfLineAssertion", extension);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/a"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/Ya"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/yFoobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/y"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/Y"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobary"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarY"), { });
}

TEST_F(ContentExtensionTest, PrefixInfixSuffixExactMatch)
{
    const char* prefixInfixSuffixExactMatchFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"infix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^prefix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"suffix$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://exact\\\\.org/$\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(prefixInfixSuffixExactMatchFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("PrefixInfixSuffixExactMatch", extension);

    testRequest(backend, mainDocumentRequest("infix://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://infix.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/infix"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("prefix://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://prefix.org/"), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/prefix"), { });

    testRequest(backend, mainDocumentRequest("https://webkit.org/suffix"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://suffix.org/"), { });
    testRequest(backend, mainDocumentRequest("suffix://webkit.org/"), { });

    testRequest(backend, mainDocumentRequest("http://exact.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://exact.org/oops"), { });
}

TEST_F(ContentExtensionTest, DuplicatedMatchAllTermsInVariousFormat)
{
    const char* duplicatedMatchAllTermsInVariousFormatFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*.*(.)*(.*)(.+)*(.?)*infix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"pre(.?)+(.+)?post\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(duplicatedMatchAllTermsInVariousFormatFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("DuplicatedMatchAllTermsInVariousFormat", extension);

    testRequest(backend, mainDocumentRequest("infix://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://infix.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/infix"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("pre://webkit.org/post"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://prepost.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://pre.org/posttail"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://pre.pre/posttail"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://pre.org/posttailpost"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("https://post.org/pre"), { });
    testRequest(backend, mainDocumentRequest("https://pre.org/pre"), { });
    testRequest(backend, mainDocumentRequest("https://post.org/post"), { });
}

TEST_F(ContentExtensionTest, TermsKnownToMatchAnything)
{
    const char* termsKnownToMatchAnythingFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre1.*post1$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre2(.*)post2$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre3(.*)?post3$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre4(.*)+post4$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre5(.*)*post5$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre6(.)*post6$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre7(.+)*post7$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre8(.?)*post8$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre9(.+)?post9$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre0(.?)+post0$\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(termsKnownToMatchAnythingFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("TermsKnownToMatchAnything", extension);

    testRequest(backend, mainDocumentRequest("pre1://webkit.org/post1"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre2://webkit.org/post2"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre3://webkit.org/post3"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre4://webkit.org/post4"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre5://webkit.org/post5"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre6://webkit.org/post6"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre7://webkit.org/post7"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre8://webkit.org/post8"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre9://webkit.org/post9"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("pre0://webkit.org/post0"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("pre1://webkit.org/post2"), { });
    testRequest(backend, mainDocumentRequest("pre2://webkit.org/post3"), { });
    testRequest(backend, mainDocumentRequest("pre3://webkit.org/post4"), { });
    testRequest(backend, mainDocumentRequest("pre4://webkit.org/post5"), { });
    testRequest(backend, mainDocumentRequest("pre5://webkit.org/post6"), { });
    testRequest(backend, mainDocumentRequest("pre6://webkit.org/post7"), { });
    testRequest(backend, mainDocumentRequest("pre7://webkit.org/post8"), { });
    testRequest(backend, mainDocumentRequest("pre8://webkit.org/post9"), { });
    testRequest(backend, mainDocumentRequest("pre9://webkit.org/post0"), { });
    testRequest(backend, mainDocumentRequest("pre0://webkit.org/post1"), { });

    testRequest(backend, mainDocumentRequest("pre0://webkit.org/post1"), { });
    testRequest(backend, mainDocumentRequest("pre1://webkit.org/post2"), { });
    testRequest(backend, mainDocumentRequest("pre2://webkit.org/post3"), { });
    testRequest(backend, mainDocumentRequest("pre3://webkit.org/post4"), { });
    testRequest(backend, mainDocumentRequest("pre4://webkit.org/post5"), { });
    testRequest(backend, mainDocumentRequest("pre5://webkit.org/post6"), { });
    testRequest(backend, mainDocumentRequest("pre6://webkit.org/post7"), { });
    testRequest(backend, mainDocumentRequest("pre7://webkit.org/post8"), { });
    testRequest(backend, mainDocumentRequest("pre8://webkit.org/post9"), { });
    testRequest(backend, mainDocumentRequest("pre9://webkit.org/post0"), { });
}

TEST_F(ContentExtensionTest, TrailingDotStar)
{
    const char* trailingDotStarFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foo.*$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bar(.*)$\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(trailingDotStarFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("TrailingDotStar", extension);

    testRequest(backend, mainDocumentRequest("https://webkit.org/"), { });

    testRequest(backend, mainDocumentRequest("foo://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://foo.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.foo/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/foo"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("bar://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://bar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.bar/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bar"), { ContentExtensions::ActionType::BlockLoad });
}

TEST_F(ContentExtensionTest, TrailingTermsCarryingNoData)
{
    const char* trailingTermsCarryingNoDataFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foob?a?r?\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bazo(ok)?a?$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"cats*$\"}}]";
    auto extensionData = ContentExtensions::compileRuleList(trailingTermsCarryingNoDataFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("TrailingTermsCarryingNoData", extension);

    testRequest(backend, mainDocumentRequest("https://webkit.org/"), { });

    // Anything is fine after foo.
    testRequest(backend, mainDocumentRequest("https://webkit.org/foo"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/foob"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/fooc"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/fooba"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/foobar-stuff"), { ContentExtensions::ActionType::BlockLoad });

    // Bazooka has to be at the tail without any character not defined by the filter.
    testRequest(backend, mainDocumentRequest("https://webkit.org/baz"), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazo"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazoa"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazob"), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazoo"), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazook"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazookb"), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazooka"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazookaa"), { });

    // The pattern must finish with cat, with any number of 's' following it, but no other character.
    testRequest(backend, mainDocumentRequest("https://cat.org/"), { });
    testRequest(backend, mainDocumentRequest("https://cats.org/"), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/cat"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/cats"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/catss"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/catsss"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/catso"), { });
}

TEST_F(ContentExtensionTest, LoadType)
{
    const char* loadTypeFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":[\"third-party\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"whatwg.org\",\"load-type\":[\"first-party\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"alwaysblock.pdf\"}}]";

    auto extensionData = ContentExtensions::compileRuleList(loadTypeFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));
        
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("LoadTypeFilter", extension);
        
    testRequest(backend, mainDocumentRequest("http://webkit.org"), { });
    testRequest(backend, {URL(URL(), "http://webkit.org"), URL(URL(), "http://not_webkit.org"), ResourceType::Document}, { ContentExtensions::ActionType::BlockLoad });
        
    testRequest(backend, mainDocumentRequest("http://whatwg.org"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, {URL(URL(), "http://whatwg.org"), URL(URL(), "http://not_whatwg.org"), ResourceType::Document}, { });
    
    testRequest(backend, mainDocumentRequest("http://foobar.org/alwaysblock.pdf"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, {URL(URL(), "http://foobar.org/alwaysblock.pdf"), URL(URL(), "http://not_foobar.org/alwaysblock.pdf"), ResourceType::Document}, { ContentExtensions::ActionType::BlockLoad });
}

TEST_F(ContentExtensionTest, ResourceType)
{
    const char* resourceTypeFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_all_types.org\",\"resource-type\":[\"document\",\"image\",\"style-sheet\",\"script\",\"font\",\"raw\",\"svg-document\",\"media\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_only_images\",\"resource-type\":[\"image\"]}}]";

    auto extensionData = ContentExtensions::compileRuleList(resourceTypeFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));
        
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("ResourceTypeFilter", extension);

    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Document), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Image), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::StyleSheet), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Script), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Font), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Raw), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::SVGDocument), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Media), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_only_images.org", ResourceType::Image), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_only_images.org", ResourceType::Document), { });
}

static void testPatternStatus(String pattern, ContentExtensions::URLFilterParser::ParseStatus status)
{
    ContentExtensions::NFA nfa;
    ContentExtensions::URLFilterParser parser(nfa);
    EXPECT_EQ(status, parser.addPattern(pattern, false, 0));
}
    
TEST_F(ContentExtensionTest, ParsingFailures)
{
    testPatternStatus("a*b?.*.?[a-z]?[a-z]*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("a*b?.*.?[a-z]?[a-z]+", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("a*b?.*.?[a-z]?[a-z]", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus(".*?a", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus(".*a", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    
    testPatternStatus("(?!)", ContentExtensions::URLFilterParser::ParseStatus::Group);
    testPatternStatus("(?=)", ContentExtensions::URLFilterParser::ParseStatus::Group);
    testPatternStatus("(?!a)", ContentExtensions::URLFilterParser::ParseStatus::Group);
    testPatternStatus("(?=a)", ContentExtensions::URLFilterParser::ParseStatus::Group);
    testPatternStatus("(regex)", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("(regex", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("((regex)", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("(?:regex)", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("(?:regex", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("[^.]+", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    
    testPatternStatus("a++", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("[a]++", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("+", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    
    testPatternStatus("[", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("[a}", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    
    // FIXME: Look into why these do not cause YARR parsing errors.  They probably should.
    testPatternStatus("a]", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{[a]", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{0", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{0,", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{0,1", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("a{0,1", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("a{a,b}", ContentExtensions::URLFilterParser::ParseStatus::Ok);

    const char nonASCII[2] = {-1, '\0'};
    testPatternStatus(nonASCII, ContentExtensions::URLFilterParser::ParseStatus::NonASCII);
    testPatternStatus("\\xff", ContentExtensions::URLFilterParser::ParseStatus::NonASCII);
    
    testPatternStatus("\\x\\r\\n", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("\\b", ContentExtensions::URLFilterParser::ParseStatus::WordBoundary);
    testPatternStatus("[\\d]", ContentExtensions::URLFilterParser::ParseStatus::AtomCharacter);
    testPatternStatus("\\d\\D\\w\\s\\v\\h\\i\\c", ContentExtensions::URLFilterParser::ParseStatus::UnsupportedCharacterClass);
    
    testPatternStatus("this|that", ContentExtensions::URLFilterParser::ParseStatus::Disjunction);
    testPatternStatus("a{0,1}b", ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("a{0,2}b", ContentExtensions::URLFilterParser::ParseStatus::InvalidQuantifier);
    testPatternStatus("", ContentExtensions::URLFilterParser::ParseStatus::EmptyPattern);
    testPatternStatus("$$", ContentExtensions::URLFilterParser::ParseStatus::MisplacedEndOfLine);
    testPatternStatus("a^", ContentExtensions::URLFilterParser::ParseStatus::MisplacedStartOfLine);
    testPatternStatus("(^)", ContentExtensions::URLFilterParser::ParseStatus::MisplacedStartOfLine);
    
    testPatternStatus("(a)\\1", ContentExtensions::URLFilterParser::ParseStatus::Ok); // This should be BackReference, right?
}

TEST_F(ContentExtensionTest, PatternMatchingTheEmptyString)
{
    // Simple atoms.
    testPatternStatus(".*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("a*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus(".?", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("a?", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);

    // Character sets.
    testPatternStatus("[a-z]*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("[a-z]?", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);

    // Groups.
    testPatternStatus("(foobar)*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(foobar)?", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.*)", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(a*)", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.?)", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(a?)", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("([a-z]*)", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("([a-z]?)", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);

    testPatternStatus("(.)*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.+)*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.?)*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.*)*", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.+)?", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.?)+", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);

    // Nested groups.
    testPatternStatus("((foo)?((.)*)(bar)*)", ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
}

} // namespace TestWebKitAPI
