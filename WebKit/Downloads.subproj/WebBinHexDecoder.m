//
//  WebBinHexDecoder.m
//  WebKit
//
//  Created by Darin Adler on Wed Nov 06 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

// This decoder decodes a particular variant of the BinHex format.
// Specifically, it's the 7-bit format known as Hqx7.

#import "WebBinHexDecoder.h"

#import <WebFoundation/WebAssertions.h>

#define SKIP_CHARACTER 0x40
#define END_CHARACTER 0x41
#define BAD_CHARACTER 0x42

static const unsigned char sixBitTable[256] = {   
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x40, 0x42, 0x42, 0x40, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x42, 0x42,
      0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x42, 0x14, 0x15, 0x41, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x42,
      0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x42, 0x2C, 0x2D, 0x2E, 0x2F, 0x42, 0x42, 0x42, 0x42,
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x42, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x42, 0x42,
      0x3D, 0x3E, 0x3F, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
      0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42
};

#define RUN_LENGTH_CODE 0x90

#define BEFORE_HEADER_LINE "(This file must be converted with BinHex 4.0)"

typedef struct {
    unsigned char name[64];
    unsigned char remainder[19];
} Header;

@implementation WebBinHexDecoder

- (int)decodeIntoBuffer:(void *)buffer size:(int)bufferSize
{
    ASSERT(buffer);
    ASSERT(bufferSize);
    
    if (_sawError || _atEnd) {
        return 0;
    }
    
    unsigned char *bufferStart = buffer;
    unsigned char *bufferP = bufferStart;
    unsigned char *bufferEnd = bufferStart + bufferSize;
    
    int sixBitBufferLength = _sixBitBufferLength;
    int byteBufferLength = _byteBufferLength;
    int byteBufferOffset = _byteBufferOffset;
    unsigned char repeatCharacter = _repeatCharacter;
    int repeatCount = _repeatCount;
    BOOL sawRepeatCharacter = _sawRepeatCharacter;
    int CRC = _CRC;
    const unsigned char *source = _source;
    const unsigned char *sourceEnd = _sourceEnd;
    
    for (;;) {
        for (;;) {
            // Empty the repeat count.
            while (repeatCount) {
                unsigned int c = repeatCharacter;
                *bufferP++ = c;
                
                int i;
                for (i = 0; i < 8; i++) {
                    c <<= 1;
                    if ((CRC <<= 1) & 0x10000) {
                        CRC = (CRC & 0xFFFF) ^ 0x1021;
                    }
                    CRC ^= (c >> 8);
                    c &= 0xFF;
                }
                
                //printf("added the character %X to the CRC, CRC is now %X\n", repeatCharacter, CRC);
                
                repeatCount -= 1;
                if (bufferP == bufferEnd) {
                    goto done;
                }
            }
            
            // Empty the byte buffer.
            if (byteBufferOffset == byteBufferLength) {
                break;
            }
            unsigned char byte = _byteBuffer[byteBufferOffset++];
            if (sawRepeatCharacter) {
                if (byte == 0) {
                    repeatCount = 1;
                    repeatCharacter = RUN_LENGTH_CODE;
                } else {
                    repeatCount = byte;
                }
                sawRepeatCharacter = NO;
            } else {
                if (byte == RUN_LENGTH_CODE) {
                    sawRepeatCharacter = YES;
                } else {
                    repeatCount = 1;
                    repeatCharacter = byte;
                }
            }
        }
        
        // Extract six-bit characters, then bytes.
        for (;;) {
            if (source == sourceEnd) {
                goto done;
            }
        
            char sixBits = sixBitTable[*source++];
            switch (sixBits) {
                case SKIP_CHARACTER:
                    continue;
                case END_CHARACTER:
                    _atEnd = YES;
                    break;
                case BAD_CHARACTER:
                    _sawError = YES;
                    return 0;
                default:
                    _sixBitBuffer[sixBitBufferLength++] = sixBits;
            }
            
            // Fill the byte buffer.
            if (sixBitBufferLength == 4 || _atEnd) {
                ASSERT(byteBufferOffset == byteBufferLength);
                _byteBuffer[0] = (_sixBitBuffer[0] << 2 | _sixBitBuffer[1] >> 4);
                _byteBuffer[1] = (_sixBitBuffer[1] << 4 | _sixBitBuffer[2] >> 2);
                _byteBuffer[2] = (_sixBitBuffer[2] << 6 | _sixBitBuffer[3]);
                byteBufferOffset = 0;
                byteBufferLength = (sixBitBufferLength * 6) / 8;
                sixBitBufferLength = 0;
                break;
            }
        }
    }

done:    
    _sixBitBufferLength = sixBitBufferLength;
    _byteBufferLength = byteBufferLength;
    _byteBufferOffset = byteBufferOffset;
    _repeatCharacter = repeatCharacter;
    _repeatCount = repeatCount;
    _sawRepeatCharacter = sawRepeatCharacter;
    _CRC = CRC;
    
    _source = source;
    
    return bufferP - bufferStart;
}

- (void)pumpCRCTwice
{
    int i;
    for (i = 0; i < 16; i++) {
        if ((_CRC <<= 1) & 0x10000) {
            _CRC = (_CRC & 0xFFFF) ^ 0x1021;
        }
    }
}

- (void)decodeAllIntoBuffer:(void *)buffer size:(int)size
{
    int decodedByteCount = [self decodeIntoBuffer:buffer size:size];
    if (decodedByteCount != size) {
        _sawError = YES;
    }
}

- (void)decodeHeader
{
    ASSERT(_CRC == 0);
    
    // Find the header line.
    int beforeHeaderLineLength = strlen(BEFORE_HEADER_LINE);
    for (;;) {
        _source = memchr(_source, '(', _sourceEnd - _source);
        if (!_source) {
            _sawError = YES;
            return;
        }
        if (_sourceEnd - _source < beforeHeaderLineLength) {
            _sawError = YES;
            return;
        }
        if (memcmp(_source, BEFORE_HEADER_LINE, beforeHeaderLineLength) == 0) {
            _source += beforeHeaderLineLength;
            break;
        }
        _source += 1;
    }
    
    // Find the colon after it.
    for (;;) {
        if (_source == _sourceEnd) {
            _sawError = YES;
            return;
        }
        unsigned char c = *_source++;
        if (c == ':') {
            break;
        }
        if (c != '\n' && c != '\r') {
            _sawError = YES;
            return;
        }
    }

    // Decode.
    Header header;
    [self decodeAllIntoBuffer:&header.name[0] size:1];
    if (header.name[0] == 0 || header.name[0] >= sizeof(header.name)) {
        _sawError = YES;
        return;
    }
    [self decodeAllIntoBuffer:&header.name[1] size:header.name[0]];
    [self decodeAllIntoBuffer:&header.remainder size:sizeof(header.remainder)];
    if (header.remainder[0] != 0) { // version
        _sawError = YES;
        return;
    }

    // Compute the CRC.
    [self pumpCRCTwice];
    int computedCRC = _CRC & 0xFFFF;
    
    // Read the CRC from the file.
    unsigned char CRC[2];
    [self decodeAllIntoBuffer:CRC size:2];
    if (computedCRC != ((CRC[0] << 8) | CRC[1])) {
        _sawError = YES;
        return;
    }
    
    // Extract the header fields we care about.
    memcpy(_name, header.name, header.name[0] + 1);
    _fileType = (((((header.remainder[1] << 8) | header.remainder[2]) << 8) | header.remainder[3]) << 8) | header.remainder[4];
    _fileCreator = (((((header.remainder[5] << 8) | header.remainder[6]) << 8) | header.remainder[7]) << 8) | header.remainder[8];
    _dataForkLengthRemaining = (((((header.remainder[11] << 8) | header.remainder[12]) << 8) | header.remainder[13]) << 8) | header.remainder[14];
    _resourceForkLengthRemaining = (((((header.remainder[15] << 8) | header.remainder[16]) << 8) | header.remainder[17]) << 8) | header.remainder[18];
    
    // Reset the CRC so it's ready to compute a fork CRC.
    _CRC = 0;
}

- (void)setUpSourceForData:(NSData *)data
{
    _source = [data bytes];
    _sourceEnd = _source + [data length];
}

- (void)decodeForkWithData:(NSData **)data count:(int *)count CRCCheckFlag:(BOOL *)CRCCheckFlag
{
    *data = nil;
    
    while (*count) {
        char buffer[8192];
        int maxBytes = MIN(*count, (int)sizeof(buffer));
        int numBytesDecoded = [self decodeIntoBuffer:buffer size:maxBytes];
        if (numBytesDecoded == 0) {
            return;
        }
        ASSERT(!_sawError);
        if (*data == nil) {
            *data = [NSMutableData dataWithBytes:buffer length:numBytesDecoded];
        } else {
            [(NSMutableData *)*data appendBytes:buffer length:numBytesDecoded];
        }
        *count -= numBytesDecoded;
    }
    
    if (!*CRCCheckFlag) {
        // Done reading the fork, now compute the CRC.
        if (!_haveComputedCRC) {
            [self pumpCRCTwice];
            _computedCRC = _CRC & 0xFFFF;
            _haveComputedCRC = YES;
        }
        
        // Done reading the fork, now read the CRC.
        if (_fileCRCLength != 2) {
            int numBytesDecoded = [self decodeIntoBuffer:&_fileCRC[_fileCRCLength] size:(2 - _fileCRCLength)];
            _fileCRCLength += numBytesDecoded;
            
            if (_fileCRCLength == 2) {
                // Check the CRC.
                if (_computedCRC != ((_fileCRC[0] << 8) | _fileCRC[1])) {
                    _sawError = YES;
                }
                
                // Set the flag so we know we already checked.
                *CRCCheckFlag = YES;
                
                // Reset the CRC variables so the next fork can use them.
                _CRC = 0;
                _haveComputedCRC = NO;
                _fileCRCLength = 0;
            }
        }
    }
}

+ (BOOL)canDecodeHeaderData:(NSData *)data
{
    WebBinHexDecoder *decoder = [[self alloc] init];
    [decoder setUpSourceForData:data];
    [decoder decodeHeader];
    BOOL sawError = decoder->_sawError;
    [decoder release];
    return !sawError;
}

- (BOOL)decodeData:(NSData *)data dataForkData:(NSData **)dataForkData resourceForkData:(NSData **)resourceForkData
{
    [self setUpSourceForData:data];
    
    if (_name[0] == 0) {
        [self decodeHeader];
    }
    ASSERT(_sawError || _name[0]);
    
    [self decodeForkWithData:dataForkData count:&_dataForkLengthRemaining CRCCheckFlag:&_dataForkCRCChecked];
    [self decodeForkWithData:resourceForkData count:&_resourceForkLengthRemaining CRCCheckFlag:&_resourceForkCRCChecked];

    return !_sawError;
}

- (BOOL)finishDecoding
{
    ASSERT(!_sawError);
    return _resourceForkCRCChecked;
}

- (NSDictionary *)fileAttributes
{
    // FIXME: What about other parts of Finder info? Bundle bit, for example.
    return [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithUnsignedLong:_fileType], NSFileHFSTypeCode,
        [NSNumber numberWithUnsignedLong:_fileCreator], NSFileHFSCreatorCode,
        nil];
}

- (NSString *)filename
{
    return [(NSString *)CFStringCreateWithPascalString(NULL, _name, kCFStringEncodingMacRoman) autorelease];
}

@end
