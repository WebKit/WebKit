//
//  WebDownload.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc.
//

#import <WebKit/WebDownloadPrivate.h>

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
#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebResponse.h>

typedef struct WebFSForkIOParam
{
    FSForkIOParam paramBlock;
    WebDownload *download;
    BOOL deleteFile;
    NSData *data;
} WebFSForkIOParam;

typedef struct WebFSRefParam
{
    FSRefParam paramBlock;
    WebDownload *download;
} WebFSRefParam;

@interface WebDownloadPrivate : NSObject
{
@public
    WebDataSource *dataSource;

    NSArray *decoderClasses;
    NSMutableArray *decoderSequence;
    NSMutableData *bufferedData;

    FSRef fileRef;
    FSRefPtr fileRefPtr;

    SInt16 dataForkRefNum;
    SInt16 resourceForkRefNum;

    // isCancelled is used to make sure we don't write after cancelling the load.
    BOOL isCancelled;

    // areWritesCancelled is only used by WriteCompletionCallback to make
    // sure that only 1 write failure cancels the load.
    BOOL areWritesCancelled;
}
@end

static void WriteCompletionCallback(ParmBlkPtr paramBlock);
static void CloseCompletionCallback(ParmBlkPtr paramBlock);
static void DeleteCompletionCallback(ParmBlkPtr paramBlock);

@implementation WebDownloadPrivate

- init
{
    [super init];
    
    decoderClasses = [[NSArray arrayWithObjects:
        [WebBinHexDecoder class],
        [WebMacBinaryDecoder class],
        [WebGZipDecoder class],
        nil] retain];

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

@end

@implementation WebDownload

- initWithRequest:(WebRequest *)request delegate:(id <WebDownloadDelegate>)delegate
{
    [super init];
    // FIXME: Implement.
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (void)cancel
{
    _private->isCancelled = YES;
    [self cleanUpAfterFailure];
}

@end

@implementation WebDownload (WebPrivate)

- _initWithLoadingHandle:(WebResource *)handle
                 request:(WebRequest *)request
                response:(WebRequest *)response
                delegate:(id <WebDownloadDelegate>)delegate
{
    [super init];
    // FIXME: Implement.
    return self;
}

// FIXME: Remove many of the following methods.

- initWithDataSource:(WebDataSource *)dSource
{
    [super init];

    _private = [[WebDownloadPrivate alloc] init];
    _private->dataSource = [dSource retain];

    LOG(Download, "Download started for: %@", [[dSource request] URL]);
    return self;
}

- (WebError *)receivedData:(NSData *)data
{
    ASSERT(data);

    return [self decodeData:[self dataIfDoneBufferingData:data]];
}

- (WebError *)finishedLoading
{
    WebError *error = [self decodeData:_private->bufferedData];
    [_private->bufferedData release];
    _private->bufferedData = nil;
    if (error) {
        return error;
    }

    if (![self finishDecoding]) {
        ERROR("Download decoding failed.");
        [self cleanUpAfterFailure];
        return [self errorWithCode:WebKitErrorDownloadDecodingFailedToComplete];
    }

    [self closeFile];

    LOG(Download, "Download complete. Saved to: %@", [_private->dataSource downloadPath]);

    return nil;
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
    for (i = 0; i < [_private->decoderClasses count]; i++) {
        Class decoderClass = [_private->decoderClasses objectAtIndex:i];

        if ([decoderClass canDecodeHeaderData:headerData]) {
            NSObject <WebDownloadDecoder> *decoder = [[[decoderClass alloc] init] autorelease];
            BOOL didDecode = [decoder decodeData:headerData dataForkData:dataForkData resourceForkData:resourceForkData];
            if (!didDecode) {
                // Though the decoder said it could decode the header, actual decoding failed. Shouldn't happen.
                ERROR("Download decoder \"%s\" failed to decode header even though it claimed to handle it.",
                      [[decoder className] lossyCString]);
                continue;
            }

            [_private->decoderSequence addObject:decoder];

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

    if (!_private->decoderSequence) {
        _private->decoderSequence = [[NSMutableArray array] retain];
        [self decodeHeaderData:data dataForkData:dataForkData resourceForkData:resourceForkData];
    } else {
        unsigned i;
        for (i = 0; i< [_private->decoderSequence count]; i++) {
            NSObject <WebDownloadDecoder> *decoder = [_private->decoderSequence objectAtIndex:i];
            BOOL didDecode = [decoder decodeData:data dataForkData:dataForkData resourceForkData:resourceForkData];

            if (!didDecode) {
                return NO;
            }

            data = *dataForkData;
        }
    }

    if ([_private->decoderSequence count] == 0) {
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
        block->paramBlock.ref = _private->fileRefPtr;
        block->download = [self retain];
        block->deleteFile = deleteFile;
        PBCloseForkAsync(&block->paramBlock);
    }
}

- (void)closeFileAndDelete:(BOOL)deleteFile
{
    [self closeFork:_private->dataForkRefNum deleteFile:deleteFile];
    [self closeFork:_private->resourceForkRefNum deleteFile:deleteFile];
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
    if (_private->fileRefPtr) {
        return nil;
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *path = [_private->dataSource downloadPath];
    NSObject <WebDownloadDecoder> *lastDecoder = [_private->decoderSequence lastObject];

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

    [_private->dataSource _setDownloadPath:path];

    NSDictionary *fileAttributes = [lastDecoder fileAttributes];
    // FIXME: This assumes that if we get any file attributes, they will include the creation
    // and modification date, which is not necessarily true.
    if (!fileAttributes) {
        WebResponse *response = [_private->dataSource response];
        fileAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
            [response createdDate], NSFileCreationDate,
            [response lastModifiedDate], NSFileModificationDate, nil];
    }

    if (![fileManager _web_createFileAtPath:path contents:nil attributes:fileAttributes]) {
        ERROR("-[NSFileManager _web_createFileAtPath:contents:attributes:] failed.");
        return [self errorWithCode:WebKitErrorCannotCreateFile];
    }

    [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:path];

    OSErr result = FSPathMakeRef((const UInt8 *)[path fileSystemRepresentation], &_private->fileRef, NULL);
    if (result == noErr) {
        _private->fileRefPtr = &_private->fileRef;
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
    if (!_private->bufferedData) {
        _private->bufferedData = [data mutableCopy];
    } else if ([_private->bufferedData length] == 0) {
        // When bufferedData's length is 0, we're done buffering.
        return data;
    } else {
        // Append new data.
        [_private->bufferedData appendData:data];
    }

    if ([_private->bufferedData length] >= WEB_DOWNLOAD_DECODER_MINIMUM_HEADER_LENGTH) {
        // We've have enough now. Make a copy so we can set bufferedData's length to 0,
        // so we're know we're done buffering.
        data = [[_private->bufferedData copy] autorelease];
        [_private->bufferedData setLength:0];
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

- (BOOL)finishDecoding
{
    NSObject <WebDownloadDecoder> *decoder;
    unsigned i;

    for (i = 0; i < [_private->decoderSequence count]; i++) {
        decoder = [_private->decoderSequence objectAtIndex:i];
        if (![decoder finishDecoding]) {
            return NO;
        }
    }

    return YES;
}

- (NSString *)path
{
    return [_private->dataSource downloadPath];
}

- (BOOL)writeForkData:(NSData *)data isDataFork:(BOOL)isDataFork
{
    ASSERT(!_private->isCancelled);
    
    OSErr result;
    SInt16 *forkRefNum = isDataFork ? &_private->dataForkRefNum : &_private->resourceForkRefNum;
    
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

        result = FSOpenFork(_private->fileRefPtr, forkName.length, forkName.unicode, fsWrPerm, forkRefNum);
        if (result != noErr) {
            ERROR("Couldn't open fork of download file.");
            return NO;
        }
    }

    data = [data copy];
    WebFSForkIOParam *block = malloc(sizeof(WebFSForkIOParam));
    block->paramBlock.ioCompletion = WriteCompletionCallback;
    block->paramBlock.forkRefNum = *forkRefNum;
    block->paramBlock.positionMode = fsAtMark;
    block->paramBlock.positionOffset = 0;
    block->paramBlock.requestCount = [data length];
    block->paramBlock.buffer = (Ptr)[data bytes];
    block->download = [self retain];
    block->data = data;
    PBWriteForkAsync(&block->paramBlock);

    return YES;
}

- (WebError *)errorWithCode:(int)code
{
    return [WebError errorWithCode:code
                          inDomain:WebErrorDomainWebKit
                        failingURL:[[_private->dataSource _URL] absoluteString]];
}

- (void)cancelWithError:(WebError *)error
{
    [_private->dataSource _stopLoadingWithError:error];
}

- (SInt16)dataForkReferenceNumber
{
    return _private->dataForkRefNum;
}

- (void)setDataForkReferenceNumber:(SInt16)forkRefNum
{
    _private->dataForkRefNum = forkRefNum;
}

- (SInt16)resourceForkReferenceNumber
{
    return _private->resourceForkRefNum;
}

- (void)setResourceForkReferenceNumber:(SInt16)forkRefNum
{
    _private->resourceForkRefNum = forkRefNum;
}

- (BOOL)areWritesCancelled
{
    return _private->areWritesCancelled;
}

- (void)setWritesCancelled:(BOOL)cancelled
{
    _private->areWritesCancelled = cancelled;
}

@end

static void WriteCompletionCallback(ParmBlkPtr paramBlock)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    WebFSForkIOParam *block = (WebFSForkIOParam *)paramBlock;
    WebDownload *download = block->download;
    
    if (block->paramBlock.ioResult != noErr && ![download areWritesCancelled]) {
        ERROR("Writing to fork of download file failed with error: %d", block->paramBlock.ioResult);
        [download setWritesCancelled:YES];
        [download performSelectorOnMainThread:@selector(cancelWithError:)
                                          withObject:[download errorWithCode:WebKitErrorCannotWriteToFile]
                                       waitUntilDone:NO];
    }

    [download release];
    [block->data release];
    [pool release];
    free(block);
}

static void CloseCompletionCallback(ParmBlkPtr paramBlock)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    WebFSForkIOParam *block = (WebFSForkIOParam *)paramBlock;
    WebDownload *download = block->download;
    
    if (block->paramBlock.ioResult != noErr) {
        ERROR("Closing fork of download file failed with error: %d", block->paramBlock.ioResult);
        // FIXME: Need to report close errors.
    }

    if (block->paramBlock.forkRefNum == [download dataForkReferenceNumber]) {
        [download setDataForkReferenceNumber:0];
    } else {
        [download setResourceForkReferenceNumber:0];
    }

    // Check if both the data fork and resource fork are now closed.
    if ([download dataForkReferenceNumber] == 0 && [download resourceForkReferenceNumber] == 0) {
        if (block->deleteFile && block->paramBlock.ref) {
            WebFSRefParam *deleteBlock = malloc(sizeof(WebFSRefParam));
            deleteBlock->paramBlock.ioCompletion = DeleteCompletionCallback;
            deleteBlock->paramBlock.ref = block->paramBlock.ref;
            deleteBlock->download = [download retain];                
            PBDeleteObjectAsync(&deleteBlock->paramBlock);
        } else {
            [[NSWorkspace sharedWorkspace] performSelectorOnMainThread:@selector(_web_noteFileChangedAtPath:)
                                                            withObject:[download path]
                                                         waitUntilDone:NO];
        }
    }
    
    [download release];
    [pool release];
    free(block);
}

static void DeleteCompletionCallback(ParmBlkPtr paramBlock)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    WebFSRefParam *block = (WebFSRefParam *)paramBlock;
    WebDownload *download = block->download;
    
    if (block->paramBlock.ioResult != noErr) {
        ERROR("Removal of download file failed with error: %d", block->paramBlock.ioResult);
    } else {
        [[NSWorkspace sharedWorkspace] performSelectorOnMainThread:@selector(_web_noteFileChangedAtPath:)
                                                        withObject:[download path]
                                                     waitUntilDone:NO];
    }
    
    [download release];
    [pool release];
    free(block);
}

