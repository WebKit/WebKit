//
//  WebMacBinaryDecoder.h
//
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol WebDownloadDecoder;

@interface WebMacBinaryDecoder : NSObject <WebDownloadDecoder>
{
    int _offset;
    
    char _name[64];
    int _dataForkLength;
    int _resourceForkLength;
    int _commentLength;
    u_int32_t _creationDate;
    u_int32_t _modificationDate;
    OSType _fileType;
    OSType _fileCreator;

    int _commentEnd;
}
@end
