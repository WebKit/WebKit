//
//  WebMacBinaryDecoder.m
//
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <string.h>
#import <sys/types.h>
#import "crc16.h"
#import <WebKit/WebMacBinaryDecoder.h>


#define HEADER_ST		0
#define RESOURCE_ST		1
#define DATA_ST			2
#define COMMENT_ST		3
#define EXTRA_ST		4

#define MB_HDR_SIZE		128

#undef MIN
#define MIN(A,B)		((A) < (B) ? (A) : (B))


@implementation WebMacBinaryDecoder

// Returns YES if the decoder can decode headerData, NO otherwise.
// Header data is the first 8KB or larger chunk of received data.
+ (BOOL)canDecodeHeaderData:(NSData *)headerData
{
    const u_int8_t	*streamPtr;
    u_int32_t		crc;
    
    if ([headerData length] < MB_HDR_SIZE) return NO;
    streamPtr = [headerData bytes];
    if (streamPtr[0] || streamPtr[74] || streamPtr[126] || streamPtr[127]) return NO;
    if (!streamPtr[1] || streamPtr[1] > 31) return NO;
    crc = CRC16(streamPtr, 124, 0);
    if (*(u_int16_t *)(streamPtr + 124) != crc) return NO;
    return YES;
}



- (id)init 
{
    self = [super init];
    if (self) {
        _accumulator = [[NSMutableData alloc] init];
        _comment[0] = 0;
        _state = HEADER_ST;
        _streamComplete = NO;
    }
    return self;
}



- (void)dealloc
{
    [_accumulator release];
    [super dealloc];
}



// Decodes data and sets dataForkData and resourceForkData with the decoded data.
// dataForkData and/or resourceForkData and can be set to nil if there is no data to decode.
// This method will be called repeatedly with every chunk of received data. 
// Returns YES if no errors have occurred, NO otherwise.
- (BOOL)decodeData:(NSData *)data dataForkData:(NSData **)dataForkData resourceForkData:(NSData **)resourceForkData
{
    BOOL		ret;
    const u_int8_t	*curP;
    long		forkEnd;
    long		writeBytes;
    long		bytesLeft;
    int32_t		gap;
    NSMutableData	*tempData;
    
    ret = NO;
    if (!dataForkData || !resourceForkData) goto exit;
    ret = YES;
    *dataForkData = nil;
    *resourceForkData = nil;
    [_accumulator appendData:data];
    while (1) {
        switch (_state) {
        case HEADER_ST:
            // If there's not enough data for the whole header, we'll return
            // and wait for more to show up.
            ret = YES;
            if ([_accumulator length] < MB_HDR_SIZE) goto exit;
        
            // Extract the fields we want
            curP = [_accumulator bytes];
            curP++;
            memcpy(_name, curP, *curP + 1);			// Copy the name
            curP += 64;
            memcpy(_fInfo, curP, 16);				// Copy the FInfo
            curP += 16;
            curP += 2;						// Skip "protect" bit
            memcpy(&_dataLen, curP, 4);				// Get the data fork length
            curP += 4;
            memcpy(&_rsrcLen, curP, 4);				// Get the resource fork length
            curP += 4;
            memcpy(_dateBlock, curP, 8);			// Get the dates
            curP += 8;
            memcpy(&_comment[0], curP + 1, 1);			// Get the comment length
            curP += 2;
            memcpy((char *)_fInfo + 9, curP, 1);		// Get Finder flags
            curP += 27;
            _rsrcStart = MB_HDR_SIZE + ((_dataLen + 127) & 0xFFFFFF80);
            _commentStart = _rsrcStart + ((_rsrcLen + 127) & 0xFFFFFF80);
            
            // Purge the header from the accumulator
            tempData = [[NSMutableData alloc] initWithBytes:curP length:([_accumulator length] - MB_HDR_SIZE)];
            [_accumulator release];
            _accumulator = tempData;
            _curOffset = MB_HDR_SIZE;
            _state = DATA_ST;
            break;
                        
        case DATA_ST:
            forkEnd = MB_HDR_SIZE + _dataLen;
            bytesLeft = forkEnd - _curOffset;
            writeBytes = MIN((long)[_accumulator length], bytesLeft);
            *dataForkData = [NSData dataWithBytes:[_accumulator bytes] length:writeBytes];
            _curOffset += writeBytes;
            if ((long)[_accumulator length] >= bytesLeft) {
                tempData = [[NSMutableData alloc] initWithBytes:((char *)[_accumulator bytes] + writeBytes) length:([_accumulator length] - writeBytes)];
                [_accumulator release];
                _accumulator = tempData;
                _state = RESOURCE_ST;
            } else {
                [_accumulator setLength:0];
                goto exit;
            }
            break;
                
        case RESOURCE_ST:
            gap = _rsrcStart - _curOffset;
            if (gap > 0) {
                if ((int32_t)[_accumulator length] < gap) {
                    _curOffset += [_accumulator length];
                    [_accumulator setLength:0];
                    goto exit;
                } else {
                    tempData = [[NSMutableData alloc] initWithBytes:((char *)[_accumulator bytes] + gap) length:([_accumulator length] - gap)];
                    [_accumulator release];
                    _accumulator = tempData;
                    _curOffset += gap;
                }
            }
            forkEnd = _rsrcStart + _rsrcLen;
            bytesLeft = forkEnd - _curOffset;
            writeBytes = MIN((long)[_accumulator length], bytesLeft);
            if (writeBytes) {
                *resourceForkData = [NSData dataWithBytes:[_accumulator bytes] length:writeBytes];
            }
            _curOffset += writeBytes;
            if ((long)[_accumulator length] >= bytesLeft) {
                tempData = [[NSMutableData alloc] initWithBytes:((char *)[_accumulator bytes] + writeBytes) length:([_accumulator length] - writeBytes)];
                [_accumulator release];
                _accumulator = tempData;
                _state = COMMENT_ST;
            } else {
                [_accumulator setLength:0];
                goto exit;
            }
            break;
                        
        case COMMENT_ST:
            gap = _commentStart - _curOffset;
            if (gap > 0) {
                if ((int32_t)[_accumulator length] < gap) {
                    _curOffset += [_accumulator length];
                    [_accumulator setLength:0];
                    goto exit;
                } else {
                    tempData = [[NSMutableData alloc] initWithBytes:((char *)[_accumulator bytes] + gap) length:([_accumulator length] - gap)];
                    [_accumulator release];
                    _accumulator = tempData;
                    _curOffset += gap;
                }
            }
            // Wait until we receive the whole comment
            if ((char)[_accumulator length] < _comment[0]) goto exit;
            memcpy(_comment + 1, [_accumulator bytes], _comment[0]);
            _streamComplete = YES;
            
            // Now throw out any extra data and set the state so
            // that we just throw away anything we receive
            [_accumulator setLength:0];
            _state = EXTRA_ST;
            goto exit;
            break;
                        
        case EXTRA_ST:
            [_accumulator setLength:0];
            goto exit;
            break;
        }
    }
	
exit:
	return ret;
}



// Returns YES if decoding successfully finished, NO otherwise.
- (BOOL)finishDecoding
{
    return _streamComplete;
}



// Returns a dictionary of 4 file attributes. The attributes (as defined in NSFileManager.h) are:
// NSFileModificationDate, NSFileHFSCreatorCode, NSFileHFSTypeCode, NSFileCreationDate
// fileAttributes is called after finishDecoding.
- (NSDictionary *)fileAttributes
{
    NSDate		*crDate;
    NSDate		*modDate;
    NSNumber		*type;
    NSNumber		*creator;
    
    crDate = [[NSDate dateWithString:@"1904-01-01 00:00:00 +0000"] addTimeInterval:_dateBlock[0]];
    modDate = [[NSDate dateWithString:@"1904-01-01 00:00:00 +0000"] addTimeInterval:_dateBlock[1]];
    type = [NSNumber numberWithUnsignedLong:_fInfo[0]];
    creator = [NSNumber numberWithUnsignedLong:_fInfo[1]];
    return [NSDictionary dictionaryWithObjectsAndKeys:crDate, NSFileCreationDate,
                                                      modDate, NSFileModificationDate,
                                                      type, NSFileHFSTypeCode,
                                                      creator, NSFileHFSCreatorCode,
                                                      nil];
}


@end
