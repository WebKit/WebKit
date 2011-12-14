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
#import "WebProcessShim.h"

#import <Security/SecItem.h>
#import <wtf/Platform.h>

#define DYLD_INTERPOSE(_replacement,_replacee) \
    __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
    __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

namespace WebKit {

#if !defined(BUILDING_ON_SNOW_LEOPARD)

extern "C" void WebKitWebProcessSecItemShimInitialize(const WebProcessSecItemShimCallbacks&);

static WebProcessSecItemShimCallbacks secItemShimCallbacks;

static OSStatus shimSecItemCopyMatching(CFDictionaryRef query, CFTypeRef* result)
{
    return secItemShimCallbacks.secItemCopyMatching(query, result);
}

static OSStatus shimSecItemAdd(CFDictionaryRef query, CFTypeRef* result)
{
    return secItemShimCallbacks.secItemAdd(query, result);
}

static OSStatus shimSecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
    return secItemShimCallbacks.secItemUpdate(query, attributesToUpdate);
}

static OSStatus shimSecItemDelete(CFDictionaryRef query)
{
    return secItemShimCallbacks.secItemDelete(query);
}

DYLD_INTERPOSE(shimSecItemCopyMatching, SecItemCopyMatching)
DYLD_INTERPOSE(shimSecItemAdd, SecItemAdd)
DYLD_INTERPOSE(shimSecItemUpdate, SecItemUpdate)
DYLD_INTERPOSE(shimSecItemDelete, SecItemDelete)

__attribute__((visibility("default")))
void WebKitWebProcessSecItemShimInitialize(const WebProcessSecItemShimCallbacks& callbacks)
{
    secItemShimCallbacks = callbacks;
}

#endif // !defined(BUILDING_ON_SNOW_LEOPARD)

#if defined(BUILDING_ON_SNOW_LEOPARD)

extern "C" void WebKitWebProcessKeychainItemShimInitialize(const WebProcessKeychainItemShimCallbacks&);

static WebProcessKeychainItemShimCallbacks keychainItemShimCallbacks;

static OSStatus shimSecKeychainItemCopyContent(SecKeychainItemRef item, SecItemClass* itemClass, SecKeychainAttributeList* attrList, UInt32* length, void** outData)
{
    return keychainItemShimCallbacks.secKeychainItemCopyContent(item, itemClass, attrList, length, outData);
}

static OSStatus shimSecKeychainItemCreateFromContent(SecItemClass itemClass, SecKeychainAttributeList* attrList, UInt32 length, const void* data, 
                                                     SecKeychainRef keychainRef, SecAccessRef initialAccess, SecKeychainItemRef *itemRef)
{
    // We don't support shimming SecKeychainItemCreateFromContent with Keychain or Access arguments at this time
    if (keychainRef || initialAccess)
        return SecKeychainItemCreateFromContent(itemClass, attrList, length, data, keychainRef, initialAccess, itemRef);
    return keychainItemShimCallbacks.secKeychainItemCreateFromContent(itemClass, attrList, length, data, itemRef);
}

static OSStatus shimSecKeychainItemModifyContent(SecKeychainItemRef itemRef, const SecKeychainAttributeList* attrList, UInt32 length, const void* data)
{
    return keychainItemShimCallbacks.secKeychainItemModifyContent(itemRef, attrList, length, data);
}

static OSStatus shimSecKeychainItemFreeContent(SecKeychainAttributeList* attrList, void* data)
{    
    bool attrListHandled = !attrList || keychainItemShimCallbacks.freeAttributeListContent(attrList);
    bool keychainItemContentHandled = !data || keychainItemShimCallbacks.freeKeychainItemContentData(data);
    
    // If both items were handled by the shim handlers, return now.
    if (attrListHandled && keychainItemContentHandled)
        return errSecSuccess;
    
    // Have the native function handle whichever item wasn't already handled.
    return SecKeychainItemFreeContent(attrListHandled ? attrList : NULL, keychainItemContentHandled ? data : NULL);
}

DYLD_INTERPOSE(shimSecKeychainItemCopyContent, SecKeychainItemCopyContent)
DYLD_INTERPOSE(shimSecKeychainItemCreateFromContent, SecKeychainItemCreateFromContent)
DYLD_INTERPOSE(shimSecKeychainItemModifyContent, SecKeychainItemModifyContent)
DYLD_INTERPOSE(shimSecKeychainItemFreeContent, SecKeychainItemFreeContent)

__attribute__((visibility("default")))
void WebKitWebProcessKeychainItemShimInitialize(const WebProcessKeychainItemShimCallbacks& callbacks)
{
    keychainItemShimCallbacks = callbacks;
}

#endif // defined(BUILDING_ON_SNOW_LEOPARD)

} // namespace WebKit
