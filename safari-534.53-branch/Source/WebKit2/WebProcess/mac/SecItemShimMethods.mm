/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SecItemShimMethods.h"

#if !defined(BUILDING_ON_SNOW_LEOPARD)

#import "CoreIPCClientRunLoop.h"
#import "SecItemRequestData.h"
#import "SecItemResponseData.h"
#import "WebProcess.h"
#import "WebProcessProxyMessages.h"
#import "WebProcessShim.h"
#import <Security/SecItem.h>
#import <dlfcn.h>

namespace WebKit {

// FIXME (https://bugs.webkit.org/show_bug.cgi?id=60975) - Once CoreIPC supports sync messaging from a secondary thread,
// we can remove SecItemAPIContext and these 4 main-thread methods, and we can have the shim methods call out directly 
// from whatever thread they're on.

struct SecItemAPIContext {
    CFDictionaryRef query;
    CFDictionaryRef attributesToUpdate;
    CFTypeRef resultObject;
    OSStatus resultCode;
};

static void webSecItemCopyMatchingMainThread(void* voidContext)
{
    SecItemAPIContext* context = (SecItemAPIContext*)voidContext;
    
    SecItemRequestData requestData(context->query);
    SecItemResponseData response;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::SecItemCopyMatching(requestData), Messages::WebProcessProxy::SecItemCopyMatching::Reply(response), 0)) {
        context->resultCode = errSecInteractionNotAllowed;
        ASSERT_NOT_REACHED();
        return;
    }
    
    context->resultObject = response.resultObject().leakRef();
    context->resultCode = response.resultCode();
}

static OSStatus webSecItemCopyMatching(CFDictionaryRef query, CFTypeRef* result)
{
    SecItemAPIContext context;
    context.query = query;
    
    callOnCoreIPCClientRunLoopAndWait(webSecItemCopyMatchingMainThread, &context);
    
    if (result)
        *result = context.resultObject;
    return context.resultCode;
}

static void webSecItemAddOnMainThread(void* voidContext)
{
    SecItemAPIContext* context = (SecItemAPIContext*)voidContext;
    
    SecItemRequestData requestData(context->query);
    SecItemResponseData response;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::SecItemAdd(requestData), Messages::WebProcessProxy::SecItemAdd::Reply(response), 0)) {
        context->resultCode = errSecInteractionNotAllowed;
        ASSERT_NOT_REACHED();
        return;
    }
    
    context->resultObject = response.resultObject().leakRef();
    context->resultCode = response.resultCode();
}

static OSStatus webSecItemAdd(CFDictionaryRef query, CFTypeRef* result)
{
    SecItemAPIContext context;
    context.query = query;
    
    callOnCoreIPCClientRunLoopAndWait(webSecItemAddOnMainThread, &context);
    
    if (result)
        *result = context.resultObject;
    return context.resultCode;
}

static void webSecItemUpdateOnMainThread(void* voidContext)
{
    SecItemAPIContext* context = (SecItemAPIContext*)voidContext;
    
    SecItemRequestData requestData(context->query, context->attributesToUpdate);
    SecItemResponseData response;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::SecItemUpdate(requestData), Messages::WebProcessProxy::SecItemUpdate::Reply(response), 0)) {
        context->resultCode = errSecInteractionNotAllowed;
        ASSERT_NOT_REACHED();
        return;
    }
    
    context->resultCode = response.resultCode();
}

static OSStatus webSecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
    SecItemAPIContext context;
    context.query = query;
    context.attributesToUpdate = attributesToUpdate;
    
    callOnCoreIPCClientRunLoopAndWait(webSecItemUpdateOnMainThread, &context);
    
    return context.resultCode;
}

static void webSecItemDeleteOnMainThread(void* voidContext)
{
    SecItemAPIContext* context = (SecItemAPIContext*)voidContext;
    
    SecItemRequestData requestData(context->query);
    SecItemResponseData response;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::SecItemDelete(requestData), Messages::WebProcessProxy::SecItemDelete::Reply(response), 0)) {
        context->resultCode = errSecInteractionNotAllowed;
        ASSERT_NOT_REACHED();
        return;
    }
    
    context->resultCode = response.resultCode();
}

static OSStatus webSecItemDelete(CFDictionaryRef query)
{
    SecItemAPIContext context;
    context.query = query;
    
    callOnCoreIPCClientRunLoopAndWait(webSecItemDeleteOnMainThread, &context);

    return context.resultCode;
}

void initializeSecItemShim()
{
    const WebProcessSecItemShimCallbacks callbacks = {
        webSecItemCopyMatching,
        webSecItemAdd,
        webSecItemUpdate,
        webSecItemDelete
    };
    
    WebProcessSecItemShimInitializeFunc func = reinterpret_cast<WebProcessSecItemShimInitializeFunc>(dlsym(RTLD_DEFAULT, "WebKitWebProcessSecItemShimInitialize"));
    func(callbacks);
}

} // namespace WebKit

#endif // !BUILDING_ON_SNOW_LEOPARD
