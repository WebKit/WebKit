/*	
    WebImageRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebError;
@class WebDataSource;
@class WebImageRenderer;
@protocol WebDocumentRepresentation;

@interface WebImageRepresentation : NSObject <WebDocumentRepresentation>
{
    WebImageRenderer *image;
}

- (WebImageRenderer *)image;
- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource;
- (void)receivedError:(WebError *)error withDataSource:(WebDataSource *)dataSource;
- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource;
@end
