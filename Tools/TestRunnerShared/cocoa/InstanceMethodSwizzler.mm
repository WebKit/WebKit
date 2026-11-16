/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#import "InstanceMethodSwizzler.h"

InstanceMethodSwizzler::InstanceMethodSwizzler(Class cls, SEL selector, IMP implementation)
    : m_method(class_getInstanceMethod(cls, selector))
    , m_originalImplementation(method_setImplementation(m_method, implementation))
{
}

InstanceMethodSwizzler::~InstanceMethodSwizzler()
{
    method_setImplementation(m_method, m_originalImplementation);
}

InstanceMethodSwapper::InstanceMethodSwapper(Class theClass, SEL originalSelector, SEL swizzledSelector)
    : m_class { theClass }
    , m_method { class_getInstanceMethod(theClass, originalSelector) }
    , m_originalSelector { originalSelector }
    , m_originalImplementation { method_getImplementation(m_method) }
{
    auto swizzledMethod = class_getInstanceMethod(theClass, swizzledSelector);
    auto swizzledImplementation = method_getImplementation(swizzledMethod);
    class_replaceMethod(theClass, swizzledSelector, m_originalImplementation, method_getTypeEncoding(m_method));
    class_replaceMethod(theClass, originalSelector, swizzledImplementation, method_getTypeEncoding(swizzledMethod));
}

InstanceMethodSwapper::~InstanceMethodSwapper()
{
    class_replaceMethod(m_class, m_originalSelector, m_originalImplementation, method_getTypeEncoding(m_method));
}
