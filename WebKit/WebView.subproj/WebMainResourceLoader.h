/*	
    WebMainResourceClient.h

    Private header.
    
    Copyright 2001, 2002 Apple Computer Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>
#import <WebKit/WebControllerPolicyDelegate.h>

@class WebDownload;
@class WebDataSource;
@class WebResourceHandle;
@class WebResourceRequest;
@class WebResourceResponse;

@protocol WebResourceHandleDelegate;
@protocol WebResourceLoadDelegate;

@interface WebMainResourceClient : WebBaseResourceHandleDelegate
{
    WebDownload *download;
    NSMutableData *resourceData;
    int _contentLength; // for logging only
    int _bytesReceived; // for logging only
}

- initWithDataSource:(WebDataSource *)dataSource;
- (WebDownload *)download;
- (NSData *)resourceData;

@end
