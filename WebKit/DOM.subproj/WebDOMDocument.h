/*	
    WebDOMDocument.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebDOMNode.h>

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

- (BOOL)hasFeature: (NSString *)feature : (NSString *)version;

- (id<WebDOMDocumentType>)createDocumentType: (NSString *)qualifiedName
                                      :(NSString *)publicId
                                      :(NSString *)systemId;

- (id<WebDOMDocument>)createDocument: (NSString *)namespaceURI
                              : (NSString *)qualifiedName
                              : (id<WebDOMDocumentType>)doctype;

@end


@protocol WebDOMDocument <WebDOMNode>

- (id<WebDOMDocumentType>)doctype;

- (id<WebDOMDocument>)implementation;

- (id<WebDOMDocument>)documentElement;

- (id<WebDOMDocument>)createElement:(NSString *)tagName;

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

- (id<WebDOMEntityReference>)getElementsByTagName:(NSString *)tagname;

- (id<WebDOMEntityReference>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;

- (id<WebDOMEntityReference>)importNode:(id<WebDOMNode>)importedNode :(BOOL)deep;

@end
