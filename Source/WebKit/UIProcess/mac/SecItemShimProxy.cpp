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

#include "Connection.h"
#include "Logging.h"
#include "SecItemRequestData.h"
#include "SecItemResponseData.h"
#include "SecItemShimProxyMessages.h"
#include <Security/SecBase.h>
#include <Security/SecIdentity.h>
#include <Security/SecItem.h>
#include <WebCore/CertificateInfo.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/cf/VectorCF.h>

#if HAVE(SEC_KEYCHAIN)
#import <Security/SecKeychainItem.h>
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
WTF_DECLARE_CF_TYPE_TRAIT(SecKeychainItem);
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SecItemShimProxy);

#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)

// We received these dictionaries over IPC so they shouldn't contain any "in-memory" objects (rdar://104253249).
static bool dictionaryContainsInMemoryObject(CFDictionaryRef dictionary)
{
    if (!dictionary)
        return false;

    // kSecUseItemList is deprecated on iOS 12+.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (CFDictionaryContainsKey(dictionary, kSecUseItemList))
        return true;
ALLOW_DEPRECATED_DECLARATIONS_END

    return CFDictionaryContainsKey(dictionary, kSecValueRef);
}

SecItemShimProxy& SecItemShimProxy::singleton()
{
    static NeverDestroyed<UniqueRef<SecItemShimProxy>> proxy = makeUniqueRefWithoutRefCountedCheck<SecItemShimProxy>();
    return proxy.get();
}

SecItemShimProxy::SecItemShimProxy()
    : m_queue(WorkQueue::create("com.apple.WebKit.SecItemShimProxy"_s))
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

void SecItemShimProxy::secItemRequest(IPC::Connection& connection, const SecItemRequestData& request, CompletionHandler<void(std::optional<SecItemResponseData>&&)>&& response)
{
    MESSAGE_CHECK_COMPLETION(!dictionaryContainsInMemoryObject(request.protectedQuery().get()), connection, response(SecItemResponseData { errSecParam, nullptr }));
    MESSAGE_CHECK_COMPLETION(!dictionaryContainsInMemoryObject(request.protectedAttributesToMatch().get()), connection, response(SecItemResponseData { errSecParam, nullptr }));

    switch (request.type()) {
    case SecItemRequestData::Type::Invalid:
        LOG_ERROR("SecItemShimProxy::secItemRequest received an invalid data request. Please file a bug if you know how you caused this.");
        response(SecItemResponseData { errSecParam, nullptr });
        break;

    case SecItemRequestData::Type::CopyMatching: {
        CFTypeRef resultRawObject = nullptr;
        OSStatus resultCode = SecItemCopyMatching(request.protectedQuery().get(), &resultRawObject);
        auto result = adoptCF(resultRawObject);

        SecItemResponseData::Result resultData;
        if (result) {
            auto resultType = CFGetTypeID(result.get());
            CFArrayRef resultArray = (CFArrayRef)result.get();
            if (resultType == CFArrayGetTypeID() && CFArrayGetCount(resultArray)) {
                auto containedType = CFGetTypeID(RetainPtr { CFArrayGetValueAtIndex(resultArray, 0) }.get());
                if (containedType == SecCertificateGetTypeID()) {
                    resultData = Vector<RetainPtr<SecCertificateRef>>(makeVector(resultArray, [] (SecCertificateRef element) {
                        return std::optional(RetainPtr<SecCertificateRef> { element });
                    }));
#if HAVE(SEC_KEYCHAIN)
                    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                } else if (containedType == SecKeychainItemGetTypeID()) {
                    resultData = Vector<RetainPtr<SecKeychainItemRef>>(makeVector(resultArray, [] (SecKeychainItemRef element) {
                        return std::optional(RetainPtr<SecKeychainItemRef> { element });
                    }));
                    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
                } else
                    resultData = WTFMove(result);
            } else
                resultData = WTFMove(result);
        }
        response(SecItemResponseData { resultCode, WTFMove(resultData) });
        break;
    }

    case SecItemRequestData::Type::Add: {
        // Return value of SecItemAdd is often ignored. Even if it isn't, we don't have the ability to
        // serialize SecKeychainItemRef.
        OSStatus resultCode = SecItemAdd(request.protectedQuery().get(), nullptr);
        response(SecItemResponseData { resultCode, nullptr });
        break;
    }

    case SecItemRequestData::Type::Update: {
        OSStatus resultCode = SecItemUpdate(request.protectedQuery().get(), request.protectedAttributesToMatch().get());
        response(SecItemResponseData { resultCode, nullptr });
        break;
    }

    case SecItemRequestData::Type::Delete: {
        OSStatus resultCode = SecItemDelete(request.protectedQuery().get());
        response(SecItemResponseData { resultCode, nullptr });
        break;
    }
    }
}

void SecItemShimProxy::secItemRequestSync(IPC::Connection& connection, const SecItemRequestData& data, CompletionHandler<void(std::optional<SecItemResponseData>&&)>&& completionHandler)
{
    secItemRequest(connection, data, WTFMove(completionHandler));
}

#undef MESSAGE_CHECK_COMPLETION

} // namespace WebKit

#endif // ENABLE(SEC_ITEM_SHIM)
