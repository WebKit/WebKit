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

void static testURL(ContentExtensions::ContentExtensionsBackend contentExtensionsBackend, const URL& url, Vector<ContentExtensions::ActionType> expectedActions)
{
    auto actions = contentExtensionsBackend.actionsForURL(url);
    EXPECT_EQ(expectedActions.size(), actions.size());
    if (expectedActions.size() != actions.size())
        return;

    for (unsigned i = 0; i < expectedActions.size(); ++i)
        EXPECT_EQ(expectedActions[i], actions[i].type());
}

const char* basicFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*webkit.org\"}}]";

TEST_F(ContentExtensionTest, Basic)
{
    auto extensionData = ContentExtensions::compileRuleList(basicFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("testFilter", extension);

    testURL(backend, URL(ParsedURLString, "http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
}

const char* patternsStartingWithGroupFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"(http://whatwg\\\\.org/)?webkit\134\134.org\"}}]";

TEST_F(ContentExtensionTest, PatternStartingWithGroup)
{
    auto extensionData = ContentExtensions::compileRuleList(patternsStartingWithGroupFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("PatternNestedGroupsFilter", extension);

    testURL(backend, URL(URL(), "http://whatwg.org/webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://whatwg.org/webkit.org"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/"), { });
    testURL(backend, URL(URL(), "http://whatwg.org/"), { });
    testURL(backend, URL(URL(), "http://whatwg.org"), { });
}

const char* patternNestedGroupsFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"http://webkit\\\\.org/(foo(bar)*)+\"}}]";

TEST_F(ContentExtensionTest, PatternNestedGroups)
{
    auto extensionData = ContentExtensions::compileRuleList(patternNestedGroupsFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("PatternNestedGroupsFilter", extension);

    testURL(backend, URL(URL(), "http://webkit.org/foo"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foobarbar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foofoobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foobarfoobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foob"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foor"), { ContentExtensions::ActionType::BlockLoad });


    testURL(backend, URL(URL(), "http://webkit.org/"), { });
    testURL(backend, URL(URL(), "http://webkit.org/bar"), { });
    testURL(backend, URL(URL(), "http://webkit.org/fobar"), { });
}

const char* matchPastEndOfStringFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".+\"}}]";

TEST_F(ContentExtensionTest, MatchPastEndOfString)
{
    auto extensionData = ContentExtensions::compileRuleList(matchPastEndOfStringFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("MatchPastEndOfString", extension);

    testURL(backend, URL(URL(), "http://webkit.org/"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foo"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foobarbar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foofoobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foobarfoobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foob"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "http://webkit.org/foor"), { ContentExtensions::ActionType::BlockLoad });
}

const char* startOfLineAssertionFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"^foobar\"}}]";

TEST_F(ContentExtensionTest, StartOfLineAssertion)
{
    auto extensionData = ContentExtensions::compileRuleList(startOfLineAssertionFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("StartOfLineAssertion", extension);

    testURL(backend, URL(URL(), "foobar://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "foobars:///foobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "foobarfoobar:///foobarfoobarfoobar"), { ContentExtensions::ActionType::BlockLoad });

    testURL(backend, URL(URL(), "http://webkit.org/foobarfoo"), { });
    testURL(backend, URL(URL(), "http://webkit.org/foobarf"), { });
    testURL(backend, URL(URL(), "http://foobar.org/"), { });
    testURL(backend, URL(URL(), "http://foobar.org/"), { });
}

const char* endOfLineAssertionFilter = "[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*foobar$\"}}]";

TEST_F(ContentExtensionTest, EndOfLineAssertion)
{
    auto extensionData = ContentExtensions::compileRuleList(endOfLineAssertionFilter);
    auto extension = InMemoryCompiledContentExtension::create(WTF::move(extensionData));

    ContentExtensions::ContentExtensionsBackend backend;
    backend.addContentExtension("EndOfLineAssertion", extension);

    testURL(backend, URL(URL(), "http://webkit.org/foobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "file:///foobar"), { ContentExtensions::ActionType::BlockLoad });
    testURL(backend, URL(URL(), "file:///foobarfoobarfoobar"), { ContentExtensions::ActionType::BlockLoad });

    testURL(backend, URL(URL(), "http://webkit.org/foobarfoo"), { });
    testURL(backend, URL(URL(), "http://webkit.org/foobarf"), { });
}

} // namespace TestWebKitAPI
