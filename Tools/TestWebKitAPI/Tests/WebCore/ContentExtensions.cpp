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
#include <WebCore/DFA.h>
#include <WebCore/DFABytecodeCompiler.h>
#include <WebCore/DFABytecodeInterpreter.h>
#include <WebCore/NFA.h>
#include <WebCore/NFAToDFA.h>
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

struct CompiledContentExtensionData {
    Vector<ContentExtensions::SerializedActionByte> actions;
    Vector<ContentExtensions::DFABytecode> filtersWithoutDomains;
    Vector<ContentExtensions::DFABytecode> filtersWithDomains;
    Vector<ContentExtensions::DFABytecode> domainFilters;
};

class InMemoryContentExtensionCompilationClient final : public ContentExtensions::ContentExtensionCompilationClient {
public:
    InMemoryContentExtensionCompilationClient(CompiledContentExtensionData& data)
        : m_data(data)
    {
        EXPECT_EQ(data.actions.size(), 0ull);
        EXPECT_EQ(data.filtersWithoutDomains.size(), 0ull);
        EXPECT_EQ(data.filtersWithDomains.size(), 0ull);
        EXPECT_EQ(data.domainFilters.size(), 0ull);
    }

    virtual void writeActions(Vector<ContentExtensions::SerializedActionByte>&& actions) override
    {
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.actions.size(), 0ull);
        EXPECT_EQ(m_data.filtersWithoutDomains.size(), 0ull);
        EXPECT_EQ(m_data.filtersWithDomains.size(), 0ull);
        EXPECT_EQ(m_data.domainFilters.size(), 0ull);
        m_data.actions.appendVector(actions);
    }
    
    virtual void writeFiltersWithoutDomainsBytecode(Vector<ContentExtensions::DFABytecode>&& bytecode) override
    {
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.filtersWithDomains.size(), 0ull);
        EXPECT_EQ(m_data.domainFilters.size(), 0ull);
        m_data.filtersWithoutDomains.appendVector(bytecode);
    }
    
    virtual void writeFiltersWithDomainsBytecode(Vector<ContentExtensions::DFABytecode>&& bytecode) override
    {
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.domainFilters.size(), 0ull);
        m_data.filtersWithDomains.appendVector(bytecode);
    }
    
    virtual void writeDomainFiltersBytecode(Vector<ContentExtensions::DFABytecode>&& bytecode) override
    {
        EXPECT_FALSE(finalized);
        m_data.domainFilters.appendVector(bytecode);
    }
    
    virtual void finalize() override
    {
        finalized = true;
    }

private:
    CompiledContentExtensionData& m_data;
    bool finalized { false };
};

class InMemoryCompiledContentExtension : public ContentExtensions::CompiledContentExtension {
public:
    static RefPtr<InMemoryCompiledContentExtension> createFromFilter(String&& filter)
    {
        CompiledContentExtensionData extensionData;
        InMemoryContentExtensionCompilationClient client(extensionData);
        auto compilerError = ContentExtensions::compileRuleList(client, WTF::move(filter));
        if (compilerError) {
            // Compiling should always succeed here. We have other tests for compile failures.
            EXPECT_TRUE(false);
            return nullptr;
        }

        return InMemoryCompiledContentExtension::create(WTF::move(extensionData));
    }

    static RefPtr<InMemoryCompiledContentExtension> create(CompiledContentExtensionData&& data)
    {
        return adoptRef(new InMemoryCompiledContentExtension(WTF::move(data)));
    }

    virtual ~InMemoryCompiledContentExtension()
    {
    }

    virtual const ContentExtensions::SerializedActionByte* actions() const override { return m_data.actions.data(); }
    virtual unsigned actionsLength() const override { return m_data.actions.size(); }
    virtual const ContentExtensions::DFABytecode* filtersWithoutDomainsBytecode() const override { return m_data.filtersWithoutDomains.data(); }
    virtual unsigned filtersWithoutDomainsBytecodeLength() const override { return m_data.filtersWithoutDomains.size(); }
    virtual const ContentExtensions::DFABytecode* filtersWithDomainsBytecode() const override { return m_data.filtersWithDomains.data(); }
    virtual unsigned filtersWithDomainsBytecodeLength() const override { return m_data.filtersWithDomains.size(); }
    virtual const ContentExtensions::DFABytecode* domainFiltersBytecode() const override { return m_data.domainFilters.data(); }
    virtual unsigned domainFiltersBytecodeLength() const override { return m_data.domainFilters.size(); }

private:
    InMemoryCompiledContentExtension(CompiledContentExtensionData&& data)
        : m_data(WTF::move(data))
    {
    }

    CompiledContentExtensionData m_data;
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

static ResourceLoadInfo mainDocumentRequest(const char* url, ResourceType resourceType = ResourceType::Document)
{
    return { URL(URL(), url), URL(URL(), url), resourceType };
}

static ResourceLoadInfo subResourceRequest(const char* url, const char* mainDocumentURL, ResourceType resourceType = ResourceType::Document)
{
    return { URL(URL(), url), URL(URL(), mainDocumentURL), resourceType };
}

ContentExtensions::ContentExtensionsBackend makeBackend(const char* json)
{
    auto extension = InMemoryCompiledContentExtension::createFromFilter(json);
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter", extension);
    return backend;
}

static Vector<ContentExtensions::NFA> createNFAs(ContentExtensions::CombinedURLFilters& combinedURLFilters)
{
    Vector<ContentExtensions::NFA> nfas;

    combinedURLFilters.processNFAs(std::numeric_limits<size_t>::max(), [&](ContentExtensions::NFA&& nfa) {
        nfas.append(WTF::move(nfa));
    });

    return nfas;
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

TEST_F(ContentExtensionTest, DomainTriggers)
{
    auto ifDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"webkit.org\"]}}]");
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifDomainBackend, mainDocumentRequest("http://not_webkit.org/test.htm"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.organization/test.htm"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://not_webkit.org/test.html"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.organization/test.html"), { });
    
    auto unlessDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"webkit.org\"]}}]");
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://not_webkit.org/test.htm"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.organization/test.htm"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://not_webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.organization/test.html"), { ContentExtensions::ActionType::BlockLoad });

    auto combinedBackend1 = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test_block_load\", \"if-domain\":[\"webkit.org\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"test_block_cookies\", \"unless-domain\":[\"webkit.org\"]}}]");
    testRequest(combinedBackend1, mainDocumentRequest("http://webkit.org"), { });
    testRequest(combinedBackend1, mainDocumentRequest("http://not_webkit.org"), { });
    testRequest(combinedBackend1, mainDocumentRequest("http://webkit.org/test_block_load.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/test_block_load.html", "http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/shouldnt_match.html", "http://webkit.org/"), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/test_block_load.html", "http://not_webkit.org/"), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/shouldnt_match.html", "http://not_webkit.org/"), { });
    testRequest(combinedBackend1, mainDocumentRequest("http://webkit.org/test_block_cookies.html"), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/test_block_cookies.html", "http://webkit.org/"), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/shouldnt_match.html", "http://webkit.org/"), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/test_block_cookies.html", "http://not_webkit.org/path/to/main/document.html"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/shouldnt_match.html", "http://not_webkit.org/"), { });
    
    auto combinedBackend2 = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test_block_load\\\\.html\", \"if-domain\":[\"webkit.org\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"test_block_cookies\\\\.html\", \"unless-domain\":[\"w3c.org\"]}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"test_css\\\\.html\"}}]");
    testRequest(combinedBackend2, mainDocumentRequest("http://webkit.org/test_css.html"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(combinedBackend2, mainDocumentRequest("http://webkit.org/test_css.htm"), { });
    testRequest(combinedBackend2, mainDocumentRequest("http://webkit.org/test_block_load.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(combinedBackend2, mainDocumentRequest("http://not_webkit.org/test_block_load.html"), { });
    testRequest(combinedBackend2, mainDocumentRequest("http://not_webkit.org/test_css.html"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(combinedBackend2, mainDocumentRequest("http://webkit.org/TEST_CSS.hTmL/test_block_load.html"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad});
    testRequest(combinedBackend2, mainDocumentRequest("http://w3c.org/test_css.html"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(combinedBackend2, mainDocumentRequest("http://w3c.org/test_block_load.html"), { });
    testRequest(combinedBackend2, mainDocumentRequest("http://w3c.org/test_block_cookies.html"), { });
    testRequest(combinedBackend2, mainDocumentRequest("http://w3c.org/test_css.html/test_block_cookies.html"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(combinedBackend2, mainDocumentRequest("http://not_w3c.org/test_block_cookies.html"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(combinedBackend2, mainDocumentRequest("http://not_w3c.org/test_css.html/test_block_cookies.html"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockCookies });

    auto ifDomainWithFlagsBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\", \"if-domain\":[\"webkit.org\"],\"resource-type\":[\"image\"]}}]");
    testRequest(ifDomainWithFlagsBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(ifDomainWithFlagsBackend, mainDocumentRequest("http://webkit.org/test.png", ResourceType::Image), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifDomainWithFlagsBackend, mainDocumentRequest("http://not_webkit.org/test.html"), { });
    testRequest(ifDomainWithFlagsBackend, mainDocumentRequest("http://not_webkit.org/test.png", ResourceType::Image), { });

    auto unlessDomainWithFlagsBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\", \"unless-domain\":[\"webkit.org\"],\"resource-type\":[\"image\"]}}]");
    testRequest(unlessDomainWithFlagsBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(unlessDomainWithFlagsBackend, mainDocumentRequest("http://webkit.org/test.png", ResourceType::Image), { });
    testRequest(unlessDomainWithFlagsBackend, mainDocumentRequest("http://not_webkit.org/test.html"), { });
    testRequest(unlessDomainWithFlagsBackend, mainDocumentRequest("http://not_webkit.org/test.png", ResourceType::Image), { ContentExtensions::ActionType::BlockLoad });

    // Domains should not be interepted as regular expressions.
    auto domainRegexBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"we?bkit.org\"]}}]");
    testRequest(domainRegexBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(domainRegexBackend, mainDocumentRequest("http://wbkit.org/test.html"), { });
    
    auto multipleIfDomainsBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"webkit.org\", \"w3c.org\"]}}]");
    testRequest(multipleIfDomainsBackend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(multipleIfDomainsBackend, mainDocumentRequest("http://webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(multipleIfDomainsBackend, mainDocumentRequest("http://w3c.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(multipleIfDomainsBackend, mainDocumentRequest("http://whatwg.org/test.html"), { });

    auto multipleUnlessDomainsBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"webkit.org\", \"w3c.org\"]}}]");
    testRequest(multipleUnlessDomainsBackend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(multipleUnlessDomainsBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(multipleUnlessDomainsBackend, mainDocumentRequest("http://w3c.org/test.html"), { });
    testRequest(multipleUnlessDomainsBackend, mainDocumentRequest("http://whatwg.org/test.html"), { ContentExtensions::ActionType::BlockLoad });

    // FIXME: Add and test domain-specific popup-only blocking (with layout tests).
}
    
TEST_F(ContentExtensionTest, MultipleExtensions)
{
    auto extension1 = InMemoryCompiledContentExtension::createFromFilter("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_load\"}}]");
    auto extension2 = InMemoryCompiledContentExtension::createFromFilter("[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"block_cookies\"}}]");
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter1", extension1);
    backend.addContentExtension("testFilter2", extension2);
    
    // These each have two display:none stylesheets. The second one is implied by using the default parameter ignorePreviousRules = false.
    testRequest(backend, mainDocumentRequest("http://webkit.org"), { ContentExtensions::ActionType::CSSDisplayNoneStyleSheet });
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_load.html"), { ContentExtensions::ActionType::CSSDisplayNoneStyleSheet, ContentExtensions::ActionType::BlockLoad});
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_cookies.html"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::CSSDisplayNoneStyleSheet});
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_load/block_cookies.html"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::CSSDisplayNoneStyleSheet, ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_cookies/block_load.html"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::CSSDisplayNoneStyleSheet, ContentExtensions::ActionType::BlockLoad });
    
    auto ignoreExtension1 = InMemoryCompiledContentExtension::createFromFilter("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_load\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore1\"}}]");
    auto ignoreExtension2 = InMemoryCompiledContentExtension::createFromFilter("[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"block_cookies\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore2\"}}]");
    ContentExtensions::ContentExtensionsBackend backendWithIgnore;
    backendWithIgnore.addContentExtension("testFilter1", ignoreExtension1);
    backendWithIgnore.addContentExtension("testFilter2", ignoreExtension2);
    
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org"), { ContentExtensions::ActionType::CSSDisplayNoneStyleSheet, ContentExtensions::ActionType::CSSDisplayNoneStyleSheet }, true);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/ignore1.html"), { ContentExtensions::ActionType::CSSDisplayNoneStyleSheet }, true);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_cookies/ignore1.html"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::CSSDisplayNoneStyleSheet}, true);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/ignore2.html"), { ContentExtensions::ActionType::BlockLoad, ContentExtensions::ActionType::CSSDisplayNoneStyleSheet }, true);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_cookies/ignore2.html"), { ContentExtensions::ActionType::CSSDisplayNoneStyleSheet}, true);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/block_cookies/ignore1/ignore2.html"), { }, true);
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

TEST_F(ContentExtensionTest, ResourceAndLoadType)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"BlockOnlyIfThirdPartyAndScript\",\"resource-type\":[\"script\"],\"load-type\":[\"third-party\"]}}]");
    
    testRequest(backend, subResourceRequest("http://webkit.org/BlockOnlyIfThirdPartyAndScript.js", "http://webkit.org", ResourceType::Script), { });
    testRequest(backend, subResourceRequest("http://webkit.org/BlockOnlyIfThirdPartyAndScript.png", "http://not_webkit.org", ResourceType::Image), { });
    testRequest(backend, subResourceRequest("http://webkit.org/BlockOnlyIfThirdPartyAndScript.js", "http://not_webkit.org", ResourceType::Script), { ContentExtensions::ActionType::BlockLoad });
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
    
TEST_F(ContentExtensionTest, WideNFA)
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

TEST_F(ContentExtensionTest, DeepNFA)
{
    const unsigned size = 100000;
    
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    
    // FIXME: DFAToNFA::convert takes way too long on these deep NFAs. We should optimize for that case.
    
    StringBuilder lotsOfAs;
    for (unsigned i = 0; i < size; ++i)
        lotsOfAs.append('A');
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern(lotsOfAs.toString().utf8().data(), false, 0));
    
    // FIXME: Yarr ought to be able to handle 2MB regular expressions.
    StringBuilder tooManyAs;
    for (unsigned i = 0; i < size * 20; ++i)
        tooManyAs.append('A');
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::YarrError, parser.addPattern(tooManyAs.toString().utf8().data(), false, 0));
    
    StringBuilder nestedGroups;
    for (unsigned i = 0; i < size; ++i)
        nestedGroups.append('(');
    for (unsigned i = 0; i < size; ++i)
        nestedGroups.append("B)");
    // FIXME: Add nestedGroups. Right now it also takes too long. It should be optimized.
    
    // This should not crash and not timeout.
    EXPECT_EQ(1ul, createNFAs(combinedURLFilters).size());
}

void checkCompilerError(const char* json, std::error_code expectedError)
{
    CompiledContentExtensionData extensionData;
    InMemoryContentExtensionCompilationClient client(extensionData);
    std::error_code compilerError = ContentExtensions::compileRuleList(client, json);
    EXPECT_EQ(compilerError.value(), expectedError.value());
    if (compilerError.value())
        EXPECT_STREQ(compilerError.category().name(), expectedError.category().name());
}

TEST_F(ContentExtensionTest, MatchesEverything)
{
    // Only css-display-none rules with triggers that match everything, no domain rules, and no flags
    // should go in the global display:none stylesheet. css-display-none rules with domain rules or flags
    // are applied separately on pages where they apply.
    auto backend1 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]");
    EXPECT_TRUE(nullptr != backend1.globalDisplayNoneStyleSheet(ASCIILiteral("testFilter")));
    testRequest(backend1, mainDocumentRequest("http://webkit.org"), { }); // Selector is in global stylesheet.
    
    auto backend2 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"if-domain\":[\"webkit.org\"]}}]");
    EXPECT_EQ(nullptr, backend2.globalDisplayNoneStyleSheet(ASCIILiteral("testFilter")));
    testRequest(backend2, mainDocumentRequest("http://webkit.org"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend2, mainDocumentRequest("http://w3c.org"), { });
    
    auto backend3 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"unless-domain\":[\"webkit.org\"]}}]");
    EXPECT_EQ(nullptr, backend3.globalDisplayNoneStyleSheet(ASCIILiteral("testFilter")));
    testRequest(backend3, mainDocumentRequest("http://webkit.org"), { });
    testRequest(backend3, mainDocumentRequest("http://w3c.org"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    
    auto backend4 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"load-type\":[\"third-party\"]}}]");
    EXPECT_EQ(nullptr, backend4.globalDisplayNoneStyleSheet(ASCIILiteral("testFilter")));
    testRequest(backend4, mainDocumentRequest("http://webkit.org"), { });
    testRequest(backend4, subResourceRequest("http://not_webkit.org", "http://webkit.org"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });

    // css-display-none rules after ignore-previous-rules should not be put in the default stylesheet.
    auto backend5 = makeBackend("[{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\".*\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]");
    EXPECT_EQ(nullptr, backend5.globalDisplayNoneStyleSheet(ASCIILiteral("testFilter")));
    testRequest(backend5, mainDocumentRequest("http://webkit.org"), { ContentExtensions::ActionType::CSSDisplayNoneSelector }, true);
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
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":[5]}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":{}}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":[\"invalid\"]}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":[5]}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":{}}}]", ContentExtensions::ContentExtensionError::JSONInvalidDomainList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[5]}}]", ContentExtensions::ContentExtensionError::JSONInvalidDomainList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"]}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":{}}}]", ContentExtensions::ContentExtensionError::JSONInvalidDomainList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[5]}}]", ContentExtensions::ContentExtensionError::JSONInvalidDomainList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"\"]}}]", ContentExtensions::ContentExtensionError::JSONInvalidDomainList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"A\"]}}]", ContentExtensions::ContentExtensionError::JSONDomainNotLowerCaseASCII);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"\\u00DC\"]}}]", ContentExtensions::ContentExtensionError::JSONDomainNotLowerCaseASCII);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"0\"]}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"a\"]}}]", { });

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[],\"unless-domain\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONInvalidDomainList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[]}}]", ContentExtensions::ContentExtensionError::JSONInvalidDomainList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":5}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":5}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":5,\"unless-domain\":5}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[]}}]", ContentExtensions::ContentExtensionError::JSONInvalidDomainList);
    
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"],\"unless-domain\":[]}}]", ContentExtensions::ContentExtensionError::JSONUnlessAndIfDomain);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"],\"unless-domain\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONUnlessAndIfDomain);

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\", \"unexpected-identifier-should-be-ignored\":5}}]", { });

    checkCompilerError("[{\"action\":5,\"trigger\":{\"url-filter\":\"webkit.org\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidAction);
    checkCompilerError("[{\"action\":{\"type\":\"invalid\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidActionType);
    checkCompilerError("[{\"action\":{\"type\":\"css-display-none\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidCSSDisplayNoneActionType);

    checkCompilerError("[{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"webkit.org\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*\",\"if-domain\":[\"a\"]}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*\",\"unless-domain\":[\"a\"]}}]", { });
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

    EXPECT_EQ(2ul, createNFAs(combinedURLFilters).size());
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
    EXPECT_EQ(3ul, createNFAs(combinedURLFilters).size());
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines3)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"A*D\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"A*BA+\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"A*BC\"}}]");
    
    testRequest(backend, mainDocumentRequest("http://webkit.org/D"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/AAD"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/AB"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABA"), { }, true);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABAD"), { }, true);
    testRequest(backend, mainDocumentRequest("http://webkit.org/BC"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABC"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABABC"), { ContentExtensions::ActionType::BlockCookies }, true);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABABCAD"), { ContentExtensions::ActionType::BlockCookies }, true);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABCAD"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad });
}
    
TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines3Partitioning)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A*D", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A*BA+", false, 1));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A*BC", false, 2));
    
    // "A*A" and "A*BC" can be grouped, "A*BA+" should not.
    EXPECT_EQ(2ul, createNFAs(combinedURLFilters).size());
}

TEST_F(ContentExtensionTest, SplittingLargeNFAs)
{
    const size_t expectedNFACounts[16] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 1, 1, 1};
    
    for (size_t i = 0; i < 16; i++) {
        ContentExtensions::CombinedURLFilters combinedURLFilters;
        ContentExtensions::URLFilterParser parser(combinedURLFilters);
        
        EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A+BBB", false, 1));
        EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A+CCC", false, 2));
        EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A+DDD", false, 2));
        
        Vector<ContentExtensions::NFA> nfas;
        combinedURLFilters.processNFAs(i, [&](ContentExtensions::NFA&& nfa) {
            nfas.append(WTF::move(nfa));
        });
        EXPECT_EQ(nfas.size(), expectedNFACounts[i]);
        
        Vector<ContentExtensions::DFA> dfas;
        for (auto& nfa : nfas)
            dfas.append(ContentExtensions::NFAToDFA::convert(nfa));
        
        Vector<ContentExtensions::DFABytecode> combinedBytecode;
        for (const auto& dfa : dfas) {
            Vector<ContentExtensions::DFABytecode> bytecode;
            ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
            compiler.compile();
            combinedBytecode.appendVector(bytecode);
        }
        
        Vector<bool> pagesUsed;
        ContentExtensions::DFABytecodeInterpreter interpreter(&combinedBytecode[0], combinedBytecode.size(), pagesUsed);
        
        EXPECT_EQ(interpreter.interpret("ABBBX", 0).size(), 1ull);
        EXPECT_EQ(interpreter.interpret("ACCCX", 0).size(), 1ull);
        EXPECT_EQ(interpreter.interpret("ADDDX", 0).size(), 1ull);
        EXPECT_EQ(interpreter.interpret("XBBBX", 0).size(), 0ull);
        EXPECT_EQ(interpreter.interpret("ABBX", 0).size(), 0ull);
        EXPECT_EQ(interpreter.interpret("ACCX", 0).size(), 0ull);
        EXPECT_EQ(interpreter.interpret("ADDX", 0).size(), 0ull);
    }
}
    
TEST_F(ContentExtensionTest, QuantifierInGroup)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A+)B)C)", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A)B+)C)", false, 1));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A)B+)C)D", false, 2));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A)B)C+)", false, 3));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A)B)C)", false, 4));
    
    // (((A)B+)C) and (((A)B+)C)D should be in the same NFA.
    EXPECT_EQ(4ul, createNFAs(combinedURLFilters).size());
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
