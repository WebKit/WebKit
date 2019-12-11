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

#import "ExceptionHandlers.h"

#import "DOMEventException.h"
#import "DOMException.h"
#import "DOMRangeException.h"
#import "DOMXPathException.h"
#import <WebCore/DOMException.h>

NSString * const DOMException = @"DOMException";
NSString * const DOMRangeException = @"DOMRangeException";
NSString * const DOMEventException = @"DOMEventException";
NSString * const DOMXPathException = @"DOMXPathException";

static NO_RETURN void raiseDOMErrorException(WebCore::ExceptionCode ec)
{
    ASSERT(ec);

    auto description = WebCore::DOMException::description(ec);

    NSString *reason;
    if (description.name)
        reason = [[NSString alloc] initWithFormat:@"*** %s: %@ %d", description.name.characters(), DOMException, description.legacyCode];
    else
        reason = [[NSString alloc] initWithFormat:@"*** %@ %d", DOMException, description.legacyCode];

    NSDictionary *userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithInt:description.legacyCode], DOMException, nil];

    NSException *exception = [NSException exceptionWithName:DOMException reason:reason userInfo:userInfo];

    [reason release];
    [userInfo release];

    [exception raise];

    RELEASE_ASSERT_NOT_REACHED();
}

void raiseTypeErrorException()
{
    raiseDOMErrorException(WebCore::TypeError);
}

void raiseNotSupportedErrorException()
{
    raiseDOMErrorException(WebCore::NotSupportedError);
}

void raiseDOMErrorException(WebCore::Exception&& exception)
{
    raiseDOMErrorException(exception.code());
}
