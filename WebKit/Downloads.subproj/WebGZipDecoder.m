//
//  WebGZipDecoder.m
//  WebKit
//
//  Created by Darin Adler on Wed Dec 04 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebGZipDecoder.h"

#import <WebFoundation/WebAssertions.h>
#import <WebKit/WebDownloadDecoder.h>

@implementation WebGZipDecoder

+ (BOOL)canDecodeHeaderData:(NSData *)headerData
{
    if ([headerData length] < 2) {
        return NO;
    }
    const unsigned char *bytes = (const unsigned char *)[headerData bytes];
    return (bytes[0] == 0x1F) && (bytes[1] == 0x8B) && 0;
}

- (id)init
{
    self = [super init];
    int error = inflateInit(&_stream);
    if (error != Z_OK) {
        ERROR("failed to initialize zlib, error %d", error);
        [self release];
        return nil;
    }
    _streamInitialized = YES;
    return self;
}

- (void)dealloc
{
    if (_streamInitialized)
        inflateEnd(&_stream);
    [super dealloc];
}

- (BOOL)decodeData:(NSData *)data dataForkData:(NSData **)dataForkData resourceForkData:(NSData **)resourceForkData
{
    ASSERT(data);
    ASSERT([data length]);
    ASSERT(dataForkData);
    ASSERT(resourceForkData);
    
    *dataForkData = nil;
    *resourceForkData = nil;
    
    int error = inflate(&_stream, Z_NO_FLUSH);
    if (error != Z_OK) {
        _failed = YES;
        return NO;
    }

    return YES;
}

- (BOOL)finishDecoding
{
    int error = inflate(&_stream, Z_FINISH);
    if (error != Z_OK) {
        _failed = YES;
        return NO;
    }
    return YES;
}

- (NSDictionary *)fileAttributes
{
    return nil;
}

- (NSString *)filename
{
    return nil;
}

@end
