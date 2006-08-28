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

#import "DOMInternal.h" // needed for DOM_cast<>
#import "Node.h" // implementation class

#import "AtomicString.h"
#import "DOMDocument.h"
#import "DOMNamedNodeMap.h"
#import "DOMNodeList.h"
#import "DeprecatedValueList.h"
#import "IntRect.h"
#import "NamedAttrMap.h"
#import "NamedNodeMap.h"
#import "NodeList.h"
#import "PlatformString.h"
#import "RenderObject.h"
#import <wtf/Assertions.h>

@implementation DOMNode

- (void)dealloc
{
    if (_internal)
        DOM_cast<WebCore::Node *>(_internal)->deref();
    [super dealloc];
}

- (void)finalize
{
    if (_internal)
        DOM_cast<WebCore::Node *>(_internal)->deref();
    [super finalize];
}

- (NSString *)nodeName
{
    return [self _node]->nodeName();
}

- (NSString *)nodeValue
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return [self _node]->nodeValue();
}

- (void)setNodeValue:(NSString *)string
{
    ASSERT(string);
    
    WebCore::ExceptionCode ec = 0;
    [self _node]->setNodeValue(string, ec);
    raiseOnDOMError(ec);
}

- (unsigned short)nodeType
{
    return [self _node]->nodeType();
}

- (DOMNode *)parentNode
{
    return [DOMNode _nodeWith:[self _node]->parentNode()];
}

- (DOMNodeList *)childNodes
{
    return [DOMNodeList _nodeListWith:[self _node]->childNodes().get()];
}

- (DOMNode *)firstChild
{
    return [DOMNode _nodeWith:[self _node]->firstChild()];
}

- (DOMNode *)lastChild
{
    return [DOMNode _nodeWith:[self _node]->lastChild()];
}

- (DOMNode *)previousSibling
{
    return [DOMNode _nodeWith:[self _node]->previousSibling()];
}

- (DOMNode *)nextSibling
{
    return [DOMNode _nodeWith:[self _node]->nextSibling()];
}

- (DOMNamedNodeMap *)attributes
{
    return [DOMNamedNodeMap _namedNodeMapWith:[self _node]->attributes()];
}

- (DOMDocument *)ownerDocument
{
    return [DOMDocument _documentWith:[self _node]->document()];
}

- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild
{
    ASSERT(newChild);
    ASSERT(refChild);

    WebCore::ExceptionCode ec = 0;
    if ([self _node]->insertBefore([newChild _node], [refChild _node], ec))
        return newChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild
{
    ASSERT(newChild);
    ASSERT(oldChild);

    WebCore::ExceptionCode ec = 0;
    if ([self _node]->replaceChild([newChild _node], [oldChild _node], ec))
        return oldChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild
{
    ASSERT(oldChild);

    WebCore::ExceptionCode ec = 0;
    if ([self _node]->removeChild([oldChild _node], ec))
        return oldChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)appendChild:(DOMNode *)newChild
{
    ASSERT(newChild);

    WebCore::ExceptionCode ec = 0;
    if ([self _node]->appendChild([newChild _node], ec))
        return newChild;
    raiseOnDOMError(ec);
    return nil;
}

- (BOOL)hasChildNodes
{
    return [self _node]->hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    return [DOMNode _nodeWith:[self _node]->cloneNode(deep).get()];
}

- (void)normalize
{
    [self _node]->normalize();
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    return [self _node]->isSupported(feature, version);
}

- (NSString *)namespaceURI
{
    return [self _node]->namespaceURI();
}

- (NSString *)prefix
{
    return [self _node]->prefix();
}

- (void)setPrefix:(NSString *)prefix
{
    ASSERT(prefix);

    WebCore::ExceptionCode ec = 0;
    WebCore::String prefixStr(prefix);
    [self _node]->setPrefix(prefixStr.impl(), ec);
    raiseOnDOMError(ec);
}

- (NSString *)localName
{
    return [self _node]->localName();
}

- (BOOL)hasAttributes
{
    return [self _node]->hasAttributes();
}

- (BOOL)isSameNode:(DOMNode *)other
{
    return [self _node]->isSameNode([other _node]);
}

- (BOOL)isEqualNode:(DOMNode *)other
{
    return [self _node]->isEqualNode([other _node]);
}

- (BOOL)isDefaultNamespace:(NSString *)namespaceURI
{
    return [self _node]->isDefaultNamespace(namespaceURI);
}

- (NSString *)lookupPrefix:(NSString *)namespaceURI
{
    return [self _node]->lookupPrefix(namespaceURI);
}

- (NSString *)lookupNamespaceURI:(NSString *)prefix
{
    return [self _node]->lookupNamespaceURI(prefix);
}

- (NSString *)textContent
{
    return [self _node]->textContent();
}

- (void)setTextContent:(NSString *)text
{
    WebCore::ExceptionCode ec = 0;
    [self _node]->setTextContent(text, ec);
    raiseOnDOMError(ec);
}

@end

@implementation DOMNode (DOMNodeExtensions)

- (NSRect)boundingBox
{
    WebCore::RenderObject *renderer = [self _node]->renderer();
    if (renderer)
        return renderer->absoluteBoundingBoxRect();
    return NSZeroRect;
}

- (NSArray *)lineBoxRects
{
    WebCore::RenderObject *renderer = [self _node]->renderer();
    if (renderer) {
        NSMutableArray *results = [[NSMutableArray alloc] init];
        WebCore::DeprecatedValueList<WebCore::IntRect> rects = renderer->lineBoxRects();
        if (!rects.isEmpty()) {
            for (WebCore::DeprecatedValueList<WebCore::IntRect>::ConstIterator it = rects.begin(); it != rects.end(); ++it)
                [results addObject:[NSValue valueWithRect:*it]];
        }
        return [results autorelease];
    }
    return nil;
}

@end
