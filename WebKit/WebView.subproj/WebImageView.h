/*	
    WebImageView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebImageRepresentation;
@protocol WebDocumentView;

@interface WebImageView : NSView <WebDocumentView>
{
    WebImageRepresentation *rep;
    BOOL needsLayout;
}
+ (NSArray *)supportedImageMIMETypes;
@end
