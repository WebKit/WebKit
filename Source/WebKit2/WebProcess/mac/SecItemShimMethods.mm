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

#import "KeychainShimResponseMap.h"
#import "SecItemRequestData.h"
#import "SecItemResponseData.h"
#import "WebProcess.h"
#import "WebProcessProxyMessages.h"
#import "WebProcessShim.h"
#import <Security/SecItem.h>
#import <dlfcn.h>

namespace WebKit {

static KeychainShimResponseMap<SecItemResponseData>& responseMap()
{
    AtomicallyInitializedStatic(KeychainShimResponseMap<SecItemResponseData>&, responseMap = *new KeychainShimResponseMap<SecItemResponseData>);
    return responseMap;
}

static uint64_t generateSecItemRequestID()
{
    static int64_t uniqueSecItemRequestID;
    return OSAtomicIncrement64Barrier(&uniqueSecItemRequestID);
}

void didReceiveSecItemResponse(uint64_t requestID, const SecItemResponseData& response)
{
    responseMap().didReceiveResponse(requestID, adoptPtr(new SecItemResponseData(response)));
}

static PassOwnPtr<SecItemResponseData> sendSeqItemRequest(SecItemRequestData::Type requestType, CFDictionaryRef query, CFDictionaryRef attributesToMatch = 0)
{
    uint64_t requestID = generateSecItemRequestID();
    if (!WebProcess::shared().connection()->send(Messages::WebProcessProxy::SecItemRequest(requestID, SecItemRequestData(requestType, query, attributesToMatch)), 0))
        return nullptr;

    return responseMap().waitForResponse(requestID);
}

static OSStatus webSecItemCopyMatching(CFDictionaryRef query, CFTypeRef* result)
{
    OwnPtr<SecItemResponseData> response = sendSeqItemRequest(SecItemRequestData::CopyMatching, query);
    if (!response)
        return errSecInteractionNotAllowed;

    *result = response->resultObject().leakRef();
    return response->resultCode();
}

static OSStatus webSecItemAdd(CFDictionaryRef query, CFTypeRef* result)
{
    OwnPtr<SecItemResponseData> response = sendSeqItemRequest(SecItemRequestData::Add, query);
    if (!response)
        return errSecInteractionNotAllowed;
    
    *result = response->resultObject().leakRef();
    return response->resultCode();
}

static OSStatus webSecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
    OwnPtr<SecItemResponseData> response = sendSeqItemRequest(SecItemRequestData::Update, query, attributesToUpdate);
    if (!response)
        return errSecInteractionNotAllowed;
    
    return response->resultCode();
}

static OSStatus webSecItemDelete(CFDictionaryRef query)
{
    OwnPtr<SecItemResponseData> response = sendSeqItemRequest(SecItemRequestData::Delete, query);
    if (!response)
        return errSecInteractionNotAllowed;
    
    return response->resultCode();
}

void initializeSecItemShim()
{
    return;
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
