//
//  IFPluginStream.h
//  WebKit
//
//  Created by Chris Blumenberg on Tue Apr 09 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <npapi.h>

@interface IFPluginStream : NSObject {
    uint16 transferMode;
    int32 offset;
    NPStream *npStream;
    NSString *filename, *mimeType;
    NSMutableData *data;
}

- initWithURL:(NSURL *) streamURL mimeType:(NSString *)mime notifyData:(void *)notifyData;
- (uint16) transferMode;
- (int32) offset;
- (NPStream *) npStream;
- (NSString *) filename;
- (NSString *) mimeType;
- (NSMutableData *) data;

- (void) setFilename:(NSString *)file;
- (void) setData:(NSMutableData *)newData;
- (void) setTransferMode:(uint16)tMode;
- (void) incrementOffset:(int32)addition;

@end
