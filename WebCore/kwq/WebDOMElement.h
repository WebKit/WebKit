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

@protocol WebDOMElement;
@protocol WebDOMNode;
@protocol WebDOMNodeList;


@protocol WebDOMAttr <WebDOMNode>

- (NSString *)name;

- (BOOL)specified;

- (NSString *)value;

- (void)setValue:(NSString *)value;

- (id<WebDOMElement>)ownerElement;

@end


@protocol WebDOMCharacterData <WebDOMNode>

- (NSString *)data;

- (void)setData:(NSString *)data;

- (unsigned long)length;

- (NSString *)substringData:(unsigned long)offset :(unsigned long)count;

- (void)appendData:(NSString *)arg;

- (void)insertData:(unsigned long)offset :(NSString *)arg;

- (void)deleteData:(unsigned long)offset :(unsigned long) count;

- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg;
@end


@protocol WebDOMComment <WebDOMCharacterData>

@end


@protocol WebDOMText <WebDOMCharacterData>

- (id<WebDOMText>)splitText:(unsigned long)offset;

@end


@protocol WebDOMCDATASection <WebDOMText>

@end


@protocol WebDOMProcessingInstruction <WebDOMNode>

- (NSString *)target;

- (NSString *)data;

- (void)setData:(NSString *)data;

@end


@protocol WebDOMEntityReference <WebDOMNode>

@end


@protocol WebDOMElement <NSObject>

- (NSString *)tagName;

- (NSString *)getAttribute:(NSString *)name;

- (void)setAttribute:(NSString *)name :(NSString *)value;

- (void)removeAttribute:(NSString *)name;

- (id<WebDOMAttr>)getAttributeNode:(NSString *)name;

- (id<WebDOMAttr>)setAttributeNode:(id<WebDOMAttr>)newAttr;

- (id<WebDOMAttr>)removeAttributeNode:(id<WebDOMAttr>)oldAttr;

- (id<WebDOMNodeList>)getElementsByTagName:(NSString *)name;

- (id<WebDOMNodeList>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;

- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName;

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value;

- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName;

- (id<WebDOMAttr>)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName;

- (id<WebDOMAttr>)setAttributeNodeNS:(id<WebDOMAttr>)newAttr;

- (BOOL)hasAttribute:(NSString *)name;

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName;

@end
