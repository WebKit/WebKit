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

#include "config.h"

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityTextMarker.h"

#include "AccessibilityUIElement.h"
#include "JSAccessibilityTextMarker.h"

namespace WTR {
    
Ref<AccessibilityTextMarker> AccessibilityTextMarker::create(PlatformTextMarker marker)
{
    return adoptRef(*new AccessibilityTextMarker(marker));
}

Ref<AccessibilityTextMarker> AccessibilityTextMarker::create(const AccessibilityTextMarker& marker)
{
    return adoptRef(*new AccessibilityTextMarker(marker));
}

AccessibilityTextMarker::AccessibilityTextMarker(PlatformTextMarker marker)
    : m_textMarker(marker)
{
}

AccessibilityTextMarker::AccessibilityTextMarker(const AccessibilityTextMarker& marker)
    : JSWrappable()
    , m_textMarker(marker.platformTextMarker())
{
}

AccessibilityTextMarker::~AccessibilityTextMarker()
{
}

PlatformTextMarker AccessibilityTextMarker::platformTextMarker() const
{
#if PLATFORM(COCOA)
    return m_textMarker.get();
#else
    return m_textMarker;
#endif
}
    
JSClassRef AccessibilityTextMarker::wrapperClass()
{
    return JSAccessibilityTextMarker::accessibilityTextMarkerClass();
}

} // namespace WTR
#endif // ENABLE(ACCESSIBILITY)

