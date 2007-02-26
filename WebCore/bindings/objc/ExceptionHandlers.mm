/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ExceptionHandlers.h"

#include "Event.h"
#include "RangeException.h"
#include "SVGException.h"
#include "XPathEvaluator.h"

NSString * DOMException = @"DOMException";
NSString * DOMRangeException = @"DOMRangeException";
NSString * DOMEventException = @"DOMEventException";
NSString * DOMSVGException = @"DOMSVGException";
NSString * DOMXPathException = @"DOMXPathException";

namespace WebCore {

void raiseDOMException(ExceptionCode ec)
{
    ASSERT(ec);

    NSString *name = DOMException;

    int code = ec;
    if (ec >= RangeExceptionOffset && ec <= RangeExceptionMax) {
        name = DOMRangeException;
        code -= RangeExceptionOffset;
    } else if (ec >= EventExceptionOffset && ec <= EventExceptionMax) {
        name = DOMEventException;
        code -= EventExceptionOffset;
#if ENABLE(SVG)
    } else if (ec >= SVGExceptionOffset && ec <= SVGExceptionMax) {
        name = DOMSVGException;
        code -= SVGExceptionOffset;
#endif
#if ENABLE(XPATH)
    } else if (ec >= XPathExceptionOffset && ec <= XPathExceptionMax) {
        name = DOMXPathException;
        code -= XPathExceptionOffset;
#endif
    }

    NSString *reason = [NSString stringWithFormat:@"*** Exception received from DOM API: %d", code];
    NSException *exception = [NSException exceptionWithName:name reason:reason
        userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:code] forKey:name]];
    [exception raise];
}

} // namespace WebCore
