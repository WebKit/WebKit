/*	
    IFTextRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class IFError;
@class IFWebDataSource;
@protocol IFDocumentRepresentation;

@interface IFTextRepresentation : NSObject <IFDocumentRepresentation>
{

}

- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource;
- (void)receivedError:(IFError *)error withDataSource:(IFWebDataSource *)dataSource;
- (void)finishedLoadingWithDataSource:(IFWebDataSource *)dataSource;
@end
