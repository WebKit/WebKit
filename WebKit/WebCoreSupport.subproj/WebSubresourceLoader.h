//
//  IFResourceURLHandleClient.h
//  WebKit
//
//  Created by Darin Adler on Sat Jun 15 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebFoundation/IFURLHandleClient.h>

@protocol WebCoreResourceLoader;
@class IFWebDataSource;

@interface IFResourceURLHandleClient : NSObject <IFURLHandleClient>
{
    id <WebCoreResourceLoader> loader;
    IFWebDataSource *dataSource;
    NSURL *currentURL;
}

+ (IFURLHandle *)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader
    withURL:(NSURL *)URL dataSource:(IFWebDataSource *)dataSource;

@end
