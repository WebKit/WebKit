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
#include "NetworkProcess.h"
#include "SecItemRequestData.h"
#include "SecItemResponseData.h"
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

static WeakPtr<NetworkProcess>& globalNetworkProcess()
{
    static NeverDestroyed<WeakPtr<NetworkProcess>> networkProcess;
    return networkProcess.get();
}

static std::optional<SecItemResponseData> sendSecItemRequest(SecItemRequestData::Type requestType, CFDictionaryRef query, CFDictionaryRef attributesToMatch = 0)
{
    if (RunLoop::isMain()) {
        auto sendSync = globalNetworkProcess()->parentProcessConnection()->sendSync(Messages::SecItemShimProxy::SecItemRequestSync(SecItemRequestData(requestType, query, attributesToMatch)), 0);
        auto [response] = sendSync.takeReplyOr(std::nullopt);
        return response;
    }

    std::optional<SecItemResponseData> response;
    BinarySemaphore semaphore;

    RunLoop::main().dispatch([&] {
        if (!globalNetworkProcess()) {
            semaphore.signal();
            return;
        }

        globalNetworkProcess()->parentProcessConnection()->sendWithAsyncReply(Messages::SecItemShimProxy::SecItemRequest(SecItemRequestData(requestType, query, attributesToMatch)), [&](auto reply) {
            if (reply)
                response = WTFMove(*reply);

            semaphore.signal();
        });
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

void initializeSecItemShim(NetworkProcess& process)
{
    globalNetworkProcess() = process;

    struct _CFNFrameworksStubs stubs = {
        .version = 0,
        .SecItem_stub_CopyMatching = webSecItemCopyMatching,
        .SecItem_stub_Add = webSecItemAdd,
        .SecItem_stub_Update = webSecItemUpdate,
        .SecItem_stub_Delete = webSecItemDelete,
    };

    _CFURLConnectionSetFrameworkStubs(&stubs);
}

} // namespace WebKit

#endif // ENABLE(SEC_ITEM_SHIM)
