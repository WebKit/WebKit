/*	
    WebDocument.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebDataSource;
@class WebError;

/*!
    @protocol WebDocumentView
*/
@protocol WebDocumentView <NSObject>
/*!
    @method setDataSource:
    @param dataSource
*/
- (void)setDataSource: (WebDataSource *)dataSource;

/*!
    @method dataSourceUpdated:
    @param dataSource
*/
- (void)dataSourceUpdated: (WebDataSource *)dataSource;

/*!
    @method setNeedsLayout:
    @param flag
*/
- (void)setNeedsLayout: (BOOL)flag;

/*!
    @method layout
*/
- (void)layout;
@end


/*!
    @protocol WebDocumentDragSettings
*/
@protocol WebDocumentDragSettings <NSObject>
/*!
    @method setCanDragFrom:
    @param flag
*/
- (void)setCanDragFrom: (BOOL)flag;

/*!
    @method canDragFrom
*/
- (BOOL)canDragFrom;

/*!
    @method setCanDragTo:
    @param flag
*/
- (void)setCanDragTo: (BOOL)flag;

/*!
    @method canDragTo
*/
- (BOOL)canDragTo;
@end


/*!
    @protocol WebDocumentSearching
*/
@protocol WebDocumentSearching <NSObject>
/*!
    @method searchFor:direction:caseSensitive:
*/
- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag;
@end


/*!
    @protocol WebDocumentTextEncoding
*/
@protocol WebDocumentTextEncoding <NSObject>
/*!
    @method supportsTextEncoding
*/
- (BOOL)supportsTextEncoding;
@end


/*!
    @protocol WebDocumentRepresentation
*/
@protocol WebDocumentRepresentation <NSObject>
/*!
    @method setDataSource:
    @param dataSource
*/
- (void)setDataSource: (WebDataSource *)dataSource;

/*!
    @method receivedData:withDataSource:
    @param data
    @param dataSource
*/
- (void)receivedData: (NSData *)data withDataSource: (WebDataSource *)dataSource;

/*!
    @method receivedError:withDataSource:
    @param error
    @param dataSource
*/
- (void)receivedError: (WebError *)error withDataSource: (WebDataSource *)dataSource;

/*!
    @method finishLoadingWithDataSource:
    @param dataSource
*/
- (void)finishedLoadingWithDataSource: (WebDataSource *)dataSource;
@end
