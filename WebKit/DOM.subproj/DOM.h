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

//=========================================================================
//=========================================================================
//=========================================================================

// Important Note:
// Though this file appears as an exported header from WebKit, the
// version you should edit is in WebCore. The WebKit version is copied
// to WebKit during the build process.

//=========================================================================
//=========================================================================
//=========================================================================

enum DOMNodeType {
    ELEMENT_NODE                   = 1,
    ATTRIBUTE_NODE                 = 2,
    TEXT_NODE                      = 3,
    CDATA_SECTION_NODE             = 4,
    ENTITY_REFERENCE_NODE          = 5,
    ENTITY_NODE                    = 6,
    PROCESSING_INSTRUCTION_NODE    = 7,
    COMMENT_NODE                   = 8,
    DOCUMENT_NODE                  = 9,
    DOCUMENT_TYPE_NODE             = 10,
    DOCUMENT_FRAGMENT_NODE         = 11,
    NOTATION_NODE                  = 12,
};

enum DOMExceptionCode {
    INDEX_SIZE_ERR                 = 1,
    DOMSTRING_SIZE_ERR             = 2,
    HIERARCHY_REQUEST_ERR          = 3,
    WRONG_DOCUMENT_ERR             = 4,
    INVALID_CHARACTER_ERR          = 5,
    NO_DATA_ALLOWED_ERR            = 6,
    NO_MODIFICATION_ALLOWED_ERR    = 7,
    NOT_FOUND_ERR                  = 8,
    NOT_SUPPORTED_ERR              = 9,
    INUSE_ATTRIBUTE_ERR            = 10,
    INVALID_STATE_ERR              = 11,
    SYNTAX_ERR                     = 12,
    INVALID_MODIFICATION_ERR       = 13,
    NAMESPACE_ERR                  = 14,
    INVALID_ACCESS_ERR             = 15,
};

extern NSString * const DOMErrorDomain;

@class NSError;
@class NSString;

@protocol NSObject;
@protocol DOMNode;
@protocol DOMNamedNodeMap;
@protocol DOMNodeList;
@protocol DOMImplementation;
@protocol DOMDocumentFragment;
@protocol DOMDocument;
@protocol DOMCharacterData;
@protocol DOMAttr;
@protocol DOMElement;
@protocol DOMText;
@protocol DOMComment;
@protocol DOMCDATASection;
@protocol DOMDocumentType;
@protocol DOMNotation;
@protocol DOMEntity;
@protocol DOMEntityReference;
@protocol DOMProcessingInstruction;
@protocol DOMRange;

@protocol DOMNode <NSObject>
- (NSString *)nodeName;
- (NSString *)nodeValue;
- (void)setNodeValue:(NSString *)string error:(NSError **)error;
- (unsigned short)nodeType;
- (id <DOMNode>)parentNode;
- (id <DOMNodeList>)childNodes;
- (id <DOMNode>)firstChild;
- (id <DOMNode>)lastChild;
- (id <DOMNode>)previousSibling;
- (id <DOMNode>)nextSibling;
- (id <DOMNamedNodeMap>)attributes;
- (id <DOMDocument>)ownerDocument;
- (id <DOMNode>)insertBefore:(id <DOMNode>)newChild :(id <DOMNode>)refChild error:(NSError **)error;
- (id <DOMNode>)replaceChild:(id <DOMNode>)newChild :(id <DOMNode>)oldChild error:(NSError **)error;
- (id <DOMNode>)removeChild:(id <DOMNode>)oldChild error:(NSError **)error;
- (id <DOMNode>)appendChild:(id <DOMNode>)newChild error:(NSError **)error;
- (BOOL)hasChildNodes;
- (id <DOMNode>)cloneNode:(BOOL)deep;
- (void)normalize;
- (BOOL)isSupported:(NSString *)feature :(NSString *)version;
- (NSString *)namespaceURI;
- (NSString *)prefix;
- (void)setPrefix:(NSString *)prefix error:(NSError **)error;
- (NSString *)localName;
- (BOOL)hasAttributes;
- (NSString *)HTMLString;
// begin deprecated methods
- (void)setNodeValue:(NSString *)string;
- (id<DOMNode>)insert:(id<DOMNode>)newChild before:(id<DOMNode>)refChild;
- (id<DOMNode>)replace:(id<DOMNode>)newChild child:(id<DOMNode>)oldChild;
- (id<DOMNode>)removeChild:(id<DOMNode>)oldChild;
- (id<DOMNode>)appendChild:(id<DOMNode>)newChild;
- (void)setPrefix:(NSString *)prefix;
// end deprecated methods
@end

@protocol DOMNamedNodeMap <NSObject>
- (id <DOMNode>)getNamedItem:(NSString *)name;
- (id <DOMNode>)setNamedItem:(id <DOMNode>)arg error:(NSError **)error;
- (id <DOMNode>)removeNamedItem:(NSString *)name error:(NSError **)error;
- (id <DOMNode>)item:(unsigned long)index;
- (unsigned long)length;
- (id <DOMNode>)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
- (id <DOMNode>)setNamedItemNS:(id <DOMNode>)arg error:(NSError **)error;
- (id <DOMNode>)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName error:(NSError **)error;
// begin deprecated methods
- (id<DOMNode>)setNamedItem:(id<DOMNode>)arg;
- (id<DOMNode>)removeNamedItem:(NSString *)name;
- (id<DOMNode>)setNamedItemNS:(id<DOMNode>)arg;
- (id<DOMNode>)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
// end deprecated methods
@end


@protocol DOMNodeList <NSObject>
- (id <DOMNode>)item:(unsigned long)index;
- (unsigned long)length;
@end


@protocol DOMImplementation <NSObject>
- (BOOL)hasFeature:(NSString *)feature :(NSString *)version;
- (id <DOMDocumentType>)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId error:(NSError **)error;
- (id <DOMDocument>)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(id <DOMDocumentType>)doctype error:(NSError **)error;
// begin deprecated methods
- (id<DOMDocumentType>)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId :(int *)exceptionCode;
- (id<DOMDocument>)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(id<DOMDocumentType>)doctype;
// end deprecated methods
@end


@protocol DOMDocumentFragment <DOMNode>
@end


@protocol DOMDocument <DOMNode>
- (id <DOMDocumentType>)doctype;
- (id <DOMImplementation>)implementation;
- (id <DOMElement>)documentElement;
- (id <DOMElement>)createElement:(NSString *)tagName error:(NSError **)error;
- (id <DOMDocumentFragment>)createDocumentFragment;
- (id <DOMText>)createTextNode:(NSString *)data;
- (id <DOMComment>)createComment:(NSString *)data;
- (id <DOMCDATASection>)createCDATASection:(NSString *)data error:(NSError **)error;
- (id <DOMProcessingInstruction>)createProcessingInstruction:(NSString *)target :(NSString *)data error:(NSError **)error;
- (id <DOMAttr>)createAttribute:(NSString *)name error:(NSError **)error;
- (id <DOMEntityReference>)createEntityReference:(NSString *)name error:(NSError **)error;
- (id <DOMNodeList>)getElementsByTagName:(NSString *)tagname;
- (id <DOMNode>)importNode:(id <DOMNode>)importedNode :(BOOL)deep error:(NSError **)error;
- (id <DOMElement>)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error;
- (id <DOMAttr>)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error;
- (id <DOMNodeList>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (id <DOMElement>)getElementById:(NSString *)elementId;
// begin deprecated methods
- (id<DOMElement>)createElement:(NSString *)tagName;
- (id<DOMElement>)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName;
- (id<DOMCDATASection>)createCDATASection:(NSString *)data;
- (id<DOMProcessingInstruction>)createProcessingInstruction:(NSString *)target :(NSString *)data;
- (id<DOMAttr>)createAttribute:(NSString *)name;
- (id<DOMEntityReference>)createEntityReference:(NSString *)name;
- (id<DOMNode>)importNode:(id<DOMNode>)importedNode :(BOOL)deep;
// end deprecated methods
@end


@protocol DOMCharacterData <DOMNode>
- (NSString *)data;
- (void)setData:(NSString *)data error:(NSError **)error;
- (unsigned long)length;
- (NSString *)substringData:(unsigned long)offset :(unsigned long)count error:(NSError **)error;
- (void)appendData:(NSString *)arg error:(NSError **)error;
- (void)insertData:(unsigned long)offset :(NSString *)arg error:(NSError **)error;
- (void)deleteData:(unsigned long)offset :(unsigned long) count error:(NSError **)error;
- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg error:(NSError **)error;
// begin deprecated methods
- (void)setData: (NSString *)data;
- (NSString *)substringData: (unsigned long)offset :(unsigned long)count;
- (void)appendData:(NSString *)arg;
- (void)insertData:(unsigned long)offset :(NSString *)arg;
- (void)deleteData:(unsigned long)offset :(unsigned long)count;
- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg;
// end deprecated methods
@end


@protocol DOMAttr <DOMNode>
- (NSString *)name;
- (BOOL)specified;
- (NSString *)value;
- (void)setValue:(NSString *)value error:(NSError **)error;
- (id <DOMElement>)ownerElement;
// begin deprecated methods
- (void)setValue:(NSString *)value;
// end deprecated methods
@end


@protocol DOMElement <DOMNode>
- (NSString *)tagName;
- (NSString *)getAttribute:(NSString *)name;
- (void)setAttribute:(NSString *)name :(NSString *)value error:(NSError **)error;
- (void)removeAttribute:(NSString *)name error:(NSError **)error;
- (id <DOMAttr>)getAttributeNode:(NSString *)name;
- (id <DOMAttr>)setAttributeNode:(id <DOMAttr>)newAttr error:(NSError **)error;
- (id <DOMAttr>)removeAttributeNode:(id <DOMAttr>)oldAttr error:(NSError **)error;
- (id <DOMNodeList>)getElementsByTagName:(NSString *)name;
- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value error:(NSError **)error;
- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName error:(NSError **)error;
- (id <DOMAttr>)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName;
- (id <DOMAttr>)setAttributeNodeNS:(id <DOMAttr>)newAttr error:(NSError **)error;
- (id <DOMNodeList>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (BOOL)hasAttribute:(NSString *)name;
- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
// begin deprecated methods
- (void)setAttribute:(NSString *)name :(NSString *)value;
- (void)removeAttribute:(NSString *)name;
- (id<DOMAttr>)setAttributeNode:(id<DOMAttr>)newAttr;
- (id<DOMAttr>)removeAttributeNode:(id<DOMAttr>)oldAttr;
- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value;
- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (id<DOMAttr>)setAttributeNodeNS:(id<DOMAttr>)newAttr;
// end deprecated methods
@end


@protocol DOMText <DOMCharacterData>
- (id <DOMText>)splitText:(unsigned long)offset error:(NSError **)error;
// begin deprecated methods
- (id<DOMText>)splitText:(unsigned long)offset;
// end deprecated methods
@end


@protocol DOMComment <DOMCharacterData>
@end


@protocol DOMCDATASection <DOMText>
@end


@protocol DOMDocumentType <DOMNode>
- (NSString *)name;
- (id <DOMNamedNodeMap>)entities;
- (id <DOMNamedNodeMap>)notations;
- (NSString *)publicId;
- (NSString *)systemId;
- (NSString *)internalSubset;
@end


@protocol DOMNotation <DOMNode>
- (NSString *)publicId;
- (NSString *)systemId;
@end


@protocol DOMEntity <DOMNode>
- (NSString *)publicId;
- (NSString *)systemId;
- (NSString *)notationName;
@end


@protocol DOMEntityReference <DOMNode>
@end


@protocol DOMProcessingInstruction <DOMNode>
- (NSString *)target;
- (NSString *)data;
- (void)setData:(NSString *)data error:(NSError **)error;
// begin deprecated methods
- (void)setData:(NSString *)data;
// end deprecated methods
@end


enum DOMCompareHow
{
    START_TO_START = 0,
    START_TO_END   = 1,
    END_TO_END     = 2,
    END_TO_START   = 3,
};

@protocol DOMRange <NSObject>
- (id <DOMNode>)startContainer:(NSError **)error;
- (long)startOffset:(NSError **)error;
- (id <DOMNode>)endContainer:(NSError **)error;
- (long)endOffset:(NSError **)error;
- (BOOL)collapsed:(NSError **)error;
- (id <DOMNode>)commonAncestorContainer:(NSError **)error;
- (void)setStart:(id <DOMNode>)refNode :(long)offset error:(NSError **)error;
- (void)setEnd:(id <DOMNode>)refNode :(long)offset error:(NSError **)error;
- (void)setStartBefore:(id <DOMNode>)refNode error:(NSError **)error;
- (void)setStartAfter:(id <DOMNode>)refNode error:(NSError **)error;
- (void)setEndBefore:(id <DOMNode>)refNode error:(NSError **)error;
- (void)setEndAfter:(id <DOMNode>)refNode error:(NSError **)error;
- (void)collapse:(BOOL)toStart error:(NSError **)error;
- (void)selectNode:(id <DOMNode>)refNode error:(NSError **)error;
- (void)selectNodeContents:(id <DOMNode>)refNode error:(NSError **)error;
- (short)compareBoundaryPoints:(unsigned short)how :(id <DOMRange>)sourceRange error:(NSError **)error;
- (void)deleteContents:(NSError **)error;
- (id <DOMDocumentFragment>)extractContents:(NSError **)error;
- (id <DOMDocumentFragment>)cloneContents:(NSError **)error;
- (void)insertNode:(id <DOMNode>)newNode error:(NSError **)error;
- (void)surroundContents:(id <DOMNode>)newParent error:(NSError **)error;
- (id <DOMRange>)cloneRange:(NSError **)error;
- (NSString *)toString:(NSError **)error;
- (void)detach:(NSError **)error;
@end

