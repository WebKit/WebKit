/*	
    WebDOMNamedNodeMap.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@protocol WebDOMNode;


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

- (id<WebDOMNode>)item: (unsigned long)index;

@end
