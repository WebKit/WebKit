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
#include "WKUserContentControllerRef.h"

#include "APIArray.h"
#include "APICompletionListener.h"
#include "APIContentRuleList.h"
#include "APIFrameInfo.h"
#include "APIScriptMessage.h"
#include "APIUserScript.h"
#include "InjectUserScriptImmediately.h"
#include "JavaScriptEvaluationResult.h"
#include "WKAPICast.h"
#include "WebPageProxy.h"
#include "WebScriptMessageHandler.h"
#include "WebUserContentControllerProxy.h"

using namespace WebKit;

WKTypeID WKUserContentControllerGetTypeID()
{
    return toAPI(WebUserContentControllerProxy::APIType);
}

WKUserContentControllerRef WKUserContentControllerCreate()
{
    return toAPILeakingRef(WebUserContentControllerProxy::create());
}

WKArrayRef WKUserContentControllerCopyUserScripts(WKUserContentControllerRef userContentControllerRef)
{
    return toAPILeakingRef(toProtectedImpl(userContentControllerRef)->userScripts().copy());
}

void WKUserContentControllerAddUserScript(WKUserContentControllerRef userContentControllerRef, WKUserScriptRef userScriptRef)
{
    toProtectedImpl(userContentControllerRef)->addUserScript(*toProtectedImpl(userScriptRef), InjectUserScriptImmediately::No);
}

void WKUserContentControllerRemoveAllUserScripts(WKUserContentControllerRef userContentControllerRef)
{
    toProtectedImpl(userContentControllerRef)->removeAllUserScripts();
}

void WKUserContentControllerAddUserContentFilter(WKUserContentControllerRef userContentControllerRef, WKUserContentFilterRef userContentFilterRef)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toProtectedImpl(userContentControllerRef)->addContentRuleList(*toProtectedImpl(userContentFilterRef));
#endif
}

void WKUserContentControllerRemoveAllUserContentFilters(WKUserContentControllerRef userContentControllerRef)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toProtectedImpl(userContentControllerRef)->removeAllContentRuleLists();
#endif
}

class WebScriptMessageClient : public WebScriptMessageHandler::Client {
    WTF_MAKE_TZONE_ALLOCATED(WebScriptMessageClient);
public:
    WebScriptMessageClient(const String& name, WKScriptMessageHandlerCallback callback, const void* context)
        : m_name(name)
        , m_callback(callback)
        , m_context(context) { }
private:
    void didPostMessage(WebPageProxy& page, FrameInfoData&& frameInfo, API::ContentWorld&, JavaScriptEvaluationResult&& result, CompletionHandler<void(Expected<JavaScriptEvaluationResult, String>&&)>&& completionHandler) override
    {
        Ref message = API::ScriptMessage::create(result.toAPI(), page, API::FrameInfo::create(WTFMove(frameInfo)), m_name, API::ContentWorld::pageContentWorldSingleton());
        Ref listener = API::CompletionListener::create([completionHandler = WTFMove(completionHandler)] (WKTypeRef reply) mutable {
            if (auto result = JavaScriptEvaluationResult::extract(toProtectedImpl(reply).get()))
                return completionHandler(WTFMove(*result));
            completionHandler(makeUnexpected(String()));
        });
        m_callback(toAPI(message.ptr()), toAPI(listener.ptr()), m_context);
    }

    String m_name;
    WKScriptMessageHandlerCallback m_callback;
    const void* m_context;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebScriptMessageClient);

void WKUserContentControllerAddScriptMessageHandler(WKUserContentControllerRef userContentController, WKStringRef wkName, WKScriptMessageHandlerCallback callback, const void* context)
{
    String name = toWTFString(wkName);

    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<WebScriptMessageClient>(name, callback, context), name, API::ContentWorld::pageContentWorldSingleton());
    toProtectedImpl(userContentController)->addUserScriptMessageHandler(handler);
}

void WKUserContentControllerRemoveAllUserMessageHandlers(WKUserContentControllerRef userContentController)
{
    toProtectedImpl(userContentController)->removeAllUserMessageHandlers();
}
