/*	
    IFPluginStream.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFPluginStream.h"


@implementation IFPluginStream

- initWithURL:(NSURL *)streamURL mimeType:(NSString *)mime notifyData:(void *)notifyData
{
    char * cURL;
    NSString *streamURLString;
    
    mimeType = [mime retain];
    
    streamURLString = [streamURL absoluteString];
    cURL   = malloc([streamURLString length]+1);
    [streamURLString getCString:cURL];
    
    npStream = malloc(sizeof(NPStream));
    npStream->url = cURL;
    npStream->end = 0;
    npStream->lastmodified = 0;
    npStream->notifyData = notifyData;
    offset = 0;
    
    data = [[NSMutableData dataWithCapacity:0] retain];
    
    return self;
}


- (uint16) transferMode
{
    return transferMode;
}


- (int32) offset
{
    return offset;
}


- (NPStream *) npStream
{
    return npStream;
}


- (NSString *) filename
{
    return filename;
}


- (NSString *) mimeType
{
    return mimeType;
}


- (NSMutableData *) data
{
    return data;
}


- (void) setFilename:(NSString *)file
{
    filename = [file retain];
}

- (void) setTransferMode:(uint16)tMode
{
    transferMode = tMode;
}

- (void) incrementOffset:(int32)addition
{
    offset += addition;
}

- (void) dealloc
{
    free(npStream);
    [mimeType release];
    [data release];
    if(filename)
        [filename release];
}

@end
