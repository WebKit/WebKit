/*
     WebDownload.h
     Copyright 2003, Apple, Inc. All rights reserved.

     Public header file.
*/

#import <Foundation/Foundation.h>

@class WebDownload;
@class WebDownloadPrivate;
@class WebError;
@class WebResourceRequest;
@class WebResourceResponse;

@protocol WebDownloadDecisionListener <NSObject>
-(void)setPath:(NSString *)path;
@end

@protocol WebDownloadDelegate <NSObject>
- (WebResourceRequest *)download:(WebDownload *)download willSendRequest:(WebResourceRequest *)request;
- (void)download:(WebDownload *)download didReceiveResponse:(WebResourceResponse *)response;
- (void)download:(WebDownload *)download decidePathWithListener:(id <WebDownloadDecisionListener>)listener;
- (void)download:(WebDownload *)download didReceiveDataOfLength:(unsigned)length;
- (void)downloadDidFinishLoading:(WebDownload *)download;
- (void)download:(WebDownload *)download didFailLoadingWithError:(WebError *)error;
@end

@interface WebDownload : NSObject
{
@private
    WebDownloadPrivate *_private;
}

- initWithRequest:(WebResourceRequest *)request delegate:(id <WebDownloadDelegate>)delegate;
- (void)cancel;

@end
