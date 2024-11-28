/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "UISideCompositingScope.h"

static bool uiSideCompositingEnabled { true };

@implementation NSUserDefaults (TestSupport)

- (id)swizzled_objectForKey:(NSString *)key
{
    if ([key isEqualToString:@"WebKit2UseRemoteLayerTreeDrawingArea"])
        return @(static_cast<BOOL>(uiSideCompositingEnabled));
    return [self swizzled_objectForKey:key];
}

@end

namespace TestWebKitAPI {

UISideCompositingScope::UISideCompositingScope(UISideCompositingState uiSideCompositingState)
    : m_originalMethod(class_getInstanceMethod(NSUserDefaults.class, @selector(objectForKey:)))
    , m_swizzledMethod(class_getInstanceMethod(NSUserDefaults.class, @selector(swizzled_objectForKey:)))
{
    uiSideCompositingEnabled = uiSideCompositingState == UISideCompositingState::Enabled;
    m_originalImplementation = method_getImplementation(m_originalMethod);
    m_swizzledImplementation = method_getImplementation(m_swizzledMethod);
    class_replaceMethod(NSUserDefaults.class, @selector(swizzled_objectForKey:), m_originalImplementation, method_getTypeEncoding(m_originalMethod));
    class_replaceMethod(NSUserDefaults.class, @selector(objectForKey:), m_swizzledImplementation, method_getTypeEncoding(m_swizzledMethod));
}

UISideCompositingScope::~UISideCompositingScope()
{
    class_replaceMethod(NSUserDefaults.class, @selector(swizzled_objectForKey:), m_swizzledImplementation, method_getTypeEncoding(m_originalMethod));
    class_replaceMethod(NSUserDefaults.class, @selector(objectForKey:), m_originalImplementation, method_getTypeEncoding(m_swizzledMethod));
}

} // namespace TestWebKitAPI
