//
//  WebDownloadHandler.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc.
//

#import <WebKit/WebDownloadHandler.h>

#import <WebKit/WebBinHexDecoder.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDownloadDecoder.h>
#import <WebKit/WebGZipDecoder.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebMacBinaryDecoder.h>
#import <WebKit/WebNSWorkspaceExtras.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebNSFileManagerExtras.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>

@implementation WebDownloadHandler

- initWithDataSource:(WebDataSource *)dSource
{
    [super init];
    
    dataSource = [dSource retain];

    decoderClasses = [[NSArray arrayWithObjects:
        [WebBinHexDecoder class],
        [WebMacBinaryDecoder class],
        [WebGZipDecoder class],
        nil] retain];
    
    LOG(Download, "Download started for: %@", [[dSource request] URL]);
    return self;
}

- (void)dealloc
{
    [bufferedData release];
    [dataSource release];
    [decoderClasses release];
    [decoderSequence release];
    [super dealloc];
}

- (WebError *)errorWithCode:(int)code
{
    return [WebError errorWithCode:code inDomain:WebErrorDomainWebKit failingURL:[[dataSource URL] absoluteString]];
}

- (void)decodeHeaderData:(NSData *)headerData
            dataForkData:(NSData **)dataForkData
        resourceForkData:(NSData **)resourceForkData
{
    ASSERT(headerData);
    ASSERT([headerData length]);
    ASSERT(dataForkData);
    ASSERT(resourceForkData);
    
    unsigned i;
    for (i = 0; i < [decoderClasses count]; i++) {
        Class decoderClass = [decoderClasses objectAtIndex:i];

        if ([decoderClass canDecodeHeaderData:headerData]) {
            NSObject <WebDownloadDecoder> *decoder = [[[decoderClass alloc] init] autorelease];
            BOOL didDecode = [decoder decodeData:headerData dataForkData:dataForkData resourceForkData:resourceForkData];
            if (!didDecode) {
                // Though the decoder said it could decode the header, actual decoding failed. Shouldn't happen.
                ERROR("Download decoder \"%s\" failed to decode header even though it claimed to handle it.",
                      [[decoder className] lossyCString]);
                continue;
            }

            [decoderSequence addObject:decoder];
            
            [self decodeHeaderData:*dataForkData dataForkData:dataForkData resourceForkData:resourceForkData];
            break;
        }
    }
}

- (BOOL)decodeData:(NSData *)data
      dataForkData:(NSData **)dataForkData
  resourceForkData:(NSData **)resourceForkData
{
    ASSERT(data);
    ASSERT([data length]);
    ASSERT(dataForkData);
    ASSERT(resourceForkData);
    
    if (!decoderSequence) {
        decoderSequence = [[NSMutableArray array] retain];
        [self decodeHeaderData:data dataForkData:dataForkData resourceForkData:resourceForkData];
    } else {
        unsigned i;
        for (i = 0; i< [decoderSequence count]; i++) {
            NSObject <WebDownloadDecoder> *decoder = [decoderSequence objectAtIndex:i];            
            BOOL didDecode = [decoder decodeData:data dataForkData:dataForkData resourceForkData:resourceForkData];
    
            if (!didDecode) {
                return NO;
            }
            
            data = *dataForkData;
        }
    }

    if ([decoderSequence count] == 0) {
        *dataForkData = data;
        *resourceForkData = nil;
    }
    
    return YES;
}

- (void)closeFile
{
    if (dataForkRefNum) {
        FSCloseFork(dataForkRefNum);
        dataForkRefNum = 0;
    }
    if (resourceForkRefNum) {
        FSCloseFork(resourceForkRefNum);
        resourceForkRefNum = 0;
    }
}

- (void)cleanUpAfterFailure
{
    // VERY IMPORTANT TEST.  Only cleanup if we have opened a file, since the downloadPath
    // might be an existing file that we haven't discovered that we conflict with.  This can
    // happen if we're cleaning up after we received the first response, but before the first
    // data was processed.
    if (fileRefPtr) {
        NSString *path = [dataSource downloadPath];

        [self closeFile];

        NSFileManager *fileMgr = [NSFileManager defaultManager];
        BOOL isDirectory;
        BOOL fileExists = [fileMgr fileExistsAtPath:path isDirectory:&isDirectory];
        if (fileExists && !isDirectory) {
            [fileMgr _web_removeFileOnlyAtPath:path];
            [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:path];
        } else if (!fileExists) {
            ERROR("Download file disappeared in the middle of download");
        } else {
            // Note we currently don't support downloading directories, so we know this is wrong
            ERROR("Download file is a directory - will not be removed");
        }
    }
}

- (WebError *)createFileIfNecessary
{
    if (fileRefPtr) {
        return nil;
    }
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *path = [dataSource downloadPath];
    NSObject <WebDownloadDecoder> *lastDecoder = [decoderSequence lastObject];
        
    NSString *filename = [lastDecoder filename];

    if (filename) {
        path = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:filename];
    }

    if ([fileManager fileExistsAtPath:path]) {
        NSString *pathWithoutExtension = [path stringByDeletingPathExtension];
        NSString *extension = [path pathExtension];
        NSString *pathWithAppendedNumber;
        unsigned i;

        for (i = 1; 1; i++) {
            pathWithAppendedNumber = [NSString stringWithFormat:@"%@-%d", pathWithoutExtension, i];
            if (extension && [extension length]) {
                path = [pathWithAppendedNumber stringByAppendingPathExtension:extension];
            } else {
                path = pathWithAppendedNumber;
            }
            if (![fileManager fileExistsAtPath:path]) {
                break;
            }
        }
    }

    [dataSource _setDownloadPath:path];

    NSDictionary *fileAttributes = [lastDecoder fileAttributes];
    // FIXME: This assumes that if we get any file attributes, they will include the creation
    // and modification date, which is not necessarily true.
    if (!fileAttributes) {
        WebResourceResponse *response = [dataSource response];
        fileAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
            [response createdDate], NSFileCreationDate,
            [response lastModifiedDate], NSFileModificationDate, nil];
    }
    
    if (![fileManager _web_createFileAtPath:path contents:nil attributes:fileAttributes]) {
        ERROR("-[NSFileManager _web_createFileAtPath:contents:attributes:] failed.");
        return [self errorWithCode:WebErrorCannotCreateFile];
    }

    [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:path];

    OSErr result = FSPathMakeRef((const UInt8 *)[path fileSystemRepresentation], &fileRef, NULL);
    if (result == noErr) {
        fileRefPtr = &fileRef;
    } else {
        ERROR("FSPathMakeRef failed.");
        [self cleanUpAfterFailure];
        return [self errorWithCode:WebErrorCannotCreateFile];
    }

    return nil;
}

- (BOOL)writeData:(NSData *)data toFork:(SInt16 *)forkRefNum
{
    OSErr result;
    
    if (*forkRefNum == 0) {
        HFSUniStr255 forkName;
        if (forkRefNum == &dataForkRefNum) {
            result = FSGetDataForkName(&forkName);
        } else {
            result = FSGetResourceForkName(&forkName);
        }
                
        if (result != noErr) {
            ERROR("Couldn't get fork name of download file.");
            return NO;
        }

        result = FSOpenFork(fileRefPtr, forkName.length, forkName.unicode, fsWrPerm, forkRefNum);
        if (result != noErr) {
            ERROR("Couldn't open fork of download file.");
            return NO;
        }
    }

    result = FSWriteFork(*forkRefNum, fsAtMark, 0, [data length], [data bytes], NULL);
    if (result != noErr) {
        ERROR("Couldn't write to fork of download file.");
        return NO;
    }

    return YES;
}

- (WebError *)writeDataForkData:(NSData *)dataForkData resourceForkData:(NSData *)resourceForkData
{
    WebError *error = [self createFileIfNecessary];
    if (error) {
        return error;
    }
    
    BOOL didWrite = YES;
    
    if ([dataForkData length]) {
        didWrite = [self writeData:dataForkData toFork:&dataForkRefNum];
    }

    if (didWrite && [resourceForkData length]) {
        didWrite = [self writeData:resourceForkData toFork:&resourceForkRefNum];
    }

    if (!didWrite) {
        ERROR("Writing to download file failed.");
        [self cleanUpAfterFailure];
        return [self errorWithCode:WebErrorCannotWriteToFile];
    }

    return nil;
}

- (NSData *)dataIfDoneBufferingData:(NSData *)data
{
    if (!bufferedData) {
        bufferedData = [data mutableCopy];
    } else if ([bufferedData length] == 0) {
        // When bufferedData's length is 0, we're done buffering.
        return data;
    } else {
        // Append new data. 
        [bufferedData appendData:data];
    }

    if ([bufferedData length] >= WEB_DOWNLOAD_DECODER_MINIMUM_HEADER_LENGTH) {
        // We've have enough now. Make a copy so we can set bufferedData's length to 0,
        // so we're know we're done buffering.
        data = [[bufferedData copy] autorelease];
        [bufferedData setLength:0];
        return data;
    } else {
        // Keep buffering. The header is not big enough to determine the encoding sequence.
        return nil;
    }
}

- (WebError *)decodeData:(NSData *)data
{
    if ([data length] == 0) {
        return nil;
    }
    
    NSData *dataForkData = nil;
    NSData *resourceForkData = nil;
    
    if (![self decodeData:data dataForkData:&dataForkData resourceForkData:&resourceForkData]) {
        ERROR("Download decoding failed.");
        [self cleanUpAfterFailure];
        return [self errorWithCode:WebErrorDownloadDecodingFailedMidStream];
    }

    WebError *error = [self writeDataForkData:dataForkData resourceForkData:resourceForkData];
    if (error) {
        return error;
    }

    return nil;
}

- (WebError *)receivedData:(NSData *)data
{
    ASSERT(data);
    
    return [self decodeData:[self dataIfDoneBufferingData:data]];
}

- (BOOL)finishDecoding
{
    NSObject <WebDownloadDecoder> *decoder;
    unsigned i;
    
    for (i = 0; i < [decoderSequence count]; i++) {
        decoder = [decoderSequence objectAtIndex:i];
        if (![decoder finishDecoding]) {
            return NO;
        }
    }

    return YES;
}

- (WebError *)finishedLoading
{
    WebError *error = [self decodeData:bufferedData];
    [bufferedData release];
    bufferedData = nil;
    if (error) {
        return error;
    }

    if (![self finishDecoding]) {
        ERROR("Download decoding failed.");
        [self cleanUpAfterFailure];
        return [self errorWithCode:WebErrorDownloadDecodingFailedToComplete];
    }

    [self closeFile];

    [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:[dataSource downloadPath]];

    LOG(Download, "Download complete. Saved to: %@", [dataSource downloadPath]);

    return nil;
}

- (void)cancel
{
    [self cleanUpAfterFailure];
}

@end
