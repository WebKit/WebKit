/*	
    WebMainResourceClient.h

    Private header.
    
    Copyright 2001, 2002 Apple Computer Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebDownloadHandler;
@class WebDataSource;
@protocol WebResourceClient;
@protocol WebResourceProgressHandler;

@interface WebMainResourceClient : NSObject <WebResourceClient>
{
    NSURL *currentURL;
    WebDataSource *dataSource;
    BOOL processedBufferedData;
    BOOL isFirstChunk;
    WebDownloadHandler *downloadHandler;
    id <WebResourceProgressHandler> downloadProgressHandler;
}
- initWithDataSource:(WebDataSource *)dataSource;
- (WebDownloadHandler *)downloadHandler;
@end
