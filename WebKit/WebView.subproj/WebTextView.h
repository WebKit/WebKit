/*	
    IFTextView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class IFWebDataSource;
@protocol IFDocumentLoading;
@protocol IFDocumentDragSettings;
@protocol IFDocumentSearching;

@interface IFTextView : NSTextView <IFDocumentLoading, IFDocumentDragSettings, IFDocumentSearching>
{
    BOOL canDragFrom;
    BOOL canDragTo;
}

- (void)provisionalDataSourceChanged:(IFWebDataSource *)dataSource;
- (void)provisionalDataSourceCommitted:(IFWebDataSource *)dataSource;
- (void)dataSourceUpdated:(IFWebDataSource *)dataSource; 
- (void)layout;

- (void)setCanDragFrom: (BOOL)flag;
- (BOOL)canDragFrom;
- (void)setCanDragTo: (BOOL)flag;
- (BOOL)canDragTo;

- (void)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag;

@end
