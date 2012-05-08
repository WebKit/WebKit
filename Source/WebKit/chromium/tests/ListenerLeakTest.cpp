/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "FrameTestHelpers.h"
#include "WebView.h"
#include <gtest/gtest.h>
#include <v8/include/v8-profiler.h>
#include <v8/include/v8.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

const v8::HeapGraphNode* GetProperty(const v8::HeapGraphNode* node, v8::HeapGraphEdge::Type type, const char* name)
{
    for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
        const v8::HeapGraphEdge* prop = node->GetChild(i);
        if (prop->GetType() == type) {
            v8::String::AsciiValue propName(prop->GetName());
            if (!strcmp(name, *propName))
                return prop->GetToNode();
        }
    }
    return 0;
}

int GetNumObjects(const char* constructor)
{
    v8::HandleScope scope;
    const v8::HeapSnapshot* snapshot = v8::HeapProfiler::TakeSnapshot(v8::String::New(""), v8::HeapSnapshot::kFull);
    if (!snapshot)
        return -1;
    int count = 0;
    for (int i = 0; i < snapshot->GetNodesCount(); ++i) {
        const v8::HeapGraphNode* node = snapshot->GetNode(i);
        if (node->GetType() != v8::HeapGraphNode::kObject)
            continue;
        v8::String::AsciiValue nodeName(node->GetName());
        if (!strcmp(constructor, *nodeName)) {
            const v8::HeapGraphNode* constructorProp = GetProperty(node, v8::HeapGraphEdge::kProperty, "constructor");
            // Skip an Object instance named after the constructor.
            if (constructorProp) {
                v8::String::AsciiValue constructorName(constructorProp->GetName());
                if (!strcmp(constructor, *constructorName))
                    continue;
            }
            ++count;
        }
    }
    return count;
}


class ListenerLeakTest : public testing::Test {
public:
    ListenerLeakTest() : m_webView(0) { }

    void RunTest(const std::string& filename)
    {
        std::string baseURL("http://www.example.com/");
        std::string fileName(filename);
        bool executeScript = true;
        FrameTestHelpers::registerMockedURLLoad(baseURL, fileName);
        m_webView = FrameTestHelpers::createWebViewAndLoad(baseURL + fileName, executeScript);
    }

    virtual void TearDown() OVERRIDE
    {
        if (m_webView)
            m_webView->close();
        webkit_support::UnregisterAllMockedURLs();
    }

protected:
    WebView* m_webView;
};


// This test tries to create a reference cycle between node and its listener.
// See http://crbug/17400.
TEST_F(ListenerLeakTest, ReferenceCycle)
{
    RunTest("listener/listener_leak1.html");
    ASSERT_EQ(0, GetNumObjects("EventListenerLeakTestObject1"));
}

// This test sets node onclick many times to expose a possible memory
// leak where all listeners get referenced by the node.
TEST_F(ListenerLeakTest, HiddenReferences)
{
    RunTest("listener/listener_leak2.html");
    ASSERT_EQ(1, GetNumObjects("EventListenerLeakTestObject2"));
}

} // namespace
