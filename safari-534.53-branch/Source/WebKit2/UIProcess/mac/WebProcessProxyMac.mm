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
#import <Security/SecItem.h>

namespace WebKit {

void WebProcessProxy::secItemCopyMatching(const SecItemRequestData& queryData, SecItemResponseData& result)
{
    CFDictionaryRef query = queryData.query();
    CFTypeRef resultObject;
    OSStatus resultCode;

    resultCode = SecItemCopyMatching(query, &resultObject);

    result = SecItemResponseData(resultCode, resultObject);
}

void WebProcessProxy::secItemAdd(const SecItemRequestData& queryData, SecItemResponseData& result)
{
    CFDictionaryRef query = queryData.query();
    CFTypeRef resultObject;
    OSStatus resultCode;

    resultCode = SecItemAdd(query, &resultObject);

    result = SecItemResponseData(resultCode, resultObject);
}

void WebProcessProxy::secItemUpdate(const SecItemRequestData& queryData, SecItemResponseData& result)
{
    CFDictionaryRef query = queryData.query();
    CFDictionaryRef attributesToMatch = queryData.attributesToMatch();
    OSStatus resultCode;

    resultCode = SecItemUpdate(query, attributesToMatch);

    result = SecItemResponseData(resultCode, 0);
}

void WebProcessProxy::secItemDelete(const SecItemRequestData& queryData, SecItemResponseData& result)
{
    CFDictionaryRef query = queryData.query();
    OSStatus resultCode;

    resultCode = SecItemDelete(query);

    result = SecItemResponseData(resultCode, 0);
}

void WebProcessProxy::secKeychainItemCopyContent(const SecKeychainItemRequestData& request, SecKeychainItemResponseData& response)
{
    SecKeychainItemRef item = request.keychainItem();
    SecItemClass itemClass;
    SecKeychainAttributeList* attrList = request.attributeList();    
    UInt32 length = 0;
    void* outData = 0;

    OSStatus resultCode = SecKeychainItemCopyContent(item, &itemClass, attrList, &length, &outData);
    
    RetainPtr<CFDataRef> data(AdoptCF, CFDataCreate(0, static_cast<const UInt8*>(outData), length));
    response = SecKeychainItemResponseData(resultCode, itemClass, attrList, data.get());
    
    SecKeychainItemFreeContent(attrList, outData);
}

void WebProcessProxy::secKeychainItemCreateFromContent(const SecKeychainItemRequestData& request, SecKeychainItemResponseData& response)
{
    SecKeychainItemRef keychainItem;
    
    OSStatus resultCode = SecKeychainItemCreateFromContent(request.itemClass(), request.attributeList(), request.length(), request.data(), 0, 0, &keychainItem);

    response = SecKeychainItemResponseData(resultCode, RetainPtr<SecKeychainItemRef>(AdoptCF, keychainItem));
}

void WebProcessProxy::secKeychainItemModifyContent(const SecKeychainItemRequestData& request, SecKeychainItemResponseData& response)
{
    OSStatus resultCode = SecKeychainItemModifyContent(request.keychainItem(), request.attributeList(), request.length(), request.data());
    
    response = resultCode;
}

} // namespace WebKit
