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

#import "CoreIPCClientRunLoop.h"
#import "SecKeychainItemRequestData.h"
#import "SecKeychainItemResponseData.h"
#import "WebProcess.h"
#import "WebProcessProxyMessages.h"
#import "WebProcessShim.h"
#import <dlfcn.h>

namespace WebKit {

// Methods to allow the shim to manage memory for its own AttributeList contents.
static HashSet<SecKeychainAttributeList*>& shimManagedAttributeLists()
{
    DEFINE_STATIC_LOCAL(HashSet<SecKeychainAttributeList*>, set, ());
    return set;
}

static void freeAttributeListContents(SecKeychainAttributeList* attrList)
{
    ASSERT(shimManagedAttributeLists().contains(attrList));
    ASSERT(attrList);
    
    for (size_t i = 0; i < attrList->count; ++i)
        free(attrList->attr[i].data);

    shimManagedAttributeLists().remove(attrList);
}

static void allocateAttributeListContents(const Vector<KeychainAttribute>& attributes, SecKeychainAttributeList* attrList)
{
    ASSERT(isMainThread());
    
    if (!attrList)
        return;
    
    ASSERT(!shimManagedAttributeLists().contains(attrList));
    ASSERT(attributes.size() == attrList->count);
    
    shimManagedAttributeLists().add(attrList);
    
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
static HashSet<void*>& shimManagedKeychainItemContents()
{
    DEFINE_STATIC_LOCAL(HashSet<void*>, set, ());
    return set;
}

static void allocateKeychainItemContentData(CFDataRef cfData, UInt32* length, void** data)
{
    ASSERT(isMainThread());
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
    shimManagedKeychainItemContents().add(*data);
}

// FIXME (https://bugs.webkit.org/show_bug.cgi?id=60975) - Once CoreIPC supports sync messaging from a secondary thread,
// we can remove FreeAttributeListContext, FreeKeychainItemDataContext, KeychainItemAPIContext, and these 5 main-thread methods, 
// and we can have the shim methods call out directly from whatever thread they're called on.

struct FreeAttributeListContext {
    SecKeychainAttributeList* attrList;
    bool freed;
};

static void webFreeAttributeListContentOnMainThread(void* voidContext)
{
    FreeAttributeListContext* context = (FreeAttributeListContext*)voidContext;
    
    if (!shimManagedAttributeLists().contains(context->attrList)) {
        context->freed = false;
        return;
    }
    
    freeAttributeListContents(context->attrList);
    context->freed = true;
}

static bool webFreeAttributeListContent(SecKeychainAttributeList* attrList)
{
    FreeAttributeListContext context;
    context.attrList = attrList;
    
    callOnCoreIPCClientRunLoopAndWait(webFreeAttributeListContentOnMainThread, &context);

    return context.freed;
}

struct FreeKeychainItemDataContext {
    void* data;
    bool freed;
};

static void webFreeKeychainItemContentOnMainThread(void* voidContext)
{
    FreeKeychainItemDataContext* context = (FreeKeychainItemDataContext*)voidContext;
    
    if (!shimManagedKeychainItemContents().contains(context->data)) {
        context->freed = false;
        return;
    }
        
    shimManagedKeychainItemContents().remove(context->data);
    free(context->data);
    context->freed = true;
}

static bool webFreeKeychainItemContent(void* data)
{
    FreeKeychainItemDataContext context;
    context.data = data;
    
    callOnCoreIPCClientRunLoopAndWait(webFreeKeychainItemContentOnMainThread, &context);

    return context.freed;
}

struct SecKeychainItemContext {
    SecKeychainItemRef item;
    
    SecKeychainAttributeList* attributeList;
    SecItemClass initialItemClass;
    UInt32 length;
    const void* data;
    
    SecItemClass* resultItemClass;
    UInt32* resultLength;
    void** resultData;

    OSStatus resultCode;
};

static void webSecKeychainItemCopyContentOnMainThread(void* voidContext)
{
    SecKeychainItemContext* context = (SecKeychainItemContext*)voidContext;

    SecKeychainItemRequestData requestData(context->item, context->attributeList);
    SecKeychainItemResponseData response;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::SecKeychainItemCopyContent(requestData), Messages::WebProcessProxy::SecKeychainItemCopyContent::Reply(response), 0)) {
        context->resultCode = errSecInteractionNotAllowed;
        ASSERT_NOT_REACHED();
        return;
    }
        
    allocateAttributeListContents(response.attributes(), context->attributeList);
    allocateKeychainItemContentData(response.data(), context->resultLength, context->resultData);
    if (context->resultItemClass)
        *context->resultItemClass = response.itemClass();
    context->resultCode = response.resultCode();
}

static OSStatus webSecKeychainItemCopyContent(SecKeychainItemRef item, SecItemClass* itemClass, SecKeychainAttributeList* attrList, UInt32* length, void** outData)
{
    SecKeychainItemContext context;
    memset(&context, 0, sizeof(SecKeychainItemContext));
    context.item = item;
    context.resultItemClass = itemClass;
    context.attributeList = attrList;
    context.resultLength = length;
    context.resultData = outData;

    callOnCoreIPCClientRunLoopAndWait(webSecKeychainItemCopyContentOnMainThread, &context);

    // FIXME: should return context.resultCode. Returning noErr is a workaround for <rdar://problem/9520886>;
    // the authentication should fail anyway, since on error no data will be returned.
    return noErr;
}

static void webSecKeychainItemCreateFromContentOnMainThread(void* voidContext)
{
    SecKeychainItemContext* context = (SecKeychainItemContext*)voidContext;

    SecKeychainItemRequestData requestData(context->initialItemClass, context->attributeList, context->length, context->data);
    SecKeychainItemResponseData response;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::SecKeychainItemCreateFromContent(requestData), Messages::WebProcessProxy::SecKeychainItemCreateFromContent::Reply(response), 0)) {
        context->resultCode = errSecInteractionNotAllowed;
        ASSERT_NOT_REACHED();
        return;
    }

    if (response.keychainItem())
        CFRetain(response.keychainItem());
    context->item = response.keychainItem();
    context->resultCode = response.resultCode();
}

static OSStatus webSecKeychainItemCreateFromContent(SecItemClass itemClass, SecKeychainAttributeList* attrList, UInt32 length, const void* data, SecKeychainItemRef *item)
{
    SecKeychainItemContext context;
    memset(&context, 0, sizeof(SecKeychainItemContext));
    context.initialItemClass = itemClass;
    context.attributeList = attrList;
    context.length = length;
    context.data = data;
    
    callOnCoreIPCClientRunLoopAndWait(webSecKeychainItemCreateFromContentOnMainThread, &context);
    
    if (item)
        *item = context.item;
    else
        CFRelease(context.item);

    return context.resultCode;
}

static void webSecKeychainItemModifyContentOnMainThread(void* voidContext)
{
    SecKeychainItemContext* context = (SecKeychainItemContext*)voidContext;

    SecKeychainItemRequestData requestData(context->item, context->attributeList, context->length, context->data);
    SecKeychainItemResponseData response;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::SecKeychainItemModifyContent(requestData), Messages::WebProcessProxy::SecKeychainItemModifyContent::Reply(response), 0)) {
        context->resultCode = errSecInteractionNotAllowed;
        ASSERT_NOT_REACHED();
        return;
    }
        
    context->resultCode = response.resultCode();
}

static OSStatus webSecKeychainItemModifyContent(SecKeychainItemRef itemRef, const SecKeychainAttributeList* attrList, UInt32 length, const void* data)
{
    SecKeychainItemContext context;
    context.item = itemRef;
    context.attributeList = (SecKeychainAttributeList*)attrList;
    context.length = length;
    context.data = data;
    
    callOnCoreIPCClientRunLoopAndWait(webSecKeychainItemModifyContentOnMainThread, &context);
    
    return context.resultCode;
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
