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

#import "DOMRangeInternal.h"

#import "DOMDocumentFragmentInternal.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import <WebCore/DocumentFragment.h>
#import "ExceptionHandlers.h"
#import <WebCore/JSMainThreadExecState.h>
#import <WebCore/Node.h>
#import <WebCore/Range.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/URL.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>

#define IMPL reinterpret_cast<WebCore::Range*>(_internal)

@implementation DOMRange

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMRange class], self))
        return;

    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (DOMNode *)startContainer
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->startContainer()));
}

- (int)startOffset
{
    WebCore::JSMainThreadNullState state;
    return IMPL->startOffset();
}

- (DOMNode *)endContainer
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->endContainer()));
}

- (int)endOffset
{
    WebCore::JSMainThreadNullState state;
    return IMPL->endOffset();
}

- (BOOL)collapsed
{
    WebCore::JSMainThreadNullState state;
    return IMPL->collapsed();
}

- (DOMNode *)commonAncestorContainer
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->commonAncestorContainer()));
}

- (NSString *)text
{
    WebCore::JSMainThreadNullState state;
    return IMPL->text();
}

- (void)setStart:(DOMNode *)refNode offset:(int)offset
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->setStart(*core(refNode), offset, ec);
    raiseOnDOMError(ec);
}

- (void)setEnd:(DOMNode *)refNode offset:(int)offset
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->setEnd(*core(refNode), offset, ec);
    raiseOnDOMError(ec);
}

- (void)setStartBefore:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->setStartBefore(*core(refNode), ec);
    raiseOnDOMError(ec);
}

- (void)setStartAfter:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->setStartAfter(*core(refNode), ec);
    raiseOnDOMError(ec);
}

- (void)setEndBefore:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->setEndBefore(*core(refNode), ec);
    raiseOnDOMError(ec);
}

- (void)setEndAfter:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->setEndAfter(*core(refNode), ec);
    raiseOnDOMError(ec);
}

- (void)collapse:(BOOL)toStart
{
    WebCore::JSMainThreadNullState state;
    IMPL->collapse(toStart);
}

- (void)selectNode:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->selectNode(*core(refNode), ec);
    raiseOnDOMError(ec);
}

- (void)selectNodeContents:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->selectNodeContents(*core(refNode), ec);
    raiseOnDOMError(ec);
}

- (short)compareBoundaryPoints:(unsigned short)how sourceRange:(DOMRange *)sourceRange
{
    WebCore::JSMainThreadNullState state;
    if (!sourceRange)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    short result = IMPL->compareBoundaryPointsForBindings(how, *core(sourceRange), ec);
    raiseOnDOMError(ec);
    return result;
}

- (void)deleteContents
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->deleteContents(ec);
    raiseOnDOMError(ec);
}

- (DOMDocumentFragment *)extractContents
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMDocumentFragment *result = kit(WTF::getPtr(IMPL->extractContents(ec)));
    raiseOnDOMError(ec);
    return result;
}

- (DOMDocumentFragment *)cloneContents
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMDocumentFragment *result = kit(WTF::getPtr(IMPL->cloneContents(ec)));
    raiseOnDOMError(ec);
    return result;
}

- (void)insertNode:(DOMNode *)newNode
{
    WebCore::JSMainThreadNullState state;
    if (!newNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->insertNode(*core(newNode), ec);
    raiseOnDOMError(ec);
}

- (void)surroundContents:(DOMNode *)newParent
{
    WebCore::JSMainThreadNullState state;
    if (!newParent)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->surroundContents(*core(newParent), ec);
    raiseOnDOMError(ec);
}

- (DOMRange *)cloneRange
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->cloneRange()));
}

- (NSString *)toString
{
    WebCore::JSMainThreadNullState state;
    return IMPL->toString();
}

- (void)detach
{
    WebCore::JSMainThreadNullState state;
    IMPL->detach();
}

- (DOMDocumentFragment *)createContextualFragment:(NSString *)html
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMDocumentFragment *result = kit(WTF::getPtr(IMPL->createContextualFragment(html, ec)));
    raiseOnDOMError(ec);
    return result;
}

- (short)compareNode:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    short result = IMPL->compareNode(*core(refNode), ec);
    raiseOnDOMError(ec);
    return result;
}

- (BOOL)intersectsNode:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    BOOL result = IMPL->intersectsNode(*core(refNode), ec);
    raiseOnDOMError(ec);
    return result;
}

- (short)comparePoint:(DOMNode *)refNode offset:(int)offset
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    short result = IMPL->comparePoint(*core(refNode), offset, ec);
    raiseOnDOMError(ec);
    return result;
}

- (BOOL)isPointInRange:(DOMNode *)refNode offset:(int)offset
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    BOOL result = IMPL->isPointInRange(*core(refNode), offset, ec);
    raiseOnDOMError(ec);
    return result;
}

- (void)expand:(NSString *)unit
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->expand(unit, ec);
    raiseOnDOMError(ec);
}

@end

@implementation DOMRange (DOMRangeDeprecated)

- (void)setStart:(DOMNode *)refNode :(int)offset
{
    [self setStart:refNode offset:offset];
}

- (void)setEnd:(DOMNode *)refNode :(int)offset
{
    [self setEnd:refNode offset:offset];
}

- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange
{
    return [self compareBoundaryPoints:how sourceRange:sourceRange];
}

@end

WebCore::Range* core(DOMRange *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Range*>(wrapper->_internal) : 0;
}

DOMRange *kit(WebCore::Range* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMRange *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMRange *wrapper = [[DOMRange alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}
