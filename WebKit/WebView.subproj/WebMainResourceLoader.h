/*	
    IFMainURLHandleClient.h

    Private header.
    
    Copyright 2001, 2002 Apple Computer Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class IFDownloadHandler;
@class IFWebDataSource;
@protocol IFURLHandleClient;
@protocol IFResourceProgressHandler;

@interface IFMainURLHandleClient : NSObject <IFURLHandleClient>
{
    NSURL *currentURL;
    IFWebDataSource *dataSource;
    BOOL processedBufferedData;
    BOOL isFirstChunk;
    IFDownloadHandler *downloadHandler;
    id <IFResourceProgressHandler> downloadProgressHandler;
}
- initWithDataSource:(IFWebDataSource *)dataSource;
- (IFDownloadHandler *)downloadHandler;
@end
