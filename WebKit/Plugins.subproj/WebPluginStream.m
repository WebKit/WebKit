/*	
    WebPluginStream.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/npapi.h>
#import <WebKit/WebLoadProgress.h>
#import <WebKit/WebPluginStream.h>
#import <WebKit/WebView.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebKitLogging.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSFileManagerExtras.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>

@interface WebNetscapePluginStream (ClassInternal)
- (void)receivedData:(NSData *)data withHandle:(WebResourceHandle *)handle;
- (void)receivedError:(NPError)error;
- (void)finishedLoadingWithData:(NSData *)data;
- (void)cancel;
@end

@interface WebNetscapePluginStream (WebResourceClient) <WebResourceClient>
@end

@implementation WebNetscapePluginStream

- (void) getFunctionPointersFromPluginView:(WebNetscapePluginView *)pluginView
{
    NPP_NewStream = 	[pluginView NPP_NewStream];
    NPP_WriteReady = 	[pluginView NPP_WriteReady];
    NPP_Write = 	[pluginView NPP_Write];
    NPP_StreamAsFile = 	[pluginView NPP_StreamAsFile];
    NPP_DestroyStream = [pluginView NPP_DestroyStream];
    NPP_URLNotify = 	[pluginView NPP_URLNotify];
}

- init
{
    [super init];

    isFirstChunk = YES;
    
    return self;
}

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
    [super init];
    
    if(!theURL)
        return nil;
    
    if(!thePluginPointer)
       return nil;
    
    view = [(WebNetscapePluginView *)thePluginPointer->ndata retain];
    URL = [theURL retain];
    attributes = [theAttributes retain];
    instance = thePluginPointer;
    notifyData = theNotifyData;
    resourceData = [[NSMutableData alloc] init];

    [self getFunctionPointersFromPluginView:view];
    
    isFirstChunk = YES;
    
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
    free((void *)npStream.URL);
    [URL release];
    [attributes release];
    [resourceData release];
    [super dealloc];
}

- (void)startLoad
{
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithClient:self URL:URL attributes:attributes flags:0];
    resource = [[WebResourceHandle alloc] initWithRequest:request];
    [resource loadInBackground];
    [request release];
    [[view webController] _didStartLoading:[resource URL]];
}

- (void)stop
{
    [self cancel];
    [resource release];
    resource = nil;
    [view release];
    view = nil;
}

- (void)receivedData:(NSData *)data withHandle:(WebResourceHandle *)handle
{    
    if(isFirstChunk){

        NSString *mimeType = [[handle response] contentType];
        NSString *URLString = [[handle URL] absoluteString];
        char *cURL = (char *)malloc([URLString cStringLength]+1);
        [URLString getCString:cURL];

        NSNumber *timeInterval = [handle attributeForKey:@"Last-Modified"];
        uint32 lastModified;
        
        if(timeInterval){
            NSTimeInterval lastModifiedInterval = [[NSDate dateWithTimeIntervalSinceReferenceDate:[timeInterval doubleValue]] timeIntervalSince1970];
            if(lastModifiedInterval < 0){
                lastModified = 0;
            }else{
                lastModified = (uint32)lastModifiedInterval;
            }
        }else{
            lastModified = 0;
        }
        
        npStream.ndata = self;
        npStream.URL = cURL;
        npStream.end = [handle contentLength];
        npStream.lastmodified = lastModified;
        npStream.notifyData = notifyData;
        
        offset = 0;
        
        // FIXME: Need a way to check if stream is seekable
        
        NPError npErr = NPP_NewStream(instance, (char *)[mimeType cString], &npStream, NO, &transferMode);
        LOG(Plugins, "NPP_NewStream: %d %s", npErr, [[URL absoluteString] cString]);
        
        if(npErr != NPERR_NO_ERROR){
            [self stop];
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
            [self stop];
            return;
        }
        
        isFirstChunk = NO;
    }

    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        // only need to buffer data in this case
        [resourceData appendData:data];
    }

    if(transferMode != NP_ASFILEONLY){
        int32 numBytes = NPP_WriteReady(instance, &npStream);
        LOG(Plugins, "NPP_WriteReady bytes=%lu", numBytes);
        
        numBytes = NPP_Write(instance, &npStream, offset, [data length], (void *)[data bytes]);
        LOG(Plugins, "NPP_Write bytes=%lu", numBytes);
        
        offset += [data length];
    }
}

- (void)receivedError:(NPError)error
{
    // Don't report error before we've called NPP_NewStream
    if(!isFirstChunk){
#if !LOG_DISABLED
        NPError npErr =
#endif
        NPP_DestroyStream(instance, &npStream, error);
        LOG(Plugins, "NPP_DestroyStream: %d", npErr);
    }
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
        NPP_StreamAsFile(instance, &npStream, [carbonPath cString]);
        LOG(Plugins, "NPP_StreamAsFile: %s", [carbonPath cString]);
    }
    npErr = NPP_DestroyStream(instance, &npStream, NPRES_DONE);
    LOG(Plugins, "NPP_DestroyStream: %d", npErr);
    
    if(notifyData){
        NPP_URLNotify(instance, [[URL absoluteString] cString], NPRES_DONE, notifyData);
        LOG(Plugins, "NPP_URLNotify");
    }
    
    [self stop];
}

#pragma mark WebDocumentRepresentation

- (void)setDataSource:(WebDataSource *)dataSource
{
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    if(isFirstChunk){
        WebFrame *frame = [dataSource webFrame];
        WebView *webView = [frame webView];
        view = [[webView documentView] retain];
        instance = [view pluginInstance];

        [self getFunctionPointersFromPluginView:view];
    }

    [self receivedData:data withHandle:[dataSource _mainHandle]];
}

- (void)receivedError:(WebError *)error withDataSource:(WebDataSource *)dataSource
{
    if([error errorCode] == WebResultCancelled){
        [self receivedError:NPRES_USER_BREAK];
    } else {
        [self receivedError:NPRES_NETWORK_ERR];
    }
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    [self finishedLoadingWithData:[dataSource data]];
}

@end

#pragma mark WebResourceHandle

@implementation WebNetscapePluginStream (WebResourceClient)

- (NSString *)handleWillUseUserAgent:(WebResourceHandle *)handle forURL:(NSURL *)theURL
{
    return [[view webController] userAgentForURL:theURL];
}

- (void)handleDidReceiveData:(WebResourceHandle *)handle data:(NSData *)data
{
    ASSERT(resource == handle);

    WebController *webController = [view webController];

    [self receivedData:data withHandle:handle];
    
    [webController _receivedProgress:[WebLoadProgress progressWithResourceHandle:handle]
        forResourceHandle: handle fromDataSource: [view webDataSource] complete: NO];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)handle
{
    ASSERT(resource == handle);
    
    WebController *webController = [view webController];
    
    [webController _receivedProgress:[WebLoadProgress progressWithResourceHandle:handle]
            forResourceHandle: handle fromDataSource: [view webDataSource] complete: YES];
 
    [self finishedLoadingWithData:resourceData];
          
    [webController _didStopLoading:URL];
    
    [resource release];
    resource = nil;
}

- (void)cancel
{
    if (resource == nil) {
        return;
    }
    
    [resource cancelLoadInBackground];
    
    WebController *webController = [view webController];
    
    [webController _receivedProgress:[WebLoadProgress progress]
        forResourceHandle:resource fromDataSource:[view webDataSource] complete: YES];

    [self receivedError:NPRES_USER_BREAK];
    
    [webController _didStopLoading:URL];

    [resource release];
    resource = nil;
}

- (void)handleDidFailLoading:(WebResourceHandle *)handle withError:(WebError *)result
{
    ASSERT(resource == handle);
    
    WebController *webController = [view webController];
    
    WebLoadProgress *loadProgress = [[WebLoadProgress alloc] initWithResourceHandle:handle];
    
    [webController _receivedError: result forResourceHandle: handle 
        partialProgress: loadProgress fromDataSource: [view webDataSource]];
    [loadProgress release];

    [self receivedError:NPRES_NETWORK_ERR];
    
    [webController _didStopLoading:URL];
    
    [resource release];
    resource = nil;
}

- (void)handleDidRedirect:(WebResourceHandle *)handle toURL:(NSURL *)toURL
{
    WebController *webController = [view webController];
    
    [webController _didStopLoading:URL];
    // FIXME: This next line is not sufficient. We don't do anything to remember the new URL.
    [webController _didStartLoading:toURL];
}

@end
