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

#import <IFPluginDatabase.h>
#import <WebFoundation/IFURLHandle.h>
#import <IFPluginStream.h>
#import <IFWebDataSource.h>
#import <WebFoundation/IFError.h>
#import <WebKitDebug.h>

#import <IFPlugin.h>
#import <qwidget.h>
#import <IFWebView.h>
#import <IFBaseWebController.h>
#import <IFPluginNullEventSender.h>
#import "IFNullPluginView.h"

static NSString* startupVolumeName(void);

@implementation IFPluginView

#pragma mark EVENTS

+ (void)getCarbonEvent:(EventRecord *)carbonEvent
{
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = TickCount();
    GetGlobalMouse(&carbonEvent->where);
    carbonEvent->modifiers = GetCurrentKeyModifiers();
    if (!Button())
        carbonEvent->modifiers |= btnState;
}

- (void)getCarbonEvent:(EventRecord *)carbonEvent
{
    [[self class] getCarbonEvent:carbonEvent];
}

- (EventModifiers)modifiersForEvent:(NSEvent *)event isMouseDown:(BOOL)isMouseDown
{
    EventModifiers modifiers;
    unsigned int modifierFlags = [event modifierFlags];
    
    modifiers = 0;
    
    if (!isMouseDown)
        modifiers |= btnState;
    
    if (modifierFlags & NSCommandKeyMask)
        modifiers |= cmdKey;
    
    if (modifierFlags & NSShiftKeyMask)
        modifiers |= shiftKey;

    if (modifierFlags & NSAlphaShiftKeyMask)
        modifiers |= alphaLock;

    if (modifierFlags & NSAlternateKeyMask)
        modifiers |= optionKey;

    if (modifierFlags & NSControlKeyMask)
        modifiers |= controlKey;

    return modifiers;
}

- (void)getCarbonEvent:(EventRecord *)carbonEvent withEvent:(NSEvent *)cocoaEvent isMouseDown:(BOOL)isMouseDown
{
    NSPoint where;
    
    where = [[cocoaEvent window] convertBaseToScreen:[cocoaEvent locationInWindow]];
    
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = (UInt32)([cocoaEvent timestamp] * 60); // seconds to ticks
    carbonEvent->where.h = (short)where.x;
    carbonEvent->where.v = (short)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - where.y);
    carbonEvent->modifiers = [self modifiersForEvent:cocoaEvent isMouseDown:isMouseDown];    
}

- (void)getCarbonEvent:(EventRecord *)carbonEvent withEvent:(NSEvent *)cocoaEvent
{
    [self getCarbonEvent:carbonEvent withEvent:cocoaEvent isMouseDown:Button()];
}

-(void)sendActivateEvent:(BOOL)activate
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event];
    event.what = activateEvt;
    event.message = (UInt32)[[self window] _windowRef];
    if (activate)
        event.modifiers |= activMask;
    
    acceptedEvent = NPP_HandleEvent(instance, &event); 
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(activateEvent): %d  isActive: %d\n", acceptedEvent, (event.modifiers & activeFlag));
}

- (void)sendUpdateEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event];
    event.what = updateEvt;
    event.message = (UInt32)[[self window] _windowRef];

    acceptedEvent = NPP_HandleEvent(instance, &event); 
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(updateEvt): %d  when: %lu\n", acceptedEvent, event.when);
}

-(BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event];
    event.what = getFocusEvent;
    
    acceptedEvent = NPP_HandleEvent(instance, &event); 
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(getFocusEvent): %d  when: %lu\n", acceptedEvent, event.when);
    return YES;
}

- (BOOL)resignFirstResponder
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event];
    event.what = loseFocusEvent;
    
    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(loseFocusEvent): %d  when: %lu\n", acceptedEvent, event.when);
    return YES;
}


-(void)mouseDown:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event withEvent:theEvent isMouseDown:YES];
    event.what = mouseDown;
    
    acceptedEvent = NPP_HandleEvent(instance, &event);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseDown): %d pt.v=%d, pt.h=%d ticks=%lu\n", acceptedEvent, event.where.v, event.where.h, event.when);
}

-(void)mouseUp:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    Point point;
    
    [self getCarbonEvent:&event withEvent:theEvent isMouseDown:NO];
    event.what = mouseUp;

    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d ticks=%lu\n", acceptedEvent, point.v, point.h, event.when);
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseEntered): %d\n", acceptedEvent);
}

- (void)mouseExited:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseExited): %d\n", acceptedEvent);
}

- (void)keyUp:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
        
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyUp;
    // FIXME: Unicode characters vs. Macintosh ASCII character codes?
    // FIXME: Multiple characters?
    // FIXME: Key codes?
    event.message = [[theEvent characters] characterAtIndex:0];

    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(keyUp): %d key:%c\n", acceptedEvent, (char) (event.message & charCodeMask));
}

- (void)keyDown:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyDown;
    // FIXME: Unicode characters vs. Macintosh ASCII character codes?
    // FIXME: Multiple characters?
    // FIXME: Key codes?
    event.message = [[theEvent characters] characterAtIndex:0];
    
    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(keyDown): %d key:%c\n", acceptedEvent, (char) (event.message & charCodeMask));
}

#pragma mark IFPLUGINVIEW

// Could do this as a category on NSString if we wanted.
static char *newCString(NSString *string)
{
    char *cString = new char[[string cStringLength] + 1];
    [string getCString:cString];
    return cString;
}

- (id)initWithFrame:(NSRect)r plugin:(IFPlugin *)plug url:(NSString *)location mime:(NSString *)mimeType arguments:(NSDictionary *)arguments mode:(uint16)mode
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
    canRestart = YES;
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

-(void)start
{
    NPSavedData saved;
    NPError npErr;
    NSNotificationCenter *notificationCenter;
    NSWindow *theWindow;
    
    if(isStarted || !canRestart)
        return;
    
    isStarted = YES;
    
    npErr = NPP_New((char *)[mime cString], instance, fullMode ? NP_FULL : NP_EMBED, argsCount, cAttributes, cValues, &saved);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_New: %d\n", npErr);
    
    // Create a WindowRef is one doesn't already exist
    [[self window] _windowRef];
        
    [self setWindow];
    
    theWindow = [self window];
    notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(viewHasMoved:) name:@"NSViewBoundsDidChangeNotification" object:[self findSuperview:@"NSClipView"]];
    [notificationCenter addObserver:self selector:@selector(viewHasMoved:) name:@"NSWindowDidResizeNotification" object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowWillClose:) name:@"NSWindowWillCloseNotification" object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowBecameKey:) name:@"NSWindowDidBecomeKeyNotification" object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowResignedKey:) name:@"NSWindowDidResignKeyNotification" object:theWindow];
    [notificationCenter addObserver:self selector:@selector(defaultsHaveChanged:) name:@"NSUserDefaultsDidChangeNotification" object:nil];
    
    if ([theWindow isKeyWindow])
        [self sendActivateEvent:YES];
    if(URL)
        [self newStream:[NSURL URLWithString:URL] mimeType:mime notifyData:NULL];
    eventSender = [[IFPluginNullEventSender alloc] initializeWithNPP:instance functionPointer:NPP_HandleEvent];
    [eventSender sendNullEvents];
    trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
    
    id webView = [self findSuperview:@"IFWebView"];
    webController = [webView controller];
    webDataSource = [[webController frameForView:webView] dataSource];
}

- (void)stop
{
    NPError npErr;
    
    if (!isStarted)
        return;
        
    isStarted = NO;
        
    [activeURLHandles makeObjectsPerformSelector:@selector(cancelLoadInBackground)];
    [eventSender stop];
    [eventSender release];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self removeTrackingRect:trackingTag];
    npErr = NPP_Destroy(instance, NULL);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_Destroy: %d\n", npErr);
}

#pragma mark NSVIEW

- (void)drawRect:(NSRect)rect
{
    if(!isStarted)
        [self start];
    if(isStarted)
        [self sendUpdateEvent];
}

- (BOOL)isFlipped
{
    return YES;
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

- (void) defaultsHaveChanged:(NSNotification *)notification
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if([defaults boolForKey:@"WebKitPluginsEnabled"]){
        canRestart = YES;
        [self start];
    }else{
        canRestart = NO;
        [self stop];
        [self setNeedsDisplay:YES];
    }
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
    
    IFError *error = [[IFError alloc] initWithErrorCode: result  inDomain:IFErrorCodeDomainWebFoundation failingURL: [sender url]];
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
