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
@class WebResourceRequest;
@class WebResourceResponse;

@protocol WebResourceHandleDelegate;
@protocol WebResourceLoadDelegate;

@interface WebMainResourceClient : NSObject <WebResourceHandleDelegate>
{
    NSURL *currentURL;
    WebDataSource *dataSource;
    WebDownloadHandler *downloadHandler;
    
    // Both of these delegates are retained by the client.
    id <WebResourceLoadDelegate> downloadProgressDelegate;
    id <WebResourceLoadDelegate> resourceProgressDelegate;

    WebContentAction policyAction;
    NSMutableData *resourceData;
    WebResourceRequest *request;
    WebResourceResponse *response;
}

- initWithDataSource:(WebDataSource *)dataSource;
- (WebDownloadHandler *)downloadHandler;
- (NSData *)resourceData;

- (void)didStartLoadingWithURL:(NSURL *)URL;
- (void)didCancelWithHandle:(WebResourceHandle *)handle;

@end
