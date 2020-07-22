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
#import <WebCore/JSExecState.h>
#import <WebCore/Node.h>
#import <WebCore/Range.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

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
    raiseOnDOMError(IMPL->setStart(*core(refNode), offset));
}

- (void)setEnd:(DOMNode *)refNode offset:(int)offset
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->setEnd(*core(refNode), offset));
}

- (void)setStartBefore:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->setStartBefore(*core(refNode)));
}

- (void)setStartAfter:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->setStartAfter(*core(refNode)));
}

- (void)setEndBefore:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->setEndBefore(*core(refNode)));
}

- (void)setEndAfter:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->setEndAfter(*core(refNode)));
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
    raiseOnDOMError(IMPL->selectNode(*core(refNode)));
}

- (void)selectNodeContents:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->selectNodeContents(*core(refNode)));
}

- (short)compareBoundaryPoints:(unsigned short)how sourceRange:(DOMRange *)sourceRange
{
    WebCore::JSMainThreadNullState state;
    if (!sourceRange)
        raiseTypeErrorException();
    return raiseOnDOMError(IMPL->compareBoundaryPointsForBindings(how, *core(sourceRange)));
}

- (void)deleteContents
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->deleteContents());
}

- (DOMDocumentFragment *)extractContents
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->extractContents()).ptr());
}

- (DOMDocumentFragment *)cloneContents
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->cloneContents()).ptr());
}

- (void)insertNode:(DOMNode *)newNode
{
    WebCore::JSMainThreadNullState state;
    if (!newNode)
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->insertNode(*core(newNode)));
}

- (void)surroundContents:(DOMNode *)newParent
{
    WebCore::JSMainThreadNullState state;
    if (!newParent)
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->surroundContents(*core(newParent)));
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
    return kit(raiseOnDOMError(IMPL->createContextualFragment(html)).ptr());
}

- (short)compareNode:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    return raiseOnDOMError(IMPL->compareNode(*core(refNode)));
}

- (BOOL)intersectsNode:(DOMNode *)refNode
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    return raiseOnDOMError(IMPL->intersectsNode(*core(refNode)));
}

- (short)comparePoint:(DOMNode *)refNode offset:(int)offset
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    return raiseOnDOMError(IMPL->comparePoint(*core(refNode), offset));
}

- (BOOL)isPointInRange:(DOMNode *)refNode offset:(int)offset
{
    WebCore::JSMainThreadNullState state;
    if (!refNode)
        raiseTypeErrorException();
    return raiseOnDOMError(IMPL->isPointInRange(*core(refNode), offset));
}

- (void)expand:(NSString *)unit
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->expand(unit));
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

DOMRange *kit(const Optional<WebCore::SimpleRange>& value)
{
    return kit(createLiveRange(value).get());
}

#undef IMPL
