/*	
    IFImageRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class IFError;
@class IFWebDataSource;
@class IFImageRenderer;
@protocol IFDocumentRepresentation;

@interface IFImageRepresentation : NSObject <IFDocumentRepresentation>
{
    IFImageRenderer *image;
}

- (IFImageRenderer *)image;
- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource isComplete:(BOOL)isComplete;
- (void)receivedError:(IFError *)error withDataSource:(IFWebDataSource *)dataSource;

@end
