/*	
    WebDocument.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebDataSource;
@class WebError;

@protocol WebDocumentView <NSObject>
- (void)provisionalDataSourceChanged: (WebDataSource *)dataSource;
- (void)provisionalDataSourceCommitted: (WebDataSource *)dataSource;
- (void)dataSourceUpdated: (WebDataSource *)dataSource;
- (void)layout;
@end

@protocol WebDocumentDragSettings <NSObject>
- (void)setCanDragFrom: (BOOL)flag;
- (BOOL)canDragFrom;
- (void)setCanDragTo: (BOOL)flag;
- (BOOL)canDragTo;
@end

@protocol WebDocumentSearching <NSObject>
- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag;
@end

@protocol WebDocumentTextEncoding <NSObject>
- (CFStringEncoding)textEncoding;
- (void)setTextEncoding: (CFStringEncoding)encoding;
- (void)setDefaultTextEncoding;
- (BOOL)usingDefaultTextEncoding;
@end

@protocol WebDocumentRepresentation <NSObject>
- (void)setDataSource: (WebDataSource *)dataSource;
- (void)receivedData: (NSData *)data withDataSource: (WebDataSource *)dataSource;
- (void)receivedError: (WebError *)error withDataSource: (WebDataSource *)dataSource;
- (void)finishedLoadingWithDataSource: (WebDataSource *)dataSource;
@end
