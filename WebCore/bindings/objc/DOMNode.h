/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#import <WebCore/DOMObject.h>
#import <WebCore/DOMEventTarget.h>

@class DOMDocument; 
@class DOMNamedNodeMap;
@class DOMNodeList;

// DOM node types
enum {
    DOM_ELEMENT_NODE                  = 1,
    DOM_ATTRIBUTE_NODE                = 2,
    DOM_TEXT_NODE                     = 3,
    DOM_CDATA_SECTION_NODE            = 4,
    DOM_ENTITY_REFERENCE_NODE         = 5,
    DOM_ENTITY_NODE                   = 6,
    DOM_PROCESSING_INSTRUCTION_NODE   = 7,
    DOM_COMMENT_NODE                  = 8,
    DOM_DOCUMENT_NODE                 = 9,
    DOM_DOCUMENT_TYPE_NODE            = 10,
    DOM_DOCUMENT_FRAGMENT_NODE        = 11,
    DOM_NOTATION_NODE                 = 12
};

@interface DOMNode : DOMObject
- (NSString *)nodeName;
- (NSString *)nodeValue;
- (void)setNodeValue:(NSString *)string;
- (unsigned short)nodeType;
- (DOMNode *)parentNode;
- (DOMNodeList *)childNodes;
- (DOMNode *)firstChild;
- (DOMNode *)lastChild;
- (DOMNode *)previousSibling;
- (DOMNode *)nextSibling;
- (DOMNamedNodeMap *)attributes;
- (DOMDocument *)ownerDocument;
- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild;
- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild;
- (DOMNode *)removeChild:(DOMNode *)oldChild;
- (DOMNode *)appendChild:(DOMNode *)newChild;
- (BOOL)hasChildNodes;
- (DOMNode *)cloneNode:(BOOL)deep;
- (void)normalize;
- (BOOL)isSupported:(NSString *)feature :(NSString *)version;
- (NSString *)namespaceURI;
- (NSString *)prefix;
- (void)setPrefix:(NSString *)prefix;
- (NSString *)localName;
- (BOOL)hasAttributes;
- (BOOL)isSameNode:(DOMNode *)other;
- (BOOL)isEqualNode:(DOMNode *)other;
- (NSString *)textContent;
- (void)setTextContent:(NSString *)text;
@end

@interface DOMNode (DOMNodeExtensions)
- (NSRect)boundingBox;
- (NSArray *)lineBoxRects;
@end

@interface DOMNode (DOMEventTarget) <DOMEventTarget>
@end
