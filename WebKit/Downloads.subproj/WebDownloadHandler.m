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
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>

typedef struct WebFSForkIOParam
{
    FSForkIOParam paramBlock;
    WebDownloadHandler *downloadHandler;
    BOOL deleteFile;
    NSData *data;
} WebFSForkIOParam;

typedef struct WebFSRefParam
{
    FSRefParam paramBlock;
    WebDownloadHandler *downloadHandler;
} WebFSRefParam;

static void WriteCompletionCallback(ParmBlkPtr paramBlock);
static void CloseCompletionCallback(ParmBlkPtr paramBlock);
static void DeleteCompletionCallback(ParmBlkPtr paramBlock);

@interface WebDownloadHandler (WebPrivate)
- (NSString *)path;
- (WebError *)errorWithCode:(int)code;
- (void)cancelWithError:(WebError *)error;
- (SInt16)dataForkReferenceNumber;
- (void)setDataForkReferenceNumber:(SInt16)forkRefNum;
- (SInt16)resourceForkReferenceNumber;
- (void)setResourceForkReferenceNumber:(SInt16)forkRefNum;
- (BOOL)writeForkData:(NSData *)data isDataFork:(BOOL)isDataFork;
- (BOOL)areWritesCancelled;
- (void)setWritesCancelled:(BOOL)cancelled;
@end

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

- (void)closeFork:(SInt16)forkRefNum deleteFile:(BOOL)deleteFile
{
    if (forkRefNum) {
        WebFSForkIOParam *block = malloc(sizeof(WebFSForkIOParam));
        block->paramBlock.ioCompletion = CloseCompletionCallback;
        block->paramBlock.forkRefNum = forkRefNum;
        block->paramBlock.ref = fileRefPtr;
        block->downloadHandler = [self retain];
        block->deleteFile = deleteFile;
        PBCloseForkAsync(&block->paramBlock);
    }
}

- (void)closeFileAndDelete:(BOOL)deleteFile
{
    [self closeFork:dataForkRefNum deleteFile:deleteFile];
    [self closeFork:resourceForkRefNum deleteFile:deleteFile];
}

- (void)closeFile
{
    [self closeFileAndDelete:NO];
}

- (void)cleanUpAfterFailure
{
    [self closeFileAndDelete:YES];
}

- (WebError *)createFileIfNecessary
{
    if (fileRefPtr) {
        return nil;
    }
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *path = [dataSource downloadPath];
    NSObject <WebDownloadDecoder> *lastDecoder = [decoderSequence lastObject];
        
    NSString *filename = [[lastDecoder filename] _web_filenameByFixingIllegalCharacters];

    if ([filename length] != 0) {
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
        return [self errorWithCode:WebKitErrorCannotCreateFile];
    }

    [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:path];

    OSErr result = FSPathMakeRef((const UInt8 *)[path fileSystemRepresentation], &fileRef, NULL);
    if (result == noErr) {
        fileRefPtr = &fileRef;
    } else {
        ERROR("FSPathMakeRef failed.");
        [self cleanUpAfterFailure];
        return [self errorWithCode:WebKitErrorCannotCreateFile];
    }

    return nil;
}

- (WebError *)writeDataForkData:(NSData *)dataForkData resourceForkData:(NSData *)resourceForkData
{
    WebError *error = [self createFileIfNecessary];
    if (error) {
        return error;
    }
    
    BOOL didWrite = YES;
    
    if ([dataForkData length]) {
        didWrite = [self writeForkData:dataForkData isDataFork:YES];
    }

    if (didWrite && [resourceForkData length]) {
        didWrite = [self writeForkData:resourceForkData isDataFork:NO];
    }

    if (!didWrite) {
        [self cleanUpAfterFailure];
        return [self errorWithCode:WebKitErrorCannotWriteToFile];
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
        return [self errorWithCode:WebKitErrorDownloadDecodingFailedMidStream];
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
        return [self errorWithCode:WebKitErrorDownloadDecodingFailedToComplete];
    }

    [self closeFile];

    LOG(Download, "Download complete. Saved to: %@", [dataSource downloadPath]);

    return nil;
}

- (void)cancel
{
    isCancelled = YES;
    [self cleanUpAfterFailure];
}

@end


@implementation WebDownloadHandler (WebPrivate)

- (NSString *)path
{
    return [dataSource downloadPath];
}

- (BOOL)writeForkData:(NSData *)data isDataFork:(BOOL)isDataFork
{
    ASSERT(!isCancelled);
    
    OSErr result;
    SInt16 *forkRefNum = isDataFork ? &dataForkRefNum : &resourceForkRefNum;
    
    if (*forkRefNum == 0) {
        HFSUniStr255 forkName;
        if (isDataFork) {
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
    
    WebFSForkIOParam *block = malloc(sizeof(WebFSForkIOParam));
    block->paramBlock.ioCompletion = WriteCompletionCallback;
    block->paramBlock.forkRefNum = *forkRefNum;
    block->paramBlock.positionMode = fsAtMark;
    block->paramBlock.positionOffset = 0;
    block->paramBlock.requestCount = [data length];
    block->paramBlock.buffer = (Ptr)[data bytes];
    block->downloadHandler = [self retain];
    block->data = [data copy];
    PBWriteForkAsync(&block->paramBlock);

    return YES;
}

- (WebError *)errorWithCode:(int)code
{
    return [WebError errorWithCode:code inDomain:WebErrorDomainWebKit failingURL:[[dataSource URL] absoluteString]];
}

- (void)cancelWithError:(WebError *)error
{
    [dataSource _stopLoadingWithError:error];
}

- (SInt16)dataForkReferenceNumber
{
    return dataForkRefNum;
}

- (void)setDataForkReferenceNumber:(SInt16)forkRefNum
{
    dataForkRefNum = forkRefNum;
}

- (SInt16)resourceForkReferenceNumber
{
    return resourceForkRefNum;
}

- (void)setResourceForkReferenceNumber:(SInt16)forkRefNum
{
    resourceForkRefNum = forkRefNum;
}

- (BOOL)areWritesCancelled
{
    return areWritesCancelled;
}

- (void)setWritesCancelled:(BOOL)cancelled
{
    areWritesCancelled = cancelled;
}

@end

static void WriteCompletionCallback(ParmBlkPtr paramBlock)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    WebFSForkIOParam *block = (WebFSForkIOParam *)paramBlock;
    WebDownloadHandler *downloadHandler = block->downloadHandler;
    
    if (block->paramBlock.ioResult != noErr && ![downloadHandler areWritesCancelled]) {
        ERROR("Writing to fork of download file failed with error: %d", block->paramBlock.ioResult);
        [downloadHandler setWritesCancelled:YES];
        [downloadHandler performSelectorOnMainThread:@selector(cancelWithError:)
                                          withObject:[downloadHandler errorWithCode:WebKitErrorCannotWriteToFile]
                                       waitUntilDone:NO];
    }

    [downloadHandler release];
    [block->data release];
    [pool release];
    free(block);
}

static void CloseCompletionCallback(ParmBlkPtr paramBlock)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    WebFSForkIOParam *block = (WebFSForkIOParam *)paramBlock;
    WebDownloadHandler *downloadHandler = block->downloadHandler;
    
    if (block->paramBlock.ioResult != noErr) {
        ERROR("Closing fork of download file failed with error: %d", block->paramBlock.ioResult);
        // FIXME: Need to report close errors.
    }

    if (block->paramBlock.forkRefNum == [downloadHandler dataForkReferenceNumber]) {
        [downloadHandler setDataForkReferenceNumber:0];
    } else {
        [downloadHandler setResourceForkReferenceNumber:0];
    }

    // Check if both the data fork and resource fork are now closed.
    if ([downloadHandler dataForkReferenceNumber] == 0 && [downloadHandler resourceForkReferenceNumber] == 0) {
        if (block->deleteFile && block->paramBlock.ref) {
            WebFSRefParam *deleteBlock = malloc(sizeof(WebFSRefParam));
            deleteBlock->paramBlock.ioCompletion = DeleteCompletionCallback;
            deleteBlock->paramBlock.ref = block->paramBlock.ref;
            deleteBlock->downloadHandler = [downloadHandler retain];                
            PBDeleteObjectAsync(&deleteBlock->paramBlock);
        } else {
            [[NSWorkspace sharedWorkspace] performSelectorOnMainThread:@selector(_web_noteFileChangedAtPath:)
                                                            withObject:[downloadHandler path]
                                                         waitUntilDone:NO];
        }
    }
    
    [downloadHandler release];
    [pool release];
    free(block);
}

static void DeleteCompletionCallback(ParmBlkPtr paramBlock)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    WebFSRefParam *block = (WebFSRefParam *)paramBlock;
    WebDownloadHandler *downloadHandler = block->downloadHandler;
    
    if (block->paramBlock.ioResult != noErr) {
        ERROR("Removal of download file failed with error: %d", block->paramBlock.ioResult);
    } else {
        [[NSWorkspace sharedWorkspace] performSelectorOnMainThread:@selector(_web_noteFileChangedAtPath:)
                                                        withObject:[downloadHandler path]
                                                     waitUntilDone:NO];
    }
    
    [downloadHandler release];
    [pool release];
    free(block);
}

