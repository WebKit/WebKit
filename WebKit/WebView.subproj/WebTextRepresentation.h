/*	
    WebTextRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebError;
@class WebDataSource;
@protocol WebDocumentRepresentation;

@interface WebTextRepresentation : NSObject <WebDocumentRepresentation>
{

}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource;
- (void)receivedError:(WebError *)error withDataSource:(WebDataSource *)dataSource;
- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource;
@end
