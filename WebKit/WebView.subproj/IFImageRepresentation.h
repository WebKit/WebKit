/*	
    IFImageRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class IFError;
@class IFWebDataSource;
@class IFImageRenderer;
@protocol IFDocumentRepresentation;

@interface IFImageRepresentation : NSObject <IFDocumentRepresentation>
{
    IFImageRenderer *image;
}

- (IFImageRenderer *)image;
- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource;
- (void)receivedError:(IFError *)error withDataSource:(IFWebDataSource *)dataSource;
- (void)finishedLoadingWithDataSource:(IFWebDataSource *)dataSource;
@end
