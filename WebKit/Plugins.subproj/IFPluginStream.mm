/*	
    IFPluginStream.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFPluginStream.h"
#import <WebFoundation/WebFoundation.h>
#import <WebKitDebug.h>
#import <WebKit/IFLoadProgress.h>
#import <WebKit/IFWebControllerPrivate.h>
#import <WebKit/IFWebController.h>

static NSString *getCarbonPath(NSString *posixPath);

@implementation IFPluginStream

- initWithURL:(NSURL *)theURL pluginPointer:(NPP)thePluginPointer
{        
    return [self initWithURL:theURL pluginPointer:thePluginPointer notifyData:nil attributes:nil];
}

- initWithURL:(NSURL *)theURL pluginPointer:(NPP)thePluginPointer notifyData:(void *)theNotifyData
{
    return [self initWithURL:theURL pluginPointer:thePluginPointer notifyData:theNotifyData attributes:nil];
}

- initWithURL:(NSURL *)theURL pluginPointer:(NPP)thePluginPointer notifyData:(void *)theNotifyData attributes:(NSDictionary *)theAttributes
{
    NSString *URLString;
    char * cURL;
    
    [super init];
    
    if(!theURL)
        return nil;
    
    if(!thePluginPointer)
       return nil;
    
    view = [(IFPluginView *)thePluginPointer->ndata retain];
    URL = [theURL retain];
    instance = thePluginPointer;
    notifyData = theNotifyData;
    
    NPP_NewStream = 	[view NPP_NewStream];
    NPP_WriteReady = 	[view NPP_WriteReady];
    NPP_Write = 	[view NPP_Write];
    NPP_StreamAsFile = 	[view NPP_StreamAsFile];
    NPP_DestroyStream = [view NPP_DestroyStream];
    NPP_URLNotify = 	[view NPP_URLNotify];
    
    URLString = [theURL absoluteString];
    cURL = (char *)malloc([URLString length]+1);
    [URLString getCString:cURL];
    
    npStream.ndata = self;
    npStream.url = cURL;
    npStream.end = 0;
    npStream.lastmodified = 0;
    npStream.notifyData = notifyData;
    offset = 0;
    
    receivedFirstChunk = NO;
    stopped = NO;
    
    URLHandle = [[IFURLHandle alloc] initWithURL:URL attributes:theAttributes flags:0];
    if(URLHandle){
        [URLHandle addClient:self];
        [URLHandle loadInBackground];
    }else{
        return nil;
    }
    
    return self;
}

- (void) dealloc
{
    [self stop];
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if(path){
        [fileManager removeFileAtPath:path handler:nil];
        [path release];
    }
    free((void *)npStream.url);
    [URL release];
    [super dealloc];
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)sender
{
    [(IFWebController *)[view webController] _didStartLoading:URL];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)data
{
    int32 numBytes;
    NPError npErr;
    
    if(!receivedFirstChunk){
        receivedFirstChunk = YES;
        
        //FIXME: Need a way to check if stream is seekable
        
        npErr = NPP_NewStream(instance, (char *)[[sender contentType] cString], &npStream, NO, &transferMode);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_NewStream: %d %s\n", npErr, [[URL absoluteString] cString]);
        
        if(npErr != NPERR_NO_ERROR){
            [self stop];
            return;
        }
        
        if(transferMode == NP_NORMAL)
            WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stream type: NP_NORMAL\n");
        else if(transferMode == NP_ASFILEONLY)
            WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stream type: NP_ASFILEONLY\n");
        else if(transferMode == NP_ASFILE)
            WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stream type: NP_ASFILE\n");
        else if(transferMode == NP_SEEK){
            WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stream type: NP_SEEK not yet supported\n");
            [self stop];
            return;
        }
    }

    if(transferMode != NP_ASFILEONLY){
        numBytes = NPP_WriteReady(instance, &npStream);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_WriteReady bytes=%lu\n", numBytes);
        
        numBytes = NPP_Write(instance, &npStream, offset, [data length], (void *)[data bytes]);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_Write bytes=%lu\n", numBytes);
        
        offset += [data length];
    }
     
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];
    loadProgress->type = IF_LOAD_TYPE_PLUGIN;
    [[view webController] receivedProgress: (IFLoadProgress *)loadProgress
        forResource: [[sender url] absoluteString] fromDataSource: [view webDataSource]];
    [loadProgress release];
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    NPError npErr;
    NSFileManager *fileManager;
    NSString *filename;
    
    // FIXME: Need a better way to get a file name from a URL
    filename = [[URL absoluteString] lastPathComponent];
    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        // FIXME: Need to use something like mkstemp?
        path = [[NSString stringWithFormat:@"/tmp/%@", filename] retain];        
        fileManager = [NSFileManager defaultManager];
        [fileManager removeFileAtPath:path handler:nil];
        [fileManager createFileAtPath:path contents:data attributes:nil];
        
        // FIXME: Will cString use the correct character set?
        NPP_StreamAsFile(instance, &npStream, [getCarbonPath(path) cString]);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_StreamAsFile: %s\n", [getCarbonPath(path) cString]);
    }
    npErr = NPP_DestroyStream(instance, &npStream, NPRES_DONE);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_DestroyStream: %d\n", npErr);
    
    if(notifyData){
        NPP_URLNotify(instance, [[URL absoluteString] cString], NPRES_DONE, notifyData);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_URLNotify\n");
    }
    
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];
    loadProgress->type = IF_LOAD_TYPE_PLUGIN;
    [[view webController] receivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: [view webDataSource]];
    [loadProgress release];
    
    [self stop];
    [(IFWebController *)[view webController] _didStopLoading:URL];
}

- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)sender
{
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = -1;
    loadProgress->bytesSoFar = -1;
    loadProgress->type = IF_LOAD_TYPE_PLUGIN;
    [[view webController] receivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: [view webDataSource]];
    [loadProgress release];
    
    [self stop];
    [(IFWebController *)[view webController] _didStopLoading:URL];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(IFError *)result
{
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];
    loadProgress->type = IF_LOAD_TYPE_PLUGIN;
    
    [[view webController] receivedError: result forResource: [[sender url] absoluteString] 
        partialProgress: loadProgress fromDataSource: [view webDataSource]];
    [loadProgress release];
    
    [self stop];
    [(IFWebController *)[view webController] _didStopLoading:URL];
}

- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)url
{
    [(IFWebController *)[view webController] _didStopLoading:URL];
    [(IFWebController *)[view webController] _didStartLoading:url];
}


- (void)stop
{
    if(!stopped){
        stopped = YES;
        if([URLHandle statusCode] == IFURLHandleStatusLoading)
            [URLHandle cancelLoadInBackground];
        [URLHandle removeClient:self];
        [URLHandle release];
        [view release];
    }
}

@end

static NSString *getCarbonPath(NSString *posixPath)
{
    OSStatus error;
    FSRef ref, rootRef, parentRef;
    FSCatalogInfo info;
    NSMutableArray *carbonPathPieces;
    HFSUniStr255 nameString;
    
    // Make an FSRef.
    error = FSPathMakeRef((const UInt8 *)[[NSFileManager defaultManager] fileSystemRepresentationWithPath:posixPath], &ref, NULL);
    if (error != noErr) {
        return nil;
    }
    
    // Get volume refNum.
    error = FSGetCatalogInfo(&ref, kFSCatInfoVolume, &info, NULL, NULL, NULL);
    if (error != noErr) {
        return nil;
    }
    
    // Get root directory FSRef.
    error = FSGetVolumeInfo(info.volume, 0, NULL, kFSVolInfoNone, NULL, NULL, &rootRef);
    if (error != noErr) {
        return nil;
    }
    
    // Get the pieces of the path.
    carbonPathPieces = [NSMutableArray array];
    for (;;) {
        error = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, &nameString, NULL, &parentRef);
        if (error != noErr) {
            return nil;
        }
        [carbonPathPieces insertObject:[NSString stringWithCharacters:nameString.unicode length:nameString.length] atIndex:0];
        if (FSCompareFSRefs(&ref, &rootRef) == noErr) {
            break;
        }
        ref = parentRef;
    }
    
    // Volume names need trailing : character.
    if ([carbonPathPieces count] == 1) {
        [carbonPathPieces addObject:@""];
    }
    
    return [carbonPathPieces componentsJoinedByString:@":"];
}
