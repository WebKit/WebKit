/*	
    WebImageView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebImageRepresentation;
@protocol WebDocumentImage;
@protocol WebDocumentView;

@interface WebImageView : NSView <WebDocumentView, WebDocumentImage>
{
    WebImageRepresentation *rep;
    BOOL needsLayout;
}
+ (NSArray *)supportedImageMIMETypes;
@end
