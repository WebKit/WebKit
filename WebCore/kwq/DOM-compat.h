/*	
    DOM-compat.h
    Copyright 2004, Apple, Inc. All rights reserved.
*/

//
// To preserve source compatibility with Message.framework
//
@protocol WebDOMNode
- (NSString *)nodeName;
- (NSString *)nodeValue;
- (unsigned short)nodeType;
- (BOOL)hasAttributes;
- (id)attributes;
- (id)firstChild;
- (id)nextSibling;
- (id)parentNode;
@end

@protocol WebDOMDocument <WebDOMNode>
@end

#define ELEMENT_NODE 1
#define TEXT_NODE 3
