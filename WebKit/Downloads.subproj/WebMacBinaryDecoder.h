//
//  WebMacBinaryDecoder.h
//
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//
#import <Foundation/Foundation.h>

#import <WebKit/WebDownloadDecoder.h>


@interface WebMacBinaryDecoder : NSObject <WebDownloadDecoder>
{
    int			_state;
    long		_dataLen;
    long		_rsrcLen;
    long		_rsrcStart;
    long		_commentStart;
    long		_curOffset;
    u_int32_t		_dateBlock[2];
    u_int32_t		_fInfo[4];
    char		_comment[256];
    char		_name[32];
    NSMutableData	*_accumulator;
    BOOL		_streamComplete;
}


@end
