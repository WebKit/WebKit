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
    
    unsigned char _name[64];
    ScriptCode _scriptCode;
    
    int _dataForkLength;
    int _resourceForkLength;
    u_int32_t _creationDate;
    u_int32_t _modificationDate;
    OSType _fileType;
    OSType _fileCreator;
}
@end
