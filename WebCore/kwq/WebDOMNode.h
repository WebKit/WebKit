/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import <Foundation/Foundation.h>

@protocol WebDOMDocument;
@protocol WebDOMNamedNodeMap;
@protocol WebDOMNodeList;

enum WebNodeType {
    ELEMENT_NODE = 1,
    ATTRIBUTE_NODE = 2,
    TEXT_NODE = 3,
    CDATA_SECTION_NODE = 4,
    ENTITY_REFERENCE_NODE = 5,
    ENTITY_NODE = 6,
    PROCESSING_INSTRUCTION_NODE = 7,
    COMMENT_NODE = 8,
    DOCUMENT_NODE = 9,
    DOCUMENT_TYPE_NODE = 10,
    DOCUMENT_FRAGMENT_NODE = 11,
    NOTATION_NODE = 12
};

@protocol WebDOMNode <NSObject>

- (NSString *)nodeName;
- (NSString *)nodeValue;
- (void)setNodeValue:(NSString *)string; 
- (unsigned short)nodeType;
- (id<WebDOMNode>)parentNode;
- (id<WebDOMNodeList>)childNodes;
- (id<WebDOMNode>)firstChild;
- (id<WebDOMNode>)lastChild;
- (id<WebDOMNode>)previousSibling;
- (id<WebDOMNode>)nextSibling;
- (id<WebDOMNamedNodeMap>)attributes;
- (id<WebDOMDocument>)ownerDocument;
- (id<WebDOMNode>)insert:(id<(WebDOMNode)>)newChild before:(id<WebDOMNode>)refChild;
- (id<WebDOMNode>)replace:(id<WebDOMNode>)newChild child:(id<WebDOMNode>)oldChild;
- (id<WebDOMNode>)removeChild:(id<WebDOMNode>)oldChild;
- (id<WebDOMNode>)appendChild:(id<WebDOMNode>)newChild;
- (BOOL)hasChildNodes;
- (id<WebDOMNode>)cloneNode:(BOOL) deep;
- (void)normalize;
- (BOOL)isSupported:(NSString *)feature :(NSString *)version;
- (NSString *)namespaceURI;
- (NSString *)prefix;
- (void)setPrefix:(NSString *)prefix;
- (NSString *)localName;
- (BOOL)hasAttributes;

@end


@protocol WebDOMNamedNodeMap <NSObject>

- (unsigned long) length;
- (id<WebDOMNode>)getNamedItem:(NSString *)name;
- (id<WebDOMNode>)setNamedItem:(id<WebDOMNode>)arg;
- (id<WebDOMNode>)removeNamedItem:(NSString *)name;
- (id<WebDOMNode>)item:(unsigned long) index;
- (id<WebDOMNode>)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
- (id<WebDOMNode>)setNamedItemNS:(id<WebDOMNode>)arg;
- (id<WebDOMNode>)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;

@end


@protocol WebDOMNodeList <NSObject>

- (unsigned long)length;
- (id<WebDOMNode>)item:(unsigned long)index;

@end

