/*	
    IFPluginView.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#define USE_CARBON 1

#import <WebKit/IFPluginView.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebView.h>
#import <WebKit/IFPluginDatabase.h>
#import <WebKit/IFPluginStream.h>
#import <WebKit/IFPluginNullEventSender.h>
#import <WebKit/IFNullPluginView.h>
#import <WebKit/IFPlugin.h>
#import <WebKit/IFNSViewExtras.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/IFError.h>
#import <WebFoundation/IFNSStringExtensions.h>
#import <WebFoundation/IFNSURLExtensions.h>

#import <AppKit/NSWindow_Private.h>
#import <Carbon/Carbon.h>

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
    NSPoint where;
    
    where = [[cocoaEvent window] convertBaseToScreen:[cocoaEvent locationInWindow]];
    
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = (UInt32)([cocoaEvent timestamp] * 60); // seconds to ticks
    carbonEvent->where.h = (short)where.x;
    carbonEvent->where.v = (short)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - where.y);
    carbonEvent->modifiers = [self modifiersForEvent:cocoaEvent];    
}

- (UInt32)keyMessageForEvent:(NSEvent *)theEvent
{
    NSData *data;
    UInt8 characterCode;
    UInt16 keyCode;
    UInt32 message=0;

    data = [[theEvent characters] dataUsingEncoding:CFStringConvertEncodingToNSStringEncoding(CFStringGetSystemEncoding())];
    if(data){
        [data getBytes:&characterCode length:1];
        keyCode = [theEvent keyCode];
        message = keyCode << 8 | characterCode;
    }
    return message;
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
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(updateEvt): %d\n", acceptedEvent);
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
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(getFocusEvent): %d\n", acceptedEvent);
    return YES;
}

- (BOOL)resignFirstResponder
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event];
    event.what = loseFocusEvent;
    
    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(loseFocusEvent): %d\n", acceptedEvent);
    return YES;
}


-(void)mouseDown:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseDown;
    
    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseDown): %d pt.v=%d, pt.h=%d\n", acceptedEvent, event.where.v, event.where.h);
}

-(void)mouseUp:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseUp;

    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d\n", acceptedEvent, event.where.v, event.where.h);
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
    
    // Set cursor back to arrow cursor.
    [[NSCursor arrowCursor] set];
}

- (void)keyUp:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;

    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyUp;
    event.message = [self keyMessageForEvent:theEvent];
    
    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(keyUp): %d key:%c\n", acceptedEvent, (char) (event.message & charCodeMask));
    
    // If the plug-in didn't accept this event,
    // pass it along so that keyboard scrolling, for example, will work.
    if (!acceptedEvent)
        [super keyUp:theEvent];
}

- (void)keyDown:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyDown;
    event.message = [self keyMessageForEvent:theEvent];
    
    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(keyDown): %d key:%c\n", acceptedEvent, (char) (event.message & charCodeMask));
    
    // If the plug-in didn't accept this event,
    // pass it along so that keyboard scrolling, for example, will work.
    if (!acceptedEvent)
        [super keyDown:theEvent];
}

#pragma mark IFPLUGINVIEW

// Could do this as a category on NSString if we wanted.
static char *newCString(NSString *string)
{
    char *cString = new char[[string cStringLength] + 1];
    [string getCString:cString];
    return cString;
}

- (id)initWithFrame:(NSRect)r plugin:(IFPlugin *)plugin url:(NSURL *)theURL baseURL:(NSURL *)theBaseURL mime:(NSString *)mimeType arguments:(NSDictionary *)arguments
{
    [super initWithFrame: r];
    
    instance = &instanceStruct;
    instance->ndata = self;

    mime = [mimeType retain];
    srcURL = [theURL retain];
    baseURL = [theBaseURL retain];
        
    // load the plug-in if it is not already loaded
    if(![plugin load])
        return nil;
    
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

    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "%s", [[arguments description] cString]);

    // Convert arguments dictionary to 2 string arrays.
    // These arrays are passed to NPP_New, but the strings need to be
    // modifiable and live the entire life of the plugin.
    
    argsCount = 0;
    
    // The Java plug-in requires the first argument to be the base URL
    if([mime isEqualToString:@"application/x-java-applet"]){
        cAttributes = new char * [[arguments count]+1];
        cValues = new char * [[arguments count]+1]; 
        cAttributes[0] = newCString(@"DOCBASE");
        cValues[0] = newCString([baseURL absoluteString]);
        argsCount++;
    }else{
        cAttributes = new char * [[arguments count]];
        cValues = new char * [[arguments count]];
    }
        
    NSEnumerator *e = [arguments keyEnumerator];
    NSString *key;
    while ((key = [e nextObject])) {
        cAttributes[argsCount] = newCString(key);
        cValues[argsCount] = newCString([arguments objectForKey:key]);
        argsCount++;
    }
    
    streams = [[NSMutableArray alloc] init];
    notificationData = [[NSMutableDictionary alloc] init];
    
    // Initialize globals
    canRestart = YES;
    isStarted = NO;
    fullMode = NO;
    
    return self;
}

-(void)dealloc
{
    unsigned i;

    [self stop];
    
    for (i = 0; i < argsCount; i++) {
        delete [] cAttributes[i];
        delete [] cValues[i];
    }
    [streams removeAllObjects];
    [streams release];
    [mime release];
    [srcURL release];
    [baseURL release];
    [notificationData release];
    delete [] cAttributes;
    delete [] cValues;
    [super dealloc];
}

- (void)setUpWindowAndPort
{
    CGrafPtr port = GetWindowPort([[self window] _windowRef]);
    NSRect windowFrame = [[self window] frame];
    NSRect contentViewFrame = [[[self window] contentView] frame];
    NSRect boundsInWindow = [self convertRect:[self bounds] toView:nil];
    NSRect visibleRectInWindow = [self convertRect:[self visibleRect] toView:nil];
    
    // flip Y coordinates
    boundsInWindow.origin.y = contentViewFrame.size.height - boundsInWindow.origin.y - boundsInWindow.size.height; 
    visibleRectInWindow.origin.y = contentViewFrame.size.height - visibleRectInWindow.origin.y - visibleRectInWindow.size.height;
    
    nPort.port = port;
    
    // FIXME: Are these values correct? Without them, Flash freaks.
    nPort.portx = -(int32)boundsInWindow.origin.x;
    nPort.porty = -(int32)boundsInWindow.origin.y;
    
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

- (void) setWindow
{
    [self setUpWindowAndPort];

#ifndef NDEBUG
    NPError npErr =
#endif
    NPP_SetWindow(instance, &window);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_SetWindow: %d, port=0x%08x\n", npErr, (int)nPort.port);

#if 0
    // Draw test    
    Rect portRect;
    GetPortBounds(port, &portRect);
    SetPort(port);
    MoveTo(0,0);
    LineTo(portRect.right, portRect.bottom);
#endif
}

-(void)start
{
    NSNotificationCenter *notificationCenter;
    NSWindow *theWindow;
    IFPluginStream *stream;
        
    if(isStarted || !canRestart)
        return;
    
    isStarted = YES;
    
#ifndef NDEBUG
    NPError npErr =
#endif
    NPP_New((char *)[mime cString], instance, fullMode ? NP_FULL : NP_EMBED, argsCount, cAttributes, cValues, NULL);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_New: %d\n", npErr);
    
    // Create a WindowRef is one doesn't already exist
    [[self window] _windowRef];
        
    [self setWindow];
    
    theWindow = [self window];
    notificationCenter = [NSNotificationCenter defaultCenter];
    for (NSView *view = self; view; view = [view superview]) {
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:) 
            name:NSViewFrameDidChangeNotification object:view];
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:) 
            name:NSViewBoundsDidChangeNotification object:view];
    }
    [notificationCenter addObserver:self selector:@selector(windowBecameKey:) 
        name:NSWindowDidBecomeKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowResignedKey:) 
        name:NSWindowDidResignKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(defaultsHaveChanged:) 
        name:NSUserDefaultsDidChangeNotification object:nil];
    
    if ([theWindow isKeyWindow])
        [self sendActivateEvent:YES];
    
    IFWebView *webView = [self _IF_superviewWithName:@"IFWebView"];
    webController = [[webView controller] retain];
    webFrame = 	    [[webController frameForView:webView] retain];
    webDataSource = [[webFrame dataSource] retain];
    
    if(srcURL){
        stream = [[IFPluginStream alloc] initWithURL:srcURL pluginPointer:instance];
        if(stream){
            [stream startLoad];
            [streams addObject:stream];
            [stream release];
        }
    }
    
    eventSender = [[IFPluginNullEventSender alloc] initializeWithNPP:instance functionPointer:NPP_HandleEvent window:theWindow];
    [eventSender sendNullEvents];
    trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
}

- (void)stop
{
    if (!isStarted)
        return;
    isStarted = NO;

    [self removeTrackingRect:trackingTag];
    
    // Stop any active streams
    [streams makeObjectsPerformSelector:@selector(stop)];
    
    // Stop the null events
    [eventSender stop];
    [eventSender release];

    // Set cursor back to arrow cursor
    [[NSCursor arrowCursor] set];
    
    // Stop notifications
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    // Release web objects here to avoid circular retain
    [webController release];
    [webFrame release];
    [webDataSource release];
    
#ifndef NDEBUG
    NPError npErr =
#endif
    NPP_Destroy(instance, NULL);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_Destroy: %d\n", npErr);
}

- (IFWebDataSource *)webDataSource
{
    return webDataSource;
}

- (IFWebController *) webController
{
    return webController;
}

#pragma mark IFDOCUMENTVIEW

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    
    instance = &instanceStruct;
    instance->ndata = self;
    
    argsCount = 0;
    cAttributes = 0;
    cValues = 0;
    
    canRestart = YES;
    isStarted = NO;
    fullMode = YES;
    
    [self setFrame:NSMakeRect(0, 0, 1, 1)];
    
    return self;
}

- (void)provisionalDataSourceChanged:(IFWebDataSource *)dataSource
{
    IFPlugin *plugin;
    
    mime = [[dataSource contentType] retain];
    plugin = [[IFPluginDatabase installedPlugins] pluginForMimeType:mime];
    
    if(![plugin load])
        return;
    
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
    
    [self start];
}

- (void)provisionalDataSourceCommitted:(IFWebDataSource *)dataSource
{
    
}

- (void)dataSourceUpdated:(IFWebDataSource *)dataSource
{

}
 
- (void)layout
{
    NSRect superFrame = [[self _IF_superviewWithName:@"IFWebView"] frame];
    
    [self setFrame:NSMakeRect(0, 0, superFrame.size.width, superFrame.size.height)];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setUpWindowAndPort];
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

- (void)viewDidMoveToWindow
{
    if (![self window])
        [self stop];
    [super viewDidMoveToWindow];
}

#pragma mark NOTIFICATIONS

-(void)viewHasMoved:(NSNotification *)notification
{
    [self setUpWindowAndPort];

    // reset the tracking rect
    [self removeTrackingRect:trackingTag];
    trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
}

-(void) windowBecameKey:(NSNotification *)notification
{
    [self sendActivateEvent:YES];
    [self performSelector:@selector(sendUpdateEvent) withObject:nil afterDelay:.001];
}

-(void) windowResignedKey:(NSNotification *)notification
{
    [self sendActivateEvent:NO];
    [self performSelector:@selector(sendUpdateEvent) withObject:nil afterDelay:.001];
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

- (void) frameStateChanged:(NSNotification *)notification
{
    IFWebFrame *frame;
    IFWebFrameState frameState;
    NSValue *notifyDataValue;
    void *notifyData;
    NSURL *url;
    
    frame = [notification object];
    url = [[frame dataSource] inputURL];
    notifyDataValue = [notificationData objectForKey:url];
    
    if(!notifyDataValue)
        return;
    
    notifyData = [notifyDataValue pointerValue];
    frameState = (IFWebFrameState)[[[notification userInfo] objectForKey:IFCurrentFrameState] intValue];
    if(frameState == IFWEBFRAMESTATE_COMPLETE){
        NPP_URLNotify(instance, [[url absoluteString] cString], NPRES_DONE, notifyData);
    }
    //FIXME: Need to send other NPReasons
}

#pragma mark PLUGIN-TO-BROWSER

- (NPError) loadURL:(NSString *)URLString inTarget:(NSString *)target withNotifyData:(void *)notifyData andHandleAttributes:(NSDictionary *)attributes
{
    IFPluginStream *stream;
    IFWebDataSource *dataSource;
    IFWebFrame *frame;
    NSURL *url;
    
    if([URLString _IF_looksLikeAbsoluteURL])
        url = [NSURL _IF_URLWithString:URLString];
    else
        url = [NSURL _IF_URLWithString:URLString relativeToURL:baseURL];
    
    if(!url)
        return NPERR_INVALID_URL;
    
    if(!target){
        stream = [[IFPluginStream alloc] initWithURL:url pluginPointer:instance notifyData:notifyData attributes:attributes];
        if(stream){
            [stream startLoad];
            [streams addObject:stream];
            [stream release];
        }else{
            return NPERR_INVALID_URL;
        }
    }else{
        frame = [webFrame frameNamed:target];
        if(!frame){
            [[webController windowContext] openNewWindowWithURL:url];
            // FIXME: Need to send NPP_URLNotify at the right time.
            // FIXME: Need to name new frame
            if(notifyData)
                NPP_URLNotify(instance, [[url absoluteString] cString], NPRES_DONE, notifyData);
        }else{
            if(notifyData){
                if(![target isEqualToString:@"_self"] && ![target isEqualToString:@"_current"] && 
                    ![target isEqualToString:@"_parent"] && ![target isEqualToString:@"_top"]){
    
                    [notificationData setObject:[NSValue valueWithPointer:notifyData] forKey:url];
                    [[NSNotificationCenter defaultCenter] addObserver:self 
                        selector:@selector(frameStateChanged:) name:IFFrameStateChangedNotification object:frame];
                }
                // Plug-in docs say to return NPERR_INVALID_PARAM here
                // but IE allows an NPP_*URLNotify when the target is _self, _current, _parent or _top
                // so we have to allow this as well. Needed for iTools.
            }
            dataSource = [[[IFWebDataSource alloc] initWithURL:url attributes:attributes] autorelease];
            if([frame setProvisionalDataSource:dataSource])
                [frame startLoading];
        }
    }
    return NPERR_NO_ERROR;
}

-(NPError)getURLNotify:(const char *)url target:(const char *)target notifyData:(void *)notifyData
{
    NSString *theTarget = nil;
        
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_GetURLNotify: %s target: %s\n", url, target);
        
    if(!url)
        return NPERR_INVALID_URL;
        
    if(target)
        theTarget = [NSString stringWithCString:target];
    
    return [self loadURL:[NSString stringWithCString:url] inTarget:theTarget withNotifyData:notifyData andHandleAttributes:nil];
}

-(NPError)getURL:(const char *)url target:(const char *)target
{
    NSString *theTarget = nil;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_GetURL: %s target: %s\n", url, target);
    
    if(!url)
        return NPERR_INVALID_URL;
        
    if(target)
        theTarget = [NSString stringWithCString:target];
    
    return [self loadURL:[NSString stringWithCString:url] inTarget:theTarget withNotifyData:NULL andHandleAttributes:nil];
}

-(NPError)postURLNotify:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData
{
    NSDictionary *attributes=nil;
    NSData *postData;
    NSURL *tempURL;
    NSString *path, *theTarget = nil;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_PostURLNotify: %s\n", url);
 
    if(!url)
        return NPERR_INVALID_URL;
        
    if(target)
        theTarget = [NSString stringWithCString:target];
 
    if(file){
        if([[NSString stringWithCString:buf] _IF_looksLikeAbsoluteURL]){
            tempURL = [NSURL fileURLWithPath:[NSString stringWithCString:url]];
            path = [tempURL path];
        }else{
            path = [NSString stringWithCString:buf];
        }
        postData = [NSData dataWithContentsOfFile:path];
    }else{
        postData = [NSData dataWithBytes:buf length:len];
    }
    
    attributes = [NSDictionary dictionaryWithObjectsAndKeys:
        postData,	IFHTTPURLHandleRequestData,
        @"POST", 	IFHTTPURLHandleRequestMethod, nil];
                
    return [self loadURL:[NSString stringWithCString:url] inTarget:theTarget 
                withNotifyData:notifyData andHandleAttributes:attributes];
}

-(NPError)postURL:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file
{
    NSString *theTarget = nil;
        
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_PostURL: %s\n", url);
    
    if(!url)
        return NPERR_INVALID_URL;
        
    if(target)
        theTarget = [NSString stringWithCString:target];
        
    return [self postURLNotify:url target:target len:len buf:buf file:file notifyData:NULL];
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
    if(!stream->ndata)
        return NPERR_INVALID_INSTANCE_ERROR;
        
    [(IFPluginStream *)stream->ndata stop];
    return NPERR_NO_ERROR;
}

-(void)status:(const char *)message
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_Status: %s\n", message);
    if(webController){
        [[webController windowContext] setStatusText:[NSString stringWithCString:message]];
    }
}

-(void)invalidateRect:(NPRect *)invalidRect
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_InvalidateRect\n");
    [self sendUpdateEvent];
}

-(void)invalidateRegion:(NPRegion)invalidateRegion
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_InvalidateRegion\n");
    [self sendUpdateEvent];
}

-(void)forceRedraw
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "forceRedraw\n");
    [self sendUpdateEvent];
}

- (NPP)pluginInstance
{
    return instance;
}

- (NPP_NewStreamProcPtr)NPP_NewStream
{
    return NPP_NewStream;
}

- (NPP_WriteReadyProcPtr)NPP_WriteReady
{
    return NPP_WriteReady;
}


- (NPP_WriteProcPtr)NPP_Write
{
    return NPP_Write;
}


- (NPP_StreamAsFileProcPtr)NPP_StreamAsFile
{
    return NPP_StreamAsFile;
}


- (NPP_DestroyStreamProcPtr)NPP_DestroyStream
{
    return NPP_DestroyStream;
}


- (NPP_URLNotifyProcPtr)NPP_URLNotify
{
    return NPP_URLNotify;
}

@end
