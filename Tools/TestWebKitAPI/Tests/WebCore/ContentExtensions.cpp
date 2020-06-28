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

#include "Utilities.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/CombinedURLFilters.h>
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

namespace WebCore {
namespace ContentExtensions {
inline std::ostream& operator<<(std::ostream& os, const ActionType& action)
{
    switch (action) {
    case ActionType::BlockLoad:
        return os << "ActionType::BlockLoad";
    case ActionType::BlockCookies:
        return os << "ActionType::BlockCookies";
    case ActionType::CSSDisplayNoneSelector:
        return os << "ActionType::CSSDisplayNone";
    case ActionType::Notify:
        return os << "ActionType::Notify";
    case ActionType::IgnorePreviousRules:
        return os << "ActionType::IgnorePreviousRules";
    case ActionType::MakeHTTPS:
        return os << "ActionType::MakeHTTPS";
    }
}
}
}

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
    Vector<ContentExtensions::DFABytecode> filtersWithoutConditions;
    Vector<ContentExtensions::DFABytecode> filtersWithConditions;
    Vector<ContentExtensions::DFABytecode> topURLFilters;
    bool conditionsApplyOnlyToDomain { false };
};

class InMemoryContentExtensionCompilationClient final : public ContentExtensions::ContentExtensionCompilationClient {
public:
    InMemoryContentExtensionCompilationClient(CompiledContentExtensionData& data)
        : m_data(data)
    {
        EXPECT_EQ(data.actions.size(), 0ull);
        EXPECT_EQ(data.filtersWithoutConditions.size(), 0ull);
        EXPECT_EQ(data.filtersWithConditions.size(), 0ull);
        EXPECT_EQ(data.topURLFilters.size(), 0ull);
    }

private:
    void writeSource(String&&) final { }

    void writeActions(Vector<ContentExtensions::SerializedActionByte>&& actions, bool conditionsApplyOnlyToDomain) final
    {
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.actions.size(), 0ull);
        EXPECT_EQ(m_data.filtersWithoutConditions.size(), 0ull);
        EXPECT_EQ(m_data.filtersWithConditions.size(), 0ull);
        EXPECT_EQ(m_data.topURLFilters.size(), 0ull);
        EXPECT_EQ(m_data.actions.size(), 0ull);
        m_data.actions.appendVector(actions);
        m_data.conditionsApplyOnlyToDomain = conditionsApplyOnlyToDomain;
    }
    
    void writeFiltersWithoutConditionsBytecode(Vector<ContentExtensions::DFABytecode>&& bytecode) final
    {
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.filtersWithConditions.size(), 0ull);
        EXPECT_EQ(m_data.topURLFilters.size(), 0ull);
        m_data.filtersWithoutConditions.appendVector(bytecode);
    }
    
    void writeFiltersWithConditionsBytecode(Vector<ContentExtensions::DFABytecode>&& bytecode) final
    {
        EXPECT_FALSE(finalized);
        EXPECT_EQ(m_data.topURLFilters.size(), 0ull);
        m_data.filtersWithConditions.appendVector(bytecode);
    }
    
    void writeTopURLFiltersBytecode(Vector<ContentExtensions::DFABytecode>&& bytecode) final
    {
        EXPECT_FALSE(finalized);
        m_data.topURLFilters.appendVector(bytecode);
    }
    
    void finalize() final
    {
        finalized = true;
    }

    CompiledContentExtensionData& m_data;
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
    const ContentExtensions::SerializedActionByte* actions() const final { return m_data.actions.data(); }
    unsigned actionsLength() const final { return m_data.actions.size(); }
    const ContentExtensions::DFABytecode* filtersWithoutConditionsBytecode() const final { return m_data.filtersWithoutConditions.data(); }
    unsigned filtersWithoutConditionsBytecodeLength() const final { return m_data.filtersWithoutConditions.size(); }
    const ContentExtensions::DFABytecode* filtersWithConditionsBytecode() const final { return m_data.filtersWithConditions.data(); }
    unsigned filtersWithConditionsBytecodeLength() const final { return m_data.filtersWithConditions.size(); }
    const ContentExtensions::DFABytecode* topURLFiltersBytecode() const final { return m_data.topURLFilters.data(); }
    unsigned topURLFiltersBytecodeLength() const final { return m_data.topURLFilters.size(); }
    bool conditionsApplyOnlyToDomain() const final { return m_data.conditionsApplyOnlyToDomain; }

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

void static testRequest(const ContentExtensions::ContentExtensionsBackend& contentExtensionsBackend, const ResourceLoadInfo& resourceLoadInfo, Vector<ContentExtensions::ActionType> expectedActions, size_t stylesheets = 1)
{
    auto actions = allActionsForResourceLoad(contentExtensionsBackend, resourceLoadInfo);
    unsigned expectedSize = actions.first.size();
    EXPECT_EQ(expectedActions.size(), expectedSize);
    if (expectedActions.size() != expectedSize)
        return;

    for (unsigned i = 0; i < expectedActions.size(); ++i)
        EXPECT_EQ(expectedActions[i], actions.first[i].type());
    EXPECT_EQ(actions.second.size(), stylesheets);
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
    AtomString::init();
    auto extension = InMemoryCompiledContentExtension::create(json);
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter", WTFMove(extension));
    return backend;
}

static Vector<ContentExtensions::NFA> createNFAs(ContentExtensions::CombinedURLFilters& combinedURLFilters)
{
    Vector<ContentExtensions::NFA> nfas;

    combinedURLFilters.processNFAs(std::numeric_limits<size_t>::max(), [&](ContentExtensions::NFA&& nfa) {
        nfas.append(WTFMove(nfa));
    });

    return nfas;
}

TEST_F(ContentExtensionTest, Basic)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
}

TEST_F(ContentExtensionTest, SingleCharacter)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^z\"}}]");
    testRequest(matchBackend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("zttp://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"y\"}}]");
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/ywebkit"), { ContentExtensions::ActionType::BlockLoad });
}

TEST_F(ContentExtensionTest, SingleCharacterDisjunction)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^z\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^c\"}}]");
    testRequest(matchBackend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("bttp://webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("cttp://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("dttp://webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("zttp://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"x\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"y\"}}]");
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/dwebkit"), { });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/xwebkit"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/ywebkit"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("http://webkit.org/zwebkit"), { });
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

TEST_F(ContentExtensionTest, EmptyGroups)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/foo()bar\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/((me)()(too))\"}}]");
    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bar"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/me"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/too"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/metoo"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foome"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foomebar"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/mefoo"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/mefootoo"), { });
}

TEST_F(ContentExtensionTest, QuantifiedEmptyGroups)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/foo()+bar\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^http://webkit\\\\.org/(()*()?(target)()+)\"}}]");
    testRequest(backend, mainDocumentRequest("http://webkit.org/foo"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bar"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/me"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/too"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/target"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foome"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/foomebar"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/mefoo"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/mefootoo"), { });
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

TEST_F(ContentExtensionTest, DotDoesNotIncludeEndOfLine)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"https://webkit\\\\.org/.\"}}]");

    testRequest(backend, mainDocumentRequest("https://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("https://webkit.org/A"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("https://webkit.org/z"), { ContentExtensions::ActionType::BlockLoad });
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

TEST_F(ContentExtensionTest, UndistinguishableActionInsidePrefixTree)
{
    // In this case, the two actions are undistinguishable. The actions of "prefix" appear inside the prefixtree
    // ending at "suffix".
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"prefix\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"prefixsuffix\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("http://prefix.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/prefix"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/aaaprefixaaa"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://prefixsuffix.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/prefixsuffix"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bbbprefixsuffixbbb"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("http://suffix.org/"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/suffix"), { });
}

TEST_F(ContentExtensionTest, DistinguishableActionInsidePrefixTree)
{
    // In this case, the two actions are distinguishable.
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"prefix\"}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"prefixsuffix\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("http://prefix.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/prefix"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/aaaprefixaaa"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://prefixsuffix.org/"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/prefixsuffix"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/bbbprefixsuffixbbb"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("http://suffix.org/"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/suffix"), { });
}

TEST_F(ContentExtensionTest, DistinguishablePrefixAreNotMerged)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"foo\\\\.org\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"bar\\\\.org\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
    testRequest(backend, mainDocumentRequest("http://foo.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://bar.org/"), { ContentExtensions::ActionType::BlockLoad });

    testRequest(backend, mainDocumentRequest("http://foor.org/"), { });
    testRequest(backend, mainDocumentRequest("http://fooar.org/"), { });
    testRequest(backend, mainDocumentRequest("http://fooba.org/"), { });
    testRequest(backend, mainDocumentRequest("http://foob.org/"), { });
    testRequest(backend, mainDocumentRequest("http://foor.org/"), { });
    testRequest(backend, mainDocumentRequest("http://foar.org/"), { });
    testRequest(backend, mainDocumentRequest("http://foba.org/"), { });
    testRequest(backend, mainDocumentRequest("http://fob.org/"), { });
    testRequest(backend, mainDocumentRequest("http://barf.org/"), { });
    testRequest(backend, mainDocumentRequest("http://barfo.org/"), { });
    testRequest(backend, mainDocumentRequest("http://baroo.org/"), { });
    testRequest(backend, mainDocumentRequest("http://baro.org/"), { });
    testRequest(backend, mainDocumentRequest("http://baf.org/"), { });
    testRequest(backend, mainDocumentRequest("http://bafo.org/"), { });
    testRequest(backend, mainDocumentRequest("http://baoo.org/"), { });
    testRequest(backend, mainDocumentRequest("http://bao.org/"), { });

    testRequest(backend, mainDocumentRequest("http://foo.orgbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://oo.orgbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://o.orgbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://.orgbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://rgbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://gbar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://foo.orgar.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://foo.orgr.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://foo.org.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://foo.orgorg/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://foo.orgrg/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://foo.orgg/"), { ContentExtensions::ActionType::BlockLoad });
}

static void compareContents(const ContentExtensions::DFABytecodeInterpreter::Actions& a, const Vector<uint64_t>& b)
{
    EXPECT_EQ(a.size(), b.size());
    for (unsigned i = 0; i < b.size(); ++i)
        EXPECT_TRUE(a.contains(b[i]));
}

TEST_F(ContentExtensionTest, SearchSuffixesWithIdenticalActionAreMerged)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("foo\\.org", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("ba\\.org", false, 0));

    Vector<ContentExtensions::NFA> nfas = createNFAs(combinedURLFilters);
    EXPECT_EQ(1ul, nfas.size());
    EXPECT_EQ(12ul, nfas.first().nodes.size());

    ContentExtensions::DFA dfa = ContentExtensions::NFAToDFA::convert(nfas.first());
    Vector<ContentExtensions::DFABytecode> bytecode;
    ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
    compiler.compile();
    ContentExtensions::DFABytecodeInterpreter interpreter(bytecode.data(), bytecode.size());
    compareContents(interpreter.interpret("foo.org", 0), { 0 });
    compareContents(interpreter.interpret("ba.org", 0), { 0 });
    compareContents(interpreter.interpret("bar.org", 0), { });

    compareContents(interpreter.interpret("paddingfoo.org", 0), { 0 });
    compareContents(interpreter.interpret("paddingba.org", 0), { 0 });
    compareContents(interpreter.interpret("paddingbar.org", 0), { });
}

TEST_F(ContentExtensionTest, SearchSuffixesWithDistinguishableActionAreNotMerged)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("foo\\.org", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("ba\\.org", false, 1));

    Vector<ContentExtensions::NFA> nfas = createNFAs(combinedURLFilters);

    EXPECT_EQ(1ul, nfas.size());
    EXPECT_EQ(17ul, nfas.first().nodes.size());

    ContentExtensions::DFA dfa = ContentExtensions::NFAToDFA::convert(nfas.first());
    Vector<ContentExtensions::DFABytecode> bytecode;
    ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
    compiler.compile();
    ContentExtensions::DFABytecodeInterpreter interpreter(bytecode.data(), bytecode.size());
    compareContents(interpreter.interpret("foo.org", 0), { 0 });
    compareContents(interpreter.interpret("ba.org", 0), { 1 });
    compareContents(interpreter.interpret("bar.org", 0), { });

    compareContents(interpreter.interpret("paddingfoo.org", 0), { 0 });
    compareContents(interpreter.interpret("paddingba.org", 0), { 1 });
    compareContents(interpreter.interpret("paddingba.orgfoo.org", 0), { 1, 0 });
    compareContents(interpreter.interpret("paddingbar.org", 0), { });
}

TEST_F(ContentExtensionTest, DomainTriggers)
{
    auto ifDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"webkit.org\"]}}]");
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://not_webkit.org/test.htm"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.organization/test.htm"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://not_webkit.org/test.html"), { });
    testRequest(ifDomainBackend, mainDocumentRequest("http://webkit.organization/test.html"), { });
    
    auto unlessDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"webkit.org\"]}}]");
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://not_webkit.org/test.htm"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.organization/test.htm"), { });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://not_webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessDomainBackend, mainDocumentRequest("http://webkit.organization/test.html"), { ContentExtensions::ActionType::BlockLoad });
    
    auto ifDomainStarBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"*webkit.org\"]}}]");
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://not_webkit.org/test.htm"), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://webkit.organization/test.htm"), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://not_webkit.org/test.html"), { });
    testRequest(ifDomainStarBackend, mainDocumentRequest("http://webkit.organization/test.html"), { });
    
    auto unlessDomainStarBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"*webkit.org\"]}}]");
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.htm"), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://not_webkit.org/test.htm"), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://webkit.organization/test.htm"), { });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://not_webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessDomainStarBackend, mainDocumentRequest("http://webkit.organization/test.html"), { ContentExtensions::ActionType::BlockLoad });

    auto ifSubDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"sub1.webkit.org\"]}}]");
    testRequest(ifSubDomainBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(ifSubDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { });
    testRequest(ifSubDomainBackend, mainDocumentRequest("http://sub1.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifSubDomainBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"), { });

    auto ifSubDomainStarBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"*sub1.webkit.org\"]}}]");
    testRequest(ifSubDomainStarBackend, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(ifSubDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { });
    testRequest(ifSubDomainStarBackend, mainDocumentRequest("http://sub1.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(ifSubDomainStarBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });

    auto unlessSubDomainBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"sub1.webkit.org\"]}}]");
    testRequest(unlessSubDomainBackend, mainDocumentRequest("http://webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessSubDomainBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessSubDomainBackend, mainDocumentRequest("http://sub1.webkit.org/test.html"), { });
    testRequest(unlessSubDomainBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    
    auto unlessSubDomainStarBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-domain\":[\"*sub1.webkit.org\"]}}]");
    testRequest(unlessSubDomainStarBackend, mainDocumentRequest("http://webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessSubDomainStarBackend, mainDocumentRequest("http://bugs.webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(unlessSubDomainStarBackend, mainDocumentRequest("http://sub1.webkit.org/test.html"), { });
    testRequest(unlessSubDomainStarBackend, mainDocumentRequest("http://sub2.sub1.webkit.org/test.html"), { });

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

TEST_F(ContentExtensionTest, DomainTriggersAlongMergedActions)
{
    auto backend = makeBackend("[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"test\\\\.html\"}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-domain\":[\"webkit.org\"]}},"
        "{\"action\":{\"type\":\"css-display-none\", \"selector\": \"*\"},\"trigger\":{\"url-filter\":\"trigger-on-scripts\\\\.html\",\"resource-type\":[\"script\"]}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore-previous\",\"resource-type\":[\"image\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"except-this\"}}]");

    testRequest(backend, mainDocumentRequest("http://webkit.org/test.htm"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/test.html"), { ContentExtensions::ActionType::BlockLoad, ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/test.html"), { ContentExtensions::ActionType::BlockCookies });

    testRequest(backend, mainDocumentRequest("http://notwebkit.org/trigger-on-scripts.html"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/trigger-on-scripts.html"), { });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/trigger-on-scripts.html", ResourceType::Script), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend, mainDocumentRequest("http://webkit.org/trigger-on-scripts.html", ResourceType::Script), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/trigger-on-scripts.html.test.html", ResourceType::Script), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://webkit.org/trigger-on-scripts.html.test.html", ResourceType::Script), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad, ContentExtensions::ActionType::BlockCookies });

    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html"), { });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html", ResourceType::Script), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html", ResourceType::Script), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html.test.html", ResourceType::Script), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html.test.html", ResourceType::Script), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad, ContentExtensions::ActionType::BlockCookies });

    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html", ResourceType::Image), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html", ResourceType::Image), { }, 0);
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/ignore-previous-trigger-on-scripts.html.test.html", ResourceType::Image), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ignore-previous-trigger-on-scripts.html.test.html", ResourceType::Image), { }, 0);

    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html"), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad, ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html", ResourceType::Image), { ContentExtensions::ActionType::BlockCookies }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html", ResourceType::Image), { ContentExtensions::ActionType::BlockCookies }, 0);
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html", ResourceType::Image), { ContentExtensions::ActionType::BlockCookies }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html", ResourceType::Image), { ContentExtensions::ActionType::BlockCookies }, 0);
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html", ResourceType::Script), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html", ResourceType::Script), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend, mainDocumentRequest("http://notwebkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html", ResourceType::Script), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, mainDocumentRequest("http://webkit.org/except-this-ignore-previous-trigger-on-scripts.html.test.html", ResourceType::Script), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad, ContentExtensions::ActionType::BlockCookies });
}

TEST_F(ContentExtensionTest, TopURL)
{
    const Vector<ContentExtensions::ActionType> blockLoad = { ContentExtensions::ActionType::BlockLoad };
    const Vector<ContentExtensions::ActionType> blockCookies = { ContentExtensions::ActionType::BlockCookies };

    auto ifTopURL = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\"^http://web.*kit.org\"]}}]");
    testRequest(ifTopURL, mainDocumentRequest("http://webkit.org/test.html"), blockLoad);
    testRequest(ifTopURL, mainDocumentRequest("http://webkit.org/test.not_html"), { });
    testRequest(ifTopURL, mainDocumentRequest("http://WEBKIT.org/test.html"), blockLoad);
    testRequest(ifTopURL, mainDocumentRequest("http://webkit.org/TEST.html"), blockLoad);
    testRequest(ifTopURL, mainDocumentRequest("http://web__kit.org/test.html"), blockLoad);
    testRequest(ifTopURL, mainDocumentRequest("http://webk__it.org/test.html"), { });
    testRequest(ifTopURL, mainDocumentRequest("http://web__kit.org/test.not_html"), { });
    testRequest(ifTopURL, mainDocumentRequest("http://not_webkit.org/test.html"), { });
    testRequest(ifTopURL, subResourceRequest("http://not_webkit.org/test.html", "http://webkit.org/not_test.html"), blockLoad);
    testRequest(ifTopURL, subResourceRequest("http://not_webkit.org/test.html", "http://not_webkit.org/not_test.html"), { });
    testRequest(ifTopURL, subResourceRequest("http://webkit.org/test.html", "http://not_webkit.org/not_test.html"), { });
    testRequest(ifTopURL, subResourceRequest("http://webkit.org/test.html", "http://not_webkit.org/test.html"), { });
    testRequest(ifTopURL, subResourceRequest("http://webkit.org/test.html", "http://webkit.org/test.html"), blockLoad);
    testRequest(ifTopURL, subResourceRequest("http://webkit.org/test.html", "http://example.com/#http://webkit.org/test.html"), { });
    testRequest(ifTopURL, subResourceRequest("http://example.com/#http://webkit.org/test.html", "http://webkit.org/test.html"), blockLoad);

    auto unlessTopURL = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-top-url\":[\"^http://web.*kit.org\"]}}]");
    testRequest(unlessTopURL, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://WEBKIT.org/test.html"), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://webkit.org/TEST.html"), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://webkit.org/test.not_html"), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://web__kit.org/test.html"), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://webk__it.org/test.html"), blockLoad);
    testRequest(unlessTopURL, mainDocumentRequest("http://web__kit.org/test.not_html"), { });
    testRequest(unlessTopURL, mainDocumentRequest("http://not_webkit.org/test.html"), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://not_webkit.org/test.html", "http://webkit.org/not_test.html"), { });
    testRequest(unlessTopURL, subResourceRequest("http://not_webkit.org/test.html", "http://not_webkit.org/not_test.html"), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://webkit.org/test.html", "http://not_webkit.org/not_test.html"), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://webkit.org/test.html", "http://not_webkit.org/test.html"), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://webkit.org/test.html", "http://webkit.org/test.html"), { });
    testRequest(unlessTopURL, subResourceRequest("http://webkit.org/test.html", "http://example.com/#http://webkit.org/test.html"), blockLoad);
    testRequest(unlessTopURL, subResourceRequest("http://example.com/#http://webkit.org/test.html", "http://webkit.org/test.html"), { });

    auto ifTopURLMatchesEverything = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\".*\"]}}]");
    testRequest(ifTopURLMatchesEverything, mainDocumentRequest("http://webkit.org/test.html"), blockLoad);
    testRequest(ifTopURLMatchesEverything, mainDocumentRequest("http://webkit.org/test.not_html"), { });
    testRequest(ifTopURLMatchesEverything, mainDocumentRequest("http://not_webkit.org/test.html"), blockLoad);
    
    auto unlessTopURLMatchesEverything = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-top-url\":[\".*\"]}}]");
    testRequest(unlessTopURLMatchesEverything, mainDocumentRequest("http://webkit.org/test.html"), { });
    testRequest(unlessTopURLMatchesEverything, mainDocumentRequest("http://webkit.org/test.not_html"), { });
    testRequest(unlessTopURLMatchesEverything, mainDocumentRequest("http://not_webkit.org/test.html"), { });
    
    auto mixedConditions = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\"^http://web.*kit.org\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"resource-type\":[\"document\"], \"url-filter\":\"test\\\\.html\", \"unless-top-url\":[\"^http://web.*kit.org\"]}}]");
    testRequest(mixedConditions, mainDocumentRequest("http://webkit.org/test.html"), blockLoad);
    testRequest(mixedConditions, mainDocumentRequest("http://not_webkit.org/test.html"), blockCookies);
    testRequest(mixedConditions, subResourceRequest("http://webkit.org/test.html", "http://not_webkit.org", ResourceType::Document), blockCookies);
    testRequest(mixedConditions, subResourceRequest("http://webkit.org/test.html", "http://not_webkit.org", ResourceType::Image), { });

    auto caseSensitive = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\"^http://web.*kit.org/test\"], \"top-url-filter-is-case-sensitive\":true}}]");
    testRequest(caseSensitive, mainDocumentRequest("http://webkit.org/test.html"), blockLoad);
    testRequest(caseSensitive, mainDocumentRequest("http://WEBKIT.org/test.html"), blockLoad); // domains are canonicalized before running regexes.
    testRequest(caseSensitive, mainDocumentRequest("http://webkit.org/TEST.html"), { });

    auto caseInsensitive = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"if-top-url\":[\"^http://web.*kit.org/test\"]}}]");
    testRequest(caseInsensitive, mainDocumentRequest("http://webkit.org/test.html"), blockLoad);
    testRequest(caseInsensitive, mainDocumentRequest("http://webkit.org/TEST.html"), blockLoad);
}

TEST_F(ContentExtensionTest, MultipleExtensions)
{
    auto extension1 = InMemoryCompiledContentExtension::create("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_load\"}}]");
    auto extension2 = InMemoryCompiledContentExtension::create("[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"block_cookies\"}}]");
    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter1", WTFMove(extension1));
    backend.addContentExtension("testFilter2", WTFMove(extension2));
    
    testRequest(backend, mainDocumentRequest("http://webkit.org"), { }, 2);
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_load.html"), { ContentExtensions::ActionType::BlockLoad }, 2);
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_cookies.html"), { ContentExtensions::ActionType::BlockCookies }, 2);
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_load/block_cookies.html"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad }, 2);
    testRequest(backend, mainDocumentRequest("http://webkit.org/block_cookies/block_load.html"), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad }, 2);
    
    auto ignoreExtension1 = InMemoryCompiledContentExtension::create("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block_load\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore1\"}}]");
    auto ignoreExtension2 = InMemoryCompiledContentExtension::create("[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"block_cookies\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore2\"}}]");
    ContentExtensions::ContentExtensionsBackend backendWithIgnore;
    backendWithIgnore.addContentExtension("testFilter1", WTFMove(ignoreExtension1));
    backendWithIgnore.addContentExtension("testFilter2", WTFMove(ignoreExtension2));

    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org"), { }, 2);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/ignore1.html"), { }, 1);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_cookies/ignore1.html"), { ContentExtensions::ActionType::BlockCookies }, 1);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/ignore2.html"), { ContentExtensions::ActionType::BlockLoad }, 1);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_cookies/ignore2.html"), { }, 1);
    testRequest(backendWithIgnore, mainDocumentRequest("http://webkit.org/block_load/block_cookies/ignore1/ignore2.html"), { }, 0);
}

static bool actionsEqual(const std::pair<Vector<WebCore::ContentExtensions::Action>, Vector<String>>& actual, Vector<WebCore::ContentExtensions::Action>&& expected, bool ignorePreviousRules = false)
{
    if (ignorePreviousRules) {
        if (actual.second.size())
            return false;
    } else {
        if (actual.second.size() != 1)
            return false;
        if (actual.second[0] != "testFilter")
            return false;
    }

    if (actual.first.size() != expected.size())
        return false;
    for (size_t i = 0; i < expected.size(); ++i) {
        if (actual.first[i].type() != expected[i].type())
            return false;
        if (actual.first[i].stringArgument() != expected[i].stringArgument())
            return false;
    }
    return true;
}

static const char* jsonWithStringsToCombine = "["
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
"]";

TEST_F(ContentExtensionTest, StringParameters)
{
    using namespace ContentExtensions;
    auto backend1 = makeBackend("[{\"action\":{\"type\":\"notify\",\"notification\":\"testnotification\"},\"trigger\":{\"url-filter\":\"matches\"}}]");
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend1, mainDocumentRequest("test:///matches")), Vector<Action>::from(Action { ContentExtensions::ActionType::Notify, "testnotification" })));

    auto backend2 = makeBackend(jsonWithStringsToCombine);
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://A")), Vector<Action>::from(Action { ContentExtensions::ActionType::Notify, "AAA" })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://B")), Vector<Action>::from(Action { ContentExtensions::ActionType::Notify, "BBB" })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://C")), Vector<Action>::from(Action { ContentExtensions::ActionType::CSSDisplayNoneSelector, "CCC,selectorCombinedWithC" })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://D")), Vector<Action>::from(Action { ContentExtensions::ActionType::CSSDisplayNoneSelector, "DDD" })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://E")), { }, true));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://F")), Vector<Action>::from(Action { ContentExtensions::ActionType::BlockLoad })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://G")), Vector<Action>::from(Action { ContentExtensions::ActionType::Notify, "GGG" })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://GIK")), Vector<Action>::from(Action { ContentExtensions::ActionType::Notify, "GGG" })));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://AJ")), Vector<Action>::from(
        Action { ContentExtensions::ActionType::Notify, "AAA" },
        Action { ContentExtensions::ActionType::Notify, "AAA" } // ignore-previous-rules makes the AAA actions need to be unique.
    )));
    // FIXME: Add a test that matches actions with AAA with ignore-previous-rules between them and makes sure we only get one notification.
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://AE")), { }, true));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://ABCDE")), { }, true));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://ABCDEFG")), Vector<Action>::from(
        Action { ContentExtensions::ActionType::Notify, "GGG" },
        Action { ContentExtensions::ActionType::BlockLoad }
    ), true));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://FG")), Vector<Action>::from(
        Action { ContentExtensions::ActionType::Notify, "GGG" },
        Action { ContentExtensions::ActionType::BlockLoad }
    )));
    ASSERT_TRUE(actionsEqual(allActionsForResourceLoad(backend2, mainDocumentRequest("http://EFG")), Vector<Action>::from(
        Action { ContentExtensions::ActionType::Notify, "GGG" },
        Action { ContentExtensions::ActionType::BlockLoad }
    ), true));
}

template<typename T, size_t cStringLength>
static int sequenceInstances(const Vector<T> vector, const char (&sequence)[cStringLength])
{
    static_assert(sizeof(T) == sizeof(char), "sequenceInstances should only be used for various byte vectors.");

    size_t sequenceLength = cStringLength - 1;
    size_t instances = 0;
    for (size_t i = 0; i <= vector.size() - sequenceLength; ++i) {
        for (size_t j = 0; j < sequenceLength; j++) {
            if (vector[i + j] != sequence[j])
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

    ASSERT_EQ(sequenceInstances(data.actions, "AAA"), 2);
    ASSERT_EQ(sequenceInstances(data.actions, "GGG"), 1);

    ASSERT_EQ(data.actions.size(), 78u);
    ASSERT_EQ(data.filtersWithoutConditions.size(),  313u);
    ASSERT_EQ(data.filtersWithConditions.size(),  5u);
    ASSERT_EQ(data.topURLFilters.size(),  5u);
    ASSERT_FALSE(data.conditionsApplyOnlyToDomain);

    auto extensionWithFlags = InMemoryCompiledContentExtension::create("["
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"A\"}},"
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"C\"}},"
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"A\",\"resource-type\":[\"document\"]}},"
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"A\",\"resource-type\":[\"document\",\"font\"]}},"
        "{\"action\":{\"type\":\"notify\",\"notification\":\"AAA\"},\"trigger\":{\"url-filter\":\"B\",\"resource-type\":[\"document\"]}}"
    "]");
    ASSERT_EQ(sequenceInstances(extensionWithFlags->data().actions, "AAA"), 3); // There are 3 sets of unique flags for AAA actions.
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

TEST_F(ContentExtensionTest, UselessTermsMatchingEverythingAreEliminated)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern(".*web", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.*)web", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.)*web", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.+)*web", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.?)*web", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.+)?web", false, 0));
    EXPECT_EQ(ContentExtensions::URLFilterParser::ParseStatus::Ok, parser.addPattern("(.?)+web", false, 0));

    Vector<ContentExtensions::NFA> nfas = createNFAs(combinedURLFilters);
    EXPECT_EQ(1ul, nfas.size());
    EXPECT_EQ(7ul, nfas.first().nodes.size());

    ContentExtensions::DFA dfa = ContentExtensions::NFAToDFA::convert(nfas.first());
    Vector<ContentExtensions::DFABytecode> bytecode;
    ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
    compiler.compile();
    ContentExtensions::DFABytecodeInterpreter interpreter(bytecode.data(), bytecode.size());
    compareContents(interpreter.interpret("eb", 0), { });
    compareContents(interpreter.interpret("we", 0), { });
    compareContents(interpreter.interpret("weeb", 0), { });
    compareContents(interpreter.interpret("web", 0), { 0 });
    compareContents(interpreter.interpret("wweb", 0), { 0 });
    compareContents(interpreter.interpret("wwebb", 0), { 0 });
    compareContents(interpreter.interpret("http://theweb.com/", 0), { 0 });
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
    
    testRequest(backend, mainDocumentRequest("http://webkit.org"), { }, 0);
    testRequest(backend, {URL(URL(), "http://webkit.org"), URL(URL(), "http://not_webkit.org"), ResourceType::Document}, { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend, {URL(URL(), "http://webkit.org"), URL(URL(), "http://not_webkit.org"), ResourceType::Image}, { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad });
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
                ruleList.append("{\"action\":{\"type\":\"");
                
                // Make every other rule ignore-previous-rules to not combine actions.
                if (!((c1 + c2 + c3) % 2))
                    ruleList.append("ignore-previous-rules");
                else {
                    ruleList.append("css-display-none");
                    ruleList.append("\",\"selector\":\"");
                    ruleList.append(c1);
                    ruleList.append(c2);
                    ruleList.append(c3);
                }
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

    testRequest(backend, mainDocumentRequest("http://webkit.org/AAA"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend, mainDocumentRequest("http://webkit.org/YAA"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ZAA"), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/LAA/AAA"), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/LAA/MAA"), { ContentExtensions::ActionType::CSSDisplayNoneSelector }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { });
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
    });
    EXPECT_EQ(nfas.size(), 1ull);
    
    Vector<ContentExtensions::DFA> dfas;
    for (auto& nfa : nfas)
        dfas.append(ContentExtensions::NFAToDFA::convert(nfa));
    EXPECT_EQ(dfas.size(), 1ull);
    
    Vector<ContentExtensions::DFABytecode> combinedBytecode;
    for (const auto& dfa : dfas) {
        Vector<ContentExtensions::DFABytecode> bytecode;
        ContentExtensions::DFABytecodeCompiler compiler(dfa, bytecode);
        compiler.compile();
        combinedBytecode.appendVector(bytecode);
    }
    
    ContentExtensions::DFABytecodeInterpreter interpreter(&combinedBytecode[0], combinedBytecode.size());
    
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
                    auto matches = interpreter.interpret(pattern.toString().utf8(), 0);
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

    compareContents(interpreter.interpret("CAAAAx", 0), {expectedIndex('C', 4), expectedIndex('A', 1)});
    compareContents(interpreter.interpret("KAAAAx", 0), {expectedIndex('K', 4), expectedIndex('A', 1)});
    compareContents(interpreter.interpret("AKAAAx", 0), {expectedIndex('K', 3), expectedIndex('K', 4)});
    compareContents(interpreter.interpret("AKxAAAAx", 0), {expectedIndex('A', 1)});
    compareContents(interpreter.interpret("AKAxAAAAx", 0), {expectedIndex('A', 1)});
    compareContents(interpreter.interpret("AKAxZKAxZKZxAAAAx", 0), {expectedIndex('A', 1)});
    compareContents(interpreter.interpret("ZAAAA", 0), {expectedIndex('Z', 4), expectedIndex('A', 1)});
    compareContents(interpreter.interpret("ZZxZAAAB", 0), {expectedIndex('Z', 4), expectedIndex('B', 1)});
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
    auto parsedRules = ContentExtensions::parseRuleList(json);
    std::error_code compilerError;
    if (parsedRules.has_value())
        compilerError = ContentExtensions::compileRuleList(client, json, WTFMove(parsedRules.value()));
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
    auto backend1 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]");
    EXPECT_TRUE(nullptr != backend1.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend1, mainDocumentRequest("http://webkit.org"), { }); // Selector is in global stylesheet.
    
    auto backend2 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"if-domain\":[\"webkit.org\"]}}]");
    EXPECT_EQ(nullptr, backend2.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend2, mainDocumentRequest("http://webkit.org"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(backend2, mainDocumentRequest("http://w3c.org"), { });
    
    auto backend3 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"unless-domain\":[\"webkit.org\"]}}]");
    EXPECT_EQ(nullptr, backend3.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend3, mainDocumentRequest("http://webkit.org"), { });
    testRequest(backend3, mainDocumentRequest("http://w3c.org"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    
    auto backend4 = makeBackend("[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\",\"load-type\":[\"third-party\"]}}]");
    EXPECT_EQ(nullptr, backend4.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend4, mainDocumentRequest("http://webkit.org"), { });
    testRequest(backend4, subResourceRequest("http://not_webkit.org", "http://webkit.org"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });

    // css-display-none rules after ignore-previous-rules should not be put in the default stylesheet.
    auto backend5 = makeBackend("[{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\".*\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]");
    EXPECT_EQ(nullptr, backend5.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend5, mainDocumentRequest("http://webkit.org"), { ContentExtensions::ActionType::CSSDisplayNoneSelector }, 0);
    
    auto backend6 = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*\",\"if-domain\":[\"webkit.org\",\"*w3c.org\"],\"resource-type\":[\"document\",\"script\"]}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"ignore\",\"if-domain\":[\"*webkit.org\",\"w3c.org\"]}},"
        "{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\".*\",\"unless-domain\":[\"webkit.org\",\"whatwg.org\"],\"resource-type\":[\"script\",\"image\"],\"load-type\":[\"third-party\"]}}]");
    EXPECT_EQ(nullptr, backend6.globalDisplayNoneStyleSheet("testFilter"_s));
    testRequest(backend6, mainDocumentRequest("http://webkit.org"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend6, mainDocumentRequest("http://w3c.org"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend6, mainDocumentRequest("http://whatwg.org"), { });
    testRequest(backend6, mainDocumentRequest("http://sub.webkit.org"), { });
    testRequest(backend6, mainDocumentRequest("http://sub.w3c.org"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend6, mainDocumentRequest("http://sub.whatwg.org"), { });
    testRequest(backend6, mainDocumentRequest("http://webkit.org/ignore"), { }, 0);
    testRequest(backend6, mainDocumentRequest("http://w3c.org/ignore"), { }, 0);
    testRequest(backend6, mainDocumentRequest("http://whatwg.org/ignore"), { });
    testRequest(backend6, mainDocumentRequest("http://sub.webkit.org/ignore"), { }, 0);
    testRequest(backend6, mainDocumentRequest("http://sub.w3c.org/ignore"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend6, mainDocumentRequest("http://sub.whatwg.org/ignore"), { });
    testRequest(backend6, subResourceRequest("http://example.com/image.png", "http://webkit.org/", ResourceType::Image), { });
    testRequest(backend6, subResourceRequest("http://example.com/image.png", "http://w3c.org/", ResourceType::Image), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend6, subResourceRequest("http://example.com/doc.html", "http://webkit.org/", ResourceType::Document), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend6, subResourceRequest("http://example.com/script.js", "http://webkit.org/", ResourceType::Script), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend6, subResourceRequest("http://example.com/script.js", "http://w3c.org/", ResourceType::Script), { ContentExtensions::ActionType::BlockCookies, ContentExtensions::ActionType::BlockLoad });
    testRequest(backend6, subResourceRequest("http://example.com/script.js", "http://example.com/", ResourceType::Script), { });
    testRequest(backend6, subResourceRequest("http://example.com/ignore/image.png", "http://webkit.org/", ResourceType::Image), { }, 0);
    testRequest(backend6, subResourceRequest("http://example.com/ignore/image.png", "http://example.com/", ResourceType::Image), { });
    testRequest(backend6, subResourceRequest("http://example.com/ignore/image.png", "http://example.org/", ResourceType::Image), { ContentExtensions::ActionType::BlockCookies });
    testRequest(backend6, subResourceRequest("http://example.com/doc.html", "http://example.org/", ResourceType::Document), { });
    testRequest(backend6, subResourceRequest("http://example.com/", "http://example.com/", ResourceType::Font), { });
    testRequest(backend6, subResourceRequest("http://example.com/ignore", "http://webkit.org/", ResourceType::Image), { }, 0);
    testRequest(backend6, subResourceRequest("http://example.com/ignore", "http://webkit.org/", ResourceType::Font), { }, 0);
    testRequest(backend6, subResourceRequest("http://example.com/", "http://example.com/", ResourceType::Script), { });
    testRequest(backend6, subResourceRequest("http://example.com/ignore", "http://example.com/", ResourceType::Script), { });
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
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":5}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":\"first-party\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":null}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"load-type\":false}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":{}}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":[\"invalid\"]}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":[5]}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidStringInTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":5}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":\"document\"}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":null}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"resource-type\":false}}]",
        ContentExtensions::ContentExtensionError::JSONInvalidTriggerFlagsArray);
    
    checkCompilerError("[{\"action\":{\"type\":\"notify\"},\"trigger\":{\"url-filter\":\".*\"}}]", ContentExtensions::ContentExtensionError::JSONInvalidNotification);
    checkCompilerError("[{\"action\":{\"type\":\"notify\",\"notification\":5},\"trigger\":{\"url-filter\":\".*\"}}]", ContentExtensions::ContentExtensionError::JSONInvalidNotification);
    checkCompilerError("[{\"action\":{\"type\":\"notify\",\"notification\":[]},\"trigger\":{\"url-filter\":\".*\"}}]", ContentExtensions::ContentExtensionError::JSONInvalidNotification);
    checkCompilerError("[{\"action\":{\"type\":\"notify\",\"notification\":\"here's my notification\"},\"trigger\":{\"url-filter\":\".*\"}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"notify\",\"notification\":\"\\u1234\"},\"trigger\":{\"url-filter\":\".*\"}}]", { });
    
    StringBuilder rules;
    rules.append("[");
    for (unsigned i = 0; i < 49999; ++i)
        rules.append("{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"a\"}},");
    String rules50000 = rules.toString();
    String rules50001 = rules.toString();
    rules50000.append("{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"a\"}}]");
    rules50001.append("{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"a\"}},{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"a\"}}]");
    checkCompilerError(rules50000.utf8().data(), { });
    checkCompilerError(rules50001.utf8().data(), ContentExtensions::ContentExtensionError::JSONTooManyRules);
    
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":{}}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[5]}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"]}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":\"a\"}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":false}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":null}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":{}}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[5]}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"\"]}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":\"a\"}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":null}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":false}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"A\"]}}]", ContentExtensions::ContentExtensionError::JSONDomainNotLowerCaseASCII);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"\\u00DC\"]}}]", ContentExtensions::ContentExtensionError::JSONDomainNotLowerCaseASCII);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"0\"]}}]", { });
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"a\"]}}]", { });

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[],\"unless-domain\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[]}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":5}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":5}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":5,\"unless-domain\":5}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[]}}]", ContentExtensions::ContentExtensionError::JSONInvalidConditionList);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"],\"unless-domain\":[]}}]", ContentExtensions::ContentExtensionError::JSONMultipleConditions);

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

    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"],\"unless-domain\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"],\"unless-top-url\":[]}}]", ContentExtensions::ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"],\"unless-top-url\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"],\"if-top-url\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[],\"unless-domain\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"],\"if-domain\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONMultipleConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"]}}, {\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-domain\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONTopURLAndDomainConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"if-top-url\":[\"a\"]}}, {\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\",\"unless-domain\":[\"a\"]}}]", ContentExtensions::ContentExtensionError::JSONTopURLAndDomainConditions);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"test\\\\.html\", \"unless-top-url\":[\"[\"]}}]", ContentExtensions::ContentExtensionError::JSONInvalidRegex);
    checkCompilerError("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"webkit.org\", \"unexpected-identifier-should-be-ignored\":5}}]", { });
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
        "{\"action\":{\"type\":\"make-https\"},\"trigger\":{\"url-filter\":\"A*BC\"}}]");
    
    testRequest(backend, mainDocumentRequest("http://webkit.org/D"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/AAD"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(backend, mainDocumentRequest("http://webkit.org/AB"), { });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABA"), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABAD"), { }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/BC"), { ContentExtensions::ActionType::MakeHTTPS });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABC"), { ContentExtensions::ActionType::MakeHTTPS });
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABABC"), { ContentExtensions::ActionType::MakeHTTPS }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABABCAD"), { ContentExtensions::ActionType::MakeHTTPS }, 0);
    testRequest(backend, mainDocumentRequest("http://webkit.org/ABCAD"), { ContentExtensions::ActionType::MakeHTTPS, ContentExtensions::ActionType::BlockLoad });
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
            nfas.append(WTFMove(nfa));
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
        
        ContentExtensions::DFABytecodeInterpreter interpreter(&combinedBytecode[0], combinedBytecode.size());
        
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
    
    testPatternStatus("(a)\\1", ContentExtensions::URLFilterParser::ParseStatus::Ok); // Back references are disabled, it parse as octal 1
    testPatternStatus("(<A>a)\\k<A>", ContentExtensions::URLFilterParser::ParseStatus::Ok); // Named back references aren't handled, it parse as "k<A>"
    testPatternStatus("(?<A>a)\\k<A>", ContentExtensions::URLFilterParser::ParseStatus::BackReference);
    testPatternStatus("\\1(a)", ContentExtensions::URLFilterParser::ParseStatus::Ok); // Forward references are disabled, it parse as octal 1
    testPatternStatus("\\8(a)", ContentExtensions::URLFilterParser::ParseStatus::Ok); // Forward references are disabled, it parse as '8'
    testPatternStatus("\\9(a)", ContentExtensions::URLFilterParser::ParseStatus::Ok); // Forward references are disabled, it parse as '9'
    testPatternStatus("\\k<A>(<A>a)", ContentExtensions::URLFilterParser::ParseStatus::Ok); // Named forward references aren't handled, it parse as "k<A>"
    testPatternStatus("\\k<A>(?<A>a)", ContentExtensions::URLFilterParser::ParseStatus::YarrError);
    testPatternStatus("\\k<A>(a)", ContentExtensions::URLFilterParser::ParseStatus::Ok); // Unmatched named forward references aren't handled, it parse as "k<A>"
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
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"^[c-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"[c-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase2)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"^b\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"l\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.k.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.l.xxx/"), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase3)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[a-m]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[a-m]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase4)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^l\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[a-m]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("k://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("l://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"l\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[a-m]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.k.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.l.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase5)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[e-h]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[e-h]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase6)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[e-h]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[e-h]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase7)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^e\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"e\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase8)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[a-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[a-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase9)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[a-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[a-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase10)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^e\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"e\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase11)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[d-f]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[d-f]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { });
}


TEST_F(ContentExtensionTest, RangeOverlapCase12)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[d-g]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[d-g]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase13)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[c-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[c-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase14)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[b-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[c-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[b-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[c-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase15)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^[c-f]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^[c-f]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[c-f]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[c-f]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"), { });
}

TEST_F(ContentExtensionTest, RangeOverlapCase16)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^c\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^c\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"c\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"c\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { });
}

TEST_F(ContentExtensionTest, QuantifiedOneOrMoreRangesCase11And13)
{
    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"[c-e]+[g-i]+YYY\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[b-d]+[h-j]+YYY\"}}]");

    // The interesting ranges only match between 'b' and 'k', plus a gap in 'f'.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.abyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.acyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.adyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aeyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.afyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.agyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ahyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aiyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ajyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.akyyy.xxx/"), { });

    // 'b' is the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.byyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bbyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bcyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bdyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.beyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bfyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bgyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.biyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bkyyy.xxx/"), { });

    // 'c' is the first character of the first rule, and it overlaps the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cbyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ccyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cdyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ceyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cfyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.chyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ciyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ckyyy.xxx/"), { });

    // 'd' is in the first range of both rule. This series cover overlaps between the two rules.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.degyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dehyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfgyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfhyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.djyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
}

TEST_F(ContentExtensionTest, QuantifiedOneOrMoreRangesCase11And13InGroups)
{
    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"([c-e])+([g-i]+YYY)\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"[b-d]+[h-j]+YYY\"}}]");

    // The interesting ranges only match between 'b' and 'k', plus a gap in 'f'.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.abyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.acyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.adyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aeyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.afyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.agyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ahyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aiyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ajyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.akyyy.xxx/"), { });

    // 'b' is the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.byyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bbyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bcyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bdyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.beyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bfyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bgyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.biyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bkyyy.xxx/"), { });

    // 'c' is the first character of the first rule, and it overlaps the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cbyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ccyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cdyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ceyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cfyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.chyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ciyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ckyyy.xxx/"), { });

    // 'd' is in the first range of both rule. This series cover overlaps between the two rules.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.degyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dehyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfgyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfhyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.djyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase1)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"^(bar)*[c-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"(bar)*[c-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"), { });
}


TEST_F(ContentExtensionTest, CombinedRangeOverlapCase2)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"^(bar)*b\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { }, 0);
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[a-m]\"}},"
        "{\"action\":{\"type\":\"ignore-previous-rules\"},\"trigger\":{\"url-filter\":\"(bar)*l\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.k.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.l.xxx/"), { }, 0);
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase3)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[a-m]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[a-m]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase4)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*l\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[a-m]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("k://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("l://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("m://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("n://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*l\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[a-m]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.k.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.l.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.m.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.n.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase5)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[e-h]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[e-h]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase6)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[e-h]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[e-h]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase7)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*e\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[a-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*e\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase8)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[a-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[a-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase9)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[a-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*e\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[a-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase10)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*e\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("i://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[e-h]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*e\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.i.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase11)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[d-f]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[d-f]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { });
}


TEST_F(ContentExtensionTest, CombinedRangeOverlapCase12)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[d-g]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[e-g]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[d-g]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase13)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[c-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[b-d]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[c-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase14)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[b-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[c-e]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("h://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[b-e]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[c-e]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.h.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase15)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*[c-f]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*[c-f]\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("f://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("g://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[c-f]\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[c-f]\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.f.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.g.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedRangeOverlapCase16)
{
    auto matchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^(foo)*c\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"^(bar)*c\"}}]");

    testRequest(matchBackend, mainDocumentRequest("a://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("b://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("c://www.webkit.org/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(matchBackend, mainDocumentRequest("d://www.webkit.org/"), { });
    testRequest(matchBackend, mainDocumentRequest("e://www.webkit.org/"), { });

    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*c\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*c\"}}]");
    testRequest(searchBackend, mainDocumentRequest("zzz://www.a.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.b.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.c.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.d.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.e.xxx/"), { });
}

TEST_F(ContentExtensionTest, CombinedQuantifiedOneOrMoreRangesCase11And13)
{
    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*[c-e]+[g-i]+YYY\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[b-d]+[h-j]+YYY\"}}]");

    // The interesting ranges only match between 'b' and 'k', plus a gap in 'f'.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.abyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.acyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.adyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aeyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.afyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.agyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ahyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aiyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ajyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.akyyy.xxx/"), { });

    // 'b' is the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.byyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bbyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bcyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bdyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.beyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bfyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bgyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.biyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bkyyy.xxx/"), { });

    // 'c' is the first character of the first rule, and it overlaps the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cbyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ccyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cdyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ceyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cfyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.chyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ciyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ckyyy.xxx/"), { });

    // 'd' is in the first range of both rule. This series cover overlaps between the two rules.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.degyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dehyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfgyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfhyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.djyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
}

TEST_F(ContentExtensionTest, CombinedQuantifiedOneOrMoreRangesCase11And13InGroups)
{
    auto searchBackend = makeBackend("[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(foo)*([c-e])+([g-i]+YYY)\"}},"
        "{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\"(bar)*[b-d]+[h-j]+YYY\"}}]");

    // The interesting ranges only match between 'b' and 'k', plus a gap in 'f'.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.abyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.acyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.adyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aeyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.afyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.agyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ahyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.aiyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ajyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.akyyy.xxx/"), { });

    // 'b' is the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.byyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bbyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bcyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bdyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.beyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bfyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bgyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.biyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.bkyyy.xxx/"), { });

    // 'c' is the first character of the first rule, and it overlaps the first character of the second rule.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cayyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cbyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ccyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cdyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ceyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cfyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.chyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ciyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.cjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ckyyy.xxx/"), { });

    // 'd' is in the first range of both rule. This series cover overlaps between the two rules.
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddgyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddhhyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector, ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.degyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dehyyy.xxx/"), { ContentExtensions::ActionType::BlockLoad });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfgyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.dfhyyy.xxx/"), { });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.djyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
    testRequest(searchBackend, mainDocumentRequest("zzz://www.ddjjyyy.xxx/"), { ContentExtensions::ActionType::CSSDisplayNoneSelector });
}

TEST_F(ContentExtensionTest, ValidSelector)
{
    EXPECT_TRUE(WebCore::ContentExtensions::isValidCSSSelector("a[href*=hsv]"));
}

} // namespace TestWebKitAPI
