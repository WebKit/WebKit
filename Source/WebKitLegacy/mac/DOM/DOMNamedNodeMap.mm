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

#import "DOMNamedNodeMapInternal.h"

#import "DOMNodeInternal.h"
#import "DOMInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/Attr.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NamedNodeMap.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL reinterpret_cast<WebCore::NamedNodeMap*>(_internal)

@implementation DOMNamedNodeMap

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMNamedNodeMap class], self))
        return;

    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (unsigned)length
{
    WebCore::JSMainThreadNullState state;
    return IMPL->length();
}

- (DOMNode *)getNamedItem:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getNamedItem(name)));
}

- (DOMNode *)setNamedItem:(DOMNode *)node
{
    WebCore::JSMainThreadNullState state;
    if (!node)
        raiseTypeErrorException();
    auto& coreNode = *core(node);
    if (!is<WebCore::Attr>(coreNode))
        raiseTypeErrorException();
    return kit(raiseOnDOMError(IMPL->setNamedItem(downcast<WebCore::Attr>(coreNode))).get());
}

- (DOMNode *)removeNamedItem:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->removeNamedItem(name)).ptr());
}

- (DOMNode *)item:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->item(index)));
}

- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getNamedItemNS(namespaceURI, localName)));
}

- (DOMNode *)setNamedItemNS:(DOMNode *)node
{
    return [self setNamedItem:node];
}

- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->removeNamedItemNS(namespaceURI, localName)).ptr());
}

@end

@implementation DOMNamedNodeMap (DOMNamedNodeMapDeprecated)

- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    return [self getNamedItemNS:namespaceURI localName:localName];
}

- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    return [self removeNamedItemNS:namespaceURI localName:localName];
}

@end

DOMNamedNodeMap *kit(WebCore::NamedNodeMap* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMNamedNodeMap *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMNamedNodeMap *wrapper = [[DOMNamedNodeMap alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}
