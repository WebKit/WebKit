/*	
        WebBaseNetscapePluginView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginStream.h>
#import <WebKit/WebNetscapePluginNullEventSender.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebView.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebHTTPResourceRequest.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>

#import <AppKit/NSEvent_Private.h>

#import <Carbon/Carbon.h>

@implementation WebBaseNetscapePluginView

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

- (EventModifiers)modifiersForEvent:(NSEvent *)event
{
    EventModifiers modifiers;
    unsigned int modifierFlags = [event modifierFlags];
    NSEventType eventType = [event type];
    
    modifiers = 0;
    
    if (eventType != NSLeftMouseDown && eventType != NSRightMouseDown)
        modifiers |= btnState;
    
    if (modifierFlags & NSCommandKeyMask)
        modifiers |= cmdKey;
    
    if (modifierFlags & NSShiftKeyMask)
        modifiers |= shiftKey;

    if (modifierFlags & NSAlphaShiftKeyMask)
        modifiers |= alphaLock;

    if (modifierFlags & NSAlternateKeyMask)
        modifiers |= optionKey;

    if (modifierFlags & NSControlKeyMask || eventType == NSRightMouseDown)
        modifiers |= controlKey;
    
    return modifiers;
}

- (void)getCarbonEvent:(EventRecord *)carbonEvent withEvent:(NSEvent *)cocoaEvent
{
    if ([cocoaEvent _eventRef] && ConvertEventRefToEventRecord([cocoaEvent _eventRef], carbonEvent)) {
        return;
    }
    
    NSPoint where = [[cocoaEvent window] convertBaseToScreen:[cocoaEvent locationInWindow]];
        
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = (UInt32)([cocoaEvent timestamp] * 60); // seconds to ticks
    carbonEvent->where.h = (short)where.x;
    carbonEvent->where.v = (short)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - where.y);
    carbonEvent->modifiers = [self modifiersForEvent:cocoaEvent];
}

- (UInt32)keyMessageForEvent:(NSEvent *)event
{
    NSData *data = [[event characters] dataUsingEncoding:CFStringConvertEncodingToNSStringEncoding(CFStringGetSystemEncoding())];
    if (!data) {
        return 0;
    }
    UInt8 characterCode;
    [data getBytes:&characterCode length:1];
    UInt16 keyCode = [event keyCode];
    return keyCode << 8 | characterCode;
}

- (BOOL)sendEvent:(EventRecord *)event
{
    BOOL defers = [[self controller] _defersCallbacks];
    if (!defers) {
        [[self controller] _setDefersCallbacks:YES];
    }

    BOOL acceptedEvent = NO;
    if (NPP_HandleEvent) {
        acceptedEvent = NPP_HandleEvent(instance, event);
    }

    if (!defers) {
        [[self controller] _setDefersCallbacks:NO];
    }
    
    return acceptedEvent;
}

- (void)sendActivateEvent:(BOOL)activate
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = activateEvt;
    WindowRef windowRef = [[self window] windowRef];
    event.message = (UInt32)windowRef;
    if (activate)
        event.modifiers |= activeFlag;
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(activateEvent): %d  isActive: %d", acceptedEvent, activate);
}

- (BOOL)sendUpdateEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = updateEvt;
    WindowRef windowRef = [[self window] windowRef];
    event.message = (UInt32)windowRef;

    BOOL acceptedEvent = [self sendEvent:&event];

    LOG(Plugins, "NPP_HandleEvent(updateEvt): %d", acceptedEvent);

    return acceptedEvent;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = getFocusEvent;
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(getFocusEvent): %d", acceptedEvent);
    return YES;
}

- (BOOL)resignFirstResponder
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = loseFocusEvent;
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(loseFocusEvent): %d", acceptedEvent);
    return YES;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    EventRecord event;

    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseDown;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(mouseDown): %d pt.v=%d, pt.h=%d", acceptedEvent, event.where.v, event.where.h);
}

- (void)mouseUp:(NSEvent *)theEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseUp;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d", acceptedEvent, event.where.v, event.where.h);
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(mouseEntered): %d", acceptedEvent);
}

- (void)mouseExited:(NSEvent *)theEvent
{
    EventRecord event;
        
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(mouseExited): %d", acceptedEvent);
    
    // Set cursor back to arrow cursor.
    [[NSCursor arrowCursor] set];
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    // Do nothing so that other responders don't respond to the drag that initiated in this view.
}

- (void)keyUp:(NSEvent *)theEvent
{
    EventRecord event;

    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyUp;

    if (event.message == 0) {
        event.message = [self keyMessageForEvent:theEvent];
    }
    
    BOOL acceptedEvent = [self sendEvent:&event];

    LOG(Plugins, "NPP_HandleEvent(keyUp): %d charCode:%c keyCode:%lu",
                     acceptedEvent, (char) (event.message & charCodeMask), (event.message & keyCodeMask));
    
    // If the plug-in didn't accept this event,
    // pass it along so that keyboard scrolling, for example, will work.
    if (!acceptedEvent){
        [super keyUp:theEvent];
    }
}

- (void)keyDown:(NSEvent *)theEvent
{
    EventRecord event;

    // Some command keys are sent with both performKeyEquivalent and keyDown.
    // We should send only 1 keyDown to the plug-in, so we'll ignore this one.
    if([theEvent modifierFlags] & NSCommandKeyMask){
        return;
    }
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyDown;

    if (event.message == 0) {
        event.message = [self keyMessageForEvent:theEvent];
    }
    
    BOOL acceptedEvent = [self sendEvent:&event];

    LOG(Plugins, "NPP_HandleEvent(keyDown): %d charCode:%c keyCode:%lu",
                     acceptedEvent, (char) (event.message & charCodeMask), (event.message & keyCodeMask));
    
    // If the plug-in didn't accept this event,
    // pass it along so that keyboard scrolling, for example, will work.
    if (!acceptedEvent){
        [super keyDown:theEvent];
    }
}

- (BOOL)isInResponderChain
{
    NSResponder *responder = [[self window] firstResponder];

    while(responder != nil){
        if(responder == self){
            return YES;
        }
        responder = [responder nextResponder];
    }
    return NO;
}

// Stop overriding performKeyEquivalent because the gain is not worth the frustation.
// Need to find a better way to pass command-modified keys to plug-ins. 3080103
#if 0
// Must subclass performKeyEquivalent: for command-modified keys to work.
- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
    EventRecord event;

    if(![self isInResponderChain]){
        return NO;
    }
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyDown;

    if (event.message == 0) {
        event.message = [self keyMessageForEvent:theEvent];
    }

    BOOL acceptedEvent = [self sendEvent:&event];

    LOG(Plugins, "NPP_HandleEvent(performKeyEquivalent): %d charCode:%c keyCode:%lu",
                     acceptedEvent, (char) (event.message & charCodeMask), (event.message & keyCodeMask));
    
    return acceptedEvent;
}
#endif

#pragma mark WEB_NETSCAPE_PLUGIN

- (void)setUpWindowAndPort
{
    CGrafPtr port = GetWindowPort([[self window] windowRef]);
    NSRect contentViewFrame = [[[self window] contentView] frame];
    NSRect boundsInWindow = [self convertRect:[self bounds] toView:nil];
    NSRect visibleRectInWindow = [self convertRect:[self visibleRect] toView:nil];
    
    // flip Y coordinates
    boundsInWindow.origin.y = contentViewFrame.size.height - boundsInWindow.origin.y - boundsInWindow.size.height; 
    visibleRectInWindow.origin.y = contentViewFrame.size.height - visibleRectInWindow.origin.y - visibleRectInWindow.size.height;
    
    nPort.port = port;
    
    // FIXME: Are these values correct? Without them, Flash freaks.
    nPort.portx = (int32)-boundsInWindow.origin.x;
    nPort.porty = (int32)-boundsInWindow.origin.y;
    
    window.window = &nPort;
    
    window.x = (int32)boundsInWindow.origin.x; 
    window.y = (int32)boundsInWindow.origin.y;

    window.width = (uint32)boundsInWindow.size.width;
    window.height = (uint32)boundsInWindow.size.height;

    window.clipRect.top = (uint16)visibleRectInWindow.origin.y;
    window.clipRect.left = (uint16)visibleRectInWindow.origin.x;
    window.clipRect.bottom = (uint16)(visibleRectInWindow.origin.y + visibleRectInWindow.size.height);
    window.clipRect.right = (uint16)(visibleRectInWindow.origin.x + visibleRectInWindow.size.width);
    
    window.type = NPWindowTypeWindow;
}

- (void)setWindow
{
    [self setUpWindowAndPort];

    NPError npErr;
    npErr = NPP_SetWindow(instance, &window);
    LOG(Plugins, "NPP_SetWindow: %d, port=0x%08x, window.x:%d window.y:%d",
                     npErr, (int)nPort.port, (int)window.x, (int)window.y);

#if 0
    // Draw test    
    Rect portRect;
    GetPortBounds(nPort.port, &portRect);
    SetPort(nPort.port);
    MoveTo(0,0);
    LineTo(portRect.right, portRect.bottom);
#endif
}

- (void)removeTrackingRect
{
    if (trackingTag) {
        [self removeTrackingRect:trackingTag];
        // Must release the window to balance the retain in resetTrackingRect.
        [[self window] release];
        trackingTag = 0;
    }
}

- (void)resetTrackingRect
{
    [self removeTrackingRect];
    if (isStarted) {
        // Must retain the window so that removeTrackingRect can work after the window is closed.
        [[self window] retain];
        trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
    }
}

-(void)start
{
    if (isStarted || !canRestart || NPP_New == 0){
        return;
    }
    
    isStarted = YES;
    
    NPError npErr;
    npErr = NPP_New((char *)[MIMEType cString], instance, mode, argsCount, cAttributes, cValues, NULL);
    LOG(Plugins, "NPP_New: %d", npErr);
    
    // Create a WindowRef is one doesn't already exist
    [[self window] windowRef];
        
    [self setWindow];
    
    NSWindow *theWindow = [self window];
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    NSView *view;
    for (view = self; view; view = [view superview]) {
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:) 
            name:NSViewFrameDidChangeNotification object:view];
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:) 
            name:NSViewBoundsDidChangeNotification object:view];
    }
    [notificationCenter addObserver:self selector:@selector(windowWillClose:)
        name:NSWindowWillCloseNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowBecameKey:) 
        name:NSWindowDidBecomeKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowResignedKey:) 
        name:NSWindowDidResignKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(defaultsHaveChanged:) 
        name:NSUserDefaultsDidChangeNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(windowDidMiniaturize:)
        name:NSWindowDidMiniaturizeNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidDeminiaturize:)
        name:NSWindowDidDeminiaturizeNotification object:theWindow];

    if ([theWindow isKeyWindow]) {
        [self sendActivateEvent:YES];
    }
    
    eventSender = [[WebNetscapePluginNullEventSender alloc] initWithPluginView:self];
    if (![theWindow isMiniaturized]) {
        [eventSender sendNullEvents];
    }
    [self resetTrackingRect];
}

- (void)stop
{
    [self removeTrackingRect];

    if (!isStarted){
        return;
    }
    
    isStarted = NO;

    // Stop any active streams
    [streams makeObjectsPerformSelector:@selector(stop)];
    
    // Stop the null events
    [eventSender stop];
    [eventSender release];

    // Set cursor back to arrow cursor
    [[NSCursor arrowCursor] set];
    
    // Stop notifications
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    NPError npErr;
    npErr = NPP_Destroy(instance, NULL);
    LOG(Plugins, "NPP_Destroy: %d", npErr);
}

- (WebDataSource *)dataSource
{
    // Do nothing. Overridden by subclasses.
    return nil;
}

- (WebFrame *)webFrame
{
    return [[self dataSource] webFrame];
}

- (WebController *)controller
{
    return [[self webFrame] controller];
}

- (NPP)pluginPointer
{
    return instance;
}

- (WebNetscapePluginPackage *)plugin
{
    return plugin;
}

- (void)setPlugin:(WebNetscapePluginPackage *)thePlugin;
{
    [plugin release];
    plugin = [thePlugin retain];

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
}

- (void)setMIMEType:(NSString *)theMIMEType
{
    [MIMEType release];
    MIMEType = [theMIMEType retain];
}

- (void)setBaseURL:(NSURL *)theBaseURL
{
    [baseURL release];
    baseURL = [theBaseURL retain];
}

- (void)setAttributes:(NSDictionary *)attributes
{
    LOG(Plugins, "%@", attributes);

    // Convert arguments dictionary to 2 string arrays.
    // These arrays are passed to NPP_New, but the strings need to be
    // modifiable and live the entire life of the plugin.

    // The Java plug-in requires the first argument to be the base URL
    if ([MIMEType isEqualToString:@"application/x-java-applet"]) {
        cAttributes = (char **)malloc(([attributes count] + 1) * sizeof(char *));
        cValues = (char **)malloc(([attributes count] + 1) * sizeof(char *));
        cAttributes[0] = strdup("DOCBASE");
        cValues[0] = strdup([[baseURL absoluteString] UTF8String]);
        argsCount++;
    } else {
        cAttributes = (char **)malloc([attributes count] * sizeof(char *));
        cValues = (char **)malloc([attributes count] * sizeof(char *));
    }

    NSEnumerator *e = [attributes keyEnumerator];
    NSString *key;
    while ((key = [e nextObject])) {
        cAttributes[argsCount] = strdup([key UTF8String]);
        cValues[argsCount] = strdup([[attributes objectForKey:key] UTF8String]);
        argsCount++;
    }
}

- (void)setMode:(int)theMode
{
    mode = theMode;
}

#pragma mark NSVIEW

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];

    instance = &instanceStruct;
    instance->ndata = self;

    canRestart = YES;

    streams = [[NSMutableArray alloc] init];
    streamNotifications = [[NSMutableDictionary alloc] init];

    return self;
}

-(void)dealloc
{
    unsigned i;

    [self stop];

    for (i = 0; i < argsCount; i++) {
        free(cAttributes[i]);
        free(cValues[i]);
    }
    [streams removeAllObjects];
    [streams release];
    [MIMEType release];
    [baseURL release];
    [streamNotifications removeAllObjects];
    [streamNotifications release];
    free(cAttributes);
    free(cValues);
    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    if (!isStarted) {
        return;
    }
    
    if([NSGraphicsContext currentContextDrawingToScreen]){
        // AppKit tried to help us by doing a BeginUpdate.
        // But the invalid region at that level didn't include AppKit's notion of what was not valid.
        // We reset the port's visible region to counteract what BeginUpdate did.
        RgnHandle savedVisibleRegion = NewRgn();
        GetPortVisibleRegion(nPort.port, savedVisibleRegion);
        Rect portBounds;
        GetPortBounds(nPort.port, &portBounds);
        RgnHandle portBoundsAsRegion = NewRgn();
        RectRgn(portBoundsAsRegion, &portBounds);
        SetPortVisibleRegion(nPort.port, portBoundsAsRegion);
        DisposeRgn(portBoundsAsRegion);
        
        [self sendUpdateEvent];

        SetPortVisibleRegion(nPort.port, savedVisibleRegion);
        DisposeRgn(savedVisibleRegion);
    }else{
        // Printing

        PixMapHandle srcMap, dstMap;
        Rect rect;

        dstMap = NewPixMap();
        srcMap = GetPortPixMap(nPort.port);
        SetRect(&rect, window.x, window.y, window.x + window.width, window.y + window.height);
        CopyBits((BitMap *)srcMap, (BitMap *)dstMap, &rect, &rect, srcCopy, NULL);

        NSLog(@"%d", QDError());
    }
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    // We must remove the tracking rect before we move to the new window.
    // Once we move to the new window, it will be too late.
    [self removeTrackingRect];
    [super viewWillMoveToWindow:newWindow];
}

- (void)viewDidMoveToWindow
{
    if (![self window]) {
        [self stop];
    }
    
    [self resetTrackingRect];
}

#pragma mark NOTIFICATIONS

-(void)viewHasMoved:(NSNotification *)notification
{
    [self setUpWindowAndPort];
    [self resetTrackingRect];
}

-(void)windowWillClose:(NSNotification *)notification
{
    [self stop];
}

-(void)windowBecameKey:(NSNotification *)notification
{
    [self sendActivateEvent:YES];
}

-(void)windowResignedKey:(NSNotification *)notification
{
    [self sendActivateEvent:NO];
}

-(void)windowDidMiniaturize:(NSNotification *)notification
{
    [eventSender stop];
}

-(void)windowDidDeminiaturize:(NSNotification *)notification
{
    [eventSender sendNullEvents];
}

- (void)defaultsHaveChanged:(NSNotification *)notification
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:@"WebKitPluginsEnabled"]) {
        canRestart = YES;
        [self start];
    } else {
        canRestart = NO;
        [self stop];
        [self setNeedsDisplay:YES];
    }
}

- (void)frameStateChanged:(NSNotification *)notification
{
    WebFrame *frame = [notification object];
    NSURL *URL = [[[frame dataSource] request] URL];
    NSValue *notifyDataValue = [streamNotifications objectForKey:URL];
    
    if(!notifyDataValue){
        return;
    }
    
    void *notifyData = [notifyDataValue pointerValue];
    WebFrameState frameState = [[[notification userInfo] objectForKey:WebCurrentFrameState] intValue];
    if (frameState == WebFrameStateComplete) {
        NPP_URLNotify(instance, [[URL absoluteString] cString], NPRES_DONE, notifyData);
        [streamNotifications removeObjectForKey:URL];
    }
    //FIXME: Need to send other NPReasons
}

@end

@implementation WebBaseNetscapePluginView (WebNPPCallbacks)

- (NSURL *)pluginURLFromCString:(const char *)URLCString
{
    NSString *URLString;
    NSURL *URL;

    URLString = [NSString stringWithCString:URLCString];

    if ([URLString _web_looksLikeAbsoluteURL]) {
        URL = [NSURL _web_URLWithString:URLString];
    }
    else {
        URL = [NSURL _web_URLWithString:URLString relativeToURL:baseURL];
    }
    
    return URL;
}

- (NPError)loadRequest:(WebResourceRequest *)request inTarget:(NSString *)target withNotifyData:(void *)notifyData
{
    NSURL *URL = [request URL];

    if(!URL){
        return NPERR_INVALID_URL;
    }
    
    if(!target){
        WebNetscapePluginStream *stream = [[WebNetscapePluginStream alloc] initWithRequest:request
                                                                             pluginPointer:instance
                                                                                notifyData:notifyData];
        if(stream){
            [streams addObject:stream];
            [stream start];
            [stream release];
        }else{
            return NPERR_INVALID_URL;
        }
    }else{
        WebDataSource *dataSource = [[WebDataSource alloc] initWithRequest:request];

        if(dataSource){
            WebFrame *frame = [[self webFrame] findOrCreateFramedNamed:target];
            if ([frame setProvisionalDataSource:dataSource]) {
                if(notifyData){
                    if(![target isEqualToString:@"_self"] && ![target isEqualToString:@"_current"] &&
                       ![target isEqualToString:@"_parent"] && ![target isEqualToString:@"_top"]){

                        [streamNotifications setObject:[NSValue valueWithPointer:notifyData] forKey:URL];
                        [[NSNotificationCenter defaultCenter] addObserver:self
                            selector:@selector(frameStateChanged:) name:WebFrameStateChangedNotification object:frame];
                    }
                }
                
                [frame startLoading];
            }
            [dataSource release];
        }
    }
    
    return NPERR_NO_ERROR;
}

-(NPError)getURLNotify:(const char *)URL target:(const char *)target notifyData:(void *)notifyData
{
    NSString *theTarget = nil;
    NSURL *pluginURL;
    WebResourceRequest *request;
        
    LOG(Plugins, "NPN_GetURLNotify: %s target: %s", URL, target);
        
    if(!URL)
        return NPERR_INVALID_URL;
        
    if(target)
        theTarget = [NSString stringWithCString:target];

    pluginURL = [self pluginURLFromCString:URL]; 

    if(!pluginURL)
        return NPERR_INVALID_URL;
        
    request = [[[WebResourceRequest alloc] initWithURL:pluginURL] autorelease];
    
    return [self loadRequest:request inTarget:theTarget withNotifyData:notifyData];
}

-(NPError)getURL:(const char *)URL target:(const char *)target
{
    NSString *theTarget = nil;
    NSURL *pluginURL;
    WebResourceRequest *request;
    
    LOG(Plugins, "NPN_GetURL: %s target: %s", URL, target);
    
    if(!URL)
        return NPERR_INVALID_URL;
        
    if(target)
        theTarget = [NSString stringWithCString:target];

    pluginURL = [self pluginURLFromCString:URL]; 

    if(!pluginURL)
        return NPERR_INVALID_URL;
        
    request = [[[WebResourceRequest alloc] initWithURL:pluginURL] autorelease];
    
    return [self loadRequest:request inTarget:theTarget withNotifyData:NULL];
}

-(NPError)postURLNotify:(const char *)URL target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData
{
    NSData *postData;
    NSURL *tempURL;
    NSString *path, *theTarget = nil;
    NSURL *pluginURL;
    WebResourceRequest *request;
    
    LOG(Plugins, "NPN_PostURLNotify: %s", URL);
 
    if(!URL)
        return NPERR_INVALID_URL;
        
    if(target)
        theTarget = [NSString stringWithCString:target];
 
    if(file){
        if([[NSString stringWithCString:buf] _web_looksLikeAbsoluteURL]){
            tempURL = [NSURL fileURLWithPath:[NSString stringWithCString:URL]];
            path = [tempURL path];
        }else{
            path = [NSString stringWithCString:buf];
        }
        postData = [NSData dataWithContentsOfFile:path];
    }else{
        postData = [NSData dataWithBytes:buf length:len];
    }

    pluginURL = [self pluginURLFromCString:URL]; 

    if(!pluginURL)
        return NPERR_INVALID_URL;
        
    request = [[[WebResourceRequest alloc] initWithURL:pluginURL] autorelease];
    [request setMethod:@"POST"];
    [request setData:postData];
    
    return [self loadRequest:request inTarget:theTarget withNotifyData:notifyData];
}

-(NPError)postURL:(const char *)URL target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file
{
    NSString *theTarget = nil;
        
    LOG(Plugins, "NPN_PostURL: %s", URL);
    
    if(!URL)
        return NPERR_INVALID_URL;
        
    if(target)
        theTarget = [NSString stringWithCString:target];
        
    return [self postURLNotify:URL target:target len:len buf:buf file:file notifyData:NULL];
}

-(NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream
{
    LOG(Plugins, "NPN_NewStream");
    return NPERR_GENERIC_ERROR;
}

-(NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer
{
    LOG(Plugins, "NPN_Write");
    return NPERR_GENERIC_ERROR;
}

-(NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason
{
    LOG(Plugins, "NPN_DestroyStream");
    if(!stream->ndata)
        return NPERR_INVALID_INSTANCE_ERROR;
        
    [(WebNetscapePluginStream *)stream->ndata stop];
    return NPERR_NO_ERROR;
}

-(void)status:(const char *)message
{
    LOG(Plugins, "NPN_Status: %s", message);
    if([self controller]){
        [[[self controller] windowOperationsDelegate] setStatusText:[NSString stringWithCString:message]];
    }
}

-(void)invalidateRect:(NPRect *)invalidRect
{
    LOG(Plugins, "NPN_InvalidateRect");
    [self sendUpdateEvent];
}

-(void)invalidateRegion:(NPRegion)invalidateRegion
{
    LOG(Plugins, "NPN_InvalidateRegion");
    [self sendUpdateEvent];
}

-(void)forceRedraw
{
    LOG(Plugins, "forceRedraw");
    [self sendUpdateEvent];
}

@end
