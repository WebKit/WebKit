/*	
        WebBaseNetscapePluginStream.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseNetscapePluginStream.h>
#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebPlugin.h>

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/WebNSFileManagerExtras.h>

@implementation WebBaseNetscapePluginStream

- (void)dealloc
{    
    if(path){
        [[NSFileManager defaultManager] removeFileAtPath:path handler:nil];
        [path release];
    }
    free((void *)stream.URL);
    [URL release];
    [super dealloc];
}

- (void)setPluginPointer:(NPP)pluginPointer
{
    instance = pluginPointer;
    
    WebNetscapePlugin *plugin = [(WebBaseNetscapePluginView *)instance->ndata plugin];

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
    
    NSString *mimeType = [response contentType];
    NSString *URLString = [URL absoluteString];
    char *cURL = (char *)malloc([URLString cStringLength]+1);
    [URLString getCString:cURL];

    NSNumber *timeInterval = nil;
    uint32 lastModified = 0;

    if ([response isKindOfClass:[WebHTTPResourceResponse class]]) {
        timeInterval = [[(WebHTTPResourceResponse *)response headers] objectForKey:@"Last-Modified"];
        if(timeInterval){
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
    npErr = NPP_NewStream(instance, (char *)[mimeType cString], &stream, NO, &transferMode);
    LOG(Plugins, "NPP_NewStream: %d %s", npErr, [[URL absoluteString] cString]);

    if(npErr != NPERR_NO_ERROR){
        // FIXME: Need to properly handle this error.
        return;
    }

    if(transferMode == NP_NORMAL)
        LOG(Plugins, "Stream type: NP_NORMAL");
    else if(transferMode == NP_ASFILEONLY)
        LOG(Plugins, "Stream type: NP_ASFILEONLY");
    else if(transferMode == NP_ASFILE)
        LOG(Plugins, "Stream type: NP_ASFILE");
    else if(transferMode == NP_SEEK){
        LOG(Plugins, "Stream type: NP_SEEK not yet supported");
        // FIXME: Need to properly handle this error.
        return;
    }
}

- (void)receivedData:(NSData *)data
{   
    if(transferMode != NP_ASFILEONLY){
        int32 numBytes = NPP_WriteReady(instance, &stream);
        LOG(Plugins, "NPP_WriteReady bytes=%lu", numBytes);
        
        numBytes = NPP_Write(instance, &stream, offset, [data length], (void *)[data bytes]);
        LOG(Plugins, "NPP_Write bytes=%lu", numBytes);
        
        offset += [data length];
    }
}

- (void)receivedError:(NPError)error
{
    NPError npErr;
    npErr = NPP_DestroyStream(instance, &stream, error);
    LOG(Plugins, "NPP_DestroyStream: %d", npErr);
}

- (void)finishedLoadingWithData:(NSData *)data
{
    NPError npErr;
    NSFileManager *fileManager;
    NSString *filename, *carbonPath;
    
    filename = [[URL path] lastPathComponent];
    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        // FIXME: Need to use something like mkstemp?
        path = [[NSString stringWithFormat:@"/tmp/%@", filename] retain];        
        fileManager = [NSFileManager defaultManager];
        [fileManager removeFileAtPath:path handler:nil];
        [fileManager createFileAtPath:path contents:data attributes:nil];
        
        // FIXME: Will cString use the correct character set?
        carbonPath = [[NSFileManager defaultManager] _web_carbonPathForPath:path];
        NPP_StreamAsFile(instance, &stream, [carbonPath cString]);
        LOG(Plugins, "NPP_StreamAsFile: %s", [carbonPath cString]);
    }
    npErr = NPP_DestroyStream(instance, &stream, NPRES_DONE);
    LOG(Plugins, "NPP_DestroyStream: %d", npErr);
    
    if(notifyData){
        NPP_URLNotify(instance, [[URL absoluteString] cString], NPRES_DONE, notifyData);
        LOG(Plugins, "NPP_URLNotify");
    }
}

@end

