/*	
    WebSubresourceClient.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>

@class WebDataSource;

@protocol WebCoreResourceHandle;
@protocol WebCoreResourceLoader;

@interface WebSubresourceClient : WebBaseResourceHandleDelegate <WebCoreResourceHandle>
{
    id <WebCoreResourceLoader> loader;
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
                                       withURL:(NSURL *)URL 
                                 customHeaders:(NSDictionary *)customHeaders
                                      referrer:(NSString *)referrer 
                                 forDataSource:(WebDataSource *)source;

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
                                       withURL:(NSURL *)URL 
                                 customHeaders:(NSDictionary *)customHeaders
                                      postData:(NSArray *)postData 
                                      referrer:(NSString *)referrer 
                                 forDataSource:(WebDataSource *)source;

@end
