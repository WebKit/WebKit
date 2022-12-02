/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#if ENABLE(CONTENT_EXTENSIONS)

#include "Utilities.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/CombinedURLFilters.h>
#include <WebCore/CommonAtomStrings.h>
#include <WebCore/ContentExtensionActions.h>
#include <WebCore/ContentExtensionCompiler.h>
#include <WebCore/ContentExtensionError.h>
#include <WebCore/ContentExtensionParser.h>
#include <WebCore/ContentExtensionsBackend.h>
#include <WebCore/DFA.h>
#include <WebCore/DFABytecodeCompiler.h>
#include <WebCore/DFABytecodeInterpreter.h>
#include <WebCore/NFA.h>
#include <WebCore/NFAToDFA.h>
#include <WebCore/ResourceLoadInfo.h>
#include <WebCore/URLFilterParser.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {
using namespace WebCore;
using namespace WebCore::ContentExtensions;

class ContentExtensionTest : public testing::Test {
public:
    virtual void SetUp()
    {
        JSC::initialize();
        WTF::initializeMainThread();
    }
};

struct CompiledContentExtensionData {
    Vector<ContentExtensions::SerializedActionByte> actions;
    Vector<ContentExtensions::DFABytecode> urlFilters;
    Vector<ContentExtensions::DFABytecode> topURLFilters;
    Vector<ContentExtensions::DFABytecode> frameURLFilters;
};

class InMemoryContentExtensionCompilationClient final : public ContentExtensions::ContentExtensionCompilationClient {
public:
    InMemoryContentExtensionCompilationClient(CompiledContentExtensionData& data)
        : m_data(data)
    {
        EXPECT_EQ(data.actions.size(), 0ull);
        EXPECT_EQ(data.urlFilters.size(), 0ull);
        EXPECT_EQ(data.topURLFilters.size(), 0ull);
        EXPECT_EQ(data.frameURLFilters.size(), 0ull);
    }

private:
    void writeSource(String&&) final
    {
        EXPECT_FALSE(sourceWritten);
        sourceWritten = true;
    }

    void writeActions(Vector<SerializedActionByte>&& actions) final
    {
        EXPECT_TRUE(sourceWritten);
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.actions.size(), 0ull);
        EXPECT_EQ(m_data.urlFilters.size(), 0ull);
        EXPECT_EQ(m_data.topURLFilters.size(), 0ull);
        EXPECT_EQ(m_data.frameURLFilters.size(), 0ull);
        m_data.actions.appendVector(actions);
    }
    
    void writeURLFiltersBytecode(Vector<DFABytecode>&& bytecode) final
    {
        EXPECT_TRUE(sourceWritten);
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.topURLFilters.size(), 0ull);
        EXPECT_EQ(m_data.frameURLFilters.size(), 0ull);
        m_data.urlFilters.appendVector(bytecode);
    }

    void writeTopURLFiltersBytecode(Vector<DFABytecode>&& bytecode) final
    {
        EXPECT_TRUE(sourceWritten);
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.frameURLFilters.size(), 0ull);
        m_data.topURLFilters.appendVector(bytecode);
    }

    void writeFrameURLFiltersBytecode(Vector<DFABytecode>&& bytecode) final
    {
        EXPECT_TRUE(sourceWritten);
        EXPECT_FALSE(finalized);
        m_data.frameURLFilters.appendVector(bytecode);
    }
    
    void finalize() final
    {
        EXPECT_TRUE(sourceWritten);
        EXPECT_FALSE(finalized);
        finalized = true;
    }

    CompiledContentExtensionData& m_data;
    bool sourceWritten { false };
    bool finalized { false };
};

class InMemoryCompiledContentExtension : public ContentExtensions::CompiledContentExtension {
public:
    static Ref<InMemoryCompiledContentExtension> create(String&& filter)
    {
        CompiledContentExtensionData extensionData;
        InMemoryContentExtensionCompilationClient client(extensionData);
        auto parsedRules = ContentExtensions::parseRuleList(filter);
        auto compilerError = ContentExtensions::compileRuleList(client, WTFMove(filter), WTFMove(parsedRules.value()));

        // Compiling should always succeed here. We have other tests for compile failures.
        EXPECT_FALSE(compilerError);

        return adoptRef(*new InMemoryCompiledContentExtension(WTFMove(extensionData)));
    }

    const CompiledContentExtensionData& data() { return m_data; };

private:
    Span<const uint8_t> serializedActions() const final { return { m_data.actions.data(), m_data.actions.size() }; }
    Span<const uint8_t> urlFiltersBytecode() const final { return { m_data.urlFilters.data(), m_data.urlFilters.size() }; }
    Span<const uint8_t> topURLFiltersBytecode() const final { return { m_data.topURLFilters.data(), m_data.topURLFilters.size() }; }
    Span<const uint8_t> frameURLFiltersBytecode() const final { return { m_data.frameURLFilters.data(), m_data.frameURLFilters.size() }; }

    InMemoryCompiledContentExtension(CompiledContentExtensionData&& data)
        : m_data(WTFMove(data))
    { }

    CompiledContentExtensionData m_data;
};

static std::pair<Vector<WebCore::ContentExtensions::Action>, Vector<String>> allActionsForResourceLoad(const ContentExtensions::ContentExtensionsBackend& backend, const ResourceLoadInfo& info)
{
    Vector<ContentExtensions::Action> actions;
    Vector<String> identifiersApplyingStylesheets;
    for (auto&& actionsFromContentRuleList : backend.actionsForResourceLoad(info)) {
        for (auto&& action : WTFMove(actionsFromContentRuleList.actions))
            actions.append(WTFMove(action));
        if (!actionsFromContentRuleList.sawIgnorePreviousRules)
            identifiersApplyingStylesheets.append(actionsFromContentRuleList.contentRuleListIdentifier);
    }
    return { WTFMove(actions), WTFMove(identifiersApplyingStylesheets) };
}

#define testRequest(...) testRequestImpl(__LINE__, __VA_ARGS__)

void static testRequestImpl(int line, const ContentExtensions::ContentExtensionsBackend& contentExtensionsBackend, const ResourceLoadInfo& resourceLoadInfo, Vector<size_t> expectedActions, size_t stylesheets = 1)
{
    auto actions = allActionsForResourceLoad(contentExtensionsBackend, resourceLoadInfo);
    unsigned expectedSize = actions.first.size();
    EXPECT_EQ(expectedActions.size(), expectedSize);
    if (expectedActions.size() != expectedSize) {
        WTFLogAlways("Test failure at line %d", line);
        return;
    }

    for (unsigned i = 0; i < expectedActions.size(); ++i)
        EXPECT_EQ(expectedActions[i], actions.first[i].data().index());
    EXPECT_EQ(actions.second.size(), stylesheets);
}

static ResourceLoadInfo mainDocumentRequest(ASCIILiteral urlString, ResourceType resourceType = ResourceType::Document)
{
    URL url { urlString };
    return { url, url, url, resourceType };
}

static ResourceLoadInfo subResourceRequest(ASCIILiteral url, ASCIILiteral mainDocumentURLString, ResourceType resourceType = ResourceType::Document)
{
    URL mainDocumentURL { mainDocumentURLString };
    return { URL { url }, mainDocumentURL, mainDocumentURL, resourceType };
}

static ResourceLoadInfo requestInTopAndFrameURLs(ASCIILiteral url, ASCIILiteral topURL, ASCIILiteral frameURL, ResourceType resourceType = ResourceType::Document)
{
    return { URL { url }, URL { topURL }, URL { frameURL }, resourceType };
}

ContentExtensions::ContentExtensionsBackend makeBackend(String&& json)
{
    WebCore::initializeCommonAtomStrings();
    auto extension = InMemoryCompiledContentExtension::create(WTFMove(json));
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter"_s, WTFMove(extension), { });
    return backend;
}

static Vector<ContentExtensions::NFA> createNFAs(ContentExtensions::CombinedURLFilters& combinedURLFilters)
{
    Vector<ContentExtensions::NFA> nfas;
    combinedURLFilters.processNFAs(std::numeric_limits<size_t>::max(), [&](ContentExtensions::NFA&& nfa) {
        nfas.append(WTFMove(nfa));
        return true;
    });
    return nfas;
}

template <class T> constexpr std::size_t variantIndex = WTF::alternativeIndexV<T, ContentExtensions::ActionData>;

TEST_F(ContentExtensionTest, Basic)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
}

TEST_F(ContentExtensionTest, SingleCharacter)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^z\"}}]"_s);
    testRequest(matchBackend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("zttp://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"y\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/ywebkit"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
}

TEST_F(ContentExtensionTest, SingleCharacterDisjunction)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^z\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^c\"}}]"_s);
    testRequest(matchBackend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("bttp://webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("cttp://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("dttp://webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("zttp://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"x\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"y\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/dwebkit"_s), { });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/xwebkit"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/ywebkit"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/zwebkit"_s), { });
}

TEST_F(ContentExtensionTest, RangeBasic)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"w[0-9]c\", \"url-filter-is-case-sensitive\":true}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"[A-H][a-z]cko\", \"url-filter-is-case-sensitive\":true}}]"_s);

    testRequest(backend, mainDocumentRequest("http://w3c.org"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("w2c://whatwg.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/w0c"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/wac"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/wAc"_s), { });

    // Note: URL parsing and canonicalization lowercase the scheme and hostname.
    testRequest(backend, mainDocumentRequest("Aacko://webkit.org"_s), { });
    testRequest(backend, mainDocumentRequest("aacko://webkit.org"_s), { });
    testRequest(backend, mainDocumentRequest("http://gCcko.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://gccko.org/"_s), { });

    testRequest(backend, mainDocumentRequest("http://webkit.org/Gecko"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/gecko"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/GEcko"_s), { });
}

TEST_F(ContentExtensionTest, RangeExclusionGeneratingUniversalTransition)
{
    // Transition of the type ([^X]X) effictively transition on every input.
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[^a]+afoobar\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://w3c.org"_s), { });

    testRequest(backend, mainDocumentRequest("http://w3c.org/foobafoobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://w3c.org/foobarfoobar"_s), { });
    testRequest(backend, mainDocumentRequest("http://w3c.org/FOOBAFOOBAR"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://w3c.org/FOOBARFOOBAR"_s), { });

    // The character before the "a" prefix cannot be another "a".
    testRequest(backend, mainDocumentRequest("http://w3c.org/aafoobar"_s), { });
    testRequest(backend, mainDocumentRequest("http://w3c.org/Aafoobar"_s), { });
    testRequest(backend, mainDocumentRequest("http://w3c.org/aAfoobar"_s), { });
    testRequest(backend, mainDocumentRequest("http://w3c.org/AAfoobar"_s), { });
}

TEST_F(ContentExtensionTest, PatternStartingWithGroup)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(http://whatwg\\\\.org/)?webkit\134\134.org\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://whatwg.org/webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://whatwg.org/webkit.org"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://whatwg.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://whatwg.org"_s), { });
}

TEST_F(ContentExtensionTest, PatternNestedGroups)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/(foo(bar)*)+\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarbar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foofoobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foob"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foor"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bar"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/fobar"_s), { });
}

TEST_F(ContentExtensionTest, EmptyGroups)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/foo()bar\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/((me)()(too))\"}}]"_s);
    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bar"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/me"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/too"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/metoo"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foome"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foomebar"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/mefoo"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/mefootoo"_s), { });
}

TEST_F(ContentExtensionTest, QuantifiedEmptyGroups)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/foo()+bar\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/(()*()?(target)()+)\"}}]"_s);
    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bar"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/me"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/too"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/target"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foome"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foomebar"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/mefoo"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/mefootoo"_s), { });
}

TEST_F(ContentExtensionTest, MatchPastEndOfString)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".+\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarbar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foofoobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foob"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foor"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
}

TEST_F(ContentExtensionTest, StartOfLineAssertion)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^foobar\"}}]"_s);

    testRequest(backend, mainDocumentRequest("foobar://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("foobars:///foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("foobarfoobar:///foobarfoobarfoobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoo"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarf"_s), { });
    testRequest(backend, mainDocumentRequest("http://foobar.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://foobar.org/"_s), { });
}

TEST_F(ContentExtensionTest, EndOfLineAssertion)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foobar$\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("file:///foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("file:///foobarfoobarfoobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarfoo"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarf"_s), { });
}

TEST_F(ContentExtensionTest, EndOfLineAssertionWithInvertedCharacterSet)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[^y]$\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/a"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/Ya"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/yFoobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/y"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/Y"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobary"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobarY"_s), { });
}

TEST_F(ContentExtensionTest, DotDoesNotIncludeEndOfLine)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"https://webkit\\\\.org/.\"}}]"_s);

    testRequest(backend, mainDocumentRequest("https://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/A"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/z"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
}

TEST_F(ContentExtensionTest, PrefixInfixSuffixExactMatch)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"infix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^prefix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"suffix$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://exact\\\\.org/$\"}}]"_s);

    testRequest(backend, mainDocumentRequest("infix://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://infix.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/infix"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("prefix://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://prefix.org/"_s), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/prefix"_s), { });

    testRequest(backend, mainDocumentRequest("https://webkit.org/suffix"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://suffix.org/"_s), { });
    testRequest(backend, mainDocumentRequest("suffix://webkit.org/"_s), { });

    testRequest(backend, mainDocumentRequest("http://exact.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://exact.org/oops"_s), { });
}

TEST_F(ContentExtensionTest, DuplicatedMatchAllTermsInVariousFormat)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*.*(.)*(.*)(.+)*(.?)*infix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"pre(.?)+(.+)?post\"}}]"_s);

    testRequest(backend, mainDocumentRequest("infix://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://infix.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/infix"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("pre://webkit.org/post"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://prepost.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://pre.org/posttail"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://pre.pre/posttail"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://pre.org/posttailpost"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("https://post.org/pre"_s), { });
    testRequest(backend, mainDocumentRequest("https://pre.org/pre"_s), { });
    testRequest(backend, mainDocumentRequest("https://post.org/post"_s), { });
}

TEST_F(ContentExtensionTest, UndistinguishableActionInsidePrefixTree)
{
    // In this case, the two actions are undistinguishable. The actions of "prefix" appear inside the prefixtree
    // ending at "suffix".
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"prefix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"prefixsuffix\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://prefix.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/prefix"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/aaaprefixaaa"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://prefixsuffix.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/prefixsuffix"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bbbprefixsuffixbbb"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("http://suffix.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/suffix"_s), { });
}

TEST_F(ContentExtensionTest, DistinguishableActionInsidePrefixTree)
{
    // In this case, the two actions are distinguishable.
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"prefix\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"prefixsuffix\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://prefix.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/prefix"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/aaaprefixaaa"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://prefixsuffix.org/"_s), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/prefixsuffix"_s), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bbbprefixsuffixbbb"_s), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("http://suffix.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/suffix"_s), { });
}

TEST_F(ContentExtensionTest, DistinguishablePrefixAreNotMerged)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foo\\\\.org\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bar\\\\.org\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://foo.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://bar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("http://foor.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://fooar.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://fooba.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://foob.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://foor.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://foar.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://foba.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://fob.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://barf.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://barfo.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://baroo.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://baro.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://baf.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://bafo.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://baoo.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://bao.org/"_s), { });

    testRequest(backend, mainDocumentRequest("http://foo.orgbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://oo.orgbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://o.orgbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://.orgbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://rgbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://gbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://foo.orgar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://foo.orgr.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://foo.org.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://foo.orgorg/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://foo.orgrg/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://foo.orgg/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
}

static void compareContents(const DFABytecodeInterpreter::Actions& a, const Vector<uint64_t>& b)
{
    EXPECT_EQ(a.size(), b.size());
    for (unsigned i = 0; i < b.size(); ++i)
        EXPECT_TRUE(a.contains(b[i]));
}

TEST_F(ContentExtensionTest, SearchSuffixesWithIdenticalActionAreMerged)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("foo\\.org"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("ba\\.org"_s, false, 0));

    Vector<ContentExtensions::NFA> nfas = createNFAs(combinedURLFilters);
    EXPECT_EQ(1ul, nfas.size());
    EXPECT_EQ(12ul, nfas.first().nodes.size());

    ContentExtensions::DFA dfa = *ContentExtensions::NFAToDFA::convert(WTFMove(nfas.first()));
    Vector<ContentExtensions::DFABytecode> bytecode;
    ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
    compiler.compile();
    DFABytecodeInterpreter interpreter({ bytecode.data(), bytecode.size() });
    compareContents(interpreter.interpret("foo.org"_s, 0), { 0 });
    compareContents(interpreter.interpret("ba.org"_s, 0), { 0 });
    compareContents(interpreter.interpret("bar.org"_s, 0), { });

    compareContents(interpreter.interpret("paddingfoo.org"_s, 0), { 0 });
    compareContents(interpreter.interpret("paddingba.org"_s, 0), { 0 });
    compareContents(interpreter.interpret("paddingbar.org"_s, 0), { });
}

TEST_F(ContentExtensionTest, SearchSuffixesWithDistinguishableActionAreNotMerged)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("foo\\.org"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("ba\\.org"_s, false, 1));

    Vector<ContentExtensions::NFA> nfas = createNFAs(combinedURLFilters);

    EXPECT_EQ(1ul, nfas.size());
    EXPECT_EQ(17ul, nfas.first().nodes.size());

    ContentExtensions::DFA dfa = *ContentExtensions::NFAToDFA::convert(WTFMove(nfas.first()));
    Vector<ContentExtensions::DFABytecode> bytecode;
    ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
    compiler.compile();
    DFABytecodeInterpreter interpreter({ bytecode.data(), bytecode.size() });
    compareContents(interpreter.interpret("foo.org"_s, 0), { 0 });
    compareContents(interpreter.interpret("ba.org"_s, 0), { 1 });
    compareContents(interpreter.interpret("bar.org"_s, 0), { });

    compareContents(interpreter.interpret("paddingfoo.org"_s, 0), { 0 });
    compareContents(interpreter.interpret("paddingba.org"_s, 0), { 1 });
    compareContents(interpreter.interpret("paddingba.orgfoo.org"_s, 0), { 1, 0 });
    compareContents(interpreter.interpret("paddingbar.org"_s, 0), { });
}

TEST_F(ContentExtensionTest, DomainTriggers)
{
    auto ifDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"webkit.org\"]}}]"_s);
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.org/test.htm"_s), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"_s), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(ifDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"_s), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"_s), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://not_webkit.org/test.htm"_s), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.organization/test.htm"_s), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://not_webkit.org/test.html"_s), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.organization/test.html"_s), { });
    
    auto unlessDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"webkit.org\"]}}]"_s);
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.org/test.htm"_s), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"_s), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://not_webkit.org/test.htm"_s), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.organization/test.htm"_s), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://not_webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.organization/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    
    auto ifDomainStarBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"*webkit.org\"]}}]"_s);
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://webkit.org/test.htm"_s), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"_s), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://not_webkit.org/test.htm"_s), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://webkit.organization/test.htm"_s), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://not_webkit.org/test.html"_s), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://webkit.organization/test.html"_s), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://example.com/.webkit.org/in/path/test.html"_s), { });

    auto unlessDomainStarBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"*webkit.org\"]}}]"_s);
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://webkit.org/test.htm"_s), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"_s), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"_s), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"_s), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://not_webkit.org/test.htm"_s), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://webkit.organization/test.htm"_s), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://not_webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://webkit.organization/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    auto ifSubDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"sub1.webkit.org\"]}}]"_s);
    testRequest(ifSubDomainBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(ifSubDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"_s), { });
    testRequest(ifSubDomainBackend, mainDocumentRequest("http://sub1.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(ifSubDomainBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"_s), { });

    auto ifSubDomainStarBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"*sub1.webkit.org\"]}}]"_s);
    testRequest(ifSubDomainStarBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(ifSubDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"_s), { });
    testRequest(ifSubDomainStarBackend, mainDocumentRequest("http://sub1.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(ifSubDomainStarBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    auto unlessSubDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"sub1.webkit.org\"]}}]"_s);
    testRequest(unlessSubDomainBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(unlessSubDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(unlessSubDomainBackend, mainDocumentRequest("http://sub1.webkit.org/test.html"_s), { });
    testRequest(unlessSubDomainBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    
    auto unlessSubDomainStarBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"*sub1.webkit.org\"]}}]"_s);
    testRequest(unlessSubDomainStarBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(unlessSubDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(unlessSubDomainStarBackend, mainDocumentRequest("http://sub1.webkit.org/test.html"_s), { });
    testRequest(unlessSubDomainStarBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"_s), { });

    auto combinedBackend1 = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test_block_load\", \"if-domain\":[\"webkit.org\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"test_block_cookies\", \"unless-domain\":[\"webkit.org\"]}}]"_s);
    testRequest(combinedBackend1, mainDocumentRequest("http://webkit.org"_s), { });
    testRequest(combinedBackend1, mainDocumentRequest("http://not_webkit.org"_s), { });
    testRequest(combinedBackend1, mainDocumentRequest("http://webkit.org/test_block_load.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/test_block_load.html"_s, "http://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/shouldnt_match.html"_s, "http://webkit.org/"_s), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/test_block_load.html"_s, "http://not_webkit.org/"_s), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/shouldnt_match.html"_s, "http://not_webkit.org/"_s), { });
    testRequest(combinedBackend1, mainDocumentRequest("http://webkit.org/test_block_cookies.html"_s), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/test_block_cookies.html"_s, "http://webkit.org/"_s), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/shouldnt_match.html"_s, "http://webkit.org/"_s), { });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/test_block_cookies.html"_s, "http://not_webkit.org/path/to/main/document.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(combinedBackend1, subResourceRequest("http://whatwg.org/shouldnt_match.html"_s, "http://not_webkit.org/"_s), { });
    
    auto combinedBackend2 = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test_block_load\\\\.html\", \"if-domain\":[\"webkit.org\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"test_block_cookies\\\\.html\", \"unless-domain\":[\"w3c.org\"]}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"test_css\\\\.html\"}}]"_s);
    testRequest(combinedBackend2, mainDocumentRequest("http://webkit.org/test_css.html"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(combinedBackend2, mainDocumentRequest("http://webkit.org/test_css.htm"_s), { });
    testRequest(combinedBackend2, mainDocumentRequest("http://webkit.org/test_block_load.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(combinedBackend2, mainDocumentRequest("http://not_webkit.org/test_block_load.html"_s), { });
    testRequest(combinedBackend2, mainDocumentRequest("http://not_webkit.org/test_css.html"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(combinedBackend2, mainDocumentRequest("http://webkit.org/TEST_CSS.hTmL/test_block_load.html"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(combinedBackend2, mainDocumentRequest("http://w3c.org/test_css.html"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(combinedBackend2, mainDocumentRequest("http://w3c.org/test_block_load.html"_s), { });
    testRequest(combinedBackend2, mainDocumentRequest("http://w3c.org/test_block_cookies.html"_s), { });
    testRequest(combinedBackend2, mainDocumentRequest("http://w3c.org/test_css.html/test_block_cookies.html"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(combinedBackend2, mainDocumentRequest("http://not_w3c.org/test_block_cookies.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(combinedBackend2, mainDocumentRequest("http://not_w3c.org/test_css.html/test_block_cookies.html"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockCookiesAction> });

    auto ifDomainWithFlagsBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\", \"if-domain\":[\"webkit.org\"],\"resource-type\":[\"image\"]}}]"_s);
    testRequest(ifDomainWithFlagsBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(ifDomainWithFlagsBackend, mainDocumentRequest("http://webkit.org/test.png"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(ifDomainWithFlagsBackend, mainDocumentRequest("http://not_webkit.org/test.html"_s), { });
    testRequest(ifDomainWithFlagsBackend, mainDocumentRequest("http://not_webkit.org/test.png"_s, ResourceType::Image), { });

    auto unlessDomainWithFlagsBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\", \"unless-domain\":[\"webkit.org\"],\"resource-type\":[\"image\"]}}]"_s);
    testRequest(unlessDomainWithFlagsBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(unlessDomainWithFlagsBackend, mainDocumentRequest("http://webkit.org/test.png"_s, ResourceType::Image), { });
    testRequest(unlessDomainWithFlagsBackend, mainDocumentRequest("http://not_webkit.org/test.html"_s), { });
    testRequest(unlessDomainWithFlagsBackend, mainDocumentRequest("http://not_webkit.org/test.png"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockLoadAction> });

    // Domains should not be interepted as regular expressions.
    auto domainRegexBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"we?bkit.org\"]}}]"_s);
    testRequest(domainRegexBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(domainRegexBackend, mainDocumentRequest("http://wbkit.org/test.html"_s), { });
    
    auto multipleIfDomainsBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"webkit.org\", \"w3c.org\"]}}]"_s);
    testRequest(multipleIfDomainsBackend, mainDocumentRequest("http://webkit.org/test.htm"_s), { });
    testRequest(multipleIfDomainsBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(multipleIfDomainsBackend, mainDocumentRequest("http://w3c.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(multipleIfDomainsBackend, mainDocumentRequest("http://whatwg.org/test.html"_s), { });

    auto multipleUnlessDomainsBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"webkit.org\", \"w3c.org\"]}}]"_s);
    testRequest(multipleUnlessDomainsBackend, mainDocumentRequest("http://webkit.org/test.htm"_s), { });
    testRequest(multipleUnlessDomainsBackend, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(multipleUnlessDomainsBackend, mainDocumentRequest("http://w3c.org/test.html"_s), { });
    testRequest(multipleUnlessDomainsBackend, mainDocumentRequest("http://whatwg.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    // FIXME: Add and test domain-specific popup-only blocking (with layout tests).
}

TEST_F(ContentExtensionTest, DomainTriggersAlongMergedActions)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"test\\\\.html\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"webkit.org\"]}},"
        "{\"action\":{\"type\":\"css-display-none\", \"selector\": \"*\"},\"trigger\":{\"url-filter\":\"trigger-on-scripts\\\\.html\",\"resource-type\":[\"script\"]}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore-previous\",\"resource-type\":[\"image\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"except-this\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/test.htm"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockLoadAction>, variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/test.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });

    testRequest(backend, mainDocumentRequest("http://notwebkit.org/trigger-on-scripts.html"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/trigger-on-scripts.html"_s), { });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/trigger-on-scripts.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/trigger-on-scripts.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/trigger-on-scripts.html.test.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/trigger-on-scripts.html.test.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction>, variantIndex<ContentExtensions::BlockCookiesAction> });

    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html"_s), { });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html.test.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html.test.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction>, variantIndex<ContentExtensions::BlockCookiesAction> });

    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html"_s, ResourceType::Image), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html"_s, ResourceType::Image), { }, 0);
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html.test.html"_s, ResourceType::Image), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html.test.html"_s, ResourceType::Image), { }, 0);

    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockLoadAction>, variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockCookiesAction> }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockCookiesAction> }, 0);
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockCookiesAction> }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockCookiesAction> }, 0);
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html"_s, ResourceType::Script), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction>, variantIndex<ContentExtensions::BlockCookiesAction> });
}

TEST_F(ContentExtensionTest, TopURL)
{
    const Vector<size_t> blockLoad = { variantIndex<ContentExtensions::BlockLoadAction> };
    const Vector<size_t> blockCookies = { variantIndex<ContentExtensions::BlockCookiesAction> };

    auto ifTopURL = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\"^http://web.*kit.org\"]}}]"_s);
    testRequest(ifTopURL, mainDocumentRequest("http://webkit.org/test.html"_s), blockLoad);
    testRequest(ifTopURL, mainDocumentRequest("http://webkit.org/test.not_html"_s), { });
    testRequest(ifTopURL, mainDocumentRequest("http://WEBKIT.org/test.html"_s), blockLoad);
    testRequest(ifTopURL, mainDocumentRequest("http://webkit.org/TEST.html"_s), blockLoad);
    testRequest(ifTopURL, mainDocumentRequest("http://web__kit.org/test.html"_s), blockLoad);
    testRequest(ifTopURL, mainDocumentRequest("http://webk__it.org/test.html"_s), { });
    testRequest(ifTopURL, mainDocumentRequest("http://web__kit.org/test.not_html"_s), { });
    testRequest(ifTopURL, mainDocumentRequest("http://not_webkit.org/test.html"_s), { });
    testRequest(ifTopURL, subResourceRequest("http://not_webkit.org/test.html"_s, "http://webkit.org/not_test.html"_s), blockLoad);
    testRequest(ifTopURL, subResourceRequest("http://not_webkit.org/test.html"_s, "http://not_webkit.org/not_test.html"_s), { });
    testRequest(ifTopURL, subResourceRequest("http://webkit.org/test.html"_s, "http://not_webkit.org/not_test.html"_s), { });
    testRequest(ifTopURL, subResourceRequest("http://webkit.org/test.html"_s, "http://not_webkit.org/test.html"_s), { });
    testRequest(ifTopURL, subResourceRequest("http://webkit.org/test.html"_s, "http://webkit.org/test.html"_s), blockLoad);
    testRequest(ifTopURL, subResourceRequest("http://webkit.org/test.html"_s, "http://example.com/#http://webkit.org/test.html"_s), { });
    testRequest(ifTopURL, subResourceRequest("http://example.com/#http://webkit.org/test.html"_s, "http://webkit.org/test.html"_s), blockLoad);

    auto unlessTopURL = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-top-url\":[\"^http://web.*kit.org\"]}}]"_s);
    testRequest(unlessTopURL, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://WEBKIT.org/test.html"_s), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://webkit.org/TEST.html"_s), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://webkit.org/test.not_html"_s), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://web__kit.org/test.html"_s), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://webk__it.org/test.html"_s), blockLoad);
    testRequest(unlessTopURL, mainDocumentRequest("http://web__kit.org/test.not_html"_s), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://not_webkit.org/test.html"_s), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://not_webkit.org/test.html"_s, "http://webkit.org/not_test.html"_s), { });
    testRequest(unlessTopURL, subResourceRequest("http://not_webkit.org/test.html"_s, "http://not_webkit.org/not_test.html"_s), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://webkit.org/test.html"_s, "http://not_webkit.org/not_test.html"_s), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://webkit.org/test.html"_s, "http://not_webkit.org/test.html"_s), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://webkit.org/test.html"_s, "http://webkit.org/test.html"_s), { });
    testRequest(unlessTopURL, subResourceRequest("http://webkit.org/test.html"_s, "http://example.com/#http://webkit.org/test.html"_s), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://example.com/#http://webkit.org/test.html"_s, "http://webkit.org/test.html"_s), { });

    auto ifTopURLMatchesEverything = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\".*\"]}}]"_s);
    testRequest(ifTopURLMatchesEverything, mainDocumentRequest("http://webkit.org/test.html"_s), blockLoad);
    testRequest(ifTopURLMatchesEverything, mainDocumentRequest("http://webkit.org/test.not_html"_s), { });
    testRequest(ifTopURLMatchesEverything, mainDocumentRequest("http://not_webkit.org/test.html"_s), blockLoad);
    
    auto unlessTopURLMatchesEverything = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-top-url\":[\".*\"]}}]"_s);
    testRequest(unlessTopURLMatchesEverything, mainDocumentRequest("http://webkit.org/test.html"_s), { });
    testRequest(unlessTopURLMatchesEverything, mainDocumentRequest("http://webkit.org/test.not_html"_s), { });
    testRequest(unlessTopURLMatchesEverything, mainDocumentRequest("http://not_webkit.org/test.html"_s), { });
    
    auto mixedConditions = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\"^http://web.*kit.org\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"resource-type\":[\"document\"], \"url-filter\":\"test\\\\.html\", \"unless-top-url\":[\"^http://web.*kit.org\"]}}]"_s);
    testRequest(mixedConditions, mainDocumentRequest("http://webkit.org/test.html"_s), blockLoad);
    testRequest(mixedConditions, mainDocumentRequest("http://not_webkit.org/test.html"_s), blockCookies);
    testRequest(mixedConditions, subResourceRequest("http://webkit.org/test.html"_s, "http://not_webkit.org"_s, ResourceType::Document), blockCookies);
    testRequest(mixedConditions, subResourceRequest("http://webkit.org/test.html"_s, "http://not_webkit.org"_s, ResourceType::Image), { });

    auto caseSensitive = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\"^http://web.*kit.org/test\"], \"top-url-filter-is-case-sensitive\":true}}]"_s);
    testRequest(caseSensitive, mainDocumentRequest("http://webkit.org/test.html"_s), blockLoad);
    testRequest(caseSensitive, mainDocumentRequest("http://WEBKIT.org/test.html"_s), blockLoad); // domains are canonicalized before running regexes.
    testRequest(caseSensitive, mainDocumentRequest("http://webkit.org/TEST.html"_s), { });

    auto caseInsensitive = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\"^http://web.*kit.org/test\"]}}]"_s);
    testRequest(caseInsensitive, mainDocumentRequest("http://webkit.org/test.html"_s), blockLoad);
    testRequest(caseInsensitive, mainDocumentRequest("http://webkit.org/TEST.html"_s), blockLoad);
}

TEST_F(ContentExtensionTest, MultipleExtensions)
{
    auto extension1 = InMemoryCompiledContentExtension::create("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_load\"}}]"_s);
    auto extension2 = InMemoryCompiledContentExtension::create("[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"block_cookies\"}}]"_s);
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter1"_s, WTFMove(extension1), { });
    backend.addContentExtension("testFilter2"_s, WTFMove(extension2), { });
    
    testRequest(backend, mainDocumentRequest("http://webkit.org"_s), { }, 2);
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_load.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> }, 2);
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_cookies.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction> }, 2);
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_load/block_cookies.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockLoadAction> }, 2);
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_cookies/block_load.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockLoadAction> }, 2);
    
    auto ignoreExtension1 = InMemoryCompiledContentExtension::create("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_load\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore1\"}}]"_s);
    auto ignoreExtension2 = InMemoryCompiledContentExtension::create("[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"block_cookies\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore2\"}}]"_s);
    ContentExtensions::ContentExtensionsBackend backendWithIgnore;
    backendWithIgnore.addContentExtension("testFilter1"_s, WTFMove(ignoreExtension1), { });
    backendWithIgnore.addContentExtension("testFilter2"_s, WTFMove(ignoreExtension2), { });

    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org"_s), { }, 2);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/ignore1.html"_s), { }, 1);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_cookies/ignore1.html"_s), { variantIndex<ContentExtensions::BlockCookiesAction> }, 1);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/ignore2.html"_s), { variantIndex<ContentExtensions::BlockLoadAction> }, 1);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_cookies/ignore2.html"_s), { }, 1);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/block_cookies/ignore1/ignore2.html"_s), { }, 0);
}

static bool actionsEqual(const std::pair<Vector<WebCore::ContentExtensions::Action>, Vector<String>>& actual, Vector<WebCore::ContentExtensions::Action>&& expected, bool ignorePreviousRules = false)
{
    if (ignorePreviousRules) {
        if (actual.second.size())
            return false;
    } else {
        if (actual.second.size() != 1)
            return false;
        if (actual.second[0] != "testFilter"_s)
            return false;
    }

    if (actual.first.size() != expected.size())
        return false;
    for (size_t i = 0; i < expected.size(); ++i) {
        if (actual.first[i] != expected[i])
            return false;
    }
    return true;
}

static constexpr auto jsonWithStringsToCombine = "["
    "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"A\"}},"
    "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"A\"}},"
    "{\"action\":{\"type\":\"notify\",\"notification\":\"BBB\"},\"trigger\":{\"url-filter\":\"B\"}},"
    "{\"action\":{\"type\":\"css-display-none\",\"selector\":\"CCC\"},\"trigger\":{\"url-filter\":\"C\"}},"
    "{\"action\":{\"type\":\"css-display-none\",\"selector\":\"selectorCombinedWithC\"},\"trigger\":{\"url-filter\":\"C\"}},"
    "{\"action\":{\"type\":\"css-display-none\",\"selector\":\"DDD\"},\"trigger\":{\"url-filter\":\"D\"}},"
    "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"E\"}},"
    "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"F\"}},"
    "{\"action\":{\"type\":\"notify\",\"notification\":\"GGG\"},\"trigger\":{\"url-filter\":\"G\"}},"
    "{\"action\":{\"type\":\"notify\",\"notification\":\"GGG\"},\"trigger\":{\"url-filter\":\"I\"}},"
    "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"J\"}},"
    "{\"action\":{\"type\":\"notify\",\"notification\":\"GGG\"},\"trigger\":{\"url-filter\":\"K\"}}"
"]"_s;

TEST_F(ContentExtensionTest, StringParameters)
{
    auto backend1 = makeBackend("[{\"action\":{\"type\":\"notify\",\"notification\":\"testnotification\"},\"trigger\":{\"url-filter\":\"matches\"}}]"_s);
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend1, mainDocumentRequest("test:///matches"_s)), Vector<Action>::from(Action { ContentExtensions::NotifyAction { { "testnotification"_s } } })));

    auto backend2 = makeBackend(jsonWithStringsToCombine);
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://A"_s)), Vector<Action>::from(Action { ContentExtensions::NotifyAction { { "AAA"_s } } })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://B"_s)), Vector<Action>::from(Action { ContentExtensions::NotifyAction { { "BBB"_s } } })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://C"_s)), Vector<Action>::from(Action { ContentExtensions::CSSDisplayNoneSelectorAction { { "CCC,selectorCombinedWithC"_s } } })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://D"_s)), Vector<Action>::from(Action { ContentExtensions::CSSDisplayNoneSelectorAction { { "DDD"_s } } })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://E"_s)), { }, true));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://F"_s)), Vector<Action>::from(Action { ContentExtensions::BlockLoadAction() })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://G"_s)), Vector<Action>::from(Action { ContentExtensions::NotifyAction { { "GGG"_s } } })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://GIK"_s)), Vector<Action>::from(Action { ContentExtensions::NotifyAction { { "GGG"_s } } })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://AJ"_s)), Vector<Action>::from(
    Action { ContentExtensions::NotifyAction { { "AAA"_s } } },
    Action { ContentExtensions::NotifyAction { { "AAA"_s } } } // ignore-previous-rules makes the AAA actions need to be unique.
    )));
    // FIXME: Add a test that matches actions with AAA with ignore-previous-rules between them and makes sure we only get one notification.
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://AE"_s)), { }, true));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://ABCDE"_s)), { }, true));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://ABCDEFG"_s)), Vector<Action>::from(
        Action { ContentExtensions::NotifyAction { { "GGG"_s } } },
        Action { ContentExtensions::BlockLoadAction() }
    ), true));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://FG"_s)), Vector<Action>::from(
        Action { ContentExtensions::NotifyAction { { "GGG"_s } } },
        Action { ContentExtensions::BlockLoadAction() }
    )));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://EFG"_s)), Vector<Action>::from(
        Action { ContentExtensions::NotifyAction { { "GGG"_s } } },
        Action { ContentExtensions::BlockLoadAction() }
    ), true));
}

template<typename T>
static int sequenceInstances(const Vector<T> vector, ASCIILiteral sequence)
{
    static_assert(sizeof(T) == sizeof(char), "sequenceInstances should only be used for various byte vectors.");

    size_t sequenceLength = sequence.length();
    size_t instances = 0;
    for (size_t i = 0; i <= vector.size() - sequenceLength; ++i) {
        for (size_t j = 0; j < sequenceLength; j++) {
            if (vector[i + j] != sequence.characterAt(j))
                break;
            if (j == sequenceLength - 1)
                instances++;
        }
    }
    return instances;
}

TEST_F(ContentExtensionTest, StringCombining)
{
    auto extension = InMemoryCompiledContentExtension::create(jsonWithStringsToCombine);
    const auto& data = extension->data();

    ASSERT_EQ(sequenceInstances(data.actions, "AAA"_s), 2);
    ASSERT_EQ(sequenceInstances(data.actions, "GGG"_s), 1);

    ASSERT_EQ(data.actions.size(), 72u);
    ASSERT_EQ(data.urlFilters.size(), 288u);
    ASSERT_EQ(data.topURLFilters.size(), 5u);
    ASSERT_EQ(data.frameURLFilters.size(), 5u);

    auto extensionWithFlags = InMemoryCompiledContentExtension::create("["
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"A\"}},"
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"C\"}},"
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"A\",\"resource-type\":[\"document\"]}},"
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"A\",\"resource-type\":[\"document\",\"font\"]}},"
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"B\",\"resource-type\":[\"document\"]}}"
    "]"_s);
    ASSERT_EQ(sequenceInstances(extensionWithFlags->data().actions, "AAA"_s), 3); // There are 3 sets of unique flags for AAA actions.
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
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^pre0(.?)+post0$\"}}]"_s);

    testRequest(backend, mainDocumentRequest("pre1://webkit.org/post1"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre2://webkit.org/post2"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre3://webkit.org/post3"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre4://webkit.org/post4"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre5://webkit.org/post5"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre6://webkit.org/post6"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre7://webkit.org/post7"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre8://webkit.org/post8"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre9://webkit.org/post9"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("pre0://webkit.org/post0"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("pre1://webkit.org/post2"_s), { });
    testRequest(backend, mainDocumentRequest("pre2://webkit.org/post3"_s), { });
    testRequest(backend, mainDocumentRequest("pre3://webkit.org/post4"_s), { });
    testRequest(backend, mainDocumentRequest("pre4://webkit.org/post5"_s), { });
    testRequest(backend, mainDocumentRequest("pre5://webkit.org/post6"_s), { });
    testRequest(backend, mainDocumentRequest("pre6://webkit.org/post7"_s), { });
    testRequest(backend, mainDocumentRequest("pre7://webkit.org/post8"_s), { });
    testRequest(backend, mainDocumentRequest("pre8://webkit.org/post9"_s), { });
    testRequest(backend, mainDocumentRequest("pre9://webkit.org/post0"_s), { });
    testRequest(backend, mainDocumentRequest("pre0://webkit.org/post1"_s), { });

    testRequest(backend, mainDocumentRequest("pre0://webkit.org/post1"_s), { });
    testRequest(backend, mainDocumentRequest("pre1://webkit.org/post2"_s), { });
    testRequest(backend, mainDocumentRequest("pre2://webkit.org/post3"_s), { });
    testRequest(backend, mainDocumentRequest("pre3://webkit.org/post4"_s), { });
    testRequest(backend, mainDocumentRequest("pre4://webkit.org/post5"_s), { });
    testRequest(backend, mainDocumentRequest("pre5://webkit.org/post6"_s), { });
    testRequest(backend, mainDocumentRequest("pre6://webkit.org/post7"_s), { });
    testRequest(backend, mainDocumentRequest("pre7://webkit.org/post8"_s), { });
    testRequest(backend, mainDocumentRequest("pre8://webkit.org/post9"_s), { });
    testRequest(backend, mainDocumentRequest("pre9://webkit.org/post0"_s), { });
}

TEST_F(ContentExtensionTest, TrailingDotStar)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foo.*$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bar(.*)$\"}}]"_s);

    testRequest(backend, mainDocumentRequest("https://webkit.org/"_s), { });

    testRequest(backend, mainDocumentRequest("foo://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://foo.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.foo/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/foo"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("bar://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://bar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.bar/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
}

TEST_F(ContentExtensionTest, TrailingTermsCarryingNoData)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foob?a?r?\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bazo(ok)?a?$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"cats*$\"}}]"_s);

    testRequest(backend, mainDocumentRequest("https://webkit.org/"_s), { });

    // Anything is fine after foo.
    testRequest(backend, mainDocumentRequest("https://webkit.org/foo"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/foob"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/fooc"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/fooba"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/foobar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/foobar-stuff"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    // Bazooka has to be at the tail without any character not defined by the filter.
    testRequest(backend, mainDocumentRequest("https://webkit.org/baz"_s), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazo"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazoa"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazob"_s), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazoo"_s), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazook"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazookb"_s), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazooka"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/bazookaa"_s), { });

    // The pattern must finish with cat, with any number of 's' following it, but no other character.
    testRequest(backend, mainDocumentRequest("https://cat.org/"_s), { });
    testRequest(backend, mainDocumentRequest("https://cats.org/"_s), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/cat"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/cats"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/catss"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/catsss"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/catso"_s), { });
}

TEST_F(ContentExtensionTest, UselessTermsMatchingEverythingAreEliminated)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern(".*web"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.*)web"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.)*web"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.+)*web"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.?)*web"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.+)?web"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.?)+web"_s, false, 0));

    Vector<ContentExtensions::NFA> nfas = createNFAs(combinedURLFilters);
    EXPECT_EQ(1ul, nfas.size());
    EXPECT_EQ(7ul, nfas.first().nodes.size());

    ContentExtensions::DFA dfa = *ContentExtensions::NFAToDFA::convert(WTFMove(nfas.first()));
    Vector<ContentExtensions::DFABytecode> bytecode;
    ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
    compiler.compile();
    DFABytecodeInterpreter interpreter({ bytecode.data(), bytecode.size() });
    compareContents(interpreter.interpret("eb"_s, 0), { });
    compareContents(interpreter.interpret("we"_s, 0), { });
    compareContents(interpreter.interpret("weeb"_s, 0), { });
    compareContents(interpreter.interpret("web"_s, 0), { 0 });
    compareContents(interpreter.interpret("wweb"_s, 0), { 0 });
    compareContents(interpreter.interpret("wwebb"_s, 0), { 0 });
    compareContents(interpreter.interpret("http://theweb.com/"_s, 0), { 0 });
}

TEST_F(ContentExtensionTest, LoadType)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":[\"third-party\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"whatwg.org\",\"load-type\":[\"first-party\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"alwaysblock.pdf\"}}]"_s);
    
    testRequest(backend, mainDocumentRequest("http://webkit.org"_s), { });
    testRequest(backend, { URL { "http://webkit.org"_str }, URL { "http://not_webkit.org"_str }, URL { "http://not_webkit.org"_str }, ResourceType::Document }, { variantIndex<ContentExtensions::BlockLoadAction> });
        
    testRequest(backend, mainDocumentRequest("http://whatwg.org"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, { URL { "http://whatwg.org"_str }, URL { "http://not_whatwg.org"_str }, URL { "http://not_whatwg.org"_str }, ResourceType::Document }, { });
    
    testRequest(backend, mainDocumentRequest("http://foobar.org/alwaysblock.pdf"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, { URL { "http://foobar.org/alwaysblock.pdf"_str }, URL { "http://not_foobar.org/alwaysblock.pdf"_str }, URL { "http://not_foobar.org/alwaysblock.pdf"_str }, ResourceType::Document }, { variantIndex<ContentExtensions::BlockLoadAction> });
}

TEST_F(ContentExtensionTest, ResourceType)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_all_types.org\",\"resource-type\":[\"document\",\"image\",\"style-sheet\",\"script\",\"font\",\"raw\",\"svg-document\",\"media\",\"popup\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_only_images\",\"resource-type\":[\"image\"]}}]"_s);

    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::Document), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::StyleSheet), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::Script), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::Font), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::Other), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::SVGDocument), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::Media), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_all_types.org"_s, ResourceType::Popup), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_only_images.org"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://block_only_images.org"_s, ResourceType::Document), { });
}

TEST_F(ContentExtensionTest, ResourceAndLoadType)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"BlockOnlyIfThirdPartyAndScript\",\"resource-type\":[\"script\"],\"load-type\":[\"third-party\"]}}]"_s);
    
    testRequest(backend, subResourceRequest("http://webkit.org/BlockOnlyIfThirdPartyAndScript.js"_s, "http://webkit.org"_s, ResourceType::Script), { });
    testRequest(backend, subResourceRequest("http://webkit.org/BlockOnlyIfThirdPartyAndScript.png"_s, "http://not_webkit.org"_s, ResourceType::Image), { });
    testRequest(backend, subResourceRequest("http://webkit.org/BlockOnlyIfThirdPartyAndScript.js"_s, "http://not_webkit.org"_s, ResourceType::Script), { variantIndex<ContentExtensions::BlockLoadAction> });
}

TEST_F(ContentExtensionTest, ResourceOrLoadTypeMatchingEverything)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*\",\"resource-type\":[\"image\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\".*\",\"load-type\":[\"third-party\"]}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\".*\",\"load-type\":[\"first-party\"]}}]"_s);
    
    testRequest(backend, mainDocumentRequest("http://webkit.org"_s), { }, 0);
    testRequest(backend, { URL { "http://webkit.org"_str }, URL { "http://not_webkit.org"_str }, URL { "http://not_webkit.org"_str }, ResourceType::Document }, { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, { URL { "http://webkit.org"_str }, URL { "http://not_webkit.org"_str }, URL { "http://not_webkit.org"_str }, ResourceType::Image }, { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockLoadAction> });
}
    
TEST_F(ContentExtensionTest, WideNFA)
{
    // Make an NFA with about 1400 nodes that won't be combined.
    StringBuilder ruleList;
    ruleList.append('[');
    for (char c1 = 'A'; c1 <= 'Z'; ++c1) {
        for (char c2 = 'A'; c2 <= 'C'; ++c2) {
            for (char c3 = 'A'; c3 <= 'C'; ++c3) {
                if (c1 != 'A' || c2 != 'A' || c3 != 'A')
                    ruleList.append(',');
                ruleList.append("{\"action\":{\"type\":\""_s);
                
                // Make every other rule ignore-previous-rules to not combine actions.
                if (!((c1 + c2 + c3) % 2))
                    ruleList.append("ignore-previous-rules"_s);
                else {
                    ruleList.append("css-display-none"_s);
                    ruleList.append("\",\"selector\":\""_s);
                    ruleList.append(c1);
                    ruleList.append(c2);
                    ruleList.append(c3);
                }
                ruleList.append("\"},\"trigger\":{\"url-filter\":\".*"_s);
                ruleList.append(c1);
                ruleList.append(c2);
                ruleList.append(c3);
                ruleList.append("\", \"url-filter-is-case-sensitive\":true}}"_s);
            }
        }
    }
    ruleList.append(']');
    
    auto backend = makeBackend(ruleList.toString());

    testRequest(backend, mainDocumentRequest("http://webkit.org/AAA"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/YAA"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ZAA"_s), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/LAA/AAA"_s), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/LAA/MAA"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { });
}
    
#ifdef NDEBUG
static uint64_t expectedIndex(char c, unsigned position)
{
    uint64_t index = c - 'A';
    for (unsigned i = 1; i < position; ++i)
        index *= (i == 1) ? ('C' - 'A' + 1) : ('Z' - 'A' + 1);
    return index;
}
#endif

TEST_F(ContentExtensionTest, LargeJumps)
{
// A large test like this is necessary to test 24 and 32 bit jumps, but it's so large it times out in debug builds.
#ifdef NDEBUG
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    
    uint64_t patternId = 0;
    for (char c1 = 'A'; c1 <= 'Z'; ++c1) {
        for (char c2 = 'A'; c2 <= 'Z'; ++c2) {
            for (char c3 = 'A'; c3 <= 'Z'; ++c3) {
                for (char c4 = 'A'; c4 <= 'C'; ++c4) {
                    StringBuilder pattern;
                    pattern.append(c1);
                    pattern.append(c2);
                    pattern.append(c3);
                    pattern.append(c4);
                    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern(pattern.toString(), true, patternId++));
                }
            }
        }
    }
    
    Vector<ContentExtensions::NFA> nfas;
    combinedURLFilters.processNFAs(std::numeric_limits<size_t>::max(), [&](ContentExtensions::NFA&& nfa) {
        nfas.append(WTFMove(nfa));
        return true;
    });
    EXPECT_EQ(nfas.size(), 1ull);
    
    Vector<ContentExtensions::DFA> dfas;
    for (auto&& nfa : WTFMove(nfas))
        dfas.append(*ContentExtensions::NFAToDFA::convert(WTFMove(nfa)));
    EXPECT_EQ(dfas.size(), 1ull);
    
    Vector<ContentExtensions::DFABytecode> combinedBytecode;
    for (const auto& dfa : dfas) {
        Vector<ContentExtensions::DFABytecode> bytecode;
        ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
        compiler.compile();
        combinedBytecode.appendVector(bytecode);
    }
    
    DFABytecodeInterpreter interpreter({ combinedBytecode.data(), combinedBytecode.size() });
    
    patternId = 0;
    for (char c1 = 'A'; c1 <= 'Z'; ++c1) {
        for (char c2 = 'A'; c2 <= 'Z'; ++c2) {
            for (char c3 = 'A'; c3 <= 'Z'; ++c3) {
                for (char c4 = 'A'; c4 <= 'C'; ++c4) {
                    StringBuilder pattern;
                    pattern.append(c1);
                    pattern.append(c2);
                    pattern.append(c3);
                    // Test different jumping patterns distributed throughout the DFA:
                    switch ((c1 + c2 + c3 + c4) % 4) {
                    case 0:
                        // This should not match.
                        pattern.append('x');
                        pattern.append(c4);
                        break;
                    case 1:
                        // This should jump back to the root, then match.
                        pattern.append('x');
                        pattern.append(c1);
                        pattern.append(c2);
                        pattern.append(c3);
                        pattern.append(c4);
                        break;
                    case 2:
                        // This should match at the end of the string.
                        pattern.append(c4);
                        break;
                    case 3:
                        // This should match then jump back to the root.
                        pattern.append(c4);
                        pattern.append('x');
                        break;
                    }
                    auto matches = interpreter.interpret(pattern.toString(), 0);
                    switch ((c1 + c2 + c3 + c4) % 4) {
                    case 0:
                        compareContents(matches, { });
                        break;
                    case 1:
                    case 2:
                    case 3:
                        compareContents(matches, {patternId});
                        break;
                    }
                    patternId++;
                }
            }
        }
    }

    compareContents(interpreter.interpret("CAAAAx"_s, 0), {expectedIndex('C', 4), expectedIndex('A', 1)});
    compareContents(interpreter.interpret("KAAAAx"_s, 0), {expectedIndex('K', 4), expectedIndex('A', 1)});
    compareContents(interpreter.interpret("AKAAAx"_s, 0), {expectedIndex('K', 3), expectedIndex('K', 4)});
    compareContents(interpreter.interpret("AKxAAAAx"_s, 0), {expectedIndex('A', 1)});
    compareContents(interpreter.interpret("AKAxAAAAx"_s, 0), {expectedIndex('A', 1)});
    compareContents(interpreter.interpret("AKAxZKAxZKZxAAAAx"_s, 0), {expectedIndex('A', 1)});
    compareContents(interpreter.interpret("ZAAAA"_s, 0), {expectedIndex('Z', 4), expectedIndex('A', 1)});
    compareContents(interpreter.interpret("ZZxZAAAB"_s, 0), {expectedIndex('Z', 4), expectedIndex('B', 1)});
#endif
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
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern(lotsOfAs.toString(), false, 0));
    
    // FIXME: Yarr ought to be able to handle 2MB regular expressions.
    StringBuilder tooManyAs;
    for (unsigned i = 0; i < size * 20; ++i)
        tooManyAs.append('A');
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::YarrError, parser.addPattern(tooManyAs.toString(), false, 0));
    
    StringBuilder nestedGroups;
    for (unsigned i = 0; i < size; ++i)
        nestedGroups.append('(');
    for (unsigned i = 0; i < size; ++i)
        nestedGroups.append("B)"_s);
    // FIXME: Add nestedGroups. Right now it also takes too long. It should be optimized.
    
    // This should not crash and not timeout.
    EXPECT_EQ(1ul, createNFAs(combinedURLFilters).size());
}

void checkCompilerError(String&& json, std::error_code expectedError)
{
    CompiledContentExtensionData extensionData;
    InMemoryContentExtensionCompilationClient client(extensionData);
    auto parsedRules = ContentExtensions::parseRuleList(json);
    std::error_code compilerError;
    if (parsedRules.has_value())
        compilerError = ContentExtensions::compileRuleList(client, WTFMove(json), WTFMove(parsedRules.value()));
    else
        compilerError = parsedRules.error();
    EXPECT_EQ(compilerError.value(), expectedError.value());
    if (compilerError.value())
        EXPECT_STREQ(compilerError.category().name(), expectedError.category().name());
}

TEST_F(ContentExtensionTest, MatchesEverything)
{
    // Only css-display-none rules with triggers that match everything, no domain rules, and no flags
    // should go in the global display:none stylesheet. css-display-none rules with domain rules or flags
    // are applied separately on pages where they apply.
    auto backend1 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]"_s);
    EXPECT_TRUE(nullptr != backend1.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend1, mainDocumentRequest("http://webkit.org"_s), { }); // Selector is in global stylesheet.
    
    auto backend2 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"if-domain\":[\"webkit.org\"]}}]"_s);
    EXPECT_EQ(nullptr, backend2.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend2, mainDocumentRequest("http://webkit.org"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(backend2, mainDocumentRequest("http://w3c.org"_s), { });
    
    auto backend3 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"unless-domain\":[\"webkit.org\"]}}]"_s);
    EXPECT_EQ(nullptr, backend3.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend3, mainDocumentRequest("http://webkit.org"_s), { });
    testRequest(backend3, mainDocumentRequest("http://w3c.org"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    
    auto backend4 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"load-type\":[\"third-party\"]}}]"_s);
    EXPECT_EQ(nullptr, backend4.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend4, mainDocumentRequest("http://webkit.org"_s), { });
    testRequest(backend4, subResourceRequest("http://not_webkit.org"_s, "http://webkit.org"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });

    // css-display-none rules after ignore-previous-rules should not be put in the default stylesheet.
    auto backend5 = makeBackend("[{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\".*\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]"_s);
    EXPECT_EQ(nullptr, backend5.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend5, mainDocumentRequest("http://webkit.org"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> }, 0);
    
    auto backend6 = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*\",\"if-domain\":[\"webkit.org\",\"*w3c.org\"],\"resource-type\":[\"document\",\"script\"]}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore\",\"if-domain\":[\"*webkit.org\",\"w3c.org\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\".*\",\"unless-domain\":[\"webkit.org\",\"whatwg.org\"],\"resource-type\":[\"script\",\"image\"],\"load-type\":[\"third-party\"]}}]"_s);
    EXPECT_EQ(nullptr, backend6.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend6, mainDocumentRequest("http://webkit.org"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend6, mainDocumentRequest("http://w3c.org"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend6, mainDocumentRequest("http://whatwg.org"_s), { });
    testRequest(backend6, mainDocumentRequest("http://sub.webkit.org"_s), { });
    testRequest(backend6, mainDocumentRequest("http://sub.w3c.org"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend6, mainDocumentRequest("http://sub.whatwg.org"_s), { });
    testRequest(backend6, mainDocumentRequest("http://webkit.org/ignore"_s), { }, 0);
    testRequest(backend6, mainDocumentRequest("http://w3c.org/ignore"_s), { }, 0);
    testRequest(backend6, mainDocumentRequest("http://whatwg.org/ignore"_s), { });
    testRequest(backend6, mainDocumentRequest("http://sub.webkit.org/ignore"_s), { }, 0);
    testRequest(backend6, mainDocumentRequest("http://sub.w3c.org/ignore"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend6, mainDocumentRequest("http://sub.whatwg.org/ignore"_s), { });
    testRequest(backend6, subResourceRequest("http://example.com/image.png"_s, "http://webkit.org/"_s, ResourceType::Image), { });
    testRequest(backend6, subResourceRequest("http://example.com/image.png"_s, "http://w3c.org/"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend6, subResourceRequest("http://example.com/doc.html"_s, "http://webkit.org/"_s, ResourceType::Document), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend6, subResourceRequest("http://example.com/script.js"_s, "http://webkit.org/"_s, ResourceType::Script), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend6, subResourceRequest("http://example.com/script.js"_s, "http://w3c.org/"_s, ResourceType::Script), { variantIndex<ContentExtensions::BlockCookiesAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend6, subResourceRequest("http://example.com/script.js"_s, "http://example.com/"_s, ResourceType::Script), { });
    testRequest(backend6, subResourceRequest("http://example.com/ignore/image.png"_s, "http://webkit.org/"_s, ResourceType::Image), { }, 0);
    testRequest(backend6, subResourceRequest("http://example.com/ignore/image.png"_s, "http://example.com/"_s, ResourceType::Image), { });
    testRequest(backend6, subResourceRequest("http://example.com/ignore/image.png"_s, "http://example.org/"_s, ResourceType::Image), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend6, subResourceRequest("http://example.com/doc.html"_s, "http://example.org/"_s, ResourceType::Document), { });
    testRequest(backend6, subResourceRequest("http://example.com/"_s, "http://example.com/"_s, ResourceType::Font), { });
    testRequest(backend6, subResourceRequest("http://example.com/ignore"_s, "http://webkit.org/"_s, ResourceType::Image), { }, 0);
    testRequest(backend6, subResourceRequest("http://example.com/ignore"_s, "http://webkit.org/"_s, ResourceType::Font), { }, 0);
    testRequest(backend6, subResourceRequest("http://example.com/"_s, "http://example.com/"_s, ResourceType::Script), { });
    testRequest(backend6, subResourceRequest("http://example.com/ignore"_s, "http://example.com/"_s, ResourceType::Script), { });
}
    
TEST_F(ContentExtensionTest, InvalidJSON)
{
    checkCompilerError("["_s, ContentExtensionError::JSONInvalid);
    checkCompilerError("123"_s, ContentExtensionError::JSONTopLevelStructureNotAnArray);
    checkCompilerError("{}"_s, ContentExtensionError::JSONTopLevelStructureNotAnArray);
    checkCompilerError("[5]"_s, ContentExtensionError::JSONInvalidRule);
    checkCompilerError("[]"_s, ContentExtensionError::JSONContainsNoRules);

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":5}]"_s,
        ContentExtensionError::JSONInvalidTrigger);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"\"}}]"_s,
        ContentExtensionError::JSONInvalidURLFilterInTrigger);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":{}}}]"_s,
        ContentExtensionError::JSONInvalidURLFilterInTrigger);

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":{}}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":[\"invalid\"]}}]"_s,
        ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":[5]}}]"_s,
        ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":5}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":\"first-party\"}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":null}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":false}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":{}}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":[\"invalid\"]}}]"_s,
        ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":[5]}}]"_s,
        ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":5}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":\"document\"}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":null}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":false}}]"_s,
        ContentExtensionError::JSONInvalidTriggerFlagsArray);
    
    checkCompilerError("[{\"action\":{\"type\":\"notify\"},\"trigger\":{\"url-filter\":\".*\"}}]"_s, ContentExtensionError::JSONInvalidNotification);
    checkCompilerError("[{\"action\":{\"type\":\"notify\",\"notification\":5},\"trigger\":{\"url-filter\":\".*\"}}]"_s, ContentExtensionError::JSONInvalidNotification);
    checkCompilerError("[{\"action\":{\"type\":\"notify\",\"notification\":[]},\"trigger\":{\"url-filter\":\".*\"}}]"_s, ContentExtensionError::JSONInvalidNotification);
    checkCompilerError("[{\"action\":{\"type\":\"notify\",\"notification\":\"here's my notification\"},\"trigger\":{\"url-filter\":\".*\"}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"notify\",\"notification\":\"\\u1234\"},\"trigger\":{\"url-filter\":\".*\"}}]"_s, { });
    
    StringBuilder rules;
    rules.append("["_s);
    for (unsigned i = 0; i < 149999; ++i)
        rules.append("{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"a\"}},"_s);
    String rules150000 = makeString(rules.toString(), "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"a\"}}]"_s);
    String rules150001 = makeString(rules.toString(), "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"a\"}},{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"a\"}}]"_s);
    checkCompilerError(WTFMove(rules150000), { });
    checkCompilerError(WTFMove(rules150001), ContentExtensionError::JSONTooManyRules);
    
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":{}}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[5]}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"]}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":\"a\"}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":false}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":null}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":{}}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[5]}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"\"]}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":\"a\"}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":null}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":false}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"A\"]}}]"_s, ContentExtensionError::JSONDomainNotLowerCaseASCII);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"\\u00DC\"]}}]"_s, ContentExtensionError::JSONDomainNotLowerCaseASCII);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"0\"]}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"a\"]}}]"_s, { });

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[],\"unless-domain\":[\"a\"]}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[]}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":5}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":5}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":5,\"unless-domain\":5}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[]}}]"_s, ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"],\"unless-domain\":[]}}]"_s, ContentExtensionError::JSONMultipleConditions);

    checkCompilerError("[{\"action\":5,\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s,
        ContentExtensionError::JSONInvalidAction);
    checkCompilerError("[{\"action\":{\"type\":\"invalid\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s,
        ContentExtensionError::JSONInvalidActionType);
    checkCompilerError("[{\"action\":{\"type\":\"css-display-none\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s,
        ContentExtensionError::JSONInvalidCSSDisplayNoneActionType);

    checkCompilerError("[{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"webkit.org\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*\",\"if-domain\":[\"a\"]}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*\",\"unless-domain\":[\"a\"]}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[\"}}]"_s,
        ContentExtensionError::JSONInvalidRegex);

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"],\"unless-domain\":[\"a\"]}}]"_s, ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"],\"unless-top-url\":[]}}]"_s, ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"],\"if-frame-url\":[]}}]"_s, ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"],\"unless-top-url\":[\"a\"]}}]"_s, ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"],\"if-top-url\":[\"a\"]}}]"_s, ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[],\"unless-domain\":[\"a\"]}}]"_s, ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"],\"if-domain\":[\"a\"]}}]"_s, ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"]}}, {\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"]}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"]}}, {\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"a\"]}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-top-url\":[\"[\"]}}]"_s, ContentExtensionError::JSONInvalidRegex);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\", \"unexpected-identifier-should-be-ignored\":5}}]"_s, { });

    checkCompilerError("[{\"action\":{\"type\":\"redirect\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectMissing);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectInvalidType);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"query-transform\":{\"remove-parameters\":5}}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRemoveParametersNotStringArray);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"query-transform\":{\"remove-parameters\":[5]}}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRemoveParametersNotStringArray);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"query-transform\":{\"add-or-replace-parameters\":5}}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONAddOrReplaceParametersNotArray);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"query-transform\":{\"add-or-replace-parameters\":[5]}}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONAddOrReplaceParametersKeyValueNotADictionary);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"query-transform\":{\"add-or-replace-parameters\":[{}]}}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONAddOrReplaceParametersKeyValueMissingKeyString);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"query-transform\":{\"add-or-replace-parameters\":[{\"key\":5}]}}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONAddOrReplaceParametersKeyValueMissingKeyString);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"query-transform\":{\"add-or-replace-parameters\":[{\"key\":\"k\",\"value\":5}]}}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONAddOrReplaceParametersKeyValueMissingValueString);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"url\":\"https://127..1/\"}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectURLInvalid);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"url\":\"JaVaScRiPt:dostuff\"}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectToJavaScriptURL);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"port\":\"not a number\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectInvalidPort);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"port\":\"65536\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectInvalidPort);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"query\":\"no-question-mark\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectInvalidQuery);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"fragment\":\"no-number-sign\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectInvalidFragment);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"port\":\"65535\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"port\":\"\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"extension-path\":\"/does/start/with/slash/\"}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, { });
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"scheme\":\"!@#$%\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectURLSchemeInvalid);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"scheme\":\"JaVaScRiPt\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONRedirectToJavaScriptURL);
    checkCompilerError("[{\"action\":{\"type\":\"redirect\",\"redirect\":{\"transform\":{\"scheme\":\"About\"}}},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, { });

    checkCompilerError("[{\"action\":{\"type\":\"modify-headers\",\"priority\":2,\"request-headers\":5},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONModifyHeadersNotArray);
    checkCompilerError("[{\"action\":{\"type\":\"modify-headers\",\"priority\":2,\"request-headers\":[5]},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONModifyHeadersInfoNotADictionary);
    checkCompilerError("[{\"action\":{\"type\":\"modify-headers\",\"priority\":2,\"request-headers\":[{}]},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONModifyHeadersMissingOperation);
    checkCompilerError("[{\"action\":{\"type\":\"modify-headers\",\"priority\":2,\"request-headers\":[{\"operation\":\"remove\"}]},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONModifyHeadersMissingHeader);
    checkCompilerError("[{\"action\":{\"type\":\"modify-headers\",\"priority\":2,\"request-headers\":[{\"operation\":\"set\",\"header\":\"testheader\"}]},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONModifyHeadersMissingValue);
    checkCompilerError("[{\"action\":{\"type\":\"modify-headers\",\"priority\":2,\"request-headers\":[{\"operation\":\"invalid\",\"header\":\"testheader\"}]},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONModifyHeadersInvalidOperation);
    checkCompilerError("[{\"action\":{\"type\":\"modify-headers\",\"priority\":\"\",\"request-headers\":[{\"operation\":\"remove\",\"header\":\"testheader\"}]},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONModifyHeadersInvalidPriority);
    checkCompilerError("[{\"action\":{\"type\":\"modify-headers\",\"priority\":-1,\"request-headers\":[{\"operation\":\"remove\",\"header\":\"testheader\"}]},\"trigger\":{\"url-filter\":\"webkit.org\"}}]"_s, ContentExtensionError::JSONModifyHeadersInvalidPriority);
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines1)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^.*foo\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bar$\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[ab]+bang\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("foo://webkit.org/bar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bar://webkit.org/bar"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("abang://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bbang://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("cbang://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bang"_s), { });
    testRequest(backend, mainDocumentRequest("bang://webkit.org/"_s), { });
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines1Partitioning)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);

    // Those two share a prefix.
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^.*foo"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("bar$"_s, false, 1));

    // Not this one.
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^[ab]+bang"_s, false, 0));

    EXPECT_EQ(2ul, createNFAs(combinedURLFilters).size());
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines2)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^foo\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^.*[a-c]+bar\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^webkit:\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-c]+b+oom\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("foo://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("webkit://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("http://bar.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://abar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://bbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://cbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://abcbar.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://dbar.org/"_s), { });
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines2Partitioning)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);

    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^foo"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^.*[a-c]+bar"_s, false, 1));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("^webkit:"_s, false, 2));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("[a-c]+b+oom"_s, false, 3));

    // "^foo" and "^webkit:" can be grouped, the other two have a variable prefix.
    EXPECT_EQ(3ul, createNFAs(combinedURLFilters).size());
}

TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines3)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"A*D\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"A*BA+\"}},"
        "{\"action\":{\"type\":\"make-https\"},\"trigger\":{\"url-filter\":\"A*BC\"}}]"_s);
    
    testRequest(backend, mainDocumentRequest("http://webkit.org/D"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/AAD"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/AB"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABA"_s), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABAD"_s), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/BC"_s), { variantIndex<ContentExtensions::MakeHTTPSAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABC"_s), { variantIndex<ContentExtensions::MakeHTTPSAction> });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABABC"_s), { variantIndex<ContentExtensions::MakeHTTPSAction> }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABABCAD"_s), { variantIndex<ContentExtensions::MakeHTTPSAction> }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABCAD"_s), { variantIndex<ContentExtensions::MakeHTTPSAction>, variantIndex<ContentExtensions::BlockLoadAction> });
}
    
TEST_F(ContentExtensionTest, StrictPrefixSeparatedMachines3Partitioning)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A*D"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A*BA+"_s, false, 1));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A*BC"_s, false, 2));
    
    // "A*A" and "A*BC" can be grouped, "A*BA+" should not.
    EXPECT_EQ(2ul, createNFAs(combinedURLFilters).size());
}

TEST_F(ContentExtensionTest, SplittingLargeNFAs)
{
    const size_t expectedNFACounts[16] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 1, 1, 1};
    
    for (size_t i = 0; i < 16; i++) {
        ContentExtensions::CombinedURLFilters combinedURLFilters;
        ContentExtensions::URLFilterParser parser(combinedURLFilters);
        
        EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A+BBB"_s, false, 1));
        EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A+CCC"_s, false, 2));
        EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("A+DDD"_s, false, 2));
        
        Vector<ContentExtensions::NFA> nfas;
        combinedURLFilters.processNFAs(i, [&](ContentExtensions::NFA&& nfa) {
            nfas.append(WTFMove(nfa));
            return true;
        });
        EXPECT_EQ(nfas.size(), expectedNFACounts[i]);
        
        Vector<ContentExtensions::DFA> dfas;
        for (auto& nfa : WTFMove(nfas))
            dfas.append(*ContentExtensions::NFAToDFA::convert(WTFMove(nfa)));
        
        Vector<ContentExtensions::DFABytecode> combinedBytecode;
        for (const auto& dfa : dfas) {
            Vector<ContentExtensions::DFABytecode> bytecode;
            ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
            compiler.compile();
            combinedBytecode.appendVector(bytecode);
        }
        
        DFABytecodeInterpreter interpreter({ combinedBytecode.data(), combinedBytecode.size() });
        
        EXPECT_EQ(interpreter.interpret("ABBBX"_s, 0).size(), 1ull);
        EXPECT_EQ(interpreter.interpret("ACCCX"_s, 0).size(), 1ull);
        EXPECT_EQ(interpreter.interpret("ADDDX"_s, 0).size(), 1ull);
        EXPECT_EQ(interpreter.interpret("XBBBX"_s, 0).size(), 0ull);
        EXPECT_EQ(interpreter.interpret("ABBX"_s, 0).size(), 0ull);
        EXPECT_EQ(interpreter.interpret("ACCX"_s, 0).size(), 0ull);
        EXPECT_EQ(interpreter.interpret("ADDX"_s, 0).size(), 0ull);
    }
}
    
TEST_F(ContentExtensionTest, QuantifierInGroup)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A+)B)C)"_s, false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A)B+)C)"_s, false, 1));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A)B+)C)D"_s, false, 2));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A)B)C+)"_s, false, 3));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(((A)B)C)"_s, false, 4));
    
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
    testPatternStatus("a*b?.*.?[a-z]?[a-z]*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("a*b?.*.?[a-z]?[a-z]+"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("a*b?.*.?[a-z]?[a-z]"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus(".*?a"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus(".*a"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    
    testPatternStatus("(?!)"_s, ContentExtensions::URLFilterParser::ParseStatus::Group);
    testPatternStatus("(?=)"_s, ContentExtensions::URLFilterParser::ParseStatus::Group);
    testPatternStatus("(?!a)"_s, ContentExtensions::URLFilterParser::ParseStatus::Group);
    testPatternStatus("(?=a)"_s, ContentExtensions::URLFilterParser::ParseStatus::Group);
    testPatternStatus("(regex)"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("(regex"_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("((regex)"_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("(?:regex)"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("(?:regex"_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("[^.]+"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    
    testPatternStatus("a++"_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("[a]++"_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("+"_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    
    testPatternStatus("["_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("[a}"_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    
    testPatternStatus("a]"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{[a]"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{0"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{0,"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("{0,1"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("a{0,1"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("a{a,b}"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);

    const char nonASCII[2] = {-1, '\0'};
    testPatternStatus(String::fromLatin1(nonASCII), ContentExtensions::URLFilterParser::ParseStatus::NonASCII);
    testPatternStatus("\\xff"_s, ContentExtensions::URLFilterParser::ParseStatus::NonASCII);
    
    testPatternStatus("\\x\\r\\n"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("\\b"_s, ContentExtensions::URLFilterParser::ParseStatus::WordBoundary);
    testPatternStatus("[\\d]"_s, ContentExtensions::URLFilterParser::ParseStatus::AtomCharacter);
    testPatternStatus("\\d\\D\\w\\s\\v\\h\\i\\c"_s, ContentExtensions::URLFilterParser::ParseStatus::UnsupportedCharacterClass);
    
    testPatternStatus("this|that"_s, ContentExtensions::URLFilterParser::ParseStatus::Disjunction);
    testPatternStatus("a{0,1}b"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok);
    testPatternStatus("a{0,2}b"_s, ContentExtensions::URLFilterParser::ParseStatus::InvalidQuantifier);
    testPatternStatus(""_s, ContentExtensions::URLFilterParser::ParseStatus::EmptyPattern);
    testPatternStatus("$$"_s, ContentExtensions::URLFilterParser::ParseStatus::MisplacedEndOfLine);
    testPatternStatus("a^"_s, ContentExtensions::URLFilterParser::ParseStatus::MisplacedStartOfLine);
    testPatternStatus("(^)"_s, ContentExtensions::URLFilterParser::ParseStatus::MisplacedStartOfLine);
    
    testPatternStatus("(a)\\1"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok); // Back references are disabled, it parse as octal 1
    testPatternStatus("(<A>a)\\k<A>"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok); // Named back references aren't handled, it parse as "k<A>"
    testPatternStatus("(?<A>a)\\k<A>"_s, ContentExtensions::URLFilterParser::ParseStatus::BackReference);
    testPatternStatus("\\1(a)"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok); // Forward references are disabled, it parse as octal 1
    testPatternStatus("\\8(a)"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok); // Forward references are disabled, it parse as '8'
    testPatternStatus("\\9(a)"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok); // Forward references are disabled, it parse as '9'
    testPatternStatus("\\k<A>(<A>a)"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok); // Named forward references aren't handled, it parse as "k<A>"
    testPatternStatus("\\k<A>(?<A>a)"_s, ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("\\k<A>(a)"_s, ContentExtensions::URLFilterParser::ParseStatus::Ok); // Unmatched named forward references aren't handled, it parse as "k<A>"
}

TEST_F(ContentExtensionTest, PatternMatchingTheEmptyString)
{
    // Simple atoms.
    testPatternStatus(".*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("a*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus(".?"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("a?"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);

    // Character sets.
    testPatternStatus("[a-z]*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("[a-z]?"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);

    // Groups.
    testPatternStatus("(foobar)*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(foobar)?"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.*)"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(a*)"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.?)"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(a?)"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("([a-z]*)"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("([a-z]?)"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);

    testPatternStatus("(.)*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.+)*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.?)*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.*)*"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.+)?"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
    testPatternStatus("(.?)+"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);

    // Nested groups.
    testPatternStatus("((foo)?((.)*)(bar)*)"_s, ContentExtensions::URLFilterParser::ParseStatus::MatchesEverything);
}

TEST_F(ContentExtensionTest, MinimizingWithMoreFinalStatesThanNonFinalStates)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^h[a-z://]+\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://foo.com/\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://bar.com/\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://foo.com/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("http://bar.com/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("attp://foo.com/"_s), { });
    testRequest(backend, mainDocumentRequest("attp://bar.com/"_s), { });

    testRequest(backend, mainDocumentRequest("http://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bttp://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bttps://webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/b"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://webkit.org/b"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("cttp://webkit.org/B"_s), { });
    testRequest(backend, mainDocumentRequest("cttps://webkit.org/B"_s), { });
}

TEST_F(ContentExtensionTest, StatesWithDifferentActionsAreNotUnified1)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://www.webkit.org/\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"^https://www.webkit.org/\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"^attps://www.webkit.org/\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("attps://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://www.webkit.org/a"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://www.webkit.org/B"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("attps://www.webkit.org/c"_s), { variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("http://www.whatwg.org/"_s), { });
    testRequest(backend, mainDocumentRequest("https://www.whatwg.org/"_s), { });
    testRequest(backend, mainDocumentRequest("attps://www.whatwg.org/"_s), { });
}

TEST_F(ContentExtensionTest, StatesWithDifferentActionsAreNotUnified2)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://www.webkit.org/\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"^https://www.webkit.org/\"}},"
        "{\"action\":{\"type\":\"css-display-none\", \"selector\":\"#foo\"},\"trigger\":{\"url-filter\":\"^https://www.webkit.org/\"}}]"_s);

    testRequest(backend, mainDocumentRequest("http://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("https://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockCookiesAction> });
    testRequest(backend, mainDocumentRequest("https://www.whatwg.org/"_s), { });
    testRequest(backend, mainDocumentRequest("attps://www.whatwg.org/"_s), { });
}

// The order in which transitions from the root will be processed is unpredictable.
// To exercises the various options, this test exists in various version exchanging the transition to the final state.
TEST_F(ContentExtensionTest, FallbackTransitionsWithDifferentiatorDoNotMerge1)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.a\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.a\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bac\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bbc\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^BCC\"}}]"_s);

    testRequest(backend, mainDocumentRequest("aza://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bac://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bbc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bcc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("aac://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("abc://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("acc://www.webkit.org/"_s), { });

    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"_s), { });
}
TEST_F(ContentExtensionTest, FallbackTransitionsWithDifferentiatorDoNotMerge2)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bac\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bbc\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^BCC\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.a\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.a\"}}]"_s);

    testRequest(backend, mainDocumentRequest("aza://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bac://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bbc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bcc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("aac://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("abc://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("acc://www.webkit.org/"_s), { });

    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"_s), { });
}
TEST_F(ContentExtensionTest, FallbackTransitionsWithDifferentiatorDoNotMerge3)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.c\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.c\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^baa\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bba\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^BCA\"}}]"_s);

    testRequest(backend, mainDocumentRequest("azc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("baa://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bba://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bca://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("aaa://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("aba://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("aca://www.webkit.org/"_s), { });

    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"_s), { });
}
TEST_F(ContentExtensionTest, FallbackTransitionsWithDifferentiatorDoNotMerge4)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^baa\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^bba\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^BCA\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.c\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.c\"}}]"_s);

    testRequest(backend, mainDocumentRequest("azc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bzc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("baa://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bba://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bca://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("aaa://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("aba://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("aca://www.webkit.org/"_s), { });

    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bza://www.webkit.org/"_s), { });
}

TEST_F(ContentExtensionTest, FallbackTransitionsToOtherNodeInSameGroupDoesNotDifferentiateGroup)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^aac\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.c\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^b.c\"}}]"_s);

    testRequest(backend, mainDocumentRequest("aac://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("abc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("bac://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("abc://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("aaa://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("aca://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("baa://www.webkit.org/"_s), { });
}

TEST_F(ContentExtensionTest, SimpleFallBackTransitionDifferentiator1)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.bc.de\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^a.bd.ef\"}}]"_s);

    testRequest(backend, mainDocumentRequest("abbccde://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("aabcdde://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("aabddef://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("aabddef://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("abcde://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("abdef://www.webkit.org/"_s), { });
}

TEST_F(ContentExtensionTest, SimpleFallBackTransitionDifferentiator2)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^cb.\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^db.b\"}}]"_s);

    testRequest(backend, mainDocumentRequest("cba://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("cbb://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("dbab://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(backend, mainDocumentRequest("dbxb://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });

    testRequest(backend, mainDocumentRequest("cca://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("dddd://www.webkit.org/"_s), { });
    testRequest(backend, mainDocumentRequest("bbbb://www.webkit.org/"_s), { });
}

// *** We have the following ranges intersections: ***
// Full overlap.
// 1)
// A: |-----|
// B:  |---|
// 2)
// A: |-----|
// B:    |
// 3)
// A:  |---|
// B: |-----|
// 4)
// A:    |
// B: |-----|
// One edge in common
// 5)
// A: |-|
// B:   |-|
// 6)
// A: |
// B: |-|
// 7)
// A: |-|
// B:   |
// 8)
// A:   |-|
// B: |-|
// 9)
// A:   |
// B: |-|
// 10)
// A: |-|
// B: |
// B overlap on the left side of A.
// 11)
// A:   |---|
// B: |---|
// 12)
// A:   |---|
// B: |-----|
// A overlap on the left side of B
// 13)
// A: |---|
// B:   |---|
// 14)
// A: |-----|
// B:   |---|
// Equal ranges
// 15)
// A: |---|
// B: |---|
// 16)
// A: |
// B: |

TEST_F(ContentExtensionTest, RangeOverlapCase1)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"^[c-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"[c-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase2)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"^b\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"l\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.k.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.l.xxx/"_s), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase3)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[a-m]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[a-m]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase4)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^l\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[a-m]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("k://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("l://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"l\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[a-m]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.k.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.l.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase5)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[e-h]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[e-h]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase6)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[e-h]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[e-h]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase7)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^e\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"e\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase8)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[a-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[a-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase9)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[a-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[a-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase10)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^e\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"e\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase11)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[d-f]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[d-f]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { });
}


TEST_F(ContentExtensionTest, RangeOverlapCase12)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[d-g]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[d-g]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase13)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[c-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[c-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase14)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[b-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[c-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[b-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[c-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase15)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[c-f]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[c-f]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[c-f]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[c-f]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase16)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^c\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^c\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"c\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"c\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, QuantifiedOneOrMoreRangesCase11And13)
{
    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[c-e]+[g-i]+YYY\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[b-d]+[h-j]+YYY\"}}]"_s);

    // The interesting ranges only match between 'b' and 'k', plus a gap in 'f'.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.abyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.acyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.adyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aeyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.afyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.agyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ahyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aiyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ajyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.akyyy.xxx/"_s), { });

    // 'b' is the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.byyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bbyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bcyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bdyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.beyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bfyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bgyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.biyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bkyyy.xxx/"_s), { });

    // 'c' is the first character of the first rule, and it overlaps the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cbyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ccyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cdyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ceyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cfyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.chyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ciyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ckyyy.xxx/"_s), { });

    // 'd' is in the first range of both rule. This series cover overlaps between the two rules.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.degyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dehyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfgyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfhyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.djyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
}

TEST_F(ContentExtensionTest, QuantifiedOneOrMoreRangesCase11And13InGroups)
{
    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"([c-e])+([g-i]+YYY)\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[b-d]+[h-j]+YYY\"}}]"_s);

    // The interesting ranges only match between 'b' and 'k', plus a gap in 'f'.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.abyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.acyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.adyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aeyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.afyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.agyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ahyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aiyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ajyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.akyyy.xxx/"_s), { });

    // 'b' is the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.byyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bbyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bcyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bdyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.beyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bfyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bgyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.biyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bkyyy.xxx/"_s), { });

    // 'c' is the first character of the first rule, and it overlaps the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cbyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ccyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cdyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ceyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cfyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.chyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ciyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ckyyy.xxx/"_s), { });

    // 'd' is in the first range of both rule. This series cover overlaps between the two rules.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.degyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dehyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfgyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfhyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.djyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase1)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"^(bar)*[c-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"(bar)*[c-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"_s), { });
}


TEST_F(ContentExtensionTest, CombinedRangeOverlapCase2)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"^(bar)*b\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"(bar)*l\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.k.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.l.xxx/"_s), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase3)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[a-m]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[a-m]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase4)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*l\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[a-m]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("k://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("l://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*l\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[a-m]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.k.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.l.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase5)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[e-h]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[e-h]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase6)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[e-h]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[e-h]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase7)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*e\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*e\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase8)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[a-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[a-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase9)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[a-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[a-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase10)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*e\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*e\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase11)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[d-f]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[d-f]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { });
}


TEST_F(ContentExtensionTest, CombinedRangeOverlapCase12)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[d-g]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[d-g]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase13)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[c-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[c-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase14)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[b-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[c-e]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[b-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[c-e]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase15)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[c-f]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[c-f]\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[c-f]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[c-f]\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase16)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*c\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*c\"}}]"_s);

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"_s), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"_s), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*c\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*c\"}}]"_s);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"_s), { });
}

TEST_F(ContentExtensionTest, CombinedQuantifiedOneOrMoreRangesCase11And13)
{
    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[c-e]+[g-i]+YYY\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[b-d]+[h-j]+YYY\"}}]"_s);

    // The interesting ranges only match between 'b' and 'k', plus a gap in 'f'.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.abyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.acyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.adyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aeyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.afyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.agyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ahyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aiyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ajyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.akyyy.xxx/"_s), { });

    // 'b' is the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.byyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bbyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bcyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bdyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.beyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bfyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bgyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.biyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bkyyy.xxx/"_s), { });

    // 'c' is the first character of the first rule, and it overlaps the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cbyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ccyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cdyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ceyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cfyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.chyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ciyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ckyyy.xxx/"_s), { });

    // 'd' is in the first range of both rule. This series cover overlaps between the two rules.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.degyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dehyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfgyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfhyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.djyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
}

TEST_F(ContentExtensionTest, CombinedQuantifiedOneOrMoreRangesCase11And13InGroups)
{
    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*([c-e])+([g-i]+YYY)\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[b-d]+[h-j]+YYY\"}}]"_s);

    // The interesting ranges only match between 'b' and 'k', plus a gap in 'f'.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.abyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.acyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.adyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aeyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.afyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.agyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ahyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aiyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ajyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.akyyy.xxx/"_s), { });

    // 'b' is the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.byyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bbyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bcyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bdyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.beyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bfyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bgyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.biyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bkyyy.xxx/"_s), { });

    // 'c' is the first character of the first rule, and it overlaps the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cayyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cbyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ccyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cdyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ceyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cfyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.chyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ciyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ckyyy.xxx/"_s), { });

    // 'd' is in the first range of both rule. This series cover overlaps between the two rules.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddgyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhhyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction>, variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.degyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dehyyy.xxx/"_s), { variantIndex<ContentExtensions::BlockLoadAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfgyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfhyyy.xxx/"_s), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.djyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjjyyy.xxx/"_s), { variantIndex<ContentExtensions::CSSDisplayNoneSelectorAction> });
}

TEST_F(ContentExtensionTest, ValidSelector)
{
    EXPECT_TRUE(WebCore::ContentExtensions::isValidCSSSelector("a[href*=hsv]"_s));
}

TEST_F(ContentExtensionTest, Serialization)
{
    auto checkRedirectActionSerialization = [] (const RedirectAction& action, size_t expectedSerializedBufferLength) {
        Vector<uint8_t> buffer;
        action.serialize(buffer);
        EXPECT_EQ(expectedSerializedBufferLength, buffer.size());
        auto deserialized = RedirectAction::deserialize({ buffer.data(), buffer.size() });
        EXPECT_EQ(deserialized, action);
    };
    checkRedirectActionSerialization({ { RedirectAction::ExtensionPathAction { "extensionPath"_s } } }, 18);
    checkRedirectActionSerialization({ { RedirectAction::RegexSubstitutionAction { "regexSubstitution"_s, "regexFilter"_s } } }, 41);
    checkRedirectActionSerialization({ { RedirectAction::URLAction { "url"_s } } }, 8);
    checkRedirectActionSerialization({ { RedirectAction::URLTransformAction {
        String::fromUTF8("frägment"),
        "host"_s,
        "password"_s,
        { /* path */ },
        123,
        { /* query */ },
        "scheme"_s,
        "username"_s
    } } }, 68);
    checkRedirectActionSerialization({ { RedirectAction::URLTransformAction { { }, { }, { }, { }, { }, { "query"_s }, { }, { } } } }, 20);
    checkRedirectActionSerialization({ { RedirectAction::URLTransformAction { { }, { }, { }, { }, { },
        { RedirectAction::URLTransformAction::QueryTransform { { { "key1"_s, false, "value1"_s }, { String::fromUTF8("key💩"), false, "value2"_s } }, { "testString1"_s, "testString2"_s } } }, { }, { },
    } } }, 90);

    ModifyHeadersAction modifyHeaders { {
        { { ModifyHeadersAction::ModifyHeaderInfo::AppendOperation { "key1"_s, "value1"_s } } },
        { { ModifyHeadersAction::ModifyHeaderInfo::SetOperation { "key2"_s, "value2"_s } } }
    }, {
        { { ModifyHeadersAction::ModifyHeaderInfo::RemoveOperation { String::fromUTF8("💩") } } }
    }, 2 };
    Vector<uint8_t> modifyHeadersBuffer;
    modifyHeaders.serialize(modifyHeadersBuffer);
    EXPECT_EQ(modifyHeadersBuffer.size(), 59u);
    auto deserializedModifyHeaders = ModifyHeadersAction::deserialize({ modifyHeadersBuffer.data(), modifyHeadersBuffer.size() });
    EXPECT_EQ(modifyHeaders, deserializedModifyHeaders);
}

TEST_F(ContentExtensionTest, QueryTransformActions)
{
    RedirectAction::URLTransformAction::QueryTransform action {
        { { "foo"_s, true, "bar"_s }, { "one"_s, false, "two"_s } }, /* addOrReplaceParams */
        { "baz"_s }, /* removeParams */
    };

    URL testURL { "https://webkit.org/?foo=garply&baz=test&x+y=z&h%20llo=w%20rld"_s };
    action.applyToURL(testURL);
    EXPECT_STREQ("https://webkit.org/?foo=bar&x+y=z&h%20llo=w%20rld&one=two", testURL.string().utf8().data());
}

TEST_F(ContentExtensionTest, IfFrameURL)
{
    auto basic = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"https\", \"if-frame-url\":[\"whatwg\"]}}]"_s);
    testRequest(basic, requestInTopAndFrameURLs("https://example.com/"_s, "https://webkit.org/"_s, "https://whatwg.org/"_s), { variantIndex<BlockLoadAction> });
    testRequest(basic, requestInTopAndFrameURLs("https://example.com/"_s, "https://webkit.org/"_s, "https://webkit.org/"_s), { });

    auto caseSensitivity = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"https\", \"if-frame-url\":[\"whatwg\"],\"frame-url-filter-is-case-sensitive\":true}}]"_s);
    auto caseSensitivityRequest = requestInTopAndFrameURLs("https://example.com/"_s, "https://webkit.org/"_s, "https://example.com/wHaTwG"_s);
    testRequest(basic, caseSensitivityRequest, { variantIndex<BlockLoadAction> });
    testRequest(caseSensitivity, caseSensitivityRequest, { });

    auto otherFlags = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"https\", \"if-frame-url\":[\"whatwg\"],\"resource-type\":[\"image\"]}}]"_s);
    testRequest(otherFlags, requestInTopAndFrameURLs("https://example.com/"_s, "https://webkit.org/"_s, "https://whatwg.org/"_s, ResourceType::Document), { });
    testRequest(otherFlags, requestInTopAndFrameURLs("https://example.com/"_s, "https://webkit.org/"_s, "https://whatwg.org/"_s, ResourceType::Image), { variantIndex<BlockLoadAction> });

    auto otherRulesWithConditions = makeBackend("["
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"https\", \"if-frame-url\":[\"whatwg\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"https\", \"if-top-url\":[\"example\"]}}"
    "]"_s);
    testRequest(otherRulesWithConditions, requestInTopAndFrameURLs("https://example.com/"_s, "https://webkit.org/"_s, "https://whatwg.org/"_s), { variantIndex<BlockLoadAction> });
    testRequest(otherRulesWithConditions, requestInTopAndFrameURLs("https://example.com/"_s, "https://example.com/"_s, "https://webkit.org/"_s), { variantIndex<BlockCookiesAction> });
    testRequest(otherRulesWithConditions, requestInTopAndFrameURLs("https://example.com/"_s, "https://example.com/"_s, "https://whatwg.org/"_s), { variantIndex<BlockCookiesAction>, variantIndex<BlockLoadAction> });
    
    auto matchingEverything = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"https\", \"if-frame-url\":[\".*\"]}}]"_s);
    testRequest(matchingEverything, requestInTopAndFrameURLs("https://example.com/"_s, "https://webkit.org/"_s, "https://whatwg.org/"_s), { variantIndex<BlockLoadAction> });
    testRequest(matchingEverything, requestInTopAndFrameURLs("http://example.com/"_s, "https://webkit.org/"_s, "https://webkit.org/"_s), { });
}

TEST_F(ContentExtensionTest, RegexSubstitution)
{
    auto transformURL = [] (String&& regexSubstitution, String&& regexFilter, String&& originalURL, const char* expectedTransformedURL) {
        WebCore::ContentExtensions::RedirectAction::RegexSubstitutionAction action { WTFMove(regexSubstitution), WTFMove(regexFilter) };
        URL url(WTFMove(originalURL));
        action.applyToURL(url);
        EXPECT_STREQ(url.string().utf8().data(), expectedTransformedURL);
    };
    transformURL("https://\\1.xyz.com/"_s, "^https://www\\.(abc?)\\.xyz\\.com/"_s, "https://www.abc.xyz.com"_s, "https://abc.xyz.com/"_s);
    transformURL("https://\\1.\\1.xyz.com/"_s, "^https://www\\.(abc?)\\.xyz\\.com/"_s, "https://www.ab.xyz.com"_s, "https://ab.ab.xyz.com/"_s);
    transformURL("https://\\1.\\1.xyz.com/"_s, "^https://www\\.(abc?)\\.xyz\\.com/"_s, "https://ab.xyz.com"_s, "https://ab.xyz.com/"_s);
    transformURL("https://example.com/\\0\\1"_s, "^https://www\\.(abc?)\\.xyz\\.com/"_s, "https://www.ab.xyz.com"_s, "https://example.com/https://www.ab.xyz.com/ab"_s);
    transformURL("https://example.com/\\1\\2\\3\\4\\5\\6\\7\\8\\9\\10\\11\\12\\13\\14"_s, "^https://(e)(x)(a)(m)(p)(l)(e)(w)(e)(b)(s)(i)(t)(e)/"_s, "https://examplewebsite/"_s, "https://example.com/examplewee0e1e2e3e4"_s);
    transformURL("https://!@#$%^&*invalidURL\\1/"_s, "^https://www\\.(abc?)\\.xyz\\.com/"_s, "https://www.abc.xyz.com"_s, "https://www.abc.xyz.com/"_s);
}

} // namespace TestWebKitAPI

#endif // ENABLE(CONTENT_EXTENSIONS)
