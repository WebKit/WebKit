/*	
    WebSubresourceClient.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/
#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>

@class WebDataSource;
@class WebResponse;

@protocol WebCoreResourceHandle;
@protocol WebCoreResourceLoader;

@interface WebSubresourceClient : WebBaseResourceHandleDelegate <WebCoreResourceHandle>
{
    id <WebCoreResourceLoader> loader;
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
    withURL:(NSURL *)URL referrer:(NSString *)referrer forDataSource:(WebDataSource *)source;

@end
