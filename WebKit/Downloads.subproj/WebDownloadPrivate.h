/*
    WebDownloadPrivate.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <WebKit/WebDownload.h>

@class NSURLRequest;
@class NSURLConnection;
@class NSURLResponse;
@class WebResourceDelegateProxy;

@interface WebDownload (WebPrivate)
+ _downloadWithLoadingResource:(NSURLConnection *)resource
                       request:(NSURLRequest *)request
                      response:(NSURLResponse *)response
                      delegate:(id)delegate
                         proxy:(WebResourceDelegateProxy *)proxy;
- (void)_setDirectoryPath:(NSString *)directoryPath;
@end
