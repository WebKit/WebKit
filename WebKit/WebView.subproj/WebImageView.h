/*	
    WebImageView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocumentPrivate.h>

@class WebImageRepresentation;

@interface WebImageView : NSView <WebDocumentView, WebDocumentImage>
{
    WebImageRepresentation *rep;
    BOOL needsLayout;
    BOOL ignoringMouseDraggedEvents;
}
+ (NSArray *)supportedImageMIMETypes;
@end
