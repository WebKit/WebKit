/*	
    WebDOMOperationsPrivate.h
    Copyright 2004, Apple, Inc. All rights reserved.

    Private header file.
*/

@class WebBridge;

#import <WebKit/WebDOMOperations.h>

@interface DOMNode (WebDOMNodeOperationsPrivate)
- (WebBridge *)_bridge;
- (NSArray *)_URLsFromSelectors:(SEL)firstSel, ...;
- (NSArray *)_subresourceURLs;
@end

@interface DOMRange (WebDOMRangeOperationsPrivate)
- (WebBridge *)_bridge;
@end

@interface DOMDocument (WebDOMDocumentOperationsPrivate)
- (DOMRange *)_createRangeWithNode:(DOMNode *)node;
- (DOMRange *)_documentRange;
@end

