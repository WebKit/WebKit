/*
    WebDownloadPrivate.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <WebKit/WebDownload.h>

@class WebDataSource;
@class WebResource;

@interface WebDownload (WebPrivate)
- _initWithLoadingResource:(WebResource *)resource dataSource:(WebDataSource *)dataSource;
- (void)_setPath:(NSString *)path;
@end
