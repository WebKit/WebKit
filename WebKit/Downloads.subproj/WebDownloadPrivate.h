/*
    WebDownloadPrivate.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <WebKit/WebDownload.h>

@class NSURLRequest;
@class WebResource;
@class WebResponse;
@class WebResourceDelegateProxy;

@interface WebDownload (WebPrivate)
+ _downloadWithLoadingResource:(WebResource *)resource
                       request:(NSURLRequest *)request
                      response:(WebResponse *)response
                      delegate:(id)delegate
                         proxy:(WebResourceDelegateProxy *)proxy;
- (void)_setDirectoryPath:(NSString *)directoryPath;
@end
