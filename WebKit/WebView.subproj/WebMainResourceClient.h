/*	
    IFMainURLHandleClient.h

    Private header.
    
    Copyright 2001, Apple, Inc. All rights reserved.
*/


#import <WebKit/IFLocationChangeHandler.h>
#import <WebFoundation/IFURLHandle.h>

@class IFDownloadHandler;
@class IFWebDataSource;
@protocol IFURLHandleClient;

@interface IFMainURLHandleClient : NSObject <IFURLHandleClient>
{
    NSURL *url;
    id dataSource;
    BOOL processedBufferedData;
    BOOL isFirstChunk;
    IFDownloadHandler *downloadHandler;
}
- initWithDataSource: (IFWebDataSource *)ds;
- (IFDownloadHandler *) downloadHandler;
@end

