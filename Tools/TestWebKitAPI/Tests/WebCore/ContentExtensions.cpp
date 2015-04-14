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
#include <WebCore/CombinedURLFilters.h>
#include <WebCore/ContentExtensionCompiler.h>
#include <WebCore/ContentExtensionError.h>
#include <WebCore/ContentExtensionsBackend.h>
#include <WebCore/NFA.h>
#include <WebCore/ResourceLoadInfo.h>
#include <WebCore/URL.h>
#include <WebCore/URLFilterParser.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

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

class InMemoryContentExtensionCompilationClient final : public WebCore::ContentExtensions::ContentExtensionCompilationClient {
public:
    InMemoryContentExtensionCompilationClient(WebCore::ContentExtensions::CompiledContentExtensionData& data)
        : m_data(data)
    {
    }

    virtual void writeBytecode(Vector<WebCore::ContentExtensions::DFABytecode>&& bytecode) override
    {
        m_data.bytecode = WTF::move(bytecode);
    }
    
    virtual void writeActions(Vector<WebCore::ContentExtensions::SerializedActionByte>&& actions) override
    {
        m_data.actions = WTF::move(actions);
    }

private:
    WebCore::ContentExtensions::CompiledContentExtensionData& m_data;
};

class InMemoryCompiledContentExtension : public ContentExtensions::CompiledContentExtension {
public:
    static RefPtr<InMemoryCompiledContentExtension> createFromFilter(const String& filter)
    {
        WebCore::ContentExtensions::CompiledContentExtensionData extensionData;
        InMemoryContentExtensionCompilationClient client(extensionData);
        auto compilerError = ContentExtensions::compileRuleList(client, filter);
        if (compilerError)
            return nullptr;

        return InMemoryCompiledContentExtension::create(WTF::move(extensionData));
    }

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

void static testRequest(ContentExtensions::ContentExtensionsBackend contentExtensionsBackend, const ResourceLoadInfo& resourceLoadInfo, Vector<ContentExtensions::ActionType> expectedActions, bool ignorePreviousRules = false)
{
    auto actions = contentExtensionsBackend.actionsForResourceLoad(resourceLoadInfo);
    unsigned expectedSize = actions.size();
    if (!ignorePreviousRules)
        expectedSize--; // The last action is applying the compiled stylesheet.
    
    EXPECT_EQ(expectedActions.size(), expectedSize);
    if (expectedActions.size() != expectedSize)
        return;

    for (unsigned i = 0; i < expectedActions.size(); ++i)
        EXPECT_EQ(expectedActions[i], actions[i].type());
    if (!ignorePreviousRules)
        EXPECT_EQ(actions[actions.size() - 1].type(), ContentExtensions::ActionType::CSSDisplayNoneStyleSheet);
}

ResourceLoadInfo mainDocumentRequest(const char* url, ResourceType resourceType = ResourceType::Document)
{
    return { URL(URL(), url), URL(URL(), url), resourceType };
}

ContentExtensions::ContentExtensionsBackend makeBackend(const char* json)
{
    auto extension = InMemoryCompiledContentExtension::createFromFilter(json);
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter", extension);
    return backend;
}

TEST_F(ContentExtensionTest, Basic)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
}

TEST_F(ContentExtensionTest, RangeBasic)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"w[0-9]c\", \"url-filter-is-case-sensitive\":true}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"[A-H][a-z]cko\", \"url-filter-is-case-sensitive\":true}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[^a]+afoobar\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(http://whatwg\\\\.org/)?webkit\134\134.org\"}}]");

    testRequest(backend, mainDocumentRequest("http://whatwg.org/webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://whatwg.org/webkit.org"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("http://whatwg.org/"), { });
    testRequest(backend, mainDocumentRequest("http://whatwg.org"), { });
}

TEST_F(ContentExtensionTest, PatternNestedGroups)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/(foo(bar)*)+\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".+\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarbar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foofoobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foob"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foor"), { ContentExtensions::ActionType::BlockLoad });
}

TEST_F(ContentExtensionTest, StartOfLineAssertion)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^foobar\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foobar$\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("file:///foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("file:///foobarfoobarfoobar"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoo"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarf"), { });
}

TEST_F(ContentExtensionTest, EndOfLineAssertionWithInvertedCharacterSet)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[^y]$\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"infix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^prefix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"suffix$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://exact\\\\.org/$\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*.*(.)*(.*)(.+)*(.?)*infix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"pre(.?)+(.+)?post\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre1.*post1$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre2(.*)post2$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre3(.*)?post3$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre4(.*)+post4$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre5(.*)*post5$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre6(.)*post6$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre7(.+)*post7$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre8(.?)*post8$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre9(.+)?post9$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre0(.?)+post0$\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foo.*$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bar(.*)$\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foob?a?r?\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bazo(ok)?a?$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"cats*$\"}}]");

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
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":[\"third-party\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"whatwg.org\",\"load-type\":[\"first-party\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"alwaysblock.pdf\"}}]");
    
    testRequest(backend, mainDocumentRequest("http://webkit.org"), { });
    testRequest(backend, {URL(URL(), "http://webkit.org"), URL(URL(), "http://not_webkit.org"), ResourceType::Document}, { ContentExtensions::ActionType::BlockLoad });
        
    testRequest(backend, mainDocumentRequest("http://whatwg.org"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, {URL(URL(), "http://whatwg.org"), URL(URL(), "http://not_whatwg.org"), ResourceType::Document}, { });
    
    testRequest(backend, mainDocumentRequest("http://foobar.org/alwaysblock.pdf"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, {URL(URL(), "http://foobar.org/alwaysblock.pdf"), URL(URL(), "http://not_foobar.org/alwaysblock.pdf"), ResourceType::Document}, { ContentExtensions::ActionType::BlockLoad });
}

TEST_F(ContentExtensionTest, ResourceType)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_all_types.org\",\"resource-type\":[\"document\",\"image\",\"style-sheet\",\"script\",\"font\",\"raw\",\"svg-document\",\"media\",\"popup\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_only_images\",\"resource-type\":[\"image\"]}}]");

    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Document), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Image), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::StyleSheet), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Script), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Font), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Raw), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::SVGDocument), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Media), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org", ResourceType::Popup), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_only_images.org", ResourceType::Image), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://block_only_images.org", ResourceType::Document), { });
}

TEST_F(ContentExtensionTest, ResourceOrLoadTypeMatchingEverything)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*\",\"resource-type\":[\"image\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\".*\",\"load-type\":[\"third-party\"]}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\".*\",\"load-type\":[\"first-party\"]}}]");
    
    testRequest(backend, mainDocumentRequest("http://webkit.org"), { }, true);
    testRequest(backend, {URL(URL(), "http://webkit.org"), URL(URL(), "http://not_webkit.org"), ResourceType::Document}, { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, {URL(URL(), "http://webkit.org"), URL(URL(), "http://not_webkit.org"), ResourceType::Image}, { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad });
}
    
TEST_F(ContentExtensionTest, MultiDFA)
{
    // Make an NFA with about 1400 nodes.
    StringBuilder ruleList;
    ruleList.append('[');
    for (char c1 = 'A'; c1 <= 'Z'; ++c1) {
        for (char c2 = 'A'; c2 <= 'C'; ++c2) {
            for (char c3 = 'A'; c3 <= 'C'; ++c3) {
                if (c1 != 'A' || c2 != 'A' || c3 != 'A')
                    ruleList.append(',');
                ruleList.append("{\"action\":{\"type\":\"");
                
                // Put an ignore-previous-rules near the middle.
                if (c1 == 'L' && c2 == 'A' && c3 == 'A')
                    ruleList.append("ignore-previous-rules");
                else
                    ruleList.append("block");
                
                ruleList.append("\"},\"trigger\":{\"url-filter\":\".*");
                ruleList.append(c1);
                ruleList.append(c2);
                ruleList.append(c3);
                ruleList.append("\", \"url-filter-is-case-sensitive\":true}}");
            }
        }
    }
    ruleList.append(']');
    
    auto backend = makeBackend(ruleList.toString().utf8().data());

    testRequest(backend, mainDocumentRequest("http://webkit.org/AAA"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ZAA"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/LAA/AAA"), { }, true);
    testRequest(backend, mainDocumentRequest("http://webkit.org/LAA/MAA"), { ContentExtensions::ActionType::BlockLoad }, true);
    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
}

void checkCompilerError(const char* json, ContentExtensions::ContentExtensionError expectedError)
{
    WebCore::ContentExtensions::CompiledContentExtensionData extensionData;
    InMemoryContentExtensionCompilationClient client(extensionData);
    std::error_code compilerError = ContentExtensions::compileRuleList(client, json);
    EXPECT_EQ(compilerError.value(), static_cast<int>(expectedError));
}

TEST_F(ContentExtensionTest, InvalidJSON)
{
    checkCompilerError("[", ContentExtensions::ContentExtensionError::JSONInvalid);
    checkCompilerError("123", ContentExtensions::ContentExtensionError::JSONTopLevelStructureNotAnObject);
    checkCompilerError("{}", ContentExtensions::ContentExtensionError::JSONTopLevelStructureNotAnArray);
    // FIXME: Add unit test for JSONInvalidRule if that is possible to hit.
    checkCompilerError("[]", ContentExtensions::ContentExtensionError::JSONContainsNoRules);

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":5}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTrigger);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidURLFilterInTrigger);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":{}}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidURLFilterInTrigger);

    // FIXME: Add unit test for JSONInvalidObjectInTriggerFlagsArray if that is possible to hit.
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":{}}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":[\"invalid\"]}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":{}}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":[\"invalid\"]}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);

    checkCompilerError("[{\"action\":5,\"trigger\":{\"url-filter\":\"webkit.org\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidAction);
    checkCompilerError("[{\"action\":{\"type\":\"invalid\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidActionType);
    checkCompilerError("[{\"action\":{\"type\":\"css-display-none\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidCSSDisplayNoneActionType);

    checkCompilerError("[{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"webkit.org\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]",
        ContentExtensions::ContentExtensionError::RegexMatchesEverythingAfterIgnorePreviousRules);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidRegex);
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines1)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^.*foo\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bar$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[ab]+bang\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("foo://webkit.org/bar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bar://webkit.org/bar"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("abang://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bbang://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("cbang://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bang"), { });
    testRequest(backend, mainDocumentRequest("bang://webkit.org/"), { });
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines1Partitioning)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);

    // Those two share a prefix.
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^.*foo", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("bar$", false, 1));

    // Not this one.
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^[ab]+bang", false, 0));

    EXPECT_EQ(2ul, combinedURLFilters.createNFAs().size());
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines2)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^foo\"}},"
    "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^.*[a-c]+bar\"}},"
    "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^webkit:\"}},"
    "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-c]+b+oom\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("foo://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("webkit://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("http://bar.org/"), { });
    testRequest(backend, mainDocumentRequest("http://abar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://bbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://cbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://abcbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://dbar.org/"), { });
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines2Partitioning)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);

    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^foo", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^.*[a-c]+bar", false, 1));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^webkit:", false, 2));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("[a-c]+b+oom", false, 3));

    // "^foo" and "^webkit:" can be grouped, the other two have a variable prefix.
    EXPECT_EQ(3ul, combinedURLFilters.createNFAs().size());
}

static void testPatternStatus(String pattern, ContentExtensions::URLFilterParser::ParseStatus status)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
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

TEST_F(ContentExtensionTest, MinimizingWithMoreFinalStatesThanNonFinalStates)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^h[a-z://]+\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://foo.com/\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://bar.com/\"}}]");

    testRequest(backend, mainDocumentRequest("http://foo.com/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://bar.com/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("attp://foo.com/"), { });
    testRequest(backend, mainDocumentRequest("attp://bar.com/"), { });

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bttp://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bttps://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/b"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/b"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("cttp://webkit.org/B"), { });
    testRequest(backend, mainDocumentRequest("cttps://webkit.org/B"), { });
}

TEST_F(ContentExtensionTest, StatesWithDifferentActionsAreNotUnified1)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://www.webkit.org/\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"^https://www.webkit.org/\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"^attps://www.webkit.org/\"}}]");

    testRequest(backend, mainDocumentRequest("http://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://www.webkit.org/"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("attps://www.webkit.org/"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://www.webkit.org/a"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://www.webkit.org/B"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("attps://www.webkit.org/c"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://www.whatwg.org/"), { });
    testRequest(backend, mainDocumentRequest("https://www.whatwg.org/"), { });
    testRequest(backend, mainDocumentRequest("attps://www.whatwg.org/"), { });
}

TEST_F(ContentExtensionTest, StatesWithDifferentActionsAreNotUnified2)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://www.webkit.org/\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"^https://www.webkit.org/\"}},"
        "{\"action\":{\"type\":\"css-display-none\", \"selector\":\"#foo\"},\"trigger\":{\"url-filter\":\"^https://www.webkit.org/\"}}]");

    testRequest(backend, mainDocumentRequest("http://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("https://www.whatwg.org/"), { });
    testRequest(backend, mainDocumentRequest("attps://www.whatwg.org/"), { });
}

// The order in which transitions from the root will be processed is unpredictable.
// To exercises the various options, this test exists in various version exchanging the transition to the final state.
TEST_F(ContentExtensionTest, FallbackTransitionsWithDifferentiatorDoNotMerge1)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.a\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.a\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bac\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bbc\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^BCC\"}}]");

    testRequest(backend, mainDocumentRequest("aza://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bac://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bbc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bcc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("aac://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("abc://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("acc://www.webkit.org/"), { });

    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"), { });
}
TEST_F(ContentExtensionTest, FallbackTransitionsWithDifferentiatorDoNotMerge2)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bac\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bbc\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^BCC\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.a\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.a\"}}]");

    testRequest(backend, mainDocumentRequest("aza://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bac://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bbc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bcc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("aac://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("abc://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("acc://www.webkit.org/"), { });

    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"), { });
}
TEST_F(ContentExtensionTest, FallbackTransitionsWithDifferentiatorDoNotMerge3)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.c\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.c\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^baa\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bba\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^BCA\"}}]");

    testRequest(backend, mainDocumentRequest("azc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("baa://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bba://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bca://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("aaa://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("aba://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("aca://www.webkit.org/"), { });

    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"), { });
}
TEST_F(ContentExtensionTest, FallbackTransitionsWithDifferentiatorDoNotMerge4)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^baa\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bba\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^BCA\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.c\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.c\"}}]");

    testRequest(backend, mainDocumentRequest("azc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("baa://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bba://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bca://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("aaa://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("aba://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("aca://www.webkit.org/"), { });

    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"), { });
}

TEST_F(ContentExtensionTest, FallbackTransitionsToOtherNodeInSameGroupDoesNotDifferentiateGroup)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^aac\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.c\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.c\"}}]");

    testRequest(backend, mainDocumentRequest("aac://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("abc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("bac://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("abc://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("aaa://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("aca://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("baa://www.webkit.org/"), { });
}

TEST_F(ContentExtensionTest, SimpleFallBackTransitionDifferentiator1)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.bc.de\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.bd.ef\"}}]");

    testRequest(backend, mainDocumentRequest("abbccde://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("aabcdde://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("aabddef://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("aabddef://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("abcde://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("abdef://www.webkit.org/"), { });
}

TEST_F(ContentExtensionTest, SimpleFallBackTransitionDifferentiator2)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^cb.\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^db.b\"}}]");

    testRequest(backend, mainDocumentRequest("cba://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("cbb://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("dbab://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("dbxb://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("cca://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("dddd://www.webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("bbbb://www.webkit.org/"), { });
}

} // namespace TestWebKitAPI
