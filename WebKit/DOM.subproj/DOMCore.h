/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebScriptObject.h>

@class DOMDocument; 
@class DOMDocumentType;
@class DOMElement;
@class DOMNamedNodeMap;
@class DOMNodeList;

@protocol DOMEventTarget;

typedef struct DOMObjectInternal DOMObjectInternal;

@interface DOMObject : WebScriptObject <NSCopying>
{
    DOMObjectInternal *_internal;
}
@end

typedef unsigned long long DOMTimeStamp;

extern NSString * const DOMException;

enum {
    //
    // DOM exception codes
    //
    DOM_INDEX_SIZE_ERR                = 1,
    DOM_DOMSTRING_SIZE_ERR            = 2,
    DOM_HIERARCHY_REQUEST_ERR         = 3,
    DOM_WRONG_DOCUMENT_ERR            = 4,
    DOM_INVALID_CHARACTER_ERR         = 5,
    DOM_NO_DATA_ALLOWED_ERR           = 6,
    DOM_NO_MODIFICATION_ALLOWED_ERR   = 7,
    DOM_NOT_FOUND_ERR                 = 8,
    DOM_NOT_SUPPORTED_ERR             = 9,
    DOM_INUSE_ATTRIBUTE_ERR           = 10,
    DOM_INVALID_STATE_ERR             = 11,
    DOM_SYNTAX_ERR                    = 12,
    DOM_INVALID_MODIFICATION_ERR      = 13,
    DOM_NAMESPACE_ERR                 = 14,
    DOM_INVALID_ACCESS_ERR            = 15,
};

@interface DOMImplementation : DOMObject
- (BOOL)hasFeature:(NSString *)feature :(NSString *)version;
- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId;
- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype;
@end

enum {
    //
    // DOM node types
    //
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
    DOM_NOTATION_NODE                 = 12,
};

@interface DOMNode : DOMObject <DOMEventTarget>
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
@end

@interface DOMNodeList : DOMObject
- (DOMNode *)item:(unsigned long)index;
- (unsigned long)length;
@end

@interface DOMNamedNodeMap : DOMObject
- (DOMNode *)getNamedItem:(NSString *)name;
- (DOMNode *)setNamedItem:(DOMNode *)arg;
- (DOMNode *)removeNamedItem:(NSString *)name;
- (DOMNode *)item:(unsigned long)index;
- (unsigned long)length;
- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNode *)setNamedItemNS:(DOMNode *)arg;
- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
@end

@interface DOMCharacterData : DOMNode
- (NSString *)data;
- (void)setData:(NSString *)data;
- (unsigned long)length;
- (NSString *)substringData:(unsigned long)offset :(unsigned long)count;
- (void)appendData:(NSString *)arg;
- (void)insertData:(unsigned long)offset :(NSString *)arg;
- (void)deleteData:(unsigned long)offset :(unsigned long) count;
- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg;
@end

@interface DOMAttr : DOMNode
- (NSString *)name;
- (BOOL)specified;
- (NSString *)value;
- (void)setValue:(NSString *)value;
- (DOMElement *)ownerElement;
@end

@interface DOMElement : DOMNode
- (NSString *)tagName;
- (NSString *)getAttribute:(NSString *)name;
- (void)setAttribute:(NSString *)name :(NSString *)value;
- (void)removeAttribute:(NSString *)name;
- (DOMAttr *)getAttributeNode:(NSString *)name;
- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr;
- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr;
- (DOMNodeList *)getElementsByTagName:(NSString *)name;
- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value;
- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (BOOL)hasAttribute:(NSString *)name;
- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
@end

@interface DOMText : DOMCharacterData
- (DOMText *)splitText:(unsigned long)offset;
@end

@interface DOMComment : DOMCharacterData
@end

@interface DOMCDATASection : DOMText
@end

@interface DOMDocumentType : DOMNode
- (NSString *)name;
- (DOMNamedNodeMap *)entities;
- (DOMNamedNodeMap *)notations;
- (NSString *)publicId;
- (NSString *)systemId;
- (NSString *)internalSubset;
@end

@interface DOMNotation : DOMNode
- (NSString *)publicId;
- (NSString *)systemId;
@end

@interface DOMEntity : DOMNode
- (NSString *)publicId;
- (NSString *)systemId;
- (NSString *)notationName;
@end

@interface DOMEntityReference : DOMNode
@end

@interface DOMProcessingInstruction : DOMNode
- (NSString *)target;
- (NSString *)data;
- (void)setData:(NSString *)data;
@end

@interface DOMDocumentFragment : DOMNode
@end

@interface DOMDocument : DOMNode
- (DOMDocumentType *)doctype;
- (DOMImplementation *)implementation;
- (DOMElement *)documentElement;
- (DOMElement *)createElement:(NSString *)tagName;
- (DOMDocumentFragment *)createDocumentFragment;
- (DOMText *)createTextNode:(NSString *)data;
- (DOMComment *)createComment:(NSString *)data;
- (DOMCDATASection *)createCDATASection:(NSString *)data;
- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data;
- (DOMAttr *)createAttribute:(NSString *)name;
- (DOMEntityReference *)createEntityReference:(NSString *)name;
- (DOMNodeList *)getElementsByTagName:(NSString *)tagname;
- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep;
- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName;
- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMElement *)getElementById:(NSString *)elementId;
@end
