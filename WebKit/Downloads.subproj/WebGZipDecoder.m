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

#define ID1 0x1F
#define ID2 0x8B
#define DEFLATE_COMPRESSION_METHOD 8

#define FTEXT 0x01
#define FHCRC 0x02
#define FEXTRA 0x04
#define FNAME 0x08
#define FCOMMENT 0x10
#define RESERVED_FLAGS 0xE0

@implementation WebGZipDecoder

+ (BOOL)decodeHeader:(NSData *)headerData headerLength:(int *)headerLength modificationTime:(unsigned *)modificationTime filename:(NSString **)filename
{
    int headerDataLength = [headerData length];
    int length = 10;
    
    if (headerDataLength < length) {
        return NO;
    }
    
    const unsigned char *bytes = (const unsigned char *)[headerData bytes];
    
    if (bytes[0] != ID1 || bytes[1] != ID2) {
        return NO;
    }
    
    if (bytes[2] != DEFLATE_COMPRESSION_METHOD) {
        return NO;
    }
    
    unsigned char flags = bytes[3];
    if (flags & RESERVED_FLAGS) {
        return NO;
    }
    
    if (flags & FEXTRA) {
        if (headerDataLength < length + 2) {
            return NO;
        }
        length += 2 + (bytes[10] | (bytes[11] << 8));
        if (headerDataLength < length) {
            return NO;
        }
    }
    
    const char *filenameCString = 0;
    if (flags & FNAME) {
        filenameCString = (const char *)&bytes[length];
        while (bytes[length++] != 0) {
            if (headerDataLength < length) {
                return NO;
            }
        }
    } else {
        // The decoder framework won't handle files without filenames well.
        return NO;
    }
    
    if (flags & FCOMMENT) {
        while (bytes[length++] != 0) {
            if (headerDataLength < length) {
                return NO;
            }
        }
    }
    
    if (flags & FHCRC) {
        if (headerDataLength < length + 2) {
            return NO;
        }
        unsigned computedCRC = crc32(0, bytes, length) & 0xFFFF;
        unsigned expectedCRC = bytes[length] + bytes[length + 1];
        if (computedCRC != expectedCRC) {
            return NO;
        }
        length += 2;
    }
    
    if (headerLength) {
        *headerLength = length;
    }
    
    if (modificationTime) {
        *modificationTime = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
    }
    
    if (filename) {
        *filename = [(NSString *)CFStringCreateWithCString(NULL, filenameCString, kCFStringEncodingWindowsLatin1) autorelease];
    }
    
    return YES;
}

+ (BOOL)canDecodeHeaderData:(NSData *)headerData
{
    return [self decodeHeader:headerData headerLength:NULL modificationTime:NULL filename:NULL];
}

- (id)init
{
    self = [super init];
    int error = inflateInit2(&_stream, -15); // window of 15 is standard for gzip, negative number is secret zlib feature to avoid processing header
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
    [_filename release];
    [super dealloc];
}

- (BOOL)decodeData:(NSData *)data dataForkData:(NSData **)dataForkData resourceForkData:(NSData **)resourceForkData
{
    ASSERT(data);
    ASSERT([data length]);
    ASSERT(dataForkData);
    ASSERT(resourceForkData);
    ASSERT(_streamInitialized);
    
    *dataForkData = nil;
    *resourceForkData = nil;
    
    int offset = 0;

    // Decode the header.
    if (!_decodedHeader) {
        NSString *filename;
        if (![[self class] decodeHeader:data
                           headerLength:&offset
                       modificationTime:&_modificationTime
                               filename:&filename]) {
            return NO;
        }
        
        ASSERT(!_filename);
        _filename = [filename copy];
        
        _decodedHeader = YES;
    }
    
    // Decode the compressed data.
    if (!_finishedInflating) {
        NSMutableData *inflatedData = [NSMutableData data];
        *dataForkData = inflatedData;
        
        _stream.next_in = (char *)[data bytes] + offset;
        _stream.avail_in = [data length] - offset;
        
        while (_stream.avail_in) {
            char outputBuffer[16384];
        
            _stream.next_out = outputBuffer;
            _stream.avail_out = sizeof(outputBuffer);
        
            int result = inflate(&_stream, Z_NO_FLUSH);
            
            int outputLength = sizeof(outputBuffer) - _stream.avail_out;
            _size += outputLength;
            _CRC32 = crc32(_CRC32, outputBuffer, outputLength);
            [inflatedData appendBytes:outputBuffer length:outputLength];
        
            if (result == Z_STREAM_END) {
                _finishedInflating = YES;
                break;
            }
        
            if (result != Z_OK) {
                ERROR("got error from inflate, %d", result);
                return NO;
            }
        }
        
        offset = [data length] - _stream.avail_in;
    }
    
    // Decode the trailer.
    if (_finishedInflating && _trailingBytesLength < 8) {
        int numRemaining = [data length] - offset;
        if (numRemaining) {
            int numToCopy = MIN(numRemaining, 8 - _trailingBytesLength);
            memcpy(_trailingBytes + _trailingBytesLength, (char *)[data bytes] + offset, numToCopy);
            _trailingBytesLength += numToCopy;
            if (_trailingBytesLength == 8) {
                unsigned expectedCRC = _trailingBytes[0] | (_trailingBytes[1] << 8)
                    | (_trailingBytes[2] << 16) | (_trailingBytes[3] << 24);
                if (_CRC32 != expectedCRC) {
                    ERROR("CRC didn't match, computed %08X, expected %08X", _CRC32, expectedCRC);
                    return NO;
                }
                unsigned expectedSize = _trailingBytes[4] | (_trailingBytes[5] << 8)
                    | (_trailingBytes[6] << 16) | (_trailingBytes[7] << 24);
                if (_size != expectedSize) {
                    ERROR("size didn't match, computed %d, expected %d", _size, expectedSize);
                    return NO;
                }
            }
        }
    }

    return YES;
}

- (BOOL)finishDecoding
{
    ASSERT(_streamInitialized);
    return _finishedInflating && _trailingBytesLength == 8;
}

- (NSDictionary *)fileAttributes
{
    ASSERT(_decodedHeader);
    if (_modificationTime == 0) {
        return nil;
    }
    return [NSDictionary dictionaryWithObject:[NSDate dateWithTimeIntervalSinceReferenceDate:NSTimeIntervalSince1970 + _modificationTime]
                                       forKey:NSFileModificationDate];
}

- (NSString *)filename
{
    ASSERT(_decodedHeader);
    return [[_filename copy] autorelease];
}

@end
