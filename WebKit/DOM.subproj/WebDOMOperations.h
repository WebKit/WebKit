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
- (NSString *)markupString;
@end

@interface DOMDocument (WebDOMDocumentOperations)
- (WebFrame *)webFrame;
- (NSURL *)URLWithRelativeString:(NSString *)string;
@end

@interface DOMRange (WebDOMRangeOperations)

/*!
    @method webArchive
    @result A WebArchive representing the range.
*/
- (WebArchive *)webArchive;
- (NSString *)markupString;

@end
