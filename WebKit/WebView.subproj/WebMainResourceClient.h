/*	
    WebMainResourceClient.h

    Private header.
    
    Copyright 2001, 2002 Apple Computer Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebControllerPolicyDelegate.h>

@class WebDownloadHandler;
@class WebDataSource;
@class WebResourceHandle;
@class WebResourceResponse;
@protocol WebResourceHandleDelegate;
@protocol WebResourceProgressDelegate;

@interface WebMainResourceClient : NSObject <WebResourceHandleDelegate>
{
    NSURL *currentURL;
    WebDataSource *dataSource;
    BOOL isFirstChunk;
    BOOL suppressErrors;
    WebDownloadHandler *downloadHandler;
    id <WebResourceProgressDelegate> downloadProgressDelegate;
    WebContentAction policyAction;
    NSMutableData *resourceData;
}

- initWithDataSource:(WebDataSource *)dataSource;
- (WebDownloadHandler *)downloadHandler;
- (NSData *)resourceData;

- (void)didStartLoadingWithURL:(NSURL *)URL;
- (void)didCancelWithHandle:(WebResourceHandle *)handle;

@end
