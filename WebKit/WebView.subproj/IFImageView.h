/*	
    IFImageView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

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


- (void)provisionalDataSourceChanged:(IFWebDataSource *)dataSource;
- (void)provisionalDataSourceCommitted:(IFWebDataSource *)dataSource;
- (void)dataSourceUpdated:(IFWebDataSource *)dataSource; 
- (void)layout;

- (void)setCanDragFrom: (BOOL)flag;
- (BOOL)canDragFrom;
- (void)setCanDragTo: (BOOL)flag;
- (BOOL)canDragTo;


@end
