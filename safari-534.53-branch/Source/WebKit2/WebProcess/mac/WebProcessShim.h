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
#ifndef WebProcessShim_h
#define WebProcessShim_h

namespace WebKit {

struct WebProcessSecItemShimCallbacks {
    OSStatus (*secItemCopyMatching)(CFDictionaryRef query, CFTypeRef *result);
    OSStatus (*secItemAdd)(CFDictionaryRef attributes, CFTypeRef *result);
    OSStatus (*secItemUpdate)(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);
    OSStatus (*secItemDelete)(CFDictionaryRef query);
};

typedef void (*WebProcessSecItemShimInitializeFunc)(const WebProcessSecItemShimCallbacks& callbacks);

struct WebProcessKeychainItemShimCallbacks {
    OSStatus (*secKeychainItemCopyContent)(SecKeychainItemRef, SecItemClass*, SecKeychainAttributeList*, UInt32* length, void** outData);
    OSStatus (*secKeychainItemCreateFromContent)(SecItemClass, SecKeychainAttributeList*, UInt32 length, const void* data, SecKeychainItemRef*);
    OSStatus (*secKeychainItemModifyContent)(SecKeychainItemRef, const SecKeychainAttributeList*, UInt32 length, const void* data);
    bool (*freeAttributeListContent)(SecKeychainAttributeList* attrList);
    bool (*freeKeychainItemContentData)(void* data);
};

typedef void (*WebProcessKeychainItemShimInitializeFunc)(const WebProcessKeychainItemShimCallbacks& callbacks);

} // namespace WebKit

#endif // WebProcessShim_h
