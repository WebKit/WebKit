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
using PlatformTextMarkerRange = id;
#else
typedef void* PlatformTextMarkerRange;
#endif

namespace WTR {
    
class AccessibilityTextMarkerRange : public JSWrappable {
public:
    static Ref<AccessibilityTextMarkerRange> create(PlatformTextMarkerRange);
    static Ref<AccessibilityTextMarkerRange> create(const AccessibilityTextMarkerRange&);
    
    ~AccessibilityTextMarkerRange();
    
    PlatformTextMarkerRange platformTextMarkerRange() const;
    virtual JSClassRef wrapperClass();
    
    static JSObjectRef makeJSAccessibilityTextMarkerRange(JSContextRef, const AccessibilityTextMarkerRange&);
    bool isEqual(AccessibilityTextMarkerRange*);
    
private:
    AccessibilityTextMarkerRange(PlatformTextMarkerRange);
    AccessibilityTextMarkerRange(const AccessibilityTextMarkerRange&);
    
#if PLATFORM(COCOA)
    RetainPtr<id> m_textMarkerRange;
#else
    PlatformTextMarkerRange m_textMarkerRange;
#endif
};
    
#if !PLATFORM(COCOA)
inline bool AccessibilityTextMarkerRange::isEqual(AccessibilityTextMarkerRange*) { return false; }
#endif
    
#ifdef __OBJC__
inline std::optional<RefPtr<AccessibilityTextMarkerRange>> makeVectorElement(const RefPtr<AccessibilityTextMarkerRange>*, id range) { return { { AccessibilityTextMarkerRange::create(range) } }; }
#endif

} // namespace WTR
