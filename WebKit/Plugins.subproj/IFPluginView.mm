/*	
    IFPluginView.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#define USE_CARBON 1

#import "IFPluginView.h"

#import <WebKit/IFLoadProgress.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebFrame.h>

#import <AppKit/NSWindow_Private.h>
#import <Carbon/Carbon.h>

#import <WCPluginWidget.h>
#import <WCJavaAppletWidget.h>
#import <WCPluginDatabase.h>
#import <WebFoundation/IFURLHandle.h>
#import <IFPluginStream.h>
#import <IFWebDataSource.h>
#import <WebFoundation/IFError.h>
#import <WebKitDebug.h>

#import <WCPlugin.h>
#import <qwidget.h>
#import <IFWebView.h>
#import <IFBaseWebController.h>
#import <IFPluginNullEventSender.h>
#import "IFNullPluginView.h"

extern "C" {
#import <CoreGraphics/CoreGraphics.h>
#import <CoreGraphics/CoreGraphicsPrivate.h>
}

@implementation IFPluginView

#pragma mark IFPLUGINVIEW

// Could do this as a category on NSString if we wanted.
static char *
newCString(NSString *string)
{
    char *cString = new char[[string cStringLength] + 1];
    [string getCString:cString];
    return cString;
}

- initWithFrame:(NSRect)r plugin:(WCPlugin *)plug url:(NSString *)location mime:(NSString *)mimeType arguments:(NSDictionary *)arguments mode:(uint16)mode
{
    NSString *baseURLString;

    [super initWithFrame: r];
    
    // The following line doesn't work for Flash, so I have create a NPP_t struct and point to it
    //instance = malloc(sizeof(NPP_t));
    instance = &instanceStruct;
    instance->ndata = self;

    mime = [mimeType retain];
    URL = [location retain];
    plugin = [plug retain];
    
    // load the plug-in if it is not already loaded
    [plugin load];
    
    // copy function pointers
    NPP_New = 		[plugin NPP_New];
    NPP_Destroy = 	[plugin NPP_Destroy];
    NPP_SetWindow = 	[plugin NPP_SetWindow];
    NPP_NewStream = 	[plugin NPP_NewStream];
    NPP_WriteReady = 	[plugin NPP_WriteReady];
    NPP_Write = 	[plugin NPP_Write];
    NPP_StreamAsFile = 	[plugin NPP_StreamAsFile];
    NPP_DestroyStream = [plugin NPP_DestroyStream];
    NPP_HandleEvent = 	[plugin NPP_HandleEvent];
    NPP_URLNotify = 	[plugin NPP_URLNotify];
    NPP_GetValue = 	[plugin NPP_GetValue];
    NPP_SetValue = 	[plugin NPP_SetValue];
    NPP_Print = 	[plugin NPP_Print]; 

    // get base URL which was added in the args in the part
    baseURLString = [arguments objectForKey:@"WebKitBaseURL"];
    if (baseURLString)
        baseURL = [[NSURL URLWithString:baseURLString] retain];
            
    isHidden = [arguments objectForKey:@"hidden"] != nil;
    fullMode = [arguments objectForKey:@"wkfullmode"] != nil;
    
    argsCount = 0;
    if (fullMode) {
        cAttributes = 0;
        cValues = 0;
    } else {
        // Convert arguments dictionary to 2 string arrays.
        // These arrays are passed to NPP_New, but the strings need to be
        // modifiable and live the entire life of the plugin.
        
        cAttributes = new char * [[arguments count]];
        cValues = new char * [[arguments count]];
        
        NSEnumerator *e = [arguments keyEnumerator];
        NSString *key;
        while ((key = [e nextObject])) {
            if (![key isEqualToString:@"wkfullmode"]) {
                cAttributes[argsCount] = newCString(key);
                cValues[argsCount] = newCString([arguments objectForKey:key]);
                argsCount++;
            }
        }
    }
    
    // Initialize globals
    transferred = NO;
    isStarted = NO;
    filesToErase = [[NSMutableArray alloc] init];
    activeURLHandles = [[NSMutableArray alloc] init];
    
    return self;
}

-(void)dealloc
{
    unsigned i;
    NSFileManager *fileManager;
    
    [self stop];
    
    // remove downloaded files
    fileManager = [NSFileManager defaultManager];
    for (i=0; i<[filesToErase count]; i++){  
        [fileManager removeFileAtPath:[filesToErase objectAtIndex:i] handler:nil]; 
    }
    
    for (i = 0; i < argsCount; i++) {
        delete [] cAttributes[i];
        delete [] cValues[i];
    }
    
    [filesToErase release];
    [activeURLHandles release];
    [mime release];
    [URL release];
    [plugin release];
    delete [] cAttributes;
    delete [] cValues;
    [super dealloc];
}

- (void) setWindow
{
    NPError npErr;
    NSRect windowFrame, frameInWindow, visibleRectInWindow;
    
    windowFrame = [[self window] frame];
    frameInWindow = [self convertRect:[self bounds] toView:nil];
    visibleRectInWindow = [self convertRect:[self visibleRect] toView:nil];
    
    // flip Y coordinates
    frameInWindow.origin.y =  windowFrame.size.height - frameInWindow.origin.y - frameInWindow.size.height; 
    visibleRectInWindow.origin.y =  windowFrame.size.height - visibleRectInWindow.origin.y - visibleRectInWindow.size.height;
    
    nPort.port = GetWindowPort([[self window] _windowRef]);
    
    // FIXME: Are these values important?
    nPort.portx = -(int32)frameInWindow.origin.x;
    nPort.porty = -(int32)frameInWindow.origin.y;   
    window.window = &nPort;
    
    window.x = (int32)frameInWindow.origin.x; 
    window.y = (int32)frameInWindow.origin.y;

    window.width = (uint32)frameInWindow.size.width;
    window.height = (uint32)frameInWindow.size.height;

    window.clipRect.top = (uint16)visibleRectInWindow.origin.y;
    window.clipRect.left = (uint16)visibleRectInWindow.origin.x;
    window.clipRect.bottom = (uint16)(visibleRectInWindow.origin.y + visibleRectInWindow.size.height);
    window.clipRect.right = (uint16)(visibleRectInWindow.origin.x + visibleRectInWindow.size.width);
    
    window.type = NPWindowTypeWindow;
    
    npErr = NPP_SetWindow(instance, &window);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_SetWindow: %d, port=%d\n", npErr, (int)nPort.port);
}

- (void) newStream:(NSURL *)streamURL mimeType:(NSString *)mimeType notifyData:(void *)notifyData
{
    IFPluginStream *stream;
    NPStream *npStream;
    NPError npErr;    
    uint16 transferMode;
    IFURLHandle *urlHandle;
    NSDictionary *attributes;
    
    stream = [[IFPluginStream alloc] initWithURL:streamURL mimeType:mimeType notifyData:notifyData];
    npStream = [stream npStream];
    
    npErr = NPP_NewStream(instance, (char *)[mimeType cString], npStream, NO, &transferMode);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_NewStream: %d %s\n", npErr, [mimeType cString]);
    
    if(npErr != NPERR_NO_ERROR){
        [stream release];
        return;
    }
    
    [stream setTransferMode:transferMode];
    
    if(transferMode == NP_NORMAL){
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stream type: NP_NORMAL\n");
    }else if(transferMode == NP_ASFILEONLY || transferMode == NP_ASFILE){
        if(transferMode == NP_ASFILEONLY)
            WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stream type: NP_ASFILEONLY\n");
        if(transferMode == NP_ASFILE)
            WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stream type: NP_ASFILE\n");
        [stream setFilename:[[streamURL path] lastPathComponent]];
    }else if(transferMode == NP_SEEK){
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stream type: NP_SEEK not yet supported\n");
        return;
    }
    attributes = [NSDictionary dictionaryWithObject:stream forKey:IFURLHandleUserData];
    urlHandle = [[IFURLHandle alloc] initWithURL:streamURL attributes:attributes flags:0];
    if(urlHandle!=nil){
        [urlHandle addClient:self];
        [activeURLHandles addObject:urlHandle];
        [urlHandle loadInBackground];
    }
}

-(void)start
{
    NPSavedData saved;
    NPError npErr;
    
    isStarted = YES;
    
    npErr = NPP_New((char *)[mime cString], instance, fullMode ? NP_FULL : NP_EMBED, argsCount, cAttributes, cValues, &saved);

    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_New: %d\n", npErr);
    
    // Create a WindowRef is one doesn't already exist
    [[self window] _windowRef];
}

- (void)stop
{
    NPError npErr;
    
    if (isStarted){
        [activeURLHandles makeObjectsPerformSelector:@selector(cancelLoadInBackground)];
        [eventSender stop];
        [eventSender release];
        [[NSNotificationCenter defaultCenter] removeObserver:self];
        [self removeTrackingRect:trackingTag];
        npErr = NPP_Destroy(instance, NULL);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_Destroy: %d\n", npErr);
        isStarted = NO;
    }
}

- (NSView *) findSuperview:(NSString *)viewName
{
    NSView *view;
    
    view = self;
    while(view){
        view = [view superview];
        if([[view className] isEqualToString:viewName]){
            return view;
        }
    }
    return nil;
}

#pragma mark EVENTS

-(void)sendActivateEvent:(BOOL)isActive;
{
    EventRecord event;
    bool acceptedEvent;
    UnsignedWide msecs;
    
    event.what = activateEvt;
    event.message = (uint32)GetWindowPort([[self window] _windowRef]);
    Microseconds(&msecs);
    event.when = (uint32)((double)UnsignedWideToUInt64(msecs) / 1000000 * 60); // microseconds to ticks
    event.modifiers = isActive;
    acceptedEvent = NPP_HandleEvent(instance, &event); 
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(activateEvent): %d  isActive: %d\n", acceptedEvent, (event.modifiers & activeFlag));
}

-(void)sendUpdateEvent
{
    EventRecord event;
    bool acceptedEvent;
    UnsignedWide msecs;
    
    event.what = updateEvt;
    event.message = (uint32)GetWindowPort([[self window] _windowRef]);
    Microseconds(&msecs);
    event.when = (uint32)((double)UnsignedWideToUInt64(msecs) / 1000000 * 60); // microseconds to ticks
    acceptedEvent = NPP_HandleEvent(instance, &event); 
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(updateEvt): %d  when: %lu\n", acceptedEvent, event.when);
}


#pragma mark NSVIEW

- (void)drawRect:(NSRect)rect
{
    if(!transferred){
        NSNotificationCenter *notificationCenter;
        NSWindow *theWindow;
        
        [self start];
        [self setWindow];
        theWindow = [self window];
        notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:) name:@"NSViewBoundsDidChangeNotification" object:[self findSuperview:@"NSClipView"]];
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:) name:@"NSWindowDidResizeNotification" object:theWindow];
        [notificationCenter addObserver:self selector:@selector(windowWillClose:) name:@"NSWindowWillCloseNotification" object:theWindow];
        [notificationCenter addObserver:self selector:@selector(windowBecameKey:) name:@"NSWindowDidBecomeKeyNotification" object:theWindow];
        [notificationCenter addObserver:self selector:@selector(windowResignedKey:) name:@"NSWindowDidResignKeyNotification" object:theWindow];
        [self sendActivateEvent:[theWindow isKeyWindow]];
        if(URL)
            [self newStream:[NSURL URLWithString:URL] mimeType:mime notifyData:NULL];
        eventSender = [[IFPluginNullEventSender alloc] initializeWithNPP:instance functionPointer:NPP_HandleEvent];
        [eventSender sendNullEvents];
        transferred = YES;
        trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
        
        id webView = [self findSuperview:@"IFWebView"];
        webController = [webView controller];
        webDataSource = [[webController frameForView:webView] dataSource];
    }
    [self sendUpdateEvent];
}

- (BOOL)isFlipped
{
    return YES;
}

#pragma mark NSRESPONDER

-(BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    EventRecord event;
    bool acceptedEvent;
    UnsignedWide msecs;
    
    event.what = getFocusEvent;
    Microseconds(&msecs);
    event.when = (uint32)((double)UnsignedWideToUInt64(msecs) / 1000000 * 60); // microseconds to ticks
    acceptedEvent = NPP_HandleEvent(instance, &event); 
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(getFocusEvent): %d  when: %lu\n", acceptedEvent, event.when);
    return YES;
}

- (BOOL)resignFirstResponder
{
    EventRecord event;
    bool acceptedEvent;
    UnsignedWide msecs;
    
    event.what = loseFocusEvent;
    Microseconds(&msecs);
    event.when = (uint32)((double)UnsignedWideToUInt64(msecs) / 1000000 * 60); // microseconds to ticks
    acceptedEvent = NPP_HandleEvent(instance, &event); 
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(loseFocusEvent): %d  when: %lu\n", acceptedEvent, event.when);
    return YES;
}


-(void)mouseDown:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    Point pt;
    CGPoint mousePoint = CGSCurrentInputPointerPosition();
    
    pt.v = (short)mousePoint.y;
    pt.h = (short)mousePoint.x;
    event.what = mouseDown;
    event.where = pt;
    event.when = (uint32)([theEvent timestamp] * 60); // seconds to ticks
    event.modifiers = 0;
    acceptedEvent = NPP_HandleEvent(instance, &event);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseDown): %d pt.v=%d, pt.h=%d ticks=%lu\n", acceptedEvent, pt.v, pt.h, event.when);
}

-(void)mouseUp:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    Point pt;
    CGPoint mousePoint = CGSCurrentInputPointerPosition();
    
    pt.v = (short)mousePoint.y;
    pt.h = (short)mousePoint.x;
    event.what = mouseUp;
    event.where = pt;
    event.when = (uint32)([theEvent timestamp] * 60); 
    event.modifiers = 0;
    acceptedEvent = NPP_HandleEvent(instance, &event);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d ticks=%lu\n", acceptedEvent, pt.v, pt.h, event.when);
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    event.what = adjustCursorEvent;
    event.when = (uint32)([theEvent timestamp] * 60);
    event.modifiers = 1;
    acceptedEvent = NPP_HandleEvent(instance, &event);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseEntered): %d\n", acceptedEvent);
}

- (void)mouseExited:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
     
    event.what = adjustCursorEvent;
    event.when = (uint32)([theEvent timestamp] * 60);
    event.modifiers = 0;
    acceptedEvent = NPP_HandleEvent(instance, &event);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseExited): %d\n", acceptedEvent);
}

- (void)keyUp:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    event.what = keyUp;
    event.message = [[theEvent charactersIgnoringModifiers] characterAtIndex:0];
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(keyUp): %d key:%c\n", acceptedEvent, (char) (event.message & charCodeMask));
    //Note: QT Plug-in doesn't use keyUp's
}

- (void)keyDown:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    event.what = keyDown;
    event.message = [[theEvent charactersIgnoringModifiers] characterAtIndex:0];
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(keyDown): %d key:%c\n", acceptedEvent, (char) (event.message & charCodeMask));
}

#pragma mark NOTIFICATIONS

-(void) viewHasMoved:(NSNotification *)notification
{
    [self sendUpdateEvent];
    [self setWindow];
}

-(void) windowBecameKey:(NSNotification *)notification
{
    [self sendActivateEvent:YES];
}

-(void) windowResignedKey:(NSNotification *)notification
{
    [self sendActivateEvent:NO];
}

- (void) windowWillClose:(NSNotification *)notification
{
    [self stop];
}


#pragma mark IFURLHANDLE

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)sender
{

}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)data
{
    int32 bytes;
    IFPluginStream *stream;
    uint16 transferMode;
    NPStream *npStream;
    
    stream = [[sender attributes] objectForKey:IFURLHandleUserData];
    transferMode = [stream transferMode];
    npStream = [stream npStream];
    
    if(transferMode != NP_ASFILEONLY){
        bytes = NPP_WriteReady(instance, npStream);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_WriteReady bytes=%lu\n", bytes);
        bytes = NPP_Write(instance, npStream, [stream offset], [data length], (void *)[data bytes]);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_Write bytes=%lu\n", bytes);
        [stream incrementOffset:[data length]];
    }
    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY){
        [[stream data] appendData:data];
    }
        
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];
    loadProgress->type = IF_LOAD_TYPE_PLUGIN;
    [webController receivedProgress: (IFLoadProgress *)loadProgress
        forResource: [[sender url] absoluteString] fromDataSource: webDataSource];
    [loadProgress release];
    
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    NPError npErr;
    NSMutableString *filenameClassic, *path;
    IFPluginStream *stream;
    NSFileManager *fileManager;
    uint16 transferMode;
    NPStream *npStream;
    NSString *filename;
    
    stream = [[sender attributes] objectForKey:IFURLHandleUserData];
    transferMode = [stream transferMode];
    npStream = [stream npStream];
    filename = [stream filename];
    
    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        path = [NSString stringWithFormat:@"%@%@", @"/tmp/", filename];        
        [filesToErase addObject:path];
        
        fileManager = [NSFileManager defaultManager];
        [fileManager removeFileAtPath:path handler:nil];
        [fileManager createFileAtPath:path contents:[stream data] attributes:nil];
        
        filenameClassic = [NSString stringWithFormat:@"%@%@%@", startupVolumeName(), @":private:tmp:", filename];
        NPP_StreamAsFile(instance, npStream, [filenameClassic cString]);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_StreamAsFile: %s\n", [filenameClassic cString]);
    }
    npErr = NPP_DestroyStream(instance, npStream, NPRES_DONE);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_DestroyStream: %d\n", npErr);
    
    if(npStream->notifyData){
        NPP_URLNotify(instance, npStream->url, NPRES_DONE, npStream->notifyData);
        WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_URLNotify\n");
    }
    [stream release];
    [activeURLHandles removeObject:sender];
    
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];
    loadProgress->type = IF_LOAD_TYPE_PLUGIN;
    [webController receivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: webDataSource];
    [loadProgress release];
}

- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)sender
{
    IFPluginStream *stream;
    
    stream = [[sender attributes] objectForKey:IFURLHandleUserData];
    [stream release];
    [self stop];
    
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = -1;
    loadProgress->bytesSoFar = -1;
    loadProgress->type = IF_LOAD_TYPE_PLUGIN;
    [webController receivedProgress: (IFLoadProgress *)loadProgress 
        forResource: [[sender url] absoluteString] fromDataSource: webDataSource];
    [loadProgress release];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(int)result
{
    IFPluginStream *stream;
    
    stream = [[sender attributes] objectForKey:IFURLHandleUserData];
    [stream release];
    [self stop];
    
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];
    loadProgress->type = IF_LOAD_TYPE_PLUGIN;
    
    IFError *error = [[IFError alloc] initWithErrorCode: result failingURL: [sender url]];
    [webController receivedError: error forResource: [[sender url] absoluteString] 
        partialProgress: loadProgress fromDataSource: webDataSource];
    [error release];
    [loadProgress release];
}

- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)url
{
    
}

#pragma mark PLUGIN-TO-BROWSER

-(NPError)getURLNotify:(const char *)url target:(const char *)target notifyData:(void *)notifyData
{
    NSURL *newURL;
    IFWebDataSource *dataSource;
    NSURL *requestedURL;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_GetURLNotify: %s target: %s\n", url, target);
 
    if(!strcmp(url, "")){
        return NPERR_INVALID_URL;
    }else if(strstr(url, "://")){ //check if this is an absolute URL
        requestedURL = [NSURL URLWithString:[NSString stringWithCString:url]];
    }else{
        requestedURL = [NSURL URLWithString:[NSString stringWithCString:url] relativeToURL:baseURL];
    }
    
    if(target == NULL){ // send data to plug-in if target is null
        [self newStream:requestedURL mimeType:[plugin mimeTypeForURL:[NSString stringWithCString:url]] notifyData:(void *)notifyData];
    }else if(!strcmp(target, "_self") || !strcmp(target, "_current") || !strcmp(target, "_parent") || !strcmp(target, "_top")){
        if(webController){
            newURL = [NSURL URLWithString:[NSString stringWithCString:url]];
            dataSource = [[[IFWebDataSource alloc] initWithURL:newURL] autorelease];
            [[webController mainFrame] setProvisionalDataSource:dataSource];
            [[webController mainFrame] startLoading];
        }
    }else if(!strcmp(target, "_blank") || !strcmp(target, "_new")){
        printf("Error: No API to open new browser window\n");
    }
    return NPERR_NO_ERROR;
}

-(NPError)getURL:(const char *)url target:(const char *)target
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_GetURL: %s target: %s\n", url, target);
    return [self getURLNotify:url target:target notifyData:NULL];
}

-(NPError)postURLNotify:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_PostURLNotify\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)postURL:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_PostURL\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_NewStream\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_Write\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_DestroyStream\n");
    return NPERR_GENERIC_ERROR;
}

-(void)status:(const char *)message
{
    IFWebDataSource *dataSource;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_Status: %s\n", message);
    if(webController){
        dataSource = [[webController mainFrame] dataSource];
        [webController setStatusText:[NSString stringWithCString:message] forDataSource:dataSource];
    }
}

-(NPError)getValue:(NPNVariable)variable value:(void *)value
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_GetValue\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)setValue:(NPPVariable)variable value:(void *)value
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_SetValue\n");
    return NPERR_GENERIC_ERROR;
}

-(void)invalidateRect:(NPRect *)invalidRect
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_InvalidateRect\n");
}

-(void)invalidateRegion:(NPRegion)invalidateRegion
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_InvalidateRegion\n");
}

-(void)forceRedraw
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "forceRedraw\n");
}

#pragma mark PREBINDING

static NSView *IFPluginViewCreate(NSString *pluginURL, NSString *serviceType, NSArray *args, NSString *baseURL)
{
    NSMutableDictionary *arguments;
    NSString *mimeType;
    NSRange r1, r2, r3;
    WCPlugin *plugin;
    uint i;
    
    arguments = [NSMutableDictionary dictionary];
    for (i = 0; i < [args count]; i++){
        NSString *arg = [args objectAtIndex:i];
        if ([arg rangeOfString:@"__KHTML__"].length == 0) {
            r1 = [arg rangeOfString:@"="]; // parse out attributes and values
            r2 = [arg rangeOfString:@"\""];
            r3.location = r2.location + 1;
            r3.length = [arg length] - r2.location - 2; // don't include quotes
            [arguments setObject:[arg substringWithRange:r3] forKey:[arg substringToIndex:r1.location]];
        }
    }
    
    if ([baseURL length]) {
        [arguments setObject:baseURL forKey:@"WebKitBaseURL"];
    }
        
    if ([serviceType length]) {
        mimeType = serviceType;
        plugin = [[WCPluginDatabase installedPlugins] getPluginForMimeType:mimeType];
    } else {
        plugin = [[WCPluginDatabase installedPlugins] getPluginForExtension:[pluginURL pathExtension]];
        mimeType = [plugin mimeTypeForURL:pluginURL];
    }
    
    if (plugin == nil) {
        return [[[IFNullPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) mimeType:mimeType arguments:arguments] autorelease];
    }
    return [[[IFPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) plugin:plugin url:pluginURL mime:mimeType arguments:arguments mode:NP_EMBED] autorelease];
}

static NSView *IFJavaAppletViewCreate(NSDictionary *arguments)
{
    WCPlugin *plugin;
    NSMutableDictionary *argsCopy;
    
    plugin = [[WCPluginDatabase installedPlugins] getPluginForFilename:@"Java.plugin"];
    if (plugin == nil) {
        return nil;
    }
    
    argsCopy = [NSMutableDictionary dictionaryWithDictionary:arguments];
    [argsCopy setObject:[argsCopy objectForKey:@"baseURL"] forKey:@"DOCBASE"];
    [argsCopy removeObjectForKey:@"baseURL"];

    if (plugin == nil) {
        return [[[IFNullPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) mimeType:@"application/x-java-applet" arguments:argsCopy] autorelease];
    }
    return [[[IFPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) plugin:plugin url:nil mime:@"application/x-java-applet" arguments:argsCopy mode:NP_EMBED] autorelease];
}

+(void) load
{
    IFSetPluginViewCreationFunction(IFPluginViewCreate);
    IFSetJavaAppletViewCreationFunction(IFJavaAppletViewCreate);
}

@end

NSString* startupVolumeName(void)
{
    NSString* rootName = nil;
    FSRef rootRef;
    if (FSPathMakeRef ((const UInt8 *) "/", & rootRef, NULL /*isDirectory*/) == noErr) {         
        HFSUniStr255  nameString;
        if (FSGetCatalogInfo (&rootRef, kFSCatInfoNone, NULL /*catalogInfo*/, &nameString, NULL /*fsSpec*/, NULL /*parentRef*/) == noErr) {
            rootName = [NSString stringWithCharacters:nameString.unicode length:nameString.length];
        }
    }
    return rootName;
}
