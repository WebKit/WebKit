/*	
    WebPluginStream.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebLoadProgress.h>
#import <WebKit/WebPluginStream.h>
#import <WebKit/WebView.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKitDebug.h>

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/WebNSFileManagerExtras.h>
#import <WebFoundation/WebResourceRequest.h>

@interface WebNetscapePluginStream (ClassInternal)
- (void)receivedData:(NSData *)data;
- (void)receivedError:(NPError)error;
- (void)finishedLoadingWithData:(NSData *)data;
- (void)setUpGlobalsWithHandle:(WebResourceHandle *)handle;
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
    [super dealloc];
}

- (void)startLoad
{
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithClient:self URL:URL attributes:attributes flags:0];
    resource = [[WebResourceHandle alloc] initWithRequest:request];
    [resource loadInBackground];
    [request release];
}

- (void)stop
{
    [resource cancelLoadInBackground];
    [resource release];
    resource = nil;
    [view release];
    view = nil;
}

- (void)setUpGlobalsWithHandle:(WebResourceHandle *)handle
{
    NSString *URLString = [[handle URL] absoluteString];
    char *cURL = (char *)malloc([URLString cStringLength]+1);
    [URLString getCString:cURL];

    npStream.ndata = self;
    npStream.URL = cURL;
    npStream.end = 0;
    npStream.lastmodified = 0;
    npStream.notifyData = notifyData;
    offset = 0;
    
    mimeType = [[handle contentType] retain];
}

- (void)receivedData:(NSData *)data
{
    int32 numBytes;
    NPError npErr;
    
    if(isFirstChunk){
        isFirstChunk = NO;
        
        //FIXME: Need a way to check if stream is seekable
        
        npErr = NPP_NewStream(instance, (char *)[mimeType cString], &npStream, NO, &transferMode);
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
}

- (void)receivedError:(NPError)error
{
    NPError npErr;
    
    npErr = NPP_DestroyStream(instance, &npStream, error);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_DestroyStream: %d\n", npErr);
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
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_StreamAsFile: %s\n", [carbonPath cString]);
    }
    npErr = NPP_DestroyStream(instance, &npStream, NPRES_DONE);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_DestroyStream: %d\n", npErr);
    
    if(notifyData){
        NPP_URLNotify(instance, [[URL absoluteString] cString], NPRES_DONE, notifyData);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_URLNotify\n");
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
        [self setUpGlobalsWithHandle:[dataSource _mainHandle]];
    }
    
    [self receivedData:data];
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

- (void)handleDidBeginLoading:(WebResourceHandle *)handle
{
    [[view webController] _didStartLoading:URL];
}

- (void)handleDidReceiveData:(WebResourceHandle *)handle data:(NSData *)data
{
    WebController *webController = [view webController];

    if(isFirstChunk){
        [self setUpGlobalsWithHandle:handle];
    }
    [self receivedData:data];
    
    [webController _receivedProgress:[WebLoadProgress progressWithResourceHandle:handle]
        forResourceHandle: handle fromDataSource: [view webDataSource] complete: NO];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)handle data: (NSData *)data
{
    WebController *webController = [view webController];
    
    [webController _receivedProgress:[WebLoadProgress progressWithResourceHandle:handle]
            forResourceHandle: handle fromDataSource: [view webDataSource] complete: YES];
 
    [self finishedLoadingWithData:data];
          
    [webController _didStopLoading:URL];
}

- (void)handleDidCancelLoading:(WebResourceHandle *)handle
{
    WebController *webController = [view webController];
    
    [webController _receivedProgress:[WebLoadProgress progress]
        forResourceHandle: handle fromDataSource: [view webDataSource] complete: YES];

    [self receivedError:NPRES_USER_BREAK];
    
    [webController _didStopLoading:URL];
}

- (void)handleDidFailLoading:(WebResourceHandle *)handle withError:(WebError *)result
{
    WebController *webController = [view webController];
    
    WebLoadProgress *loadProgress = [[WebLoadProgress alloc] initWithResourceHandle:handle];
    
    [webController _receivedError: result forResourceHandle: handle 
        partialProgress: loadProgress fromDataSource: [view webDataSource]];
    [loadProgress release];

    [self receivedError:NPRES_NETWORK_ERR];
    
    [webController _didStopLoading:URL];
}

- (void)handleDidRedirect:(WebResourceHandle *)handle toURL:(NSURL *)toURL
{
    WebController *webController = [view webController];
    
    [webController _didStopLoading:URL];
    // FIXME: This next line is not sufficient. We don't do anything to remember the new URL.
    [webController _didStartLoading:toURL];
}

@end
