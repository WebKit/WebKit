/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DOMEntityInternal.h"

#import "DOMNodeInternal.h"
#import "Entity.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::Entity*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMEntity

- (NSString *)publicId
{
    WebCore::JSMainThreadNullState state;
    return IMPL->publicId();
}

- (NSString *)systemId
{
    WebCore::JSMainThreadNullState state;
    return IMPL->systemId();
}

- (NSString *)notationName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->notationName();
}

@end

WebCore::Entity* core(DOMEntity *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Entity*>(wrapper->_internal) : 0;
}

DOMEntity *kit(WebCore::Entity* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMEntity*>(kit(static_cast<WebCore::Node*>(value)));
}
