/*	
    WebMainResourceClient.h

    Private header.
    
    Copyright 2001, 2002 Apple Computer Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>
#import <WebKit/WebControllerPolicyDelegate.h>

@class WebDownloadHandler;
@class WebDataSource;
@class WebResourceHandle;
@class WebResourceRequest;
@class WebResourceResponse;

@protocol WebResourceHandleDelegate;
@protocol WebResourceLoadDelegate;

@interface WebMainResourceClient : WebBaseResourceHandleDelegate
{
    WebDownloadHandler *downloadHandler;
    NSMutableData *resourceData;
    int _contentLength; // for logging only
    int _bytesReceived; // for logging only
}

- initWithDataSource:(WebDataSource *)dataSource;
- (WebDownloadHandler *)downloadHandler;
- (NSData *)resourceData;

@end
