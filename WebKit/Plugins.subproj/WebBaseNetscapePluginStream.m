/*	
        WebBaseNetscapePluginStream.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseNetscapePluginStream.h>
#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginPackage.h>

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/WebNSFileManagerExtras.h>

@implementation WebBaseNetscapePluginStream

- (void)dealloc
{
    ASSERT(stream.ndata == nil);

    if (path) {
        [[NSFileManager defaultManager] removeFileAtPath:path handler:nil];
    }

    [URL release];
    free((void *)stream.URL);
    [path release];

    [super dealloc];
}

- (void)setPluginPointer:(NPP)pluginPointer
{
    instance = pluginPointer;
    
    WebNetscapePluginPackage *plugin = [(WebBaseNetscapePluginView *)instance->ndata plugin];

    NPP_NewStream = 	[plugin NPP_NewStream];
    NPP_WriteReady = 	[plugin NPP_WriteReady];
    NPP_Write = 	[plugin NPP_Write];
    NPP_StreamAsFile = 	[plugin NPP_StreamAsFile];
    NPP_DestroyStream = [plugin NPP_DestroyStream];
    NPP_URLNotify = 	[plugin NPP_URLNotify];
}

- (void)setResponse:(WebResourceResponse *)response
{
    [URL release];
    URL = [[response URL] retain];
    
    NSString *URLString = [URL absoluteString];
    char *cURL = (char *)malloc([URLString cStringLength]+1);
    [URLString getCString:cURL];

    uint32 lastModified = 0;

    if ([response isKindOfClass:[WebHTTPResourceResponse class]]) {
        NSNumber *timeInterval = [[(WebHTTPResourceResponse *)response headers] objectForKey:@"Last-Modified"];
        if(timeInterval) {
            NSTimeInterval lastModifiedInterval;
            lastModifiedInterval = [[NSDate dateWithTimeIntervalSinceReferenceDate:[timeInterval doubleValue]] timeIntervalSince1970];
            if(lastModifiedInterval > 0){
                lastModified = (uint32)lastModifiedInterval;
            }
        }
    }

    stream.ndata = self;
    stream.URL = cURL;
    stream.end = [response contentLength];
    stream.lastmodified = lastModified;
    stream.notifyData = notifyData;

    // FIXME: Need a way to check if stream is seekable

    NPError npErr;
    npErr = NPP_NewStream(instance, (char *)[[response contentType] cString], &stream, NO, &transferMode);
    LOG(Plugins, "NPP_NewStream: %d %@", npErr, URL);

    if (npErr != NPERR_NO_ERROR) {
        stream.ndata = nil;
        // FIXME: Need to properly handle this error.
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
    if (!stream.ndata) {
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
    NSString *filename = [[URL path] lastPathComponent];
    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        // FIXME: Need to use something like mkstemp?
        path = [[NSString stringWithFormat:@"/tmp/%@", filename] retain];        
        NSFileManager *fileManager = [NSFileManager defaultManager];
        [fileManager removeFileAtPath:path handler:nil];
        [fileManager createFileAtPath:path contents:data attributes:nil];
        
        // FIXME: Will cString use the correct character set?
        NSString *carbonPath = [[NSFileManager defaultManager] _web_carbonPathForPath:path];
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
