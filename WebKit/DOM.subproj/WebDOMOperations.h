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
- (WebArchive *)webArchive;
- (NSString *)markupString;
@end

@interface DOMDocument (WebDOMDocumentOperations)
- (WebFrame *)webFrame;
- (NSURL *)URLWithRelativeString:(NSString *)string;
@end

@interface DOMRange (WebDOMRangeOperations)
- (WebArchive *)webArchive;
- (NSString *)markupString;
@end
