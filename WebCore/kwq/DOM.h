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

enum DOMRangeErrorCode {
    DOMBadBoundaryPointsError         = 1,
    DOMInvalidNodeTypeError           = 2,
};

extern NSString * const DOMException;
extern NSString * const DOMRangeException;

@class DOMAttr;
@class DOMCDATASection;
@class DOMComment;
@class DOMDocument;
@class DOMDocumentType;
@class DOMElement;
@class DOMEntityReference;
@class DOMNamedNodeMap;
@class DOMNodeList;
@class DOMProcessingInstruction;
@class DOMText;

typedef struct DOMObjectInternal DOMObjectInternal;

@interface DOMObject : NSObject <NSCopying>
{
    DOMObjectInternal *_internal;
}
@end

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
- (NSString *)HTMLString;
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


@interface DOMNodeList : DOMObject
- (DOMNode *)item:(unsigned long)index;
- (unsigned long)length;
@end


@interface DOMImplementation : DOMObject
- (BOOL)hasFeature:(NSString *)feature :(NSString *)version;
- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId;
- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype;
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


enum DOMCompareHow
{
    DOMCompareStartToStart = 0,
    DOMCompareStartToEnd   = 1,
    DOMCompareEndToEnd     = 2,
    DOMCompareEndToStart   = 3,
};

@interface DOMRange : DOMObject
- (DOMNode *)startContainer;
- (long)startOffset;
- (DOMNode *)endContainer;
- (long)endOffset;
- (BOOL)collapsed;
- (DOMNode *)commonAncestorContainer;
- (void)setStart:(DOMNode *)refNode :(long)offset;
- (void)setEnd:(DOMNode *)refNode :(long)offset;
- (void)setStartBefore:(DOMNode *)refNode;
- (void)setStartAfter:(DOMNode *)refNode;
- (void)setEndBefore:(DOMNode *)refNode;
- (void)setEndAfter:(DOMNode *)refNode;
- (void)collapse:(BOOL)toStart;
- (void)selectNode:(DOMNode *)refNode;
- (void)selectNodeContents:(DOMNode *)refNode;
- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange;
- (void)deleteContents;
- (DOMDocumentFragment *)extractContents;
- (DOMDocumentFragment *)cloneContents;
- (void)insertNode:(DOMNode *)newNode;
- (void)surroundContents:(DOMNode *)newParent;
- (DOMRange *)cloneRange;
- (NSString *)toString;
- (void)detach;
@end
