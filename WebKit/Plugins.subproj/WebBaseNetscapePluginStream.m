/*	
        WebBaseNetscapePluginStream.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseNetscapePluginStream.h>

#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSURLExtras.h>

#import <Foundation/NSURLResponse.h>
#import <Foundation/NSURLResponsePrivate.h>
#import <Foundation/NSFileManager_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>

static const char *CarbonPathFromPOSIXPath(const char *posixPath);

@implementation WebBaseNetscapePluginStream

+ (NPReason)reasonForError:(NSError *)error
{
    if ([[error domain] isEqualToString:NSURLErrorDomain] && [error code] == NSURLErrorCancelled) {
        return NPRES_USER_BREAK;
    }
    return NPRES_NETWORK_ERR;
}

- (id)initWithRequestURL:(NSURL *)theRequestURL
           pluginPointer:(NPP)thePluginPointer
              notifyData:(void *)theNotifyData
        sendNotification:(BOOL)flag
{
    [super init];
 
    // Temporarily set isTerminated to YES to avoid assertion failure in dealloc in case we are released in this method.
    isTerminated = YES;

    if (theRequestURL == nil || thePluginPointer == NULL) {
        [self release];
        return nil;
    }
    
    [self setRequestURL:theRequestURL];
    [self setPluginPointer:thePluginPointer];
    notifyData = theNotifyData;
    sendNotification = flag;
    
    isTerminated = NO;
    
    return self;
}

- (void)dealloc
{
    ASSERT(isTerminated);
    ASSERT(stream.ndata == nil);

    // FIXME: It's generally considered bad style to do work, like deleting a file,
    // at dealloc time. We should change things around so that this is done at a
    // more well-defined time rather than when the last release happens.
    if (path) {
        unlink(path);
    }

    [requestURL release];
    [responseURL release];
    [plugin release];
    [deliveryData release];
    
    free((void *)stream.url);
    free(path);

    [super dealloc];
}

- (void)finalize
{
    ASSERT(isTerminated);
    ASSERT(stream.ndata == nil);

    // FIXME: Bad for all the reasons mentioned above, but even worse for GC.
    if (path) {
        unlink(path);
    }

    free((void *)stream.url);
    free(path);

    [super finalize];
}

- (uint16)transferMode
{
    return transferMode;
}

- (void)setRequestURL:(NSURL *)theRequestURL
{
    [theRequestURL retain];
    [requestURL release];
    requestURL = theRequestURL;
}

- (void)setResponseURL:(NSURL *)theResponseURL
{
    [theResponseURL retain];
    [responseURL release];
    responseURL = theResponseURL;
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

- (void)startStreamResponseURL:(NSURL *)URL
         expectedContentLength:(long long)expectedContentLength
              lastModifiedDate:(NSDate *)lastModifiedDate
                      MIMEType:(NSString *)MIMEType
{
    ASSERT(!isTerminated);
    
    if (![plugin isLoaded]) {
        return;
    }
    
    [self setResponseURL:URL];
    
    free((void *)stream.url);
    stream.url = strdup([responseURL _web_URLCString]);

    stream.ndata = self;
    stream.end = expectedContentLength > 0 ? expectedContentLength : 0;
    stream.lastmodified = [lastModifiedDate timeIntervalSince1970];
    stream.notifyData = notifyData;
    
    transferMode = NP_NORMAL;
    offset = 0;
    reason = WEB_REASON_PLUGIN_CANCELLED;

    // FIXME: Need a way to check if stream is seekable

    NPError npErr = NPP_NewStream(instance, (char *)[MIMEType UTF8String], &stream, NO, &transferMode);
    LOG(Plugins, "NPP_NewStream URL=%@ MIME=%@ error=%d", responseURL, MIMEType, npErr);

    if (npErr != NPERR_NO_ERROR) {
        ERROR("NPP_NewStream failed with error: %d responseURL: %@", npErr, responseURL);
        // Calling cancelWithReason with WEB_REASON_PLUGIN_CANCELLED cancels the load, but doesn't call NPP_DestroyStream.
        [self cancelWithReason:WEB_REASON_PLUGIN_CANCELLED];
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
            [self cancelWithReason:NPRES_NETWORK_ERR];
            break;
        default:
            ERROR("unknown stream type");
    }
}

- (void)startStreamWithResponse:(NSURLResponse *)r
{
    [self startStreamResponseURL:[r URL]
           expectedContentLength:[r expectedContentLength]
                lastModifiedDate:[r _lastModifiedDate]
                        MIMEType:[r MIMEType]];
}

- (void)destroyStream
{
    if (isTerminated || ![plugin isLoaded] || [deliveryData length] > 0 || reason == WEB_REASON_PLUGIN_CANCELLED) {
        return;
    }
    
    if (stream.ndata != NULL) {
        if (reason == NPRES_DONE && (transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY)) {
            ASSERT(path != NULL);
            const char *carbonPath = CarbonPathFromPOSIXPath(path);
            ASSERT(carbonPath != NULL);
            NPP_StreamAsFile(instance, &stream, carbonPath);
            LOG(Plugins, "NPP_StreamAsFile responseURL=%@ path=%s", responseURL, carbonPath);
        }
        
        NPError npErr;
        npErr = NPP_DestroyStream(instance, &stream, reason);
        LOG(Plugins, "NPP_DestroyStream responseURL=%@ error=%d", responseURL, npErr);
        
        stream.ndata = nil;
    }
    
    if (sendNotification) {
        // NPP_URLNotify expects the request URL, not the response URL.
        NPP_URLNotify(instance, [requestURL _web_URLCString], reason, notifyData);
        LOG(Plugins, "NPP_URLNotify requestURL=%@ reason=%d", requestURL, reason);
    }
    
    isTerminated = YES;
}

- (void)destroyStreamWithReason:(NPReason)theReason
{
    reason = theReason;
    [self destroyStream];
}

- (void)destroyStreamWithFailingReason:(NPReason)theReason
{
    ASSERT(theReason != NPRES_DONE);
    // Stop any pending data from being streamed.
    [deliveryData setLength:0];
    [self destroyStreamWithReason:theReason];
    stream.ndata = nil;
}

- (void)receivedError:(NSError *)error
{
    [self destroyStreamWithFailingReason:[[self class] reasonForError:error]];
}

- (void)cancelWithReason:(NPReason)theReason
{
    [self destroyStreamWithFailingReason:theReason];
}

- (void)finishedLoadingWithData:(NSData *)data
{
    if (![plugin isLoaded] || !stream.ndata) {
        return;
    }
    
    if ((transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) && !path) {
        path = strdup("/tmp/WebKitPlugInStreamXXXXXX");
        int fd = mkstemp(path);
        if (fd == -1) {
            // This should almost never happen.
            ERROR("can't make temporary file, almost certainly a problem with /tmp");
            // This is not a network error, but the only error codes are "network error" and "user break".
            [self destroyStreamWithFailingReason:NPRES_NETWORK_ERR];
            free(path);
            path = NULL;
            return;
        }
        int dataLength = [data length];
        if (dataLength > 0) {
            int byteCount = write(fd, [data bytes], dataLength);
            if (byteCount != dataLength) {
                // This happens only rarely, when we are out of disk space or have a disk I/O error.
                ERROR("error writing to temporary file, errno %d", errno);
                close(fd);
                // This is not a network error, but the only error codes are "network error" and "user break".
                [self destroyStreamWithFailingReason:NPRES_NETWORK_ERR];
                free(path);
                path = NULL;
                return;
            }
        }
        close(fd);
    }

    [self destroyStreamWithReason:NPRES_DONE];
}

- (void)deliverData
{
    if (![plugin isLoaded] || !stream.ndata || [deliveryData length] == 0) {
        return;
    }
    
    int32 totalBytes = [deliveryData length];
    int32 totalBytesDelivered = 0;
    
    while (totalBytesDelivered < totalBytes) {
        int32 deliveryBytes = NPP_WriteReady(instance, &stream);
        LOG(Plugins, "NPP_WriteReady responseURL=%@ bytes=%d", responseURL, deliveryBytes);
        
        if (deliveryBytes <= 0) {
            // Plug-in can't receive anymore data right now. Send it later.
            [self performSelector:@selector(deliverData) withObject:nil afterDelay:0];
            break;
        } else {
            deliveryBytes = MIN(deliveryBytes, totalBytes - totalBytesDelivered);
            NSData *subdata = [deliveryData subdataWithRange:NSMakeRange(totalBytesDelivered, deliveryBytes)];
            deliveryBytes = NPP_Write(instance, &stream, offset, [subdata length], (void *)[subdata bytes]);
            deliveryBytes = MIN((unsigned)deliveryBytes, [subdata length]);
            offset += deliveryBytes;
            totalBytesDelivered += deliveryBytes;
            LOG(Plugins, "NPP_Write responseURL=%@ bytes=%d total-delivered=%d/%d", responseURL, deliveryBytes, offset, stream.end);
        }
    }
    
    if (totalBytesDelivered > 0) {
        if (totalBytesDelivered < totalBytes) {
            NSMutableData *newDeliveryData = [[NSMutableData alloc] initWithCapacity:totalBytes - totalBytesDelivered];
            [newDeliveryData appendBytes:(char *)[deliveryData bytes] + totalBytesDelivered length:totalBytes - totalBytesDelivered];
            [deliveryData release];
            deliveryData = newDeliveryData;
        } else {
            [deliveryData setLength:0];
            [self destroyStream];
        }
    }
}

- (void)receivedData:(NSData *)data
{
    ASSERT([data length] > 0);
    
    if (transferMode != NP_ASFILEONLY) {
        if (!deliveryData) {
            deliveryData = [[NSMutableData alloc] initWithCapacity:[data length]];
        }
        [deliveryData appendData:data];
        [self deliverData];
    }
}

@end

static const char *CarbonPathFromPOSIXPath(const char *posixPath)
{
    // Returns NULL if path is to file that does not exist.
    // Doesn't add a trailing colon for directories; this is a problem for paths to a volume,
    // so this function would need to be revised if we ever wanted to call it with that.

    OSStatus error;
    FSCatalogInfo info;

    // Make an FSRef.
    FSRef ref;
    error = FSPathMakeRef((const UInt8 *)posixPath, &ref, NULL);
    if (error != noErr) {
        return NULL;
    }

    // Get volume refNum.
    error = FSGetCatalogInfo(&ref, kFSCatInfoVolume, &info, NULL, NULL, NULL);
    if (error != noErr) {
        return NULL;
    }

    // Get root directory FSRef.
    FSRef rootRef;
    error = FSGetVolumeInfo(info.volume, 0, NULL, kFSVolInfoNone, NULL, NULL, &rootRef);
    if (error != noErr) {
        return NULL;
    }

    // Get the pieces of the path.
    NSMutableData *carbonPath = [NSMutableData dataWithBytes:"" length:1];
    BOOL needColon = NO;
    for (;;) {
        FSSpec spec;
        FSRef parentRef;
        error = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL, &spec, &parentRef);
        if (error != noErr) {
            return NULL;
        }
        if (needColon) {
            [carbonPath replaceBytesInRange:NSMakeRange(0, 0) withBytes:":" length:1];
        }
        [carbonPath replaceBytesInRange:NSMakeRange(0, 0) withBytes:&spec.name[1] length:spec.name[0]];
        needColon = YES;
        if (FSCompareFSRefs(&ref, &rootRef) == noErr) {
            break;
        }
        ref = parentRef;
    }

    return (const char *)[carbonPath bytes];
}
