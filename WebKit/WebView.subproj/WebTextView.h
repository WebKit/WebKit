/*	
    WebTextView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebDataSource;
@protocol WebDocumentLoading;
@protocol WebDocumentDragSettings;
@protocol WebDocumentSearching;

@interface WebTextView : NSTextView <WebDocumentLoading, WebDocumentDragSettings, WebDocumentSearching>
{
    BOOL canDragFrom;
    BOOL canDragTo;
}

- (void)provisionalDataSourceChanged:(WebDataSource *)dataSource;
- (void)provisionalDataSourceCommitted:(WebDataSource *)dataSource;
- (void)dataSourceUpdated:(WebDataSource *)dataSource; 
- (void)layout;

- (void)setCanDragFrom: (BOOL)flag;
- (BOOL)canDragFrom;
- (void)setCanDragTo: (BOOL)flag;
- (BOOL)canDragTo;

@end
