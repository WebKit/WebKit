/*	
        WebBaseNetscapePluginStream.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseNetscapePluginStream.h>
#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginPackage.h>

#import <WebFoundation/WebResponse.h>
#import <WebFoundation/WebNSFileManagerExtras.h>

@implementation WebBaseNetscapePluginStream

- (void)dealloc
{
    ASSERT(stream.ndata == nil);

    // FIXME: It's generally considered bad style to do work, like deleting a file,
    // at dealloc time. We should change things around so that this is done at a
    // more well-defined time rather than when the last release happens.
    if (path) {
        unlink(path);
    }

    [URL release];
    free((void *)stream.URL);
    free(path);
    [plugin release];
    
    [super dealloc];
}

- (void)setPluginPointer:(NPP)pluginPointer
{
    instance = pluginPointer;
    
    plugin = [[(WebBaseNetscapePluginView *)instance->ndata plugin] retain];

    NPP_NewStream = 	[plugin NPP_NewStream];
    NPP_WriteReady = 	[plugin NPP_WriteReady];
    NPP_Write = 	[plugin NPP_Write];
    NPP_StreamAsFile = 	[plugin NPP_StreamAsFile];
    NPP_DestroyStream = [plugin NPP_DestroyStream];
    NPP_URLNotify = 	[plugin NPP_URLNotify];
}

- (void)setResponse:(WebResponse *)r
{
    if(![plugin isLoaded]){
        return;
    }
    
    [URL release];
    URL = [[r URL] retain];
    
    NSString *URLString = [URL absoluteString];
    char *cURL = (char *)malloc([URLString cStringLength]+1);
    [URLString getCString:cURL];

    stream.ndata = self;
    stream.URL = cURL;
    stream.end = [r contentLength];
    stream.lastmodified = [[r lastModifiedDate] timeIntervalSince1970];
    stream.notifyData = notifyData;

    offset = 0;

    // FIXME: Need a way to check if stream is seekable

    NPError npErr;
    npErr = NPP_NewStream(instance, (char *)[[r contentType] cString], &stream, NO, &transferMode);
    LOG(Plugins, "NPP_NewStream: %d %@", npErr, URL);

    if (npErr != NPERR_NO_ERROR) {
        ERROR("NPP_NewStream failed with error: %d", npErr);
        stream.ndata = nil;
        return;
    }

    switch (transferMode) {
        case NP_NORMAL:
            LOG(Plugins, "Stream type: NP_NORMAL");
            break;
        case NP_ASFILEONLY:
            LOG(Plugins, "Stream type: NP_ASFILEONLY");
            break;
        case NP_ASFILE:
            LOG(Plugins, "Stream type: NP_ASFILE");
            break;
        case NP_SEEK:
            ERROR("Stream type: NP_SEEK not yet supported");
            // FIXME: Need to properly handle this error.
            break;
        default:
            ERROR("unknown stream type");
    }
}

- (void)receivedData:(NSData *)data
{
    if(![plugin isLoaded] || !stream.ndata) {
        return;
    }
    
    if (transferMode != NP_ASFILEONLY) {
        int32 numBytes;
        
        numBytes = NPP_WriteReady(instance, &stream);
        LOG(Plugins, "NPP_WriteReady bytes=%lu", numBytes);
        
        numBytes = NPP_Write(instance, &stream, offset, [data length], (void *)[data bytes]);
        LOG(Plugins, "NPP_Write bytes=%lu", numBytes);
        
        offset += [data length];
    }
}

- (void)destroyStreamWithReason:(NPReason)reason
{
    if(![plugin isLoaded] || !stream.ndata) {
        return;
    }
    
    NPError npErr;
    npErr = NPP_DestroyStream(instance, &stream, reason);
    LOG(Plugins, "NPP_DestroyStream: %d", npErr);
    stream.ndata = nil;
}

- (void)receivedError:(NPError)reason
{
    [self destroyStreamWithReason:reason];
}

- (void)finishedLoadingWithData:(NSData *)data
{
    if(![plugin isLoaded] || !stream.ndata) {
        return;
    }
    
    if (transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        if (!path) {
            path = strdup("/tmp/SafariPlugInStream.XXXXXX");
            int fd = mkstemp(path);
            if (fd == -1) {
                // This should almost never happen.
                ERROR("can't make temporary file, almost certainly a problem with /tmp");
                // This is not a network error, but the only error codes are "network error" and "user break".
                [self destroyStreamWithReason:NPRES_NETWORK_ERR];
                free(path);
                path = NULL;
                return;
            }
            int dataLength = [data length];
            int byteCount = write(fd, [data bytes], dataLength);
            if (byteCount != dataLength) {
                // This happens only rarely, when we are out of disk space or have a disk I/O error.
                ERROR("error writing to temporary file, errno %d", errno);
                close(fd);
                // This is not a network error, but the only error codes are "network error" and "user break".
                [self destroyStreamWithReason:NPRES_NETWORK_ERR];
                free(path);
                path = NULL;
                return;
            }
            close(fd);
        }
        
        NSString *carbonPath = [[NSFileManager defaultManager] _web_carbonPathForPath:[NSString stringWithCString:path]];
        NPP_StreamAsFile(instance, &stream, [carbonPath cString]);
        LOG(Plugins, "NPP_StreamAsFile: %@", carbonPath);
    }

    [self destroyStreamWithReason:NPRES_DONE];
    
    if (notifyData) {
        NPP_URLNotify(instance, [[URL absoluteString] cString], NPRES_DONE, notifyData);
        LOG(Plugins, "NPP_URLNotify");
    }
}

@end
