/*	
    IFMainURLHandleClient.mm
	    
    Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFMainURLHandleClient.h>

#import <pthread.h>

#import <WebKit/IFLoadProgress.h>
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFMIMEDatabase.h>
#import <WebKit/WebKitDebug.h>
#import <WebKit/IFContentHandler.h>
#import <WebKit/IFDownloadHandler.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFLocationChangeHandler.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebFramePrivate.h>

#import <WebFoundation/IFError.h>

#import <khtmlview.h>
#import <khtml_part.h>

@implementation IFMainURLHandleClient

- initWithDataSource: (IFWebDataSource *)ds part:(KHTMLPart *)p
{
    if ((self = [super init])) {
        dataSource = [ds retain];
        part = p;
        part->ref();
        sentFakeDocForNonHTMLContentType = NO;
        examinedInitialData = NO;
        downloadStarted = NO;
        loadFinished    = NO;
        sentInitialData = NO;
        contentPolicy = IFContentPolicyNone;
        return self;
    }

    return nil;
}

- (void)dealloc
{
    part->deref();
    [dataSource release];
    [mimeHandler release];
    [resourceData release];
    [encoding release];
    [url release];
    [super dealloc];
}

// This method should never get called more than once.
// Also, this method should never be passed a IFContentPolicyNone.
- (void)setContentPolicy:(IFContentPolicy)theContentPolicy
{
    contentPolicy = theContentPolicy;
    
    if(loadFinished)
        [self processData:resourceData isComplete:YES];
}


- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)sender
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", [[[sender url] absoluteString] cString]);
}


- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)sender
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", [[[sender url] absoluteString] cString]);

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = -1;
    loadProgress->bytesSoFar = -1;
    [(IFBaseWebController *)[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}


- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", [[[sender url] absoluteString] cString]);
    
    loadFinished = YES;
    
    if(contentPolicy != IFContentPolicyNone){
        [self finishProcessingData:data];
    }else{
        // If the content policy hasn't been set, save the data until it has.
        resourceData = [data retain];
    }
    
    // update progress
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];
    [(IFBaseWebController *)[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}


- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)data
{
	int contentLength = [sender contentLength];
	int contentLengthReceived = [sender contentLengthReceived];
	
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s, data = %p, length %d\n", [[[sender url] absoluteString] cString], data, [data length]);
    
    // Check the mime type and ask the client for the content policy.
    // This only happens once.
    if(!examinedInitialData){
        WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "main content type: %s", [[sender contentType] cString]);
        [[dataSource _locationChangeHandler] requestContentPolicyForMIMEType:[sender contentType]];
        
        // FIXME: Remove/replace IFMIMEHandler stuff
        mimeHandler = [[[IFMIMEDatabase sharedMIMEDatabase] MIMEHandlerForMIMEType:[sender contentType]] retain];
        handlerType = [mimeHandler handlerType];
        
        encoding = [[sender characterSet] retain];
        url = [[sender url] retain];
        examinedInitialData = YES;
    }
    
    if(contentPolicy != IFContentPolicyNone && contentPolicy != IFContentPolicyIgnore){
        if(!sentInitialData){
            // process all data that has been received 
            //[self processData:[sender resourceData] isComplete:NO];
            
            //FIXME: Need we still depend on the content policy being set immediately because of 2925907.
            [self processData:data isComplete:NO]; 
            sentInitialData = YES;
        }else{
            [self processData:data isComplete:NO];
        }
    }
    
    if(contentPolicy == IFContentPolicyIgnore){
        [sender cancelLoadInBackground];
    }
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "%d of %d", contentLengthReceived, contentLength);
    
    // Don't send the last progress message, it will be sent via
    // IFURLHandleResourceDidFinishLoading
    if (contentLength == contentLengthReceived &&
    	contentLength != -1){
    	return;
    }
    
    // update progress
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = contentLength;
    loadProgress->bytesSoFar = contentLengthReceived;
    [(IFBaseWebController *)[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}


- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(IFError *)result
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s, result = %s\n", [[[sender url] absoluteString] cString], [[result errorDescription] lossyCString]);

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];

    [(IFBaseWebController *)[dataSource controller] _mainReceivedError: result forResource: [[sender url] absoluteString] 	partialProgress: loadProgress fromDataSource: dataSource];
}


- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)URL
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_REDIRECT, "url = %s\n", [[URL absoluteString] cString]);
    part->setBaseURL([[URL absoluteString] cString]);
    
    [dataSource _setFinalURL: URL];
    
    [[dataSource _locationChangeHandler] serverRedirectTo: URL forDataSource: dataSource];
}


- (void) processData:(NSData *)data isComplete:(BOOL)complete
{
    NSString *fakeHTMLDocument;
    const char *fakeHTMLDocumentBytes;
    IFContentHandler *contentHandler;
    IFWebFrame *frame;
        
    if(contentPolicy == IFContentPolicyShow){
        
        if(handlerType == IFMIMEHANDLERTYPE_NIL || handlerType == IFMIMEHANDLERTYPE_HTML) {
            // If data is html, send it to the part.
            part->slotData(encoding, (const char *)[data bytes], [data length]);
        }
        
        else if(handlerType == IFMIMEHANDLERTYPE_IMAGE  || 
                handlerType == IFMIMEHANDLERTYPE_PLUGIN || 
                handlerType == IFMIMEHANDLERTYPE_TEXT) {
                
            // For a non-html document, create html doc that embeds it.
            if (!sentFakeDocForNonHTMLContentType) {
                contentHandler = [[IFContentHandler alloc] initWithMIMEHandler:mimeHandler URL:url];
                fakeHTMLDocument = [contentHandler HTMLDocument];
                fakeHTMLDocumentBytes = [fakeHTMLDocument cString];
                part->slotData(encoding, (const char *)fakeHTMLDocumentBytes, strlen(fakeHTMLDocumentBytes));
                [contentHandler release];
                sentFakeDocForNonHTMLContentType = YES;
            }
            
            // For text documents, the incoming data is part of the main page.
            if(handlerType == IFMIMEHANDLERTYPE_TEXT){
                part->slotData(encoding, (const char *)[data bytes], [data length]);
            }
        }
    }
    
    else if(contentPolicy == IFContentPolicySave || contentPolicy == IFContentPolicyOpenExternally){
            if(!downloadStarted){
            
                // If this is a download, detach the provisionalDataSource from the frame
                // and have downloadHandler retain it.
                downloadHandler = [[IFDownloadHandler alloc] initWithDataSource:dataSource];
                frame = [dataSource webFrame];
                [frame->_private setProvisionalDataSource:nil];
                
                // go right to locationChangeDone as the data source never gets committed.
                [[dataSource _locationChangeHandler] locationChangeDone:nil];
                downloadStarted = YES;
            }
    }
    
    if(complete)
        [self finishProcessingData:data];
}

- (void) finishProcessingData:(NSData *)data;
{
    NSString *fakeHTMLDocument;
    const char *fakeHTMLDocumentBytes;
    IFContentHandler *contentHandler;
    
    if(contentPolicy == IFContentPolicyShow){
        if(handlerType == IFMIMEHANDLERTYPE_TEXT) {
            contentHandler = [[IFContentHandler alloc] initWithMIMEHandler:mimeHandler URL:url];
            fakeHTMLDocument = [contentHandler textHTMLDocumentBottom];
            fakeHTMLDocumentBytes = [fakeHTMLDocument cString];
            part->slotData(encoding, (const char *)fakeHTMLDocumentBytes, strlen(fakeHTMLDocumentBytes));
            [contentHandler release];
        }
    }
    
    else if(contentPolicy == IFContentPolicySave || contentPolicy == IFContentPolicyOpenExternally){
        // FIXME [cblu]: We shouldn't wait for the download to end to write to the disk.
        // Will fix once we there is an IFURLHandle flag to not cache in memory (2903660). 
        [downloadHandler downloadCompletedWithData:data];
        [downloadHandler release];
    }
}

@end
