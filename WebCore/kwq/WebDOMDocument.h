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

@protocol WebDOMAttr;
@protocol WebDOMComment;
@protocol WebDOMCDATASection;
@protocol WebDOMDocument;
@protocol WebDOMElement;
@protocol WebDOMEntityReference;
@protocol WebDOMNamedNodeMap;
@protocol WebDOMNode;
@protocol WebDOMNodeList;
@protocol WebDOMProcessingInstruction;
@protocol WebDOMText;


@protocol WebDOMDocumentType <WebDOMNode>

- (NSString *)name;

- (id<WebDOMNamedNodeMap>)entities;

- (id<WebDOMNamedNodeMap>)notations;

- (NSString *)publicId;

- (NSString *)systemId;

- (NSString *)internalSubset;

@end


@protocol WebDOMDocumentFragment <WebDOMNode>

@end


@protocol WebDOMImplementation

- (BOOL)hasFeature:(NSString *)feature :(NSString *)version;

- (id<WebDOMDocumentType>)createDocumentType:(NSString *)qualifiedName
                                      :(NSString *)publicId
                                      :(NSString *)systemId
                                      :(int *)exceptionCode;

- (id<WebDOMDocument>)createDocument:(NSString *)namespaceURI
                              :(NSString *)qualifiedName
                              :(id<WebDOMDocumentType>)doctype;

@end


@protocol WebDOMDocument <WebDOMNode>

- (id<WebDOMDocumentType>)doctype;

- (id<WebDOMImplementation>)implementation;

- (id<WebDOMElement>)documentElement;

- (id<WebDOMElement>)createElement:(NSString *)tagName;

- (id<WebDOMElement>)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName;

- (id<WebDOMDocumentFragment>)createDocumentFragment;

- (id<WebDOMText>)createTextNode:(NSString *)data;

- (id<WebDOMComment>)createComment:(NSString *)data;

- (id<WebDOMCDATASection>)createCDATASection:(NSString *)data;

- (id<WebDOMProcessingInstruction>)createProcessingInstruction:(NSString *)target :(NSString *)data;

- (id<WebDOMAttr>)createAttribute:(NSString *)name;

- (id<WebDOMAttr>)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName;

- (id<WebDOMEntityReference>)createEntityReference:(NSString *)name;

- (id<WebDOMElement>)getElementById:(NSString *)elementId;

- (id<WebDOMNodeList>)getElementsByTagName:(NSString *)tagname;

- (id<WebDOMNodeList>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;

- (id<WebDOMNode>)importNode:(id<WebDOMNode>)importedNode :(BOOL)deep;

@end
