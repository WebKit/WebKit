/*	
    WebDOMNode.h
    Copyright 2002, Apple, Inc. All rights reserved.
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

- (void)setNodeValue: (NSString *)string; 

- (unsigned short)nodeType;

- (id<WebDOMNode>)parentNode;

- (id<WebDOMNodeList>)childNodes;

- (id<WebDOMNode>)firstChild;

- (id<WebDOMNode>)lastChild;

- (id<WebDOMNode>) previousSibling;

- (id<WebDOMNode>)nextSibling;

- (id<WebDOMNamedNodeMap>)attributes;

- (id<WebDOMDocument>)ownerDocument;

- (id<WebDOMNode>)insert:(id<WebDOMDocument>)newChild before:(id<WebDOMDocument>)refChild;

- (id<WebDOMNode>)replace:(id<WebDOMDocument>)newChild child:(id<WebDOMDocument>)oldChild;

- (id<WebDOMNode>)removeChild:(id<WebDOMDocument>)oldChild;

- (id<WebDOMNode>)appendChild:(id<WebDOMDocument>)newChild;

- (BOOL)hasChildNodes;

- (id<WebDOMNode>)cloneNode: (BOOL) deep;

- (void)normalize;

- (BOOL)isSupported:(NSString *)feature : (NSString *)version;

- (NSString *)namespaceURIt;

- (NSString *)prefix;

- (void)setPrefix: (NSString *)prefix;

- (NSString *)localName;

- (BOOL)hasAttributes;

@end
