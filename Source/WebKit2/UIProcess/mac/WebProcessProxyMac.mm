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
#import "WebProcessProxy.h"

#import "SecItemRequestData.h"
#import "SecItemResponseData.h"
#import "SecKeychainItemRequestData.h"
#import "SecKeychainItemResponseData.h"
#import "WebProcessMessages.h"
#import "WKFullKeyboardAccessWatcher.h"
#import <Security/SecItem.h>

namespace WebKit {

static void handleSecItemRequest(CoreIPC::Connection* connection, uint64_t requestID, const SecItemRequestData& request)
{
    SecItemResponseData response;

    switch (request.type()) {
        case SecItemRequestData::CopyMatching: {
            CFTypeRef resultObject = 0;
            OSStatus resultCode = SecItemCopyMatching(request.query(), &resultObject);
            response = SecItemResponseData(resultCode, adoptCF(resultObject).get());
            break;
        }

        case SecItemRequestData::Add: {
            CFTypeRef resultObject = 0;
            OSStatus resultCode = SecItemAdd(request.query(), &resultObject);
            response = SecItemResponseData(resultCode, adoptCF(resultObject).get());
            break;
        }

        case SecItemRequestData::Update: {
            OSStatus resultCode = SecItemUpdate(request.query(), request.attributesToMatch());
            response = SecItemResponseData(resultCode, 0);
            break;
        }

        case SecItemRequestData::Delete: {
            OSStatus resultCode = SecItemDelete(request.query());
            response = SecItemResponseData(resultCode, 0);
            break;
        }

        default:
            return;
    }

    connection->send(Messages::WebProcess::SecItemResponse(requestID, response), 0);
}

void WebProcessProxy::secItemRequest(CoreIPC::Connection* connection, uint64_t requestID, const SecItemRequestData& request)
{
    // Since we don't want the connection work queue to be held up, we do all
    // keychain interaction work on a global dispatch queue.
    dispatch_queue_t keychainWorkQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_async(keychainWorkQueue, bind(handleSecItemRequest, RefPtr<CoreIPC::Connection>(connection), requestID, request));
}

static void handleSecKeychainItemRequest(CoreIPC::Connection* connection, uint64_t requestID, const SecKeychainItemRequestData& request)
{
    SecKeychainItemResponseData response;

    switch (request.type()) {
        case SecKeychainItemRequestData::CopyContent: {
            SecKeychainItemRef item = request.keychainItem();
            SecItemClass itemClass;
            SecKeychainAttributeList* attrList = request.attributeList();    
            UInt32 length = 0;
            void* outData = 0;

            OSStatus resultCode = SecKeychainItemCopyContent(item, &itemClass, attrList, &length, &outData);
            RetainPtr<CFDataRef> data(AdoptCF, CFDataCreate(0, static_cast<const UInt8*>(outData), length));
            response = SecKeychainItemResponseData(resultCode, itemClass, attrList, data.get());

            SecKeychainItemFreeContent(attrList, outData);
            break;
        }

        case SecKeychainItemRequestData::CreateFromContent: {
            SecKeychainItemRef keychainItem;

            OSStatus resultCode = SecKeychainItemCreateFromContent(request.itemClass(), request.attributeList(), request.length(), request.data(), 0, 0, &keychainItem);
            response = SecKeychainItemResponseData(resultCode,  adoptCF(keychainItem));
            break;
        }

        case SecKeychainItemRequestData::ModifyContent: {
            OSStatus resultCode = SecKeychainItemModifyContent(request.keychainItem(), request.attributeList(), request.length(), request.data());
            response = resultCode;
            break;
        }

        default:
            return;
    }

    connection->send(Messages::WebProcess::SecKeychainItemResponse(requestID, response), 0);
}

void WebProcessProxy::secKeychainItemRequest(CoreIPC::Connection* connection, uint64_t requestID, const SecKeychainItemRequestData& request)
{
    // Since we don't want the connection work queue to be held up, we do all
    // keychain interaction work on a global dispatch queue.
    dispatch_queue_t keychainWorkQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_async(keychainWorkQueue, bind(handleSecKeychainItemRequest, RefPtr<CoreIPC::Connection>(connection), requestID, request));
}

bool WebProcessProxy::fullKeyboardAccessEnabled()
{
    return [WKFullKeyboardAccessWatcher fullKeyboardAccessEnabled];
}

} // namespace WebKit
