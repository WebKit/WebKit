/*	
    WebImageView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebImageRepresentation;
@protocol WebDocumentView;
@protocol WebDocumentDragSettings;

@interface WebImageView : NSView <WebDocumentView, WebDocumentDragSettings>
{
    WebImageRepresentation *representation;
    BOOL canDragFrom;
    BOOL canDragTo;
    BOOL didSetFrame;
}
@end
