/*	
    WebDocumentPrivate.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebDocument.h>

@class DOMDocument;
@class DOMNode;
@class DOMRange;
@class WebArchive;

@protocol WebDocumentImage <NSObject>
- (NSImage *)image;
@end

@protocol WebDocumentMarkup <NSObject>

- (DOMDocument *)DOMDocument;

- (NSString *)markupStringFromNode:(DOMNode *)node;
- (NSString *)markupStringFromRange:(DOMRange *)range;

- (WebArchive *)webArchiveFromNode:(DOMNode *)node;
- (WebArchive *)webArchiveFromRange:(DOMRange *)range;

@end