/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "IDBRequest.h"

#include "DOMStringList.h"
#include "Document.h"
#include "Frame.h"
#include "FrameTestHelpers.h"
#include "IDBCursorBackendInterface.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBTransactionCoordinator.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebView.h"

#include <gtest/gtest.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;
using namespace WebKit;

namespace {

class IDBRequestTest : public testing::Test {
public:
    IDBRequestTest()
        : m_webView(0)
    {
    }

    void SetUp() OVERRIDE
    {
        m_webView = FrameTestHelpers::createWebViewAndLoad("about:blank");
        m_webView->setFocus(true);
    }

    void TearDown() OVERRIDE
    {
        m_webView->close();
    }

    v8::Handle<v8::Context> context()
    {
        return static_cast<WebFrameImpl*>(m_webView->mainFrame())->frame()->script()->mainWorldContext();
    }

    ScriptExecutionContext* scriptExecutionContext()
    {
        return static_cast<WebFrameImpl*>(m_webView->mainFrame())->frame()->document();
    }

private:
    WebView* m_webView;
};

TEST_F(IDBRequestTest, EventsAfterStopping)
{
    v8::HandleScope handleScope;
    v8::Context::Scope scope(context());

    IDBTransaction* transaction = 0;
    RefPtr<IDBRequest> request = IDBRequest::create(scriptExecutionContext(), IDBAny::createInvalid(), transaction);
    EXPECT_EQ(request->readyState(), "pending");
    request->stop();

    // Ensure none of the following raise assertions in stopped state:
    request->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "Description goes here."));
    request->onSuccess(DOMStringList::create());
    request->onSuccess(PassRefPtr<IDBCursorBackendInterface>(), IDBKey::createInvalid(), IDBKey::createInvalid(), 0);
    request->onSuccess(IDBKey::createInvalid());
    request->onSuccess(PassRefPtr<SharedBuffer>(0));
    request->onSuccess(PassRefPtr<SharedBuffer>(0), IDBKey::createInvalid(), IDBKeyPath());
    request->onSuccess(IDBKey::createInvalid(), IDBKey::createInvalid(), 0);
}

TEST_F(IDBRequestTest, AbortErrorAfterAbort)
{
    v8::HandleScope handleScope;
    v8::Context::Scope scope(context());

    IDBTransaction* transaction = 0;
    RefPtr<IDBRequest> request = IDBRequest::create(scriptExecutionContext(), IDBAny::createInvalid(), transaction);
    EXPECT_EQ(request->readyState(), "pending");

    // Simulate the IDBTransaction having received onAbort from back end and aborting the request:
    request->abort();

    // Now simulate the back end having fired an abort error at the request to clear up any intermediaries.
    // Ensure an assertion is not raised.
    request->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "Description goes here."));
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
