/*	
    IFImageView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class IFWebDataSource;
@class IFImageRepresentation;
@protocol IFDocumentLoading;
@protocol IFDocumentDragSettings;

@interface IFImageView : NSView <IFDocumentLoading, IFDocumentDragSettings>
{
    IFImageRepresentation *representation;
    BOOL canDragFrom;
    BOOL canDragTo;
    BOOL didSetFrame;
}
@end
