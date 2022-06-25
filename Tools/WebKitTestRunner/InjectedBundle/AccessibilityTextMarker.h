/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "JSWrappable.h"
#include <JavaScriptCore/JSObjectRef.h>
#include <wtf/Platform.h>
#include <wtf/Ref.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
using PlatformTextMarker = id;
#else
typedef void* PlatformTextMarker;
#endif

namespace WTR {

class AccessibilityUIElement;

class AccessibilityTextMarker : public JSWrappable {
public:
    static Ref<AccessibilityTextMarker> create(PlatformTextMarker);
    static Ref<AccessibilityTextMarker> create(const AccessibilityTextMarker&);

    ~AccessibilityTextMarker();
    
    PlatformTextMarker platformTextMarker() const;
    virtual JSClassRef wrapperClass();

    static JSObjectRef makeJSAccessibilityTextMarker(JSContextRef, const AccessibilityTextMarker&);
    bool isEqual(AccessibilityTextMarker*);
    
private:
    AccessibilityTextMarker(PlatformTextMarker);
    AccessibilityTextMarker(const AccessibilityTextMarker&);

#if PLATFORM(COCOA)
    RetainPtr<PlatformTextMarker> m_textMarker;
#else
    PlatformTextMarker m_textMarker;
#endif
};

#if !PLATFORM(COCOA)
inline bool AccessibilityTextMarker::isEqual(AccessibilityTextMarker*) { return false; }
#endif

} // namespace WTR
