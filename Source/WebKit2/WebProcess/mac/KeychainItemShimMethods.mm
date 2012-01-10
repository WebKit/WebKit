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
#import "KeychainItemShimMethods.h"

#if defined(BUILDING_ON_SNOW_LEOPARD)

#import "KeychainShimResponseMap.h"
#import "SecKeychainItemRequestData.h"
#import "SecKeychainItemResponseData.h"
#import "WebProcess.h"
#import "WebProcessProxyMessages.h"
#import "WebProcessShim.h"
#import <dlfcn.h>
#import <wtf/MainThread.h>

namespace WebKit {

// Methods to allow the shim to manage memory for its own AttributeList contents.
static HashSet<SecKeychainAttributeList*>& managedAttributeLists()
{
    AtomicallyInitializedStatic(HashSet<SecKeychainAttributeList*>&, managedAttributeLists = *new HashSet<SecKeychainAttributeList*>);

    return managedAttributeLists;
}

static Mutex& managedAttributeListsMutex()
{
    AtomicallyInitializedStatic(Mutex&, managedAttributeListsMutex = *new Mutex);
    return managedAttributeListsMutex;
}

static void allocateAttributeListContents(const Vector<KeychainAttribute>& attributes, SecKeychainAttributeList* attrList)
{
    if (!attrList)
        return;

    MutexLocker locker(managedAttributeListsMutex());

    ASSERT(!managedAttributeLists().contains(attrList));
    ASSERT(attributes.size() == attrList->count);
    
    managedAttributeLists().add(attrList);
    
    for (size_t i = 0; i < attrList->count; ++i) {
        ASSERT(attributes[i].tag == attrList->attr[i].tag);
        
        CFDataRef cfData = attributes[i].data.get();
        if (!cfData) {
            attrList->attr[i].length = 0;
            attrList->attr[i].data = 0;
            continue;
        }
            
        CFIndex length = CFDataGetLength(cfData);
        attrList->attr[i].length = length;
        attrList->attr[i].data = malloc(length);
        CFDataGetBytes(cfData, CFRangeMake(0, length), static_cast<UInt8*>(attrList->attr[i].data));
    }
}

// Methods to allow the shim to manage memory for its own KeychainItem content data.
static HashSet<void*>& managedKeychainItemContents()
{
    AtomicallyInitializedStatic(HashSet<void*>&, managedKeychainItemContents = *new HashSet<void*>);
    return managedKeychainItemContents;
}

static Mutex& managedKeychainItemContentsMutex()
{
    AtomicallyInitializedStatic(Mutex&, managedKeychainItemContentsMutex = *new Mutex);
    return managedKeychainItemContentsMutex;
}

static void allocateKeychainItemContentData(CFDataRef cfData, UInt32* length, void** data)
{
    ASSERT((length && data) || (!length && !data));
    if (!data)
        return;
        
    if (!cfData) {
        *data = 0;
        *length = 0;
        return;
    }
    
    *length = CFDataGetLength(cfData);
    *data = malloc(*length);
    CFDataGetBytes(cfData, CFRangeMake(0, *length), (UInt8*)*data);

    MutexLocker locker(managedKeychainItemContentsMutex());
    managedKeychainItemContents().add(*data);
}

static bool webFreeAttributeListContent(SecKeychainAttributeList* attrList)
{
    MutexLocker locker(managedAttributeListsMutex());

    if (!managedAttributeLists().contains(attrList))
        return false;

    for (size_t i = 0; i < attrList->count; ++i)
        free(attrList->attr[i].data);
    
    managedAttributeLists().remove(attrList);
    return true;
}

static bool webFreeKeychainItemContent(void* data)
{
    MutexLocker locker(managedKeychainItemContentsMutex());

    HashSet<void*>::iterator it = managedKeychainItemContents().find(data);
    if (it == managedKeychainItemContents().end())
        return false;

    managedKeychainItemContents().remove(it);
    return true;
}

static KeychainShimResponseMap<SecKeychainItemResponseData>& responseMap()
{
    AtomicallyInitializedStatic(KeychainShimResponseMap<SecKeychainItemResponseData>&, responseMap = *new KeychainShimResponseMap<SecKeychainItemResponseData>);
    return responseMap;
}

static uint64_t generateSecKeychainItemRequestID()
{
    static int64_t uniqueSecKeychainItemRequestID;
    return OSAtomicIncrement64Barrier(&uniqueSecKeychainItemRequestID);
}

void didReceiveSecKeychainItemResponse(uint64_t requestID, const SecKeychainItemResponseData& response)
{
    responseMap().didReceiveResponse(requestID, adoptPtr(new SecKeychainItemResponseData(response)));
}

static PassOwnPtr<SecKeychainItemResponseData> sendSeqKeychainItemRequest(const SecKeychainItemRequestData& request)
{
    uint64_t requestID = generateSecKeychainItemRequestID();
    if (!WebProcess::shared().connection()->send(Messages::WebProcessProxy::SecKeychainItemRequest(requestID, request), 0))
        return nullptr;

    return responseMap().waitForResponse(requestID);
}

static OSStatus webSecKeychainItemCopyContent(SecKeychainItemRef item, SecItemClass* itemClass, SecKeychainAttributeList* attrList, UInt32* length, void** outData)
{
    SecKeychainItemRequestData request(SecKeychainItemRequestData::CopyContent, item, attrList);
    OwnPtr<SecKeychainItemResponseData> response = sendSeqKeychainItemRequest(request);
    if (!response) {
        ASSERT_NOT_REACHED();
        return errSecInteractionNotAllowed;
    }

    if (itemClass)
        *itemClass = response->itemClass();
    allocateAttributeListContents(response->attributes(), attrList);
    allocateKeychainItemContentData(response->data(), length, outData);

    // FIXME: should return response->resultCode(). Returning noErr is a workaround for <rdar://problem/9520886>;
    // the authentication should fail anyway, since on error no data will be returned.
    return noErr;
}

static OSStatus webSecKeychainItemCreateFromContent(SecItemClass itemClass, SecKeychainAttributeList* attrList, UInt32 length, const void* data, SecKeychainItemRef *item)
{
    SecKeychainItemRequestData request(SecKeychainItemRequestData::CreateFromContent, itemClass, attrList, length, data);
    OwnPtr<SecKeychainItemResponseData> response = sendSeqKeychainItemRequest(request);
    if (!response) {
        ASSERT_NOT_REACHED();
        return errSecInteractionNotAllowed;
    }

    if (item)
        *item = RetainPtr<SecKeychainItemRef>(response->keychainItem()).leakRef();

    return response->resultCode();
}

static OSStatus webSecKeychainItemModifyContent(SecKeychainItemRef itemRef, const SecKeychainAttributeList* attrList, UInt32 length, const void* data)
{
    SecKeychainItemRequestData request(SecKeychainItemRequestData::ModifyContent, itemRef, (SecKeychainAttributeList*)attrList, length, data);
    OwnPtr<SecKeychainItemResponseData> response = sendSeqKeychainItemRequest(request);
    if (!response) {
        ASSERT_NOT_REACHED();
        return errSecInteractionNotAllowed;
    }

    return response->resultCode();
}

void initializeKeychainItemShim()
{
    const WebProcessKeychainItemShimCallbacks callbacks = {
        webSecKeychainItemCopyContent,
        webSecKeychainItemCreateFromContent,
        webSecKeychainItemModifyContent,
        webFreeAttributeListContent,
        webFreeKeychainItemContent
    };
                                                                                                                                
    WebProcessKeychainItemShimInitializeFunc initializeFunction = reinterpret_cast<WebProcessKeychainItemShimInitializeFunc>(dlsym(RTLD_DEFAULT, "WebKitWebProcessKeychainItemShimInitialize"));
    initializeFunction(callbacks);
}

} // namespace WebKit

#endif // defined(BUILDING_ON_SNOW_LEOPARD)
