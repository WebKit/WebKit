/*
 *  WebDownloadDecoder.h
 *  WebKit
 *
 *  Created by Chris Blumenberg on Thu Oct 17 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#include <Foundation/Foundation.h>

@protocol WebDownloadDecoder <NSObject>

// Returns YES if the decoder can decode headerData, NO otherwise.
// Header data is the first 8KB or larger chunk of received data.
+ (BOOL)canDecodeHeaderData:(NSData *)headerData;

// Decodes data and sets dataForkData and resourceForkData with the decoded data.
// dataForkData and/or resourceForkData and can be set to nil if there is no data to decode.
// This method will be called repeatedly with every chunk of received data. 
// Returns YES if no errors have occurred, NO otherwise.
- (BOOL)decodeData:(NSData *)data dataForkData:(NSData **)dataForkData resourceForkData:(NSData **)resourceForkData;

// Returns YES if decoding successfully finished, NO otherwise.
- (BOOL)finishDecoding;

// Returns a dictionary of 4 file attributes. The attributes (as defined in NSFileManager.h) are:
// NSFileModificationDate, NSFileHFSCreatorCode, NSFileHFSTypeCode, NSFileCreationDate
// fileAttributes is called after finishDecoding.
- (NSDictionary *)fileAttributes;
@end



