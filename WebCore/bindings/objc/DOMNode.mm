/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 James G. Speth <speth@end.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "DOMNode.h"

#import "AtomicString.h"
#import "DOMDocument.h"
#import "DOMInternal.h"
#import "DOMNamedNodeMap.h"
#import "DOMNodeList.h"
#import "IntRect.h"
#import "NamedAttrMap.h"
#import "NamedNodeMap.h"
#import "Node.h"
#import "NodeList.h"
#import "PlatformString.h"
#import "RenderObject.h"
#import <wtf/Assertions.h>
#import <wtf/Vector.h>

@implementation DOMNode

#define IMPL reinterpret_cast<WebCore::Node*>(_internal)

- (void)dealloc
{
    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (void)finalize
{
    if (_internal)
        IMPL->deref();
    [super finalize];
}

- (NSString *)nodeName
{
    return IMPL->nodeName();
}

- (NSString *)nodeValue
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return IMPL->nodeValue();
}

- (void)setNodeValue:(NSString *)string
{
    ASSERT(string);
    
    WebCore::ExceptionCode ec = 0;
    IMPL->setNodeValue(string, ec);
    raiseOnDOMError(ec);
}

- (unsigned short)nodeType
{
    return IMPL->nodeType();
}

- (DOMNode *)parentNode
{
    return [DOMNode _nodeWith:IMPL->parentNode()];
}

- (DOMNodeList *)childNodes
{
    return [DOMNodeList _nodeListWith:IMPL->childNodes().get()];
}

- (DOMNode *)firstChild
{
    return [DOMNode _nodeWith:IMPL->firstChild()];
}

- (DOMNode *)lastChild
{
    return [DOMNode _nodeWith:IMPL->lastChild()];
}

- (DOMNode *)previousSibling
{
    return [DOMNode _nodeWith:IMPL->previousSibling()];
}

- (DOMNode *)nextSibling
{
    return [DOMNode _nodeWith:IMPL->nextSibling()];
}

- (DOMNamedNodeMap *)attributes
{
    return [DOMNamedNodeMap _namedNodeMapWith:IMPL->attributes()];
}

- (DOMDocument *)ownerDocument
{
    return [DOMDocument _documentWith:IMPL->document()];
}

- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild
{
    ASSERT(newChild);
    ASSERT(refChild);

    WebCore::ExceptionCode ec = 0;
    if (IMPL->insertBefore([newChild _node], [refChild _node], ec))
        return newChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild
{
    ASSERT(newChild);
    ASSERT(oldChild);

    WebCore::ExceptionCode ec = 0;
    if (IMPL->replaceChild([newChild _node], [oldChild _node], ec))
        return oldChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild
{
    ASSERT(oldChild);

    WebCore::ExceptionCode ec = 0;
    if (IMPL->removeChild([oldChild _node], ec))
        return oldChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)appendChild:(DOMNode *)newChild
{
    ASSERT(newChild);

    WebCore::ExceptionCode ec = 0;
    if (IMPL->appendChild([newChild _node], ec))
        return newChild;
    raiseOnDOMError(ec);
    return nil;
}

- (BOOL)hasChildNodes
{
    return IMPL->hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    return [DOMNode _nodeWith:IMPL->cloneNode(deep).get()];
}

- (void)normalize
{
    IMPL->normalize();
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    return IMPL->isSupported(feature, version);
}

- (NSString *)namespaceURI
{
    return IMPL->namespaceURI();
}

- (NSString *)prefix
{
    return IMPL->prefix();
}

- (void)setPrefix:(NSString *)prefix
{
    ASSERT(prefix);

    WebCore::ExceptionCode ec = 0;
    IMPL->setPrefix(prefix, ec);
    raiseOnDOMError(ec);
}

- (NSString *)localName
{
    return IMPL->localName();
}

- (BOOL)hasAttributes
{
    return IMPL->hasAttributes();
}

- (BOOL)isSameNode:(DOMNode *)other
{
    return IMPL->isSameNode([other _node]);
}

- (BOOL)isEqualNode:(DOMNode *)other
{
    return IMPL->isEqualNode([other _node]);
}

- (BOOL)isDefaultNamespace:(NSString *)namespaceURI
{
    return IMPL->isDefaultNamespace(namespaceURI);
}

- (NSString *)lookupPrefix:(NSString *)namespaceURI
{
    return IMPL->lookupPrefix(namespaceURI);
}

- (NSString *)lookupNamespaceURI:(NSString *)prefix
{
    return IMPL->lookupNamespaceURI(prefix);
}

- (NSString *)textContent
{
    return IMPL->textContent();
}

- (void)setTextContent:(NSString *)text
{
    WebCore::ExceptionCode ec = 0;
    IMPL->setTextContent(text, ec);
    raiseOnDOMError(ec);
}

@end

@implementation DOMNode (DOMNodeExtensions)

- (NSRect)boundingBox
{
    WebCore::RenderObject *renderer = IMPL->renderer();
    if (renderer)
        return renderer->absoluteBoundingBoxRect();
    return NSZeroRect;
}

- (NSArray *)lineBoxRects
{
    WebCore::RenderObject *renderer = IMPL->renderer();
    if (renderer) {
        Vector<WebCore::IntRect> rects;
        renderer->lineBoxRects(rects);
        size_t size = rects.size();
        NSMutableArray *results = [NSMutableArray arrayWithCapacity:size];
        for (size_t i = 0; i < size; ++i)
            [results addObject:[NSValue valueWithRect:rects[i]]];
        return results;
    }
    return nil;
}

@end
