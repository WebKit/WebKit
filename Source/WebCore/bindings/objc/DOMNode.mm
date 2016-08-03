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
#import "DOMNodeInternal.h"

#import "DOMDocumentInternal.h"
#import "DOMElementInternal.h"
#import "DOMEventInternal.h"
#import "DOMInternal.h"
#import "DOMNamedNodeMapInternal.h"
#import "DOMNodeListInternal.h"
#import "Document.h"
#import "Element.h"
#import "Event.h"
#import "EventListener.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "NameNodeList.h"
#import "NamedNodeMap.h"
#import "Node.h"
#import "NodeList.h"
#import "ObjCEventListener.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebCoreObjCExtras.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL reinterpret_cast<WebCore::Node*>(_internal)

@implementation DOMNode

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMNode class], self))
        return;

    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (NSString *)nodeName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->nodeName();
}

- (NSString *)nodeValue
{
    WebCore::JSMainThreadNullState state;
    return IMPL->nodeValue();
}

- (void)setNodeValue:(NSString *)newNodeValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setNodeValue(newNodeValue, ec);
    WebCore::raiseOnDOMError(ec);
}

- (unsigned short)nodeType
{
    WebCore::JSMainThreadNullState state;
    return IMPL->nodeType();
}

- (DOMNode *)parentNode
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->parentNode()));
}

- (DOMNodeList *)childNodes
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->childNodes()));
}

- (DOMNode *)firstChild
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->firstChild()));
}

- (DOMNode *)lastChild
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->lastChild()));
}

- (DOMNode *)previousSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->previousSibling()));
}

- (DOMNode *)nextSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->nextSibling()));
}

- (DOMDocument *)ownerDocument
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->document()));
}

- (NSString *)namespaceURI
{
    WebCore::JSMainThreadNullState state;
    return IMPL->namespaceURI();
}

- (NSString *)prefix
{
    WebCore::JSMainThreadNullState state;
    return IMPL->prefix();
}

- (void)setPrefix:(NSString *)newPrefix
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setPrefix(newPrefix, ec);
    WebCore::raiseOnDOMError(ec);
}

- (NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->localName();
}

- (DOMNamedNodeMap *)attributes
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->attributes()));
}

- (NSString *)baseURI
{
    WebCore::JSMainThreadNullState state;
    return IMPL->baseURI();
}

- (NSString *)textContent
{
    WebCore::JSMainThreadNullState state;
    return IMPL->textContent();
}

- (void)setTextContent:(NSString *)newTextContent
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setTextContent(newTextContent, ec);
    WebCore::raiseOnDOMError(ec);
}

- (BOOL)isConnected
{
    WebCore::JSMainThreadNullState state;
    return IMPL->inDocument();
}

- (DOMElement *)parentElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->parentElement()));
}

- (BOOL)isContentEditable
{
    WebCore::JSMainThreadNullState state;
    return IMPL->isContentEditable();
}

- (DOMNode *)insertBefore:(DOMNode *)newChild refChild:(DOMNode *)refChild
{
    WebCore::JSMainThreadNullState state;
    if (!newChild)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    if (IMPL->insertBefore(*core(newChild), core(refChild), ec))
        return newChild;
    WebCore::raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild
{
    WebCore::JSMainThreadNullState state;
    if (!newChild)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    if (IMPL->insertBefore(*core(newChild), core(refChild), ec))
        return newChild;
    WebCore::raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild oldChild:(DOMNode *)oldChild
{
    WebCore::JSMainThreadNullState state;
    if (!newChild)
        WebCore::raiseTypeErrorException();
    if (!oldChild)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    if (IMPL->replaceChild(*core(newChild), *core(oldChild), ec))
        return oldChild;
    WebCore::raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild
{
    WebCore::JSMainThreadNullState state;
    if (!newChild)
        WebCore::raiseTypeErrorException();
    if (!oldChild)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    if (IMPL->replaceChild(*core(newChild), *core(oldChild), ec))
        return oldChild;
    WebCore::raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild
{
    WebCore::JSMainThreadNullState state;
    if (!oldChild)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    if (IMPL->removeChild(*core(oldChild), ec))
        return oldChild;
    WebCore::raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)appendChild:(DOMNode *)newChild
{
    WebCore::JSMainThreadNullState state;
    if (!newChild)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    if (IMPL->appendChild(*core(newChild), ec))
        return newChild;
    WebCore::raiseOnDOMError(ec);
    return nil;
}

- (BOOL)hasChildNodes
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->cloneNodeForBindings(deep, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (void)normalize
{
    WebCore::JSMainThreadNullState state;
    IMPL->normalize();
}

- (BOOL)isSupported:(NSString *)feature version:(NSString *)version
{
    WebCore::JSMainThreadNullState state;
    return IMPL->isSupportedForBindings(feature, version);
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    WebCore::JSMainThreadNullState state;
    return IMPL->isSupportedForBindings(feature, version);
}

- (BOOL)hasAttributes
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributes();
}

- (BOOL)isSameNode:(DOMNode *)other
{
    WebCore::JSMainThreadNullState state;
    return IMPL->isSameNode(core(other));
}

- (BOOL)isEqualNode:(DOMNode *)other
{
    WebCore::JSMainThreadNullState state;
    return IMPL->isEqualNode(core(other));
}

- (NSString *)lookupPrefix:(NSString *)inNamespaceURI
{
    WebCore::JSMainThreadNullState state;
    return IMPL->lookupPrefix(inNamespaceURI);
}

- (NSString *)lookupNamespaceURI:(NSString *)inPrefix
{
    WebCore::JSMainThreadNullState state;
    return IMPL->lookupNamespaceURI(inPrefix);
}

- (BOOL)isDefaultNamespace:(NSString *)inNamespaceURI
{
    WebCore::JSMainThreadNullState state;
    return IMPL->isDefaultNamespace(inNamespaceURI);
}

- (unsigned short)compareDocumentPosition:(DOMNode *)other
{
    WebCore::JSMainThreadNullState state;
    if (!other)
        WebCore::raiseTypeErrorException();
    return IMPL->compareDocumentPosition(*core(other));
}

- (BOOL)contains:(DOMNode *)other
{
    WebCore::JSMainThreadNullState state;
    return IMPL->contains(core(other));
}

- (void)inspect
{
    WebCore::JSMainThreadNullState state;
    IMPL->inspect();
}

- (void)addEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture
{
    WebCore::JSMainThreadNullState state;
    RefPtr<WebCore::EventListener> nativeEventListener = WebCore::ObjCEventListener::wrap(listener);
    IMPL->addEventListenerForBindings(type, WTF::getPtr(nativeEventListener), useCapture);
}

- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    WebCore::JSMainThreadNullState state;
    RefPtr<WebCore::EventListener> nativeEventListener = WebCore::ObjCEventListener::wrap(listener);
    IMPL->addEventListenerForBindings(type, WTF::getPtr(nativeEventListener), useCapture);
}

- (void)removeEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture
{
    WebCore::JSMainThreadNullState state;
    RefPtr<WebCore::EventListener> nativeEventListener = WebCore::ObjCEventListener::wrap(listener);
    IMPL->removeEventListenerForBindings(type, WTF::getPtr(nativeEventListener), useCapture);
}

- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    WebCore::JSMainThreadNullState state;
    RefPtr<WebCore::EventListener> nativeEventListener = WebCore::ObjCEventListener::wrap(listener);
    IMPL->removeEventListenerForBindings(type, WTF::getPtr(nativeEventListener), useCapture);
}

- (BOOL)dispatchEvent:(DOMEvent *)event
{
    WebCore::JSMainThreadNullState state;
    if (!event)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    BOOL result = IMPL->dispatchEventForBindings(*core(event), ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

@end

WebCore::Node* core(DOMNode *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Node*>(wrapper->_internal) : 0;
}

DOMNode *kit(WebCore::Node* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMNode *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMNode *wrapper = [[kitClass(value) alloc] _init];
    if (!wrapper)
        return nil;
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}
