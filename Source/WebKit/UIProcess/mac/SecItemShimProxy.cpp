/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SecItemShimProxy.h"

#if ENABLE(SEC_ITEM_SHIM)

#include "SecItemRequestData.h"
#include "SecItemResponseData.h"
#include "SecItemShimProxyMessages.h"
#include <Security/SecBase.h>
#include <Security/SecItem.h>

namespace WebKit {

SecItemShimProxy& SecItemShimProxy::singleton()
{
    static SecItemShimProxy* proxy;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        proxy = new SecItemShimProxy;
    });
    return *proxy;
}

SecItemShimProxy::SecItemShimProxy()
    : m_queue(WorkQueue::create("com.apple.WebKit.SecItemShimProxy"))
{
}

SecItemShimProxy::~SecItemShimProxy()
{
    ASSERT_NOT_REACHED();
}

void SecItemShimProxy::initializeConnection(IPC::Connection& connection)
{
    connection.addMessageReceiver(m_queue.get(), *this, Messages::SecItemShimProxy::messageReceiverName());
}

void SecItemShimProxy::secItemRequest(const SecItemRequestData& request, CompletionHandler<void(std::optional<SecItemResponseData>&&)>&& response)
{
    switch (request.type()) {
    case SecItemRequestData::Type::Invalid:
        LOG_ERROR("SecItemShimProxy::secItemRequest received an invalid data request. Please file a bug if you know how you caused this.");
        response(SecItemResponseData { errSecParam, nullptr });
        break;

    case SecItemRequestData::Type::CopyMatching: {
        CFTypeRef resultObject = nullptr;
        OSStatus resultCode = SecItemCopyMatching(request.query(), &resultObject);
        response(SecItemResponseData { resultCode, adoptCF(resultObject) });
        break;
    }

    case SecItemRequestData::Type::Add: {
        // Return value of SecItemAdd is often ignored. Even if it isn't, we don't have the ability to
        // serialize SecKeychainItemRef.
        OSStatus resultCode = SecItemAdd(request.query(), nullptr);
        response(SecItemResponseData { resultCode, nullptr });
        break;
    }

    case SecItemRequestData::Type::Update: {
        OSStatus resultCode = SecItemUpdate(request.query(), request.attributesToMatch());
        response(SecItemResponseData { resultCode, nullptr });
        break;
    }

    case SecItemRequestData::Type::Delete: {
        OSStatus resultCode = SecItemDelete(request.query());
        response(SecItemResponseData { resultCode, nullptr });
        break;
    }
    }
}

void SecItemShimProxy::secItemRequestSync(const SecItemRequestData& data, CompletionHandler<void(std::optional<SecItemResponseData>&&)>&& completionHandler)
{
    secItemRequest(data, WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(SEC_ITEM_SHIM)
