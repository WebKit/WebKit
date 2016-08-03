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
#import "DOMNamedNodeMapInternal.h"

#import "DOMNodeInternal.h"
#import "DOMInternal.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "NamedNodeMap.h"
#import "Node.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebCoreObjCExtras.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

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
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->setNamedItem(*core(node), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)removeNamedItem:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->removeNamedItem(name, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
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

- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getNamedItemNS(namespaceURI, localName)));
}

- (DOMNode *)setNamedItemNS:(DOMNode *)node
{
    WebCore::JSMainThreadNullState state;
    if (!node)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->setNamedItem(*core(node), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->removeNamedItemNS(namespaceURI, localName, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->removeNamedItemNS(namespaceURI, localName, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

@end

WebCore::NamedNodeMap* core(DOMNamedNodeMap *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::NamedNodeMap*>(wrapper->_internal) : 0;
}

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
