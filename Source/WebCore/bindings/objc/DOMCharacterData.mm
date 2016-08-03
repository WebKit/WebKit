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
#import "DOMCharacterData.h"

#import "CharacterData.h"
#import "DOMElementInternal.h"
#import "DOMNodeInternal.h"
#import "Element.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::CharacterData*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMCharacterData

- (NSString *)data
{
    WebCore::JSMainThreadNullState state;
    return IMPL->data();
}

- (void)setData:(NSString *)newData
{
    WebCore::JSMainThreadNullState state;
    IMPL->setData(newData);
}

- (unsigned)length
{
    WebCore::JSMainThreadNullState state;
    return IMPL->length();
}

- (DOMElement *)previousElementSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->previousElementSibling()));
}

- (DOMElement *)nextElementSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->nextElementSibling()));
}

- (NSString *)substringData:(unsigned)offset length:(unsigned)inLength
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    NSString *result = IMPL->substringData(offset, inLength, ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (NSString *)substringData:(unsigned)offset :(unsigned)inLength
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    NSString *result = IMPL->substringData(offset, inLength, ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (void)appendData:(NSString *)inData
{
    WebCore::JSMainThreadNullState state;
    IMPL->appendData(inData);
}

- (void)insertData:(unsigned)offset data:(NSString *)inData
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->insertData(offset, inData, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)insertData:(unsigned)offset :(NSString *)inData
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->insertData(offset, inData, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)deleteData:(unsigned)offset length:(unsigned)inLength
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->deleteData(offset, inLength, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)deleteData:(unsigned)offset :(unsigned)inLength
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->deleteData(offset, inLength, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)replaceData:(unsigned)offset length:(unsigned)inLength data:(NSString *)inData
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->replaceData(offset, inLength, inData, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)replaceData:(unsigned)offset :(unsigned)inLength :(NSString *)inData
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->replaceData(offset, inLength, inData, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)remove
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->remove(ec);
    WebCore::raiseOnDOMError(ec);
}

@end
