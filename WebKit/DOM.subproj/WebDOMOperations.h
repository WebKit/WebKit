/*	
    WebDOMOperations.h
    Copyright (C) 2004 Apple, Inc. All rights reserved.    
 
    Public header file.
*/

#import <WebKit/DOMCore.h>
#import <WebKit/DOMRange.h>

@class WebArchive;
@class WebFrame;

@interface DOMNode (WebDOMNodeOperations)

/*!
    @method webArchive
    @result A WebArchive representing the node and the children of the node.
*/
- (WebArchive *)webArchive;

@end

@interface DOMDocument (WebDOMDocumentOperations)

/*!
    @method webFrame
    @abstract Returns the frame of the DOM document.
*/
- (WebFrame *)webFrame;

/*!
    @method URLWithAttributeString
    @abstract Constructs a URL given an attribute string.
*/
- (NSURL *)URLWithAttributeString:(NSString *)string;

@end

@interface DOMRange (WebDOMRangeOperations)

/*!
    @method webArchive
    @result A WebArchive representing the range.
*/
- (WebArchive *)webArchive;

/*!
    @method markupString
    @result A markup string representing the range.
*/
- (NSString *)markupString;

@end
