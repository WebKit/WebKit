/*
    WebDownloadPrivate.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <WebKit/WebDownload.h>

@class NSURLRequest;
@class WebResource;
@class NSURLResponse;
@class WebResourceDelegateProxy;

@interface WebDownload (WebPrivate)
+ _downloadWithLoadingResource:(WebResource *)resource
                       request:(NSURLRequest *)request
                      response:(NSURLResponse *)response
                      delegate:(id)delegate
                         proxy:(WebResourceDelegateProxy *)proxy;
- (void)_setDirectoryPath:(NSString *)directoryPath;
@end
