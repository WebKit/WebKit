/*	
    IFHTMLRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>


@class IFError;
@class IFWebDataSource;
@class IFWebCoreBridge;

class KHTMLPart;

@protocol IFDocumentRepresentation;

@class IFHTMLRepresentationPrivate;

@interface IFHTMLRepresentation : NSObject <IFDocumentRepresentation>
{
    IFHTMLRepresentationPrivate *_private;
}

- (KHTMLPart *)part;
- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource;
- (void)receivedError:(IFError *)error withDataSource:(IFWebDataSource *)dataSource;
- (void)finishedLoadingWithDataSource:(IFWebDataSource *)dataSource;
@end
