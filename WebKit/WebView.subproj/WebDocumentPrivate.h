/*	
    WebDocumentPrivate.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebDocument.h>

@class DOMDocument;

@protocol WebDocumentImage <NSObject>
- (NSImage *)image;
@end

// This method is deprecated as it now lives on WebFrame.
@protocol WebDocumentDOM <NSObject>
- (DOMDocument *)DOMDocument;
@end
