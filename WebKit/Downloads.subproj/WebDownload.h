/*
     WebDownload.h
     Copyright 2003, Apple, Inc. All rights reserved.

     Public header file.
*/

#import <Foundation/Foundation.h>

@class WebDataSource;
@class WebDownloadPrivate;
@class WebError;

@interface WebDownload : NSObject
{
@private
    WebDownloadPrivate *_private;
}

- initWithDataSource:(WebDataSource *)dSource;
- (WebError *)receivedData:(NSData *)data;
- (WebError *)finishedLoading;
- (void)cancel;

@end
