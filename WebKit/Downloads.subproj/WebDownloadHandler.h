//
//  WebDownloadHandler.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 Apple computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WebDataSource;
@class WebError;
@class WebResourceResponse;

@interface WebDownloadHandler : NSObject
{
    WebDataSource *dataSource;
    
    NSArray *decoderClasses;
    NSMutableArray *decoderSequence;
    NSMutableData *bufferedData;

    FSRef fileRef;
    FSRefPtr fileRefPtr;
    
    SInt16 dataForkRefNum;
    SInt16 resourceForkRefNum;

    // isCancelled is used to make sure we don't write after cancelling the load.
    BOOL isCancelled;

    // areWritesCancelled is only used by WriteCompletionCallback to make
    // sure that only 1 write failure cancels the load.
    BOOL areWritesCancelled;
}

- initWithDataSource:(WebDataSource *)dSource;
- (WebError *)receivedData:(NSData *)data;
- (WebError *)finishedLoading;
- (void)cancel;
@end
