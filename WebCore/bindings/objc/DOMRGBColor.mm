/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DOMInternal.h" // import first to make the private/public trick work
#import "DOMRGBColorInternal.h"

#import "CSSPrimitiveValue.h"
#import "ColorMac.h"
#import "DOMCSSPrimitiveValueInternal.h"
#import "ExceptionHandlers.h"
#import "RGBColor.h"
#import "ThreadCheck.h"
#import "WebCoreObjCExtras.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL reinterpret_cast<WebCore::RGBColor*>(_internal)

@implementation DOMRGBColor

- (void)dealloc
{
    { DOM_ASSERT_MAIN_THREAD(); WebCoreThreadViolationCheckRoundOne(); }
    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (void)finalize
{
    if (_internal)
        IMPL->deref();
    [super finalize];
}

- (DOMCSSPrimitiveValue *)red
{
    return kit(WTF::getPtr(IMPL->red()));
}

- (DOMCSSPrimitiveValue *)green
{
    return kit(WTF::getPtr(IMPL->green()));
}

- (DOMCSSPrimitiveValue *)blue
{
    return kit(WTF::getPtr(IMPL->blue()));
}

- (DOMCSSPrimitiveValue *)alpha
{
    return kit(WTF::getPtr(IMPL->alpha()));
}

- (NSColor *)color
{
    return WebCore::nsColor(IMPL->color());
}

@end

WebCore::RGBColor* core(DOMRGBColor *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::RGBColor*>(wrapper->_internal) : 0;
}

DOMRGBColor *kit(WebCore::RGBColor* value)
{
    { DOM_ASSERT_MAIN_THREAD(); WebCoreThreadViolationCheckRoundOne(); };
    if (!value)
        return nil;
    if (DOMRGBColor *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMRGBColor *wrapper = [[DOMRGBColor alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}
