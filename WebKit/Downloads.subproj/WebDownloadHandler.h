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
}

- initWithDataSource:(WebDataSource *)dSource;
- (WebError *)receivedData:(NSData *)data;
- (WebError *)finishedLoading;
- (void)cancel;
@end
