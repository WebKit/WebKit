/*	
    WebDOMOperations.h
    Copyright (C) 2004 Apple, Inc. All rights reserved.    
 
    Public header file.
*/

#import <WebKit/DOM.h>

@class WebArchive;

@interface DOMNode (WebDOMNodeOperations)
- (WebArchive *)webArchive;
- (NSString *)markupString;
@end

@interface DOMRange (WebDOMRangeOperations)
- (WebArchive *)webArchive;
- (NSString *)markupString;
@end

@interface DOMHTMLImageElement (WebDOMHTMLImageElementOperations)
- (NSImage *)image;
@end
