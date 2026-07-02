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

#import "DOMNodeInternal.h"

#import "DOMDocumentInternal.h"
#import "DOMElementInternal.h"
#import "DOMEventInternal.h"
#import "DOMInternal.h"
#import "DOMNamedNodeMapInternal.h"
#import "DOMNodeListInternal.h"
#import "ExceptionHandlers.h"
#import "ObjCEventListener.h"
#import <WebCore/DOMImplementation.h>
#import <WebCore/ElementInlines.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NodeList.h>
#import <WebCore/SVGTests.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>


static inline WebCore::Node& unwrap(DOMNode& wrapper)
{
    ASSERT(wrapper._internal);
    return reinterpret_cast<WebCore::Node&>(*wrapper._internal);
}

WebCore::Node* core(DOMNode *wrapper)
{
    return wrapper ? &unwrap(*wrapper) : nullptr;
}

DOMNode *kit(WebCore::Node* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (RetainPtr wrapper = getDOMWrapper(value))
        return wrapper.autorelease();
    RetainPtr<DOMNode> wrapper = adoptNS([[kitClass(value) alloc] _init]);
    if (!wrapper)
        return nil;
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper.get(), value);
    return wrapper.autorelease();
}

@implementation DOMNode

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMNode class], self))
        return;
    if (_internal)
        unwrap(*self).deref();
    [super dealloc];
}

- (NSString *)nodeName
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).nodeName().createNSString().autorelease();
}

- (NSString *)nodeValue
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).nodeValue().createNSString().autorelease();
}

- (void)setNodeValue:(NSString *)newNodeValue
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setNodeValue(newNodeValue);
}

- (unsigned short)nodeType
{
    WebCore::JSMainThreadNullState state;
    return std::to_underlying(unwrap(*self).nodeType());
}

- (DOMNode *)parentNode
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).parentNode());
}

- (DOMNodeList *)childNodes
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).childNodes().ptr());
}

- (DOMNode *)firstChild
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).firstChild());
}

- (DOMNode *)lastChild
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).lastChild());
}

- (DOMNode *)previousSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).previousSibling());
}

- (DOMNode *)nextSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).nextSibling());
}

- (DOMDocument *)ownerDocument
{
    WebCore::JSMainThreadNullState state;
    return kit(&unwrap(*self).document());
}

- (NSString *)namespaceURI
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).namespaceURI().createNSString().autorelease();
}

- (NSString *)prefix
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).prefix().createNSString().autorelease();
}

- (void)setPrefix:(NSString *)newPrefix
{
    UNUSED_PARAM(newPrefix);
}

- (NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).localName().createNSString().autorelease();
}

- (DOMNamedNodeMap *)attributes
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).attributesMap());
}

- (NSString *)baseURI
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).baseURI().string().createNSString().autorelease();
}

- (NSString *)textContent
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).textContent().createNSString().autorelease();
}

- (void)setTextContent:(NSString *)newTextContent
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setTextContent(newTextContent);
}

- (BOOL)isConnected
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).isConnected();
}

- (DOMElement *)parentElement
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).parentElement());
}

- (BOOL)isContentEditable
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).isContentEditable();
}

- (DOMNode *)insertBefore:(DOMNode *)newChild refChild:(DOMNode *)refChild
{
    WebCore::JSMainThreadNullState state;
    if (!newChild)
        raiseTypeErrorException();
    raiseOnDOMError(unwrap(*self).insertBefore(*core(newChild), core(refChild)));
    return newChild;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild oldChild:(DOMNode *)oldChild
{
    WebCore::JSMainThreadNullState state;
    if (!newChild)
        raiseTypeErrorException();
    if (!oldChild)
        raiseTypeErrorException();
    raiseOnDOMError(unwrap(*self).replaceChild(*core(newChild), *core(oldChild)));
    return oldChild;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild
{
    WebCore::JSMainThreadNullState state;
    if (!oldChild)
        raiseTypeErrorException();
    raiseOnDOMError(unwrap(*self).removeChild(*core(oldChild)));
    return oldChild;
}

- (DOMNode *)appendChild:(DOMNode *)newChild
{
    WebCore::JSMainThreadNullState state;
    if (!newChild)
        raiseTypeErrorException();
    raiseOnDOMError(unwrap(*self).appendChild(*core(newChild)));
    return newChild;
}

- (BOOL)hasChildNodes
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).cloneNodeForBindings(deep)).ptr());
}

- (void)normalize
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).normalize();
}

- (BOOL)isSupported:(NSString *)feature version:(NSString *)version
{
    return YES;
}

- (BOOL)hasAttributes
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).hasAttributes();
}

- (BOOL)isSameNode:(DOMNode *)other
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).isSameNode(core(other));
}

- (BOOL)isEqualNode:(DOMNode *)other
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).isEqualNode(core(other));
}

- (NSString *)lookupPrefix:(NSString *)inNamespaceURI
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).lookupPrefix(inNamespaceURI).createNSString().autorelease();
}

- (NSString *)lookupNamespaceURI:(NSString *)inPrefix
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).lookupNamespaceURI(inPrefix).createNSString().autorelease();
}

- (BOOL)isDefaultNamespace:(NSString *)inNamespaceURI
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).isDefaultNamespace(inNamespaceURI);
}

- (unsigned short)compareDocumentPosition:(DOMNode *)other
{
    WebCore::JSMainThreadNullState state;
    if (!other)
        return WebCore::Node::DOCUMENT_POSITION_DISCONNECTED;
    return unwrap(*self).compareDocumentPosition(*core(other));
}

- (BOOL)contains:(DOMNode *)other
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).contains(core(other));
}

- (void)inspect
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).inspect();
}

- (void)addEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).addEventListenerForBindings(type, WebCore::ObjCEventListener::wrap(listener), static_cast<bool>(useCapture));
}

- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).addEventListenerForBindings(type, WebCore::ObjCEventListener::wrap(listener), static_cast<bool>(useCapture));
}

- (void)removeEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).removeEventListenerForBindings(type, WebCore::ObjCEventListener::wrap(listener), static_cast<bool>(useCapture));
}

- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).removeEventListenerForBindings(type, WebCore::ObjCEventListener::wrap(listener), static_cast<bool>(useCapture));
}

- (BOOL)dispatchEvent:(DOMEvent *)event
{
    WebCore::JSMainThreadNullState state;
    if (!event)
        raiseTypeErrorException();
    return raiseOnDOMError(unwrap(*self).dispatchEventForBindings(*core(event)));
}

@end

@implementation DOMNode (DOMNodeDeprecated)

- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild
{
    return [self insertBefore:newChild refChild:refChild];
}

- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild
{
    return [self replaceChild:newChild oldChild:oldChild];
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    return [self isSupported:feature version:version];
}

@end
