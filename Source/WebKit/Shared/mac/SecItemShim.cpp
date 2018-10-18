/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#include "SecItemShim.h"

#if ENABLE(SEC_ITEM_SHIM)

#include "BlockingResponseMap.h"
#include "ChildProcess.h"
#include "SecItemRequestData.h"
#include "SecItemResponseData.h"
#include "SecItemShimLibrary.h"
#include "SecItemShimProxyMessages.h"
#include <Security/Security.h>
#include <atomic>
#include <dlfcn.h>
#include <mutex>
#include <wtf/ProcessPrivilege.h>
#include <wtf/threads/BinarySemaphore.h>

#if USE(APPLE_INTERNAL_SDK)
#include <CFNetwork/CFURLConnectionPriv.h>
#else
struct _CFNFrameworksStubs {
    CFIndex version;

    OSStatus (*SecItem_stub_CopyMatching)(CFDictionaryRef query, CFTypeRef *result);
    OSStatus (*SecItem_stub_Add)(CFDictionaryRef attributes, CFTypeRef *result);
    OSStatus (*SecItem_stub_Update)(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);
    OSStatus (*SecItem_stub_Delete)(CFDictionaryRef query);
};
#endif

extern "C" void _CFURLConnectionSetFrameworkStubs(const struct _CFNFrameworksStubs* stubs);

namespace WebKit {

static ChildProcess* sharedProcess;

static WorkQueue& workQueue()
{
    static WorkQueue* workQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        workQueue = &WorkQueue::create("com.apple.WebKit.SecItemShim").leakRef();

    });

    return *workQueue;
}

static std::optional<SecItemResponseData> sendSecItemRequest(SecItemRequestData::Type requestType, CFDictionaryRef query, CFDictionaryRef attributesToMatch = 0)
{
    std::optional<SecItemResponseData> response;

    BinarySemaphore semaphore;

    sharedProcess->parentProcessConnection()->sendWithReply(Messages::SecItemShimProxy::SecItemRequest(SecItemRequestData(requestType, query, attributesToMatch)), 0, workQueue(), [&response, &semaphore](auto reply) {
        if (reply)
            response = WTFMove(std::get<0>(*reply));

        semaphore.signal();
    });

    semaphore.wait();

    return response;
}

static OSStatus webSecItemCopyMatching(CFDictionaryRef query, CFTypeRef* result)
{
    auto response = sendSecItemRequest(SecItemRequestData::CopyMatching, query);
    if (!response)
        return errSecInteractionNotAllowed;

    *result = response->resultObject().leakRef();
    return response->resultCode();
}

static OSStatus webSecItemAdd(CFDictionaryRef query, CFTypeRef* unusedResult)
{
    // Return value of SecItemAdd should be ignored for WebKit use cases. WebKit can't serialize SecKeychainItemRef, so we do not use it.
    // If someone passes a result value to be populated, the API contract is being violated so we should assert.
    if (unusedResult) {
        ASSERT_NOT_REACHED();
        return errSecParam;
    }

    auto response = sendSecItemRequest(SecItemRequestData::Add, query);
    if (!response)
        return errSecInteractionNotAllowed;

    return response->resultCode();
}

static OSStatus webSecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
    auto response = sendSecItemRequest(SecItemRequestData::Update, query, attributesToUpdate);
    if (!response)
        return errSecInteractionNotAllowed;
    
    return response->resultCode();
}

static OSStatus webSecItemDelete(CFDictionaryRef query)
{
    auto response = sendSecItemRequest(SecItemRequestData::Delete, query);
    if (!response)
        return errSecInteractionNotAllowed;
    
    return response->resultCode();
}

void initializeSecItemShim(ChildProcess& process)
{
    sharedProcess = &process;

#if PLATFORM(IOS_FAMILY)
    struct _CFNFrameworksStubs stubs = {
        .version = 0,
        .SecItem_stub_CopyMatching = webSecItemCopyMatching,
        .SecItem_stub_Add = webSecItemAdd,
        .SecItem_stub_Update = webSecItemUpdate,
        .SecItem_stub_Delete = webSecItemDelete,
    };

    _CFURLConnectionSetFrameworkStubs(&stubs);
#endif

#if PLATFORM(MAC)
    const SecItemShimCallbacks callbacks = {
        webSecItemCopyMatching,
        webSecItemAdd,
        webSecItemUpdate,
        webSecItemDelete
    };
    
    SecItemShimInitializeFunc func = reinterpret_cast<SecItemShimInitializeFunc>(dlsym(RTLD_DEFAULT, "WebKitSecItemShimInitialize"));
    func(callbacks);
#endif
}

} // namespace WebKit

#endif // ENABLE(SEC_ITEM_SHIM)
