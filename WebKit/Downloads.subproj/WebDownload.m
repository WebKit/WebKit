//
//  WebDownload.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2003 Apple Computer, Inc.
//

#import <WebKit/WebDownloadPrivate.h>

#import <WebKit/WebBinHexDecoder.h>
#import <WebKit/WebDownloadDecoder.h>
#import <WebKit/WebGZipDecoder.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebMacBinaryDecoder.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebNSWorkspaceExtras.h>
#import <WebKit/WebNSURLResponseExtras.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebNSFileManagerExtras.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLResponse.h>
#import <WebFoundation/NSURLResponsePrivate.h>

typedef struct WebFSForkIOParam
{
    FSForkIOParam paramBlock;
    WebDownload *download;
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
    NSMutableArray *decoderSequence;
    NSMutableData *bufferedData;

    FSRefPtr fileRefPtr;

    SInt16 dataForkRefNum;
    SInt16 resourceForkRefNum;

    BOOL deleteFile;
    BOOL isDownloading;
    BOOL areWritesCancelled;
    BOOL encounteredCloseError;

    NSURLConnection *resource;
    NSURLRequest *request;
    NSURLResponse *response;
    WebResourceDelegateProxy *proxy;

    id delegate;

    NSString *path;
    NSString *tempPath;
    NSString *directoryPath;
}
@end

static NSArray *decoderClasses = nil;

static void WriteCompletionCallback(ParmBlkPtr paramBlock);
static void CloseCompletionCallback(ParmBlkPtr paramBlock);
static void DeleteCompletionCallback(ParmBlkPtr paramBlock);

@interface WebDownload (ForwardDeclarations)
#pragma mark LOADING
- (void)_downloadStarted;
- (void)_downloadEnded;
- (void)_setRequest:(NSURLRequest *)request;
- (void)_setResponse:(NSURLResponse *)response;
#pragma mark CREATING
- (NSString *)_pathWithUniqueFilenameForPath:(NSString *)path;
- (BOOL)_createFSRefForPath:(NSString *)path;
- (BOOL)_createFileIfNecessary;
#pragma mark DECODING
- (void)_decodeHeaderData:(NSData *)headerData dataForkData:(NSData **)dataForkData resourceForkData:(NSData **)resourceForkData;
- (BOOL)_decodeData:(NSData *)data dataForkData:(NSData **)dataForkData resourceForkData:(NSData **)resourceForkData;
- (WebError *)_decodeData:(NSData *)data;
- (NSData *)_dataIfDoneBufferingData:(NSData *)data;
- (BOOL)_finishDecoding;
#pragma mark WRITING
- (BOOL)_writeForkData:(NSData *)data isDataFork:(BOOL)isDataFork;
- (WebError *)_writeDataForkData:(NSData *)dataForkData resourceForkData:(NSData *)resourceForkData;
#pragma mark CLOSING;
- (BOOL)_isFileClosed;
- (void)_didCloseFile:(WebError *)error;
- (void)_closeForkAsync:(SInt16)forkRefNum;
- (BOOL)_closeForkSync:(SInt16)forkRefNum;
- (void)_closeFileAsync;
- (BOOL)_closeFileSync;
#pragma mark DELETING
- (void)_didDeleteFile;
- (void)_deleteFileAsnyc;
- (void)_closeAndDeleteFileAsync;
- (void)_cancelWithError:(WebError *)error;
- (void)_cancelWithErrorCode:(int)code;
#pragma mark MISC
- (void)_setDelegate:(id)delegate;
- (void)_setPath:(NSString *)path;
- (NSString *)_currentPath;
- (WebError *)_errorWithCode:(int)code;
- (SInt16)_dataForkReferenceNumber;
- (void)_setDataForkReferenceNumber:(SInt16)forkRefNum;
- (SInt16)_resourceForkReferenceNumber;
- (void)_setResourceForkReferenceNumber:(SInt16)forkRefNum;
- (BOOL)_areWritesCancelled;
- (void)_setWritesCancelled:(BOOL)cancelled;
- (BOOL)_encounteredCloseError;
- (void)_setEncounteredCloseError:(BOOL)encounteredCloseError;
@end

@implementation WebDownloadPrivate

+ (void)initialize
{
    [WebError _registerWebKitErrors];
    
    decoderClasses = [[NSArray arrayWithObjects:
        [WebBinHexDecoder class],
        [WebMacBinaryDecoder class],
        [WebGZipDecoder class],
        nil] retain];
}

- (void)dealloc
{
    ASSERT(!resource);
    ASSERT(!delegate);
    ASSERT(!isDownloading);

    free(fileRefPtr);
    
    [bufferedData release];
    [decoderSequence release];
    [request release];
    [resource release];
    [response release];
    [proxy setDelegate:nil];
    [proxy release];
    [path release];
    [tempPath release];
    [directoryPath release];
    [super dealloc];
}

@end

@implementation WebDownload

- initWithRequest:(NSURLRequest *)request
{
    ASSERT(request);
    
    [super init];
    
    _private = [[WebDownloadPrivate alloc] init];
    _private->request = [request retain];
    
    _private->resource = [[NSURLConnection alloc] initWithRequest:request];
    if (!_private->resource) {
        [self release];
        return nil;
    }

    return self;
}

- _initWithLoadingResource:(NSURLConnection *)resource
                   request:(NSURLRequest *)request
                  response:(NSURLResponse *)response
                  delegate:(id)delegate
                     proxy:(WebResourceDelegateProxy *)proxy
{
    [super init];
     
    _private = [[WebDownloadPrivate alloc] init];
    [self _downloadStarted];
    
    _private->request =  [request retain];
    _private->resource = [resource retain];
    _private->response = [response retain];
    _private->proxy = 	 [proxy retain];
    [self _setDelegate:delegate];
    [_private->proxy setDelegate:(id <NSURLConnectionDelegate>)self];

    // Replay the delegate methods that would be called in the standalone download case.
    if ([_private->delegate respondsToSelector:@selector(download:didStartFromRequest:)]) {
        [_private->delegate download:self didStartFromRequest:_private->request];
    }

    if ([_private->delegate respondsToSelector:@selector(download:willSendRequest:redirectResponse:)]) {
        NSURLRequest *request = [_private->delegate download:self willSendRequest:_private->request redirectResponse:nil];
        if (request != _private->request) {
            // If the request is altered, cancel the resource and start a new one.
            [self cancel];
            if (request) {
                [self _setRequest:request];
                [self _setResponse:nil];
                [_private->resource release];
                _private->resource = [[NSURLConnection alloc] initWithRequest:request];
                if (!_private->resource) {
                    [self release];
                    return nil;
                }
                [_private->resource loadWithDelegate:(id <NSURLConnectionDelegate>)self];
                [self _downloadStarted];
            }
            return self;
        }
    }

    if ([_private->delegate respondsToSelector:@selector(download:didReceiveResponse:)]) {
        [_private->delegate download:self didReceiveResponse:_private->response];
    }
    
    return self;
}

+ _downloadWithLoadingResource:(NSURLConnection *)resource
                       request:(NSURLRequest *)request
                      response:(NSURLResponse *)response
                      delegate:(id)delegate
                         proxy:(WebResourceDelegateProxy *)proxy
{
    return [[[WebDownload alloc] _initWithLoadingResource:resource
                                                  request:request
                                                 response:response
                                                 delegate:delegate
                                                    proxy:proxy] autorelease];
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (void)loadWithDelegate:(id)delegate
{
    if (!_private->isDownloading) {
        [self _downloadStarted];
        
        [self _setDelegate:delegate];
        [_private->resource loadWithDelegate:(id <NSURLConnectionDelegate>)self];
        
        if ([_private->delegate respondsToSelector:@selector(download:didStartFromRequest:)]) {
            [_private->delegate download:self didStartFromRequest:_private->request];
        }
    }
}

- (void)cancel
{
    [self _cancelWithError:nil];
}

- (NSString *)path
{
    return _private->path;
}

// setPath: is not public. This is the WebDownloadDecisionListener method.
- (void)setPath:(NSString *)path
{
    [self _setPath:path];
}

#pragma mark LOADING

- (void)_downloadStarted
{
    if (!_private->isDownloading) {
        _private->isDownloading = YES;
        
        // Retain self while loading so we aren't released during the load.
        [self retain];
    }
}

- (void)_downloadEnded
{
    if (_private->isDownloading) {
        _private->isDownloading = NO;

        [_private->resource release];
        _private->resource = nil;

        [self _setDelegate:nil];

        // Balance the retain from when the load started.
        [self release];
    }
}

- (void)_setRequest:(NSURLRequest *)request
{
    if (_private->request != request) {
        [_private->request release];
        _private->request = [request retain];
    }
}

- (void)_setResponse:(NSURLResponse *)response
{
    if (_private->response != response) {
        [_private->response release];
        _private->response = [response retain];
    }
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)theRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    NSURLRequest *request = nil;
    
    if ([_private->delegate respondsToSelector:@selector(download:willSendRequest:redirectResponse:)]) {
        request = [_private->delegate download:self willSendRequest:theRequest redirectResponse:redirectResponse];
    } else {
        request = theRequest;
    }

    [self _setRequest:request];
    
    if (!request) {
        [self _downloadEnded];
    }

    return request;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
    [self _setResponse:response];
    
    if ([_private->delegate respondsToSelector:@selector(download:didReceiveResponse:)]) {
        [_private->delegate download:self didReceiveResponse:response];
    }
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    if ([_private->delegate respondsToSelector:@selector(download:didReceiveDataOfLength:)]) {
        [_private->delegate download:self didReceiveDataOfLength:[data length]];
    }
    
    WebError *error = [self _decodeData:[self _dataIfDoneBufferingData:data]];
    if (error) {
        [self _cancelWithError:error];
    }
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{    
    WebError *error = [self _decodeData:_private->bufferedData];
    [_private->bufferedData release];
    _private->bufferedData = nil;
    if (error) {
        [self _cancelWithError:error];
        return;
    }

    if (![self _finishDecoding]) {
        ERROR("Download decoding failed.");
        [self _cancelWithErrorCode:WebKitErrorDownloadDecodingFailedToComplete];
        return;
    }

    [self _closeFileAsync];
}

- (void)connection:(NSURLConnection *)connection didFailLoadingWithError:(WebError *)error
{    
    [self _cancelWithError:error];
}

#pragma mark CREATING

- (NSString *)_pathWithUniqueFilenameForPath:(NSString *)path
{
    // "Fix" the filename of the path.
    NSString *filename = [[path lastPathComponent] _web_filenameByFixingIllegalCharacters];
    path = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:filename];
        
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:path]) {
        // Don't overwrite existing files by appending "-n" or "-n.ext" to the filename.
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

    return path;
}

- (BOOL)_createFSRefForPath:(NSString *)path
{
    free(_private->fileRefPtr);    
    _private->fileRefPtr = malloc(sizeof(FSRef));
    
    OSErr result = FSPathMakeRef((const UInt8 *)[path fileSystemRepresentation], _private->fileRefPtr, NULL);
    if (result != noErr) {
        ERROR("FSPathMakeRef failed.");
        free(_private->fileRefPtr);
        _private->fileRefPtr = NULL;
        return NO;
    }

    return YES;
}

- (BOOL)_createFileIfNecessary
{
    if (_private->fileRefPtr) {
        // File is already created.
        return YES;
    }

    NSObject <WebDownloadDecoder> *lastDecoder = [_private->decoderSequence lastObject];
    NSMutableDictionary *attributes = [[lastDecoder fileAttributes] mutableCopy];
    NSFileManager *fileManager = [NSFileManager defaultManager];

    if (!attributes) {
        attributes = [[NSMutableDictionary alloc] init];
    }
    if (![attributes objectForKey:NSFileCreationDate]) {
        [attributes setObject:[_private->response createdDate] forKey:NSFileCreationDate];
    }
    if (![attributes objectForKey:NSFileModificationDate]) {
        [attributes setObject:[_private->response createdDate] forKey:NSFileModificationDate];
    }

    NSString *filename = [[lastDecoder filename] _web_filenameByFixingIllegalCharacters];
    if ([filename length] == 0) {
        filename = [_private->response suggestedFilenameForSaving];
    }

    ASSERT(!_private->path);

    NSString *path = nil;
        
    // Check if the directory is predetermined. If not, ask for a path.
    if (_private->directoryPath) {
        path = [_private->directoryPath stringByAppendingPathComponent:filename];
        [self _setPath:path];
    } else {
        if ([_private->delegate respondsToSelector:@selector(download:decidePathWithListener:suggestedFilename:)]) {
            [_private->delegate download:self
                  decidePathWithListener:(id <WebDownloadDecisionListener>)self
                       suggestedFilename:filename];
        }
    }

    if (_private->path) {
        // Path was immeditately set.
        path = _private->path;
    } else {
        // Path wasn't immeditately set. Create the file in /tmp.
        // FIXME: Consider downloading the file to the default download directory or some other location.
        char *pathC = strdup("/tmp/WebKitDownload.XXXXXX");
        if (mktemp(pathC)) {
            _private->tempPath = [[NSString alloc] initWithCString:pathC];
            path = _private->tempPath;
        }
        free(pathC);
        if (!path) {
            ERROR("mktemp failed to create a unique filename in /tmp.");
            return NO;
        }
    }

    if (![fileManager _web_createFileAtPath:path contents:nil attributes:attributes]) {
        ERROR("-[NSFileManager _web_createFileAtPath:contents:attributes:] failed.");
        return NO;
    }

    if (![self _createFSRefForPath:path]) {
        return NO;
    }

    [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:path];

    if (_private->path && [_private->delegate respondsToSelector:@selector(download:didCreateFileAtPath:)]) {
        [_private->delegate download:self didCreateFileAtPath:_private->path];
    }
    
    return YES;
}


#pragma mark DECODING

- (void)_decodeHeaderData:(NSData *)headerData
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

            [_private->decoderSequence addObject:decoder];

            [self _decodeHeaderData:*dataForkData dataForkData:dataForkData resourceForkData:resourceForkData];
            break;
        }
    }
}

- (BOOL)_decodeData:(NSData *)data
      dataForkData:(NSData **)dataForkData
  resourceForkData:(NSData **)resourceForkData
{
    ASSERT(data);
    ASSERT([data length]);
    ASSERT(dataForkData);
    ASSERT(resourceForkData);

    if (!_private->decoderSequence) {
        _private->decoderSequence = [[NSMutableArray array] retain];
        [self _decodeHeaderData:data dataForkData:dataForkData resourceForkData:resourceForkData];

        if ([_private->decoderSequence count] > 0 &&
            [_private->delegate respondsToSelector:@selector(downloadShouldDecodeEncodedFile:)] &&
            ![_private->delegate downloadShouldDecodeEncodedFile:self]) {
            [_private->decoderSequence removeAllObjects];
        }
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

- (WebError *)_decodeData:(NSData *)data
{
    if ([data length] == 0) {
        return nil;
    }

    NSData *dataForkData = nil;
    NSData *resourceForkData = nil;

    if (![self _decodeData:data dataForkData:&dataForkData resourceForkData:&resourceForkData]) {
        ERROR("Download decoding failed.");
        return [self _errorWithCode:WebKitErrorDownloadDecodingFailedMidStream];
    }

    WebError *error = [self _writeDataForkData:dataForkData resourceForkData:resourceForkData];
    if (error) {
        return error;
    }

    return nil;
}

- (NSData *)_dataIfDoneBufferingData:(NSData *)data
{
    ASSERT([data length] > 0);
    
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
        [_private->bufferedData release];
        _private->bufferedData = [[NSMutableData alloc] init];
        return data;
    } else {
        // Keep buffering. The header is not big enough to determine the encoding sequence.
        return nil;
    }
}

- (BOOL)_finishDecoding
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

#pragma mark WRITING

- (BOOL)_writeForkData:(NSData *)data isDataFork:(BOOL)isDataFork
{
    SInt16 *forkRefNum = isDataFork ? &_private->dataForkRefNum : &_private->resourceForkRefNum;

    if (*forkRefNum == 0) {
        HFSUniStr255 forkName;
        OSErr result = isDataFork ? FSGetDataForkName(&forkName) : FSGetResourceForkName(&forkName);
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

- (WebError *)_writeDataForkData:(NSData *)dataForkData resourceForkData:(NSData *)resourceForkData
{
    if (![self _createFileIfNecessary]) {
        return [self _errorWithCode:WebKitErrorCannotCreateFile];
    }

    BOOL didWrite = YES;

    if ([dataForkData length]) {
        didWrite = [self _writeForkData:dataForkData isDataFork:YES];
    }

    if (didWrite && [resourceForkData length]) {
        didWrite = [self _writeForkData:resourceForkData isDataFork:NO];
    }

    if (!didWrite) {
        [self _closeAndDeleteFileAsync];
        return [self _errorWithCode:WebKitErrorCannotWriteToFile];
    }

    return nil;
}

#pragma mark CLOSING

- (BOOL)_isFileClosed
{
    return (_private->dataForkRefNum == 0 && _private->resourceForkRefNum == 0);
}

- (void)_didCloseFile:(WebError *)error
{        
    if (_private->deleteFile) {
        [self _deleteFileAsnyc];
    } else if (error) {
        [self _cancelWithError:error];
    } else {
        [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:[self _currentPath]];
        if ([_private->delegate respondsToSelector:@selector(downloadDidFinishDownloading:)]) {
            [_private->delegate downloadDidFinishDownloading:self];
        }
        [self _downloadEnded];
    }
}

- (void)_closeForkAsync:(SInt16)forkRefNum
{
    if (forkRefNum) {
        WebFSForkIOParam *block = malloc(sizeof(WebFSForkIOParam));
        block->paramBlock.ioCompletion = CloseCompletionCallback;
        block->paramBlock.forkRefNum = forkRefNum;
        block->download = [self retain];
        PBCloseForkAsync(&block->paramBlock);
    }
}

- (BOOL)_closeForkSync:(SInt16)forkRefNum
{
    if (forkRefNum) {
        OSErr error = FSCloseFork(forkRefNum);
        if (error != noErr) {
            ERROR("FSCloseFork failed to close fork of download file with error %d", error);
            return NO;
        }
    }

    return YES;
}

- (void)_closeFileAsync
{
    [self _closeForkAsync:_private->dataForkRefNum];
    [self _closeForkAsync:_private->resourceForkRefNum];
}

- (BOOL)_closeFileSync
{
    BOOL result = YES;

    if (![self _closeForkSync:_private->dataForkRefNum]) {
        result = NO;
    }
    
    if (![self _closeForkSync:_private->resourceForkRefNum]) {
        result = NO;
    }
    
    _private->dataForkRefNum = 0;
    _private->resourceForkRefNum = 0;

    return result;
}

#pragma mark DELETING

- (void)_didDeleteFile
{
    [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:[self _currentPath]];
}

- (void)_deleteFileAsnyc
{
    if (_private->fileRefPtr) {
        WebFSRefParam *deleteBlock = malloc(sizeof(WebFSRefParam));
        deleteBlock->paramBlock.ioCompletion = DeleteCompletionCallback;
        deleteBlock->paramBlock.ref = _private->fileRefPtr;
        deleteBlock->download = [self retain];
        PBDeleteObjectAsync(&deleteBlock->paramBlock);
        free(_private->fileRefPtr);
        _private->fileRefPtr = NULL;
    }
}

- (void)_closeAndDeleteFileAsync
{
    if ([self _isFileClosed]) {
        [self _deleteFileAsnyc];
    } else {
        [self _closeFileAsync];
        _private->deleteFile = YES;
    }
}

- (void)_cancelWithError:(WebError *)error
{
    [_private->resource cancel];
    
    if (error) {
        if ([_private->delegate respondsToSelector:@selector(download:didFailDownloadingWithError:)]) {
            [_private->delegate download:self didFailDownloadingWithError:error];
        }
    }

    [self _closeAndDeleteFileAsync];
    [self _downloadEnded];
}

- (void)_cancelWithErrorCode:(int)code
{
    [self _cancelWithError:[self _errorWithCode:code]];
}

#pragma mark MISCELLANEOUS

- (void)_setDelegate:(id)delegate;
{
    [delegate retain];
    [_private->delegate release];
    _private->delegate = delegate;
}

- (void)_setPath:(NSString *)path
{
    ASSERT(path);
    ASSERT(!_private->path);
    ASSERT([path isAbsolutePath]);

    // Path has already been set.
    if (_private->path) {
        return;
    }

    if (!path || ![path isAbsolutePath]) {
        ERROR("setPath: cannot be called with a nil or non-absolute path.");
        [self cancel];
        return;
    }

    // Make sure we don't overwrite an existing file.
    _private->path = [[self _pathWithUniqueFilenameForPath:path] retain];

    // FIXME: Moving the downloading file synchronously may block the main thread for a long time
    // if the move is across volumes.
    if (_private->tempPath && ![_private->tempPath isEqualToString:_private->path]) {
        if (![self _closeFileSync]) {
            [self _cancelWithErrorCode:WebKitErrorCannotCloseFile];
            return;
        }
        if (![[NSFileManager defaultManager] movePath:_private->tempPath toPath:_private->path handler:nil]) {
            [self _cancelWithErrorCode:WebKitErrorCannotMoveFile];
            return;
        }
        if (![self _createFSRefForPath:_private->path]) {
            [self _cancelWithErrorCode:WebKitErrorCannotCreateFile];
            return;
        }

        [[NSWorkspace sharedWorkspace] _web_noteFileChangedAtPath:_private->path];

        if ([_private->delegate respondsToSelector:@selector(download:didCreateFileAtPath:)]) {
            [_private->delegate download:self didCreateFileAtPath:_private->path];
        }
    }
}

- (void)_setDirectoryPath:(NSString *)directoryPath
{
    NSString *copy = [directoryPath copy];
    [_private->directoryPath release];
    _private->directoryPath = copy;
}

- (NSString *)_currentPath
{
    return _private->path ? _private->path : _private->tempPath;
}

- (WebError *)_errorWithCode:(int)code
{
    return [WebError errorWithCode:code
                          inDomain:WebErrorDomainWebKit
                        failingURL:[[_private->request URL] absoluteString]];
}

- (SInt16)_dataForkReferenceNumber
{
    return _private->dataForkRefNum;
}

- (void)_setDataForkReferenceNumber:(SInt16)forkRefNum
{
    _private->dataForkRefNum = forkRefNum;
}

- (SInt16)_resourceForkReferenceNumber
{
    return _private->resourceForkRefNum;
}

- (void)_setResourceForkReferenceNumber:(SInt16)forkRefNum
{
    _private->resourceForkRefNum = forkRefNum;
}

- (BOOL)_areWritesCancelled
{
    return _private->areWritesCancelled;
}

- (void)_setWritesCancelled:(BOOL)cancelled
{
    _private->areWritesCancelled = cancelled;
}

- (BOOL)_encounteredCloseError
{
    return _private->encounteredCloseError;
}

- (void)_setEncounteredCloseError:(BOOL)encounteredCloseError
{
    _private->encounteredCloseError = encounteredCloseError;
}

@end

static void WriteCompletionCallback(ParmBlkPtr paramBlock)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    WebFSForkIOParam *block = (WebFSForkIOParam *)paramBlock;
    WebDownload *download = block->download;
    
    if (block->paramBlock.ioResult != noErr && ![download _areWritesCancelled]) {
        ERROR("Writing to fork of download file failed with error: %d", block->paramBlock.ioResult);
        // Prevent multiple errors from being reported by setting the areWritesCancelled boolean.
        [download _setWritesCancelled:YES];
        [download performSelectorOnMainThread:@selector(_cancelWithError:)
                                   withObject:[download _errorWithCode:WebKitErrorCannotWriteToFile]
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
        [download _setEncounteredCloseError:YES];
    }

    if (block->paramBlock.forkRefNum == [download _dataForkReferenceNumber]) {
        [download _setDataForkReferenceNumber:0];
    } else {
        [download _setResourceForkReferenceNumber:0];
    }

    if ([download _isFileClosed]) {
        WebError *error = [download _encounteredCloseError] ?
                          [download _errorWithCode:WebKitErrorCannotCloseFile] : nil;
        [download performSelectorOnMainThread:@selector(_didCloseFile:) withObject:error waitUntilDone:NO];
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
        [download performSelectorOnMainThread:@selector(_didDeleteFile) withObject:nil waitUntilDone:NO];
    }
    
    [download release];
    [pool release];
    free(block);
}

