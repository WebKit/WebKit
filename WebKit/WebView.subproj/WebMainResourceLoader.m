/*	
    IFMainURLHandleClient.mm
	    
    Copyright 2001, Apple, Inc. All rights reserved.
*/
#include <pthread.h>

#import <WebKit/IFError.h>
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFMainURLHandleClient.h>
#import <WebKit/IFMIMEDatabase.h>
#import <WebKit/WebKitDebug.h>
#import <WebKit/IFContentHandler.h>

#include <khtmlview.h>

@implementation IFMainURLHandleClient

- initWithDataSource: (IFWebDataSource *)ds part:(KHTMLPart *)p
{
    if ((self = [super init])) {
        dataSource = [ds retain];
        part = p;
        part->ref();
        sentFakeDocForNonHTMLContentType = NO;
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
    [[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    NSString *fakeHTMLDocument;
    const char *fakeHTMLDocumentBytes;
    IFContentHandler *contentHandler;

    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", [[[sender url] absoluteString] cString]);

    if([mimeHandler handlerType] == IFMIMEHANDLERTYPE_TEXT) {
        contentHandler = [[IFContentHandler alloc] initWithMIMEHandler:mimeHandler URL:[sender url]];
        fakeHTMLDocument = [contentHandler textHTMLDocumentBottom];
        fakeHTMLDocumentBytes = [fakeHTMLDocument cString];
        part->slotData(sender, (const char *)fakeHTMLDocumentBytes, strlen(fakeHTMLDocumentBytes));
        [contentHandler release];
    }

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];
    [[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)data
{
    NSString *fakeHTMLDocument;
    const char *fakeHTMLDocumentBytes;
    IFMIMEDatabase *mimeDatabase;
    IFContentHandler *contentHandler;
    
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s, data = %p, length %d\n", [[[sender url] absoluteString] cString], data, [data length]);
    
    // check the mime type
    if(!typeChecked){
        mimeDatabase = [IFMIMEDatabase sharedMIMEDatabase];
        mimeHandler = [[mimeDatabase MIMEHandlerForMIMEType:[sender contentType]] retain];
        typeChecked = YES;
    }
    
    // if it's html, send the data to the part
    // FIXME: [sender contentType] still returns nil if from cache
    if([mimeHandler handlerType] == IFMIMEHANDLERTYPE_NIL || [mimeHandler handlerType] == IFMIMEHANDLERTYPE_HTML) {
        part->slotData(sender, (const char *)[data bytes], [data length]);
    }
    
    // for non-html documents, create html doc that embeds them
    else if([mimeHandler handlerType] == IFMIMEHANDLERTYPE_IMAGE  || 
            [mimeHandler handlerType] == IFMIMEHANDLERTYPE_PLUGIN || 
            [mimeHandler handlerType] == IFMIMEHANDLERTYPE_TEXT) {
        if (!sentFakeDocForNonHTMLContentType) {
            contentHandler = [[IFContentHandler alloc] initWithMIMEHandler:mimeHandler URL:[sender url]];
            fakeHTMLDocument = [contentHandler HTMLDocument];
            fakeHTMLDocumentBytes = [fakeHTMLDocument cString];
            part->slotData(sender, (const char *)fakeHTMLDocumentBytes, strlen(fakeHTMLDocumentBytes));
            sentFakeDocForNonHTMLContentType = YES;
            [contentHandler release];
        }
        
        // for text documents, the incoming data is part of the main page
        if([mimeHandler handlerType] == IFMIMEHANDLERTYPE_TEXT){
            part->slotData(sender, (const char *)[data bytes], [data length]);
        }
    }

    // FIXME: download code goes here, stop reporting error
    else{
        [sender cancelLoadInBackground];
        IFError *error = [[IFError alloc] initWithErrorCode: IFFileDownloadNotSupportedError failingURL: [sender url]];
        [[dataSource controller] _mainReceivedError: error 
            forResource: [[sender url] absoluteString] partialProgress:nil fromDataSource: dataSource];
        [error release];
        return;
    }

    // update progress
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];
    [[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(int)result
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s, result = %d\n", [[[sender url] absoluteString] cString], result);

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];

    IFError *error = [[IFError alloc] initWithErrorCode: result failingURL: [sender url]];
    [[dataSource controller] _mainReceivedError: error forResource: [[sender url] absoluteString] partialProgress: loadProgress fromDataSource: dataSource];
    [error release];
}

- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)url
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_REDIRECT, "url = %s\n", [[url absoluteString] cString]);
    part->setBaseURL([[url absoluteString] cString]);
    
    [dataSource _setFinalURL: url];
    
    [[dataSource controller] serverRedirectTo: url forDataSource: dataSource];
}


@end
