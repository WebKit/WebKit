/*	
    WebSubresourceClient.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/
#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>

@class WebDataSource;
@class WebResourceResponse;

@protocol WebCoreResourceHandle;
@protocol WebCoreResourceLoader;

@interface WebSubresourceClient : WebBaseResourceHandleDelegate <WebCoreResourceHandle>
{
    id <WebCoreResourceLoader> loader;
    WebDataSource *dataSource;
    NSURL *currentURL;
    WebResourceHandle *handle;

    // Both of these delegates are retained by the client.
    id <WebResourceLoadDelegate> resourceProgressDelegate;
    WebResourceRequest *request;
    WebResourceResponse *response;
    
    id identifier;
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
    withURL:(NSURL *)URL referrer:(NSString *)referrer forDataSource:(WebDataSource *)source;

- (WebResourceHandle *)handle;

@end
