/*	
    WebDOMElement.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@protocol WebDOMElement;
@protocol WebDOMNode;
@protocol WebDOMNodeList;


@protocol WebDOMAttr <NSObject>

- (NSString *)name;

- (BOOL)specified;

- (NSString *)value;

- (void)setValue:(NSString *)value;

- (id<WebDOMElement>)ownerElement;

@end


@protocol WebDOMCharacterData <NSObject>

- (NSString *)data;

- (void)setData: (NSString *)data;

- (unsigned long)length;

- (NSString *)substringData: (unsigned long)offset :(unsigned long)count;

- (void)appendData:(NSString *)arg;

- (void)insertData:(unsigned long)offset :(NSString *)arg;

- (void)deleteData:(unsigned long)offset :(unsigned long) count;

- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg;
@end


@protocol WebDOMComment <WebDOMCharacterData>

@end


@protocol WebDOMText <WebDOMCharacterData>

- (id<WebDOMText>)splitText: (unsigned long)offset;

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

- (NSString *)getAttribute: (NSString *)name;

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

- (BOOL)hasAttribute: (NSString *)name;

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName;

@end
