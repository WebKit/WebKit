/*	
    IFPluginView.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#define USE_CARBON 1

#import "IFPluginView.h"

#import <WebKit/IFWebController.h>
#import <WebKit/IFWebFrame.h>

#import <AppKit/NSWindow_Private.h>
#import <Carbon/Carbon.h>

#import <IFPluginDatabase.h>
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
    
    [self getCarbonEvent:&event withEvent:theEvent isMouseDown:NO];
    event.what = mouseUp;

    acceptedEvent = NPP_HandleEvent(instance, &event);
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d ticks=%lu\n", acceptedEvent, event.where.v, event.where.h, event.when);
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

- (id)initWithFrame:(NSRect)r plugin:(IFPlugin *)plug url:(NSURL *)theURL mime:(NSString *)mimeType arguments:(NSDictionary *)arguments mode:(uint16)mode
{
    NSString *baseURLString;

    [super initWithFrame: r];
    
    // The following line doesn't work for Flash, so I have create a NPP_t struct and point to it
    //instance = malloc(sizeof(NPP_t));
    instance = &instanceStruct;
    instance->ndata = self;

    mime = [mimeType retain];
    srcURL = [theURL retain];
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
    
    streams = [[NSMutableArray alloc] init];
    
    // Initialize globals
    canRestart = YES;
    isStarted = NO;
    
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
    
    // FIXME: Are these values correct? Without them, Flash freaks.
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
    IFPluginStream *stream;
        
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

    if(srcURL){
        stream = [[IFPluginStream alloc] initWithURL:srcURL pluginPointer:instance];
        if(stream){
            [streams addObject:stream];
            [stream release];
        }
    }
    
    eventSender = [[IFPluginNullEventSender alloc] initializeWithNPP:instance functionPointer:NPP_HandleEvent];
    [eventSender sendNullEvents];
    trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
    
    id webView = [self findSuperview:@"IFWebView"];
    webController = [[webView controller] retain];
    webFrame = 	    [[webController frameForView:webView] retain];
    webDataSource = [[webFrame dataSource] retain];
}

- (void)stop
{
    NPError npErr;
    
    if (!isStarted)
        return;
    isStarted = NO;

    [self removeTrackingRect:trackingTag];        
    [streams makeObjectsPerformSelector:@selector(stop)];
    [eventSender stop];
    [eventSender release];
    [webController release];
    [webFrame release];
    [webDataSource release];    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    npErr = NPP_Destroy(instance, NULL);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_Destroy: %d\n", npErr);
}

- (IFWebDataSource *)webDataSource
{
    return webDataSource;
}

- (id <IFWebController>) webController
{
    return webController;
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


#pragma mark PLUGIN-TO-BROWSER

- (NSURL *)URLForString:(NSString *)URLString
{
    //FIXME: Do plug-ins only request http?
    if([URLString hasPrefix:@"http://"])
        return [NSURL URLWithString:URLString];
    else
        return [NSURL URLWithString:URLString relativeToURL:baseURL];
}


-(NPError)getURLNotify:(const char *)url target:(const char *)target notifyData:(void *)notifyData
{
    NSURL *requestedURL;
    IFPluginStream *stream;
    IFWebDataSource *dataSource;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_GetURLNotify: %s target: %s\n", url, target);
 
    requestedURL = [self URLForString:[NSString stringWithCString:url]];
    
    if(!requestedURL)
        return NPERR_INVALID_URL;
    
    if(target == NULL){ 
        // Send data to plug-in if target is null
        stream = [[IFPluginStream alloc] initWithURL:requestedURL pluginPointer:instance notifyData:notifyData];
        if(stream){
            [streams addObject:stream];
            [stream release];
        }
    
    }else{
        if(webController){
            // FIXME: Need to send to proper target
            dataSource = [[[IFWebDataSource alloc] initWithURL:requestedURL] autorelease];
            [[webController mainFrame] setProvisionalDataSource:dataSource];
            [[webController mainFrame] startLoading];
            // FIXME: Need to send NPP_URLNotify
        }
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
    NSURL *requestedURL;
    NSDictionary *attributes=nil;
    NSData *postData;
    IFWebDataSource *dataSource;
    IFPluginStream *stream;
        
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "postURLNotify: %s\n", url);
 
    requestedURL = [self URLForString:[NSString stringWithCString:url]];
    
    if(!requestedURL)
        return NPERR_INVALID_URL;
    
    if(file){
        // FIXME: Need function to convert from carbon path to posix
        // FIXME: security issues here?
        postData = [NSData dataWithContentsOfFile:nil];
    }else{
        postData = [NSData dataWithBytes:buf length:len];
    }
    
    attributes = [NSDictionary dictionaryWithObjectsAndKeys:
        postData,	IFHTTPURLHandleRequestData,
        @"POST", 	IFHTTPURLHandleRequestMethod, nil];
                
    if(target == NULL){ 
        // Send data to plug-in if target is null
        stream = [[IFPluginStream alloc] initWithURL:requestedURL pluginPointer:instance notifyData:notifyData attributes:attributes];
        if(stream){
            [streams addObject:stream];
            [stream release];
        }  
    }else{
        if(webController){
            // FIXME: Need to send to proper target
            dataSource = [[[IFWebDataSource alloc] initWithURL:requestedURL attributes:attributes] autorelease];
            [[webController mainFrame] setProvisionalDataSource:dataSource];
            [[webController mainFrame] startLoading];
            // FIXME: Need to send NPP_URLNotify
        }
    }
        
    return NPERR_NO_ERROR;
}

-(NPError)postURL:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPN_PostURL\n");
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
        [webController setStatusText:[NSString stringWithCString:message] forDataSource:webDataSource];
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


