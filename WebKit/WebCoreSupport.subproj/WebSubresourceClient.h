//
//  WebSubresourceClient.h
//  WebKit
//
//  Created by Darin Adler on Sat Jun 15 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebFoundation/WebResourceClient.h>

@class WebDataSource;
@protocol WebCoreResourceHandle;
@protocol WebCoreResourceLoader;

@interface WebSubresourceClient : NSObject <WebResourceClient, WebCoreResourceHandle>
{
    id <WebCoreResourceLoader> loader;
    WebDataSource *dataSource;
    NSURL *currentURL;
    WebResourceHandle *handle;
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader
    withURL:(NSURL *)URL dataSource:(WebDataSource *)dataSource;

- (WebResourceHandle *)handle;

@end
