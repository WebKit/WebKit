//
//  WebBinHexDecoder.h
//  WebKit
//
//  Created by Darin Adler on Wed Nov 06 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol WebDownloadDecoder;

@interface WebBinHexDecoder : NSObject <WebDownloadDecoder>
{
    BOOL _sawError;

    BOOL _atEnd;
    
    unsigned char _sixBitBuffer[4];
    int _sixBitBufferLength;
    
    unsigned char _byteBuffer[3];
    int _byteBufferLength;
    int _byteBufferOffset;
    
    unsigned char _repeatCharacter;
    int _repeatCount;
    
    BOOL _sawRepeatCharacter;
    
    int _CRC;
    
    const unsigned char *_source;
    const unsigned char *_sourceEnd;
    
    unsigned char _name[64];
    OSType _fileType;
    OSType _fileCreator;
    uint32_t _finderFlags;
    int _dataForkLengthRemaining;
    int _resourceForkLengthRemaining;
    
    int _computedCRC;
    BOOL _haveComputedCRC;
    
    unsigned char _fileCRC[2];
    int _fileCRCLength;
    
    BOOL _dataForkCRCChecked;
    BOOL _resourceForkCRCChecked;
}
@end
