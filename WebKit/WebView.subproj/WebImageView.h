/*	
    WebImageView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocumentInternal.h>

@class WebImageRepresentation;

@interface WebImageView : NSView <WebDocumentView, WebDocumentImage, WebDocumentElement>
{
    WebImageRepresentation *rep;
    BOOL needsLayout;
    BOOL ignoringMouseDraggedEvents;
    NSEvent *mouseDownEvent;
    unsigned int dragSourceActionMask;
}
+ (NSArray *)supportedImageMIMETypes;
@end
