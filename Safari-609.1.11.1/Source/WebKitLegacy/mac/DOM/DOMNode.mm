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
#import <WebCore/Element.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NodeList.h>
#import <WebCore/SVGTests.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>

using namespace WebCore;

static inline Node& unwrap(DOMNode& wrapper)
{
    ASSERT(wrapper._internal);
    return reinterpret_cast<Node&>(*wrapper._internal);
}

Node* core(DOMNode *wrapper)
{
    return wrapper ? &unwrap(*wrapper) : nullptr;
}

DOMNode *kit(Node* value)
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
    JSMainThreadNullState state;
    return unwrap(*self).nodeName();
}

- (NSString *)nodeValue
{
    JSMainThreadNullState state;
    return unwrap(*self).nodeValue();
}

- (void)setNodeValue:(NSString *)newNodeValue
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setNodeValue(newNodeValue));
}

- (unsigned short)nodeType
{
    JSMainThreadNullState state;
    return unwrap(*self).nodeType();
}

- (DOMNode *)parentNode
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).parentNode());
}

- (DOMNodeList *)childNodes
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).childNodes().get());
}

- (DOMNode *)firstChild
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).firstChild());
}

- (DOMNode *)lastChild
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).lastChild());
}

- (DOMNode *)previousSibling
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).previousSibling());
}

- (DOMNode *)nextSibling
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).nextSibling());
}

- (DOMDocument *)ownerDocument
{
    JSMainThreadNullState state;
    return kit(&unwrap(*self).document());
}

- (NSString *)namespaceURI
{
    JSMainThreadNullState state;
    return unwrap(*self).namespaceURI();
}

- (NSString *)prefix
{
    JSMainThreadNullState state;
    return unwrap(*self).prefix();
}

- (void)setPrefix:(NSString *)newPrefix
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setPrefix(newPrefix));
}

- (NSString *)localName
{
    JSMainThreadNullState state;
    return unwrap(*self).localName();
}

- (DOMNamedNodeMap *)attributes
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).attributes());
}

- (NSString *)baseURI
{
    JSMainThreadNullState state;
    return unwrap(*self).baseURI();
}

- (NSString *)textContent
{
    JSMainThreadNullState state;
    return unwrap(*self).textContent();
}

- (void)setTextContent:(NSString *)newTextContent
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setTextContent(newTextContent));
}

- (BOOL)isConnected
{
    JSMainThreadNullState state;
    return unwrap(*self).isConnected();
}

- (DOMElement *)parentElement
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).parentElement());
}

- (BOOL)isContentEditable
{
    JSMainThreadNullState state;
    return unwrap(*self).isContentEditable();
}

- (DOMNode *)insertBefore:(DOMNode *)newChild refChild:(DOMNode *)refChild
{
    JSMainThreadNullState state;
    if (!newChild)
        raiseTypeErrorException();
    raiseOnDOMError(unwrap(*self).insertBefore(*core(newChild), core(refChild)));
    return newChild;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild oldChild:(DOMNode *)oldChild
{
    JSMainThreadNullState state;
    if (!newChild)
        raiseTypeErrorException();
    if (!oldChild)
        raiseTypeErrorException();
    raiseOnDOMError(unwrap(*self).replaceChild(*core(newChild), *core(oldChild)));
    return oldChild;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild
{
    JSMainThreadNullState state;
    if (!oldChild)
        raiseTypeErrorException();
    raiseOnDOMError(unwrap(*self).removeChild(*core(oldChild)));
    return oldChild;
}

- (DOMNode *)appendChild:(DOMNode *)newChild
{
    JSMainThreadNullState state;
    if (!newChild)
        raiseTypeErrorException();
    raiseOnDOMError(unwrap(*self).appendChild(*core(newChild)));
    return newChild;
}

- (BOOL)hasChildNodes
{
    JSMainThreadNullState state;
    return unwrap(*self).hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).cloneNodeForBindings(deep)).ptr());
}

- (void)normalize
{
    JSMainThreadNullState state;
    unwrap(*self).normalize();
}

- (BOOL)isSupported:(NSString *)feature version:(NSString *)version
{
    return SVGTests::hasFeatureForLegacyBindings(feature, version);
}

- (BOOL)hasAttributes
{
    JSMainThreadNullState state;
    return unwrap(*self).hasAttributes();
}

- (BOOL)isSameNode:(DOMNode *)other
{
    JSMainThreadNullState state;
    return unwrap(*self).isSameNode(core(other));
}

- (BOOL)isEqualNode:(DOMNode *)other
{
    JSMainThreadNullState state;
    return unwrap(*self).isEqualNode(core(other));
}

- (NSString *)lookupPrefix:(NSString *)inNamespaceURI
{
    JSMainThreadNullState state;
    return unwrap(*self).lookupPrefix(inNamespaceURI);
}

- (NSString *)lookupNamespaceURI:(NSString *)inPrefix
{
    JSMainThreadNullState state;
    return unwrap(*self).lookupNamespaceURI(inPrefix);
}

- (BOOL)isDefaultNamespace:(NSString *)inNamespaceURI
{
    JSMainThreadNullState state;
    return unwrap(*self).isDefaultNamespace(inNamespaceURI);
}

- (unsigned short)compareDocumentPosition:(DOMNode *)other
{
    JSMainThreadNullState state;
    if (!other)
        return Node::DOCUMENT_POSITION_DISCONNECTED;
    return unwrap(*self).compareDocumentPosition(*core(other));
}

- (BOOL)contains:(DOMNode *)other
{
    JSMainThreadNullState state;
    return unwrap(*self).contains(core(other));
}

- (void)inspect
{
    JSMainThreadNullState state;
    unwrap(*self).inspect();
}

- (void)addEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture
{
    JSMainThreadNullState state;
    unwrap(*self).addEventListenerForBindings(type, ObjCEventListener::wrap(listener), static_cast<bool>(useCapture));
}

- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    JSMainThreadNullState state;
    unwrap(*self).addEventListenerForBindings(type, ObjCEventListener::wrap(listener), static_cast<bool>(useCapture));
}

- (void)removeEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture
{
    JSMainThreadNullState state;
    unwrap(*self).removeEventListenerForBindings(type, ObjCEventListener::wrap(listener), static_cast<bool>(useCapture));
}

- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    JSMainThreadNullState state;
    unwrap(*self).removeEventListenerForBindings(type, ObjCEventListener::wrap(listener), static_cast<bool>(useCapture));
}

- (BOOL)dispatchEvent:(DOMEvent *)event
{
    JSMainThreadNullState state;
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
