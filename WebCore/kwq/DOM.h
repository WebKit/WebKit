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
    DOMElementNodeType                   = 1,
    DOMAttributeNodeType                 = 2,
    DOMTextNodeType                      = 3,
    DOMCDATASectionNodeType              = 4,
    DOMEntityReferenceNodeType           = 5,
    DOMEntityNodeType                    = 6,
    DOMProcessingInstructionNodeType     = 7,
    DOMCommentNodeType                   = 8,
    DOMDocumentNodeType                  = 9,
    DOMDocumentTypeNodeType              = 10,
    DOMDocumentFragmentNodeType          = 11,
    DOMNotationNodeType                  = 12,
};

enum DOMErrorCode {
    DOMIndexSizeError                 = 1,
    DOMStringSizeError                = 2,
    DOMHierarchyRequestError          = 3,
    DOMWrongDocumentError             = 4,
    DOMInvalidCharacterError          = 5,
    DOMNoDataAllowedError             = 6,
    DOMNoModificationAllowedError     = 7,
    DOMNotFoundError                  = 8,
    DOMNotSupportedError              = 9,
    DOMInUseAttributeError            = 10,
    DOMInvalidStateError              = 11,
    DOMSyntaxError                    = 12,
    DOMInvalidModificationError       = 13,
    DOMNamespaceError                 = 14,
    DOMInvalidAccessError             = 15,
};

extern NSString * const DOMErrorDomain;

@class NSError;
@class NSString;

@class DOMNode;
@class DOMNamedNodeMap;
@class DOMNodeList;
@class DOMDocumentFragment;
@class DOMDocument;
@class DOMCharacterData;
@class DOMAttr;
@class DOMElement;
@class DOMText;
@class DOMComment;
@class DOMCDATASection;
@class DOMDocumentType;
@class DOMNotation;
@class DOMEntity;
@class DOMEntityReference;
@class DOMProcessingInstruction;
@class DOMRange;

@interface DOMNode : NSObject <NSCopying>
- (NSString *)nodeName;
- (NSString *)nodeValue;
- (void)setNodeValue:(NSString *)string error:(NSError **)error;
- (unsigned short)nodeType;
- (DOMNode *)parentNode;
- (DOMNodeList *)childNodes;
- (DOMNode *)firstChild;
- (DOMNode *)lastChild;
- (DOMNode *)previousSibling;
- (DOMNode *)nextSibling;
- (DOMNamedNodeMap *)attributes;
- (DOMDocument *)ownerDocument;
- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild error:(NSError **)error;
- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild error:(NSError **)error;
- (DOMNode *)removeChild:(DOMNode *)oldChild error:(NSError **)error;
- (DOMNode *)appendChild:(DOMNode *)newChild error:(NSError **)error;
- (BOOL)hasChildNodes;
- (DOMNode *)cloneNode:(BOOL)deep;
- (void)normalize;
- (BOOL)isSupported:(NSString *)feature :(NSString *)version;
- (NSString *)namespaceURI;
- (NSString *)prefix;
- (void)setPrefix:(NSString *)prefix error:(NSError **)error;
- (NSString *)localName;
- (BOOL)hasAttributes;
- (NSString *)HTMLString;
@end

@interface DOMNamedNodeMap : NSObject <NSCopying>
- (DOMNode *)getNamedItem:(NSString *)name;
- (DOMNode *)setNamedItem:(DOMNode *)arg error:(NSError **)error;
- (DOMNode *)removeNamedItem:(NSString *)name error:(NSError **)error;
- (DOMNode *)item:(unsigned long)index;
- (unsigned long)length;
- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNode *)setNamedItemNS:(DOMNode *)arg error:(NSError **)error;
- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName error:(NSError **)error;
@end


@interface DOMNodeList : NSObject <NSCopying>
- (DOMNode *)item:(unsigned long)index;
- (unsigned long)length;
@end


@interface DOMImplementation : NSObject <NSCopying>
- (BOOL)hasFeature:(NSString *)feature :(NSString *)version;
- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId error:(NSError **)error;
- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype error:(NSError **)error;
@end


@interface DOMDocumentFragment : DOMNode
@end


@interface DOMDocument : DOMNode
- (DOMDocumentType *)doctype;
- (DOMImplementation *)implementation;
- (DOMElement *)documentElement;
- (DOMElement *)createElement:(NSString *)tagName error:(NSError **)error;
- (DOMDocumentFragment *)createDocumentFragment;
- (DOMText *)createTextNode:(NSString *)data;
- (DOMComment *)createComment:(NSString *)data;
- (DOMCDATASection *)createCDATASection:(NSString *)data error:(NSError **)error;
- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data error:(NSError **)error;
- (DOMAttr *)createAttribute:(NSString *)name error:(NSError **)error;
- (DOMEntityReference *)createEntityReference:(NSString *)name error:(NSError **)error;
- (DOMNodeList *)getElementsByTagName:(NSString *)tagname;
- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep error:(NSError **)error;
- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error;
- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMElement *)getElementById:(NSString *)elementId;
@end


@interface DOMCharacterData : DOMNode
- (NSString *)data;
- (void)setData:(NSString *)data error:(NSError **)error;
- (unsigned long)length;
- (NSString *)substringData:(unsigned long)offset :(unsigned long)count error:(NSError **)error;
- (void)appendData:(NSString *)arg error:(NSError **)error;
- (void)insertData:(unsigned long)offset :(NSString *)arg error:(NSError **)error;
- (void)deleteData:(unsigned long)offset :(unsigned long) count error:(NSError **)error;
- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg error:(NSError **)error;
@end


@interface DOMAttr : DOMNode
- (NSString *)name;
- (BOOL)specified;
- (NSString *)value;
- (void)setValue:(NSString *)value error:(NSError **)error;
- (DOMElement *)ownerElement;
@end


@interface DOMElement : DOMNode
- (NSString *)tagName;
- (NSString *)getAttribute:(NSString *)name;
- (void)setAttribute:(NSString *)name :(NSString *)value error:(NSError **)error;
- (void)removeAttribute:(NSString *)name error:(NSError **)error;
- (DOMAttr *)getAttributeNode:(NSString *)name;
- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr error:(NSError **)error;
- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr error:(NSError **)error;
- (DOMNodeList *)getElementsByTagName:(NSString *)name;
- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value error:(NSError **)error;
- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName error:(NSError **)error;
- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr error:(NSError **)error;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (BOOL)hasAttribute:(NSString *)name;
- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
@end


@interface DOMText : DOMCharacterData
- (DOMText *)splitText:(unsigned long)offset error:(NSError **)error;
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
- (void)setData:(NSString *)data error:(NSError **)error;
@end


enum DOMCompareHow
{
    DOMCompareStartToStart = 0,
    DOMCompareStartToEnd   = 1,
    DOMCompareEndToEnd     = 2,
    DOMCompareEndToStart   = 3,
};

@interface DOMRange : NSObject
- (DOMNode *)startContainer:(NSError **)error;
- (long)startOffset:(NSError **)error;
- (DOMNode *)endContainer:(NSError **)error;
- (long)endOffset:(NSError **)error;
- (BOOL)collapsed:(NSError **)error;
- (DOMNode *)commonAncestorContainer:(NSError **)error;
- (void)setStart:(DOMNode *)refNode :(long)offset error:(NSError **)error;
- (void)setEnd:(DOMNode *)refNode :(long)offset error:(NSError **)error;
- (void)setStartBefore:(DOMNode *)refNode error:(NSError **)error;
- (void)setStartAfter:(DOMNode *)refNode error:(NSError **)error;
- (void)setEndBefore:(DOMNode *)refNode error:(NSError **)error;
- (void)setEndAfter:(DOMNode *)refNode error:(NSError **)error;
- (void)collapse:(BOOL)toStart error:(NSError **)error;
- (void)selectNode:(DOMNode *)refNode error:(NSError **)error;
- (void)selectNodeContents:(DOMNode *)refNode error:(NSError **)error;
- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange error:(NSError **)error;
- (void)deleteContents:(NSError **)error;
- (DOMDocumentFragment *)extractContents:(NSError **)error;
- (DOMDocumentFragment *)cloneContents:(NSError **)error;
- (void)insertNode:(DOMNode *)newNode error:(NSError **)error;
- (void)surroundContents:(DOMNode *)newParent error:(NSError **)error;
- (DOMRange *)cloneRange:(NSError **)error;
- (NSString *)toString:(NSError **)error;
- (void)detach:(NSError **)error;
@end

