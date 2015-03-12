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
#include <WebCore/ResourceLoadInfo.h>
#include <WebCore/URL.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {
namespace ContentExtensions {
inline std::ostream& operator<<(std::ostream& os, const ActionType& action)
{
    switch (action) {
    case ActionType::BlockLoad:
        return os << "ContentFilterAction::BlockLoad";
    case ActionType::BlockCookies:
        return os << "ContentFilterAction::BlockCookies";
    case ActionType::CSSDisplayNone:
        return os << "ContentFilterAction::CSSDisplayNone";
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
    EXPECT_EQ(expectedActions.size(), actions.size());
    if (expectedActions.size() != actions.size())
        return;

    for (unsigned i = 0; i < expectedActions.size(); ++i)
        EXPECT_EQ(expectedActions[i], actions[i].type());
}

ResourceLoadInfo mainDocumentRequest(const char* url, ResourceType resourceType = ResourceType::Document)
{
    return { URL(URL(), url), URL(URL(), url), resourceType };
}

const char* basicFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*webkit.org\"}}]";

TEST_F(ContentExtensionTest, Basic)
{
    auto extensionData = ContentExtensions::compileRuleList(basicFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter", extension);

    testRequest(backend, mainDocumentRequest("http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
}

const char* patternsStartingWithGroupFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(http://whatwg\\\\.org/)?webkit\134\134.org\"}}]";

TEST_F(ContentExtensionTest, PatternStartingWithGroup)
{
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

const char* patternNestedGroupsFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"http://webkit\\\\.org/(foo(bar)*)+\"}}]";

TEST_F(ContentExtensionTest, PatternNestedGroups)
{
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

const char* matchPastEndOfStringFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".+\"}}]";

TEST_F(ContentExtensionTest, MatchPastEndOfString)
{
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

const char* endOfLineAssertionFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*foobar$\"}}]";

TEST_F(ContentExtensionTest, EndOfLineAssertion)
{
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
    
const char* loadTypeFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*webkit.org\",\"load-type\":[\"third-party\"]}},"
    "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*whatwg.org\",\"load-type\":[\"first-party\"]}},"
    "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*alwaysblock.pdf\"}}]";

TEST_F(ContentExtensionTest, LoadType)
{
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
    
const char* resourceTypeFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*block_all_types.org\",\"resource-type\":[\"document\",\"image\",\"style-sheet\",\"script\",\"font\",\"raw\",\"svg-document\",\"media\"]}},"
    "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*block_only_images\",\"resource-type\":[\"image\"]}}]";
    
TEST_F(ContentExtensionTest, ResourceType)
{
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

} // namespace TestWebKitAPI
