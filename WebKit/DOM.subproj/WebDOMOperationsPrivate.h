/*	
    WebDOMOperationsPrivate.h
    Copyright 2004, Apple, Inc. All rights reserved.

    Private header file.
*/

@class WebBridge;

#import <WebKit/WebDOMOperations.h>

@interface DOMDocument (ToBeMadePublic)

/*!
    @method URLWithAttributeString
    @abstract Constructs a URL given an attribute string.
    @discussion This method constructs a URL given an attribute string just as WebKit does. 
    An attribute string is the value of an attribute of an element such as the href attribute on 
    the DOMHTMLAnchorElement class. This method is only applicable to attributes that refer to URLs.
*/
- (NSURL *)URLWithAttributeString:(NSString *)string;

@end

@interface DOMNode (WebDOMNodeOperationsPrivate)
- (WebBridge *)_bridge;
- (NSArray *)_URLsFromSelectors:(SEL)firstSel, ...;
- (NSArray *)_subresourceURLs;
@end

@interface DOMRange (ToBeMadePublic)

/*!
    @method markupString
    @result A markup string representing the range.
*/
- (NSString *)markupString;

@end

@interface DOMRange (WebDOMRangeOperationsPrivate)
- (WebBridge *)_bridge;
@end

