/*	
    IFMainURLHandleClient.h

    Private header.
    
    Copyright 2001, Apple, Inc. All rights reserved.
*/


#import <WebKit/IFLocationChangeHandler.h>
#import <WebKit/IFWebControllerPolicyHandler.h>
#import <WebFoundation/IFURLHandle.h>

@class IFDownloadHandler;
@class IFWebDataSource;
@protocol IFURLHandleClient;
@protocol IFResourceProgressHandler;

@interface IFMainURLHandleClient : NSObject <IFURLHandleClient>
{
    NSURL *url;
    id dataSource;
    BOOL processedBufferedData;
    BOOL isFirstChunk;
    IFDownloadHandler *downloadHandler;
    id <IFResourceProgressHandler> downloadProgressHandler;
}
- initWithDataSource: (IFWebDataSource *)ds;
- (IFDownloadHandler *) downloadHandler;
@end

