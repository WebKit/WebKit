//
//  WebSubresourceClient.h
//  WebKit
//
//  Created by Darin Adler on Sat Jun 15 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebFoundation/WebResourceClient.h>

@protocol WebCoreResourceLoader;
@class WebDataSource;

@interface WebSubresourceClient : NSObject <WebResourceClient>
{
    id <WebCoreResourceLoader> loader;
    WebDataSource *dataSource;
    NSURL *currentURL;
}

+ (WebResourceHandle *)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader
    withURL:(NSURL *)URL dataSource:(WebDataSource *)dataSource;

@end
