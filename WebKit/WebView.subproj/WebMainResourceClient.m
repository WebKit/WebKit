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
#import <WebKit/IFDownloadHandlerPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFLocationChangeHandler.h>

#import <WebFoundation/IFError.h>
#import <WebFoundation/IFURLHandle.h>

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
        downloadStarted = NO;
        typeChecked = NO;
        return self;
    }

    return nil;
}

- (void)dealloc
{
    part->deref();
    [dataSource release];
    [mimeHandler release];
    [super dealloc];
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
    NSString *fakeHTMLDocument;
    const char *fakeHTMLDocumentBytes;
    IFContentHandler *contentHandler;

    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", [[[sender url] absoluteString] cString]);
    
    if([dataSource _contentPolicy] == IFContentPolicyShow){
        if(handlerType == IFMIMEHANDLERTYPE_TEXT) {
            contentHandler = [[IFContentHandler alloc] initWithMIMEHandler:mimeHandler URL:[sender url]];
            fakeHTMLDocument = [contentHandler textHTMLDocumentBottom];
            fakeHTMLDocumentBytes = [fakeHTMLDocument cString];
            part->slotData(sender, (const char *)fakeHTMLDocumentBytes, strlen(fakeHTMLDocumentBytes));
            [contentHandler release];
        }
    }
    
    else if([dataSource _contentPolicy] == IFContentPolicySave) 
        [IFMIMEHandler saveFileWithPath:[dataSource _downloadPath] andData:[sender resourceData]];
               
    else if([dataSource _contentPolicy] == IFContentPolicyOpenExternally)
        [IFMIMEHandler saveAndOpenFileWithPath:[dataSource _downloadPath] andData:[sender resourceData]];

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];
    [(IFBaseWebController *)[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)data
{
    NSString *fakeHTMLDocument;
    const char *fakeHTMLDocumentBytes;
    IFContentHandler *contentHandler;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s, data = %p, length %d\n", [[[sender url] absoluteString] cString], data, [data length]);
    
    // check the mime type
    if(!typeChecked){
        WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "Main URL's contentType: %s", [[sender contentType] cString]);
        [[dataSource _locationChangeHandler] requestContentPolicyForMIMEType:[sender contentType]];
        mimeHandler = [[[IFMIMEDatabase sharedMIMEDatabase] MIMEHandlerForMIMEType:[sender contentType]] retain];
        handlerType = [mimeHandler handlerType];
        typeChecked = YES;
    }
    
    if([dataSource _contentPolicy] == IFContentPolicyShow){
        // if it's html, send the data to the part
        // FIXME: [sender contentType] still returns nil if from cache
        if(handlerType == IFMIMEHANDLERTYPE_NIL || handlerType == IFMIMEHANDLERTYPE_HTML) {
            part->slotData(sender, (const char *)[data bytes], [data length]);
        }
        
        // for non-html documents, create html doc that embeds them
        else if(handlerType == IFMIMEHANDLERTYPE_IMAGE  || 
                handlerType == IFMIMEHANDLERTYPE_PLUGIN || 
                handlerType == IFMIMEHANDLERTYPE_TEXT) {
            if (!sentFakeDocForNonHTMLContentType) {
                contentHandler = [[IFContentHandler alloc] initWithMIMEHandler:mimeHandler URL:[sender url]];
                fakeHTMLDocument = [contentHandler HTMLDocument];
                fakeHTMLDocumentBytes = [fakeHTMLDocument cString];
                part->slotData(sender, (const char *)fakeHTMLDocumentBytes, strlen(fakeHTMLDocumentBytes));
                [contentHandler release];
                sentFakeDocForNonHTMLContentType = YES;
            }
            
            // for text documents, the incoming data is part of the main page
            if(handlerType == IFMIMEHANDLERTYPE_TEXT){
                part->slotData(sender, (const char *)[data bytes], [data length]);
            }
        }
    
        else if(handlerType == IFMIMEHANDLERTYPE_APPLICATION){
            // can't show a type that we don't handle
            [[dataSource _locationChangeHandler] unableToImplementContentPolicy:0];
        }
    }
    
    else if([dataSource _contentPolicy] == IFContentPolicySave || 
            [dataSource _contentPolicy] == IFContentPolicyOpenExternally){
        
    }
    
    // update progress

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];
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

- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)url
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_REDIRECT, "url = %s\n", [[url absoluteString] cString]);
    part->setBaseURL([[url absoluteString] cString]);
    
    [dataSource _setFinalURL: url];
    
    [[dataSource _locationChangeHandler] serverRedirectTo: url forDataSource: dataSource];
}


@end
