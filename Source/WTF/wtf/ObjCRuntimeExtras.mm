/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "ObjCRuntimeExtras.h"

#import <objc/runtime.h>
#import <wtf/StdLibExtras.h>

namespace WTF {

MallocSpan<Method, SystemMalloc> class_copyMethodListSpan(Class cls)
{
    unsigned methodCount = 0;
    auto* methods = class_copyMethodList(cls, &methodCount);
    return adoptMallocSpan<Method, SystemMalloc>(unsafeMakeSpan(methods, methodCount));
}

MallocSpan<__unsafe_unretained Protocol *, SystemMalloc> class_copyProtocolListSpan(Class cls)
{
    unsigned protocolCount = 0;
    auto* protocols = class_copyProtocolList(cls, &protocolCount);
    return adoptMallocSpan<__unsafe_unretained Protocol *, SystemMalloc>(unsafeMakeSpan(protocols, protocolCount));
}

MallocSpan<objc_method_description, SystemMalloc> protocol_copyMethodDescriptionListSpan(Protocol *protocol, BOOL isRequiredMethod, BOOL isInstanceMethod)
{
    unsigned methodCount = 0;
    auto* methods = protocol_copyMethodDescriptionList(protocol, isRequiredMethod, isInstanceMethod, &methodCount);
    return adoptMallocSpan<objc_method_description, SystemMalloc>(unsafeMakeSpan(methods, methodCount));
}

MallocSpan<objc_property_t, SystemMalloc> protocol_copyPropertyListSpan(Protocol *protocol)
{
    unsigned propertyCount = 0;
    auto* properties = protocol_copyPropertyList(protocol, &propertyCount);
    return adoptMallocSpan<objc_property_t, SystemMalloc>(unsafeMakeSpan(properties, propertyCount));
}

MallocSpan<__unsafe_unretained Protocol *, SystemMalloc> protocol_copyProtocolListSpan(Protocol *protocol)
{
    unsigned protocolCount = 0;
    auto* protocols = protocol_copyProtocolList(protocol, &protocolCount);
    return adoptMallocSpan<__unsafe_unretained Protocol *, SystemMalloc>(unsafeMakeSpan(protocols, protocolCount));
}

} // namespace WTF
