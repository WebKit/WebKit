//
//  WKPluginView.m
//  
//
//  Created by Chris Blumenberg on Thu Dec 13 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "WKPluginView.h"
#include <Carbon/Carbon.h> 
#include "kwqdebug.h"

//#define USE_CARBON 1
//#import <AppKit/NSWindow_Private.h>

@implementation WKPluginViewNullEventSender

-(id)initializeWithNPP:(NPP)pluginInstance functionPointer:(NPP_HandleEventProcPtr)HandleEventFunction;
{
    instance = pluginInstance;
    NPP_HandleEvent = HandleEventFunction;
    return self;
}

-(void)sendNullEvents
{
    EventRecord event;
    bool acceptedEvent;
    UnsignedWide msecs;
    
    event.what = nullEvent;
    Microseconds(&msecs);
    event.when = (uint32)((double)UnsignedWideToUInt64(msecs) / 1000000 * 60); // microseconds to ticks
    acceptedEvent = NPP_HandleEvent(instance, &event);
    //KWQDebug("NPP_HandleEvent(nullEvent): %d  when: %u\n", acceptedEvent, event.when);
    [self performSelector:@selector(sendNullEvents) withObject:nil afterDelay:.1];
}

-(void) stop
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendNullEvents) object:nil];
}

@end

@implementation WKPluginView

- initWithFrame: (NSRect) r widget: (QWidget *)w plugin: (WKPlugin *)plug url: (NSString *)location mime:(NSString *)mimeType  arguments:(NSDictionary *)arguments
{
    NPError npErr;
    char cMime[200], *s;
    NPSavedData saved;
    NSArray *attributes, *values;
    NSString *attributeString;
    
    uint i;
        
    [super initWithFrame: r];

    instance = &instanceStruct;
    instance->ndata = self;
    stream = &streamStruct;
    streamOffset = 0;

    mime = mimeType;
    url = location;
    plugin = plug;
    NPP_New = 		[plugin NPP_New]; // copy function pointers
    NPP_Destroy = 	[plugin NPP_Destroy];
    NPP_SetWindow = 	[plugin NPP_SetWindow];
    NPP_NewStream = 	[plugin NPP_NewStream];
    NPP_WriteReady = 	[plugin NPP_WriteReady];
    NPP_Write = 	[plugin NPP_Write];
    NPP_StreamAsFile = 	[plugin NPP_StreamAsFile];
    NPP_DestroyStream = [plugin NPP_DestroyStream];
    NPP_HandleEvent = 	[plugin NPP_HandleEvent];
    
    attributes = [arguments allKeys];
    values = [arguments allValues];
    cAttributes = malloc(sizeof(char *) * [arguments count]);
    cValues = malloc(sizeof(char *) * [arguments count]);
    
    for(i=0; i<[arguments count]; i++){ // convert dictionary to 2 string arrays
        attributeString = [attributes objectAtIndex:i];
        s = malloc([attributeString length]+1);
        [attributeString getCString:s];
        cAttributes[i] = s;
        
        attributeString = [values objectAtIndex:i];
        s = malloc([attributeString length]+1);
        [attributeString getCString:s];
        cValues[i] = s;
    }
    [mime getCString:cMime];
    npErr = NPP_New(cMime, instance, NP_EMBED, [arguments count], cAttributes, cValues, &saved);
    KWQDebug("NPP_New: %d\n", npErr);
    
    if([attributes containsObject:@"HIDDEN"]){
        hidden = TRUE;
    }else{
        hidden = FALSE;
    }
    transferred = FALSE;
    trackingTag = [self addTrackingRect:r owner:self userData:nil assumeInside:NO];
    eventSender = [[[WKPluginViewNullEventSender alloc] initializeWithNPP:instance functionPointer:NPP_HandleEvent] autorelease];
    [eventSender sendNullEvents];
    return self;
}

- (void)drawRect:(NSRect)rect
{
    NPError npErr;
    char cMime[200], cURL[800];
    //WindowRef windowRef;
    
    //windowRef = [[self window] _windowRef]; // give the window a WindowRef
    //windowRef->port = [self qdPort];
    
    //MoveTo(0,0); // diagnol line test
    //LineTo((short)rect.size.width, (short)rect.size.height);
    
    [self setWindow:rect];
    if(!transferred){
        [self sendActivateEvent];
        [url getCString:cURL];
        stream->url = cURL;
        stream->end = 0;
        stream->lastmodified = 0;
        stream->notifyData = NULL;
        [mime getCString:cMime];
        
        npErr = NPP_NewStream(instance, cMime, stream, FALSE, &transferMode);
        KWQDebug("NPP_NewStream: %d\n", npErr);
        
        if(transferMode == NP_NORMAL){
            KWQDebug("Stream type: NP_NORMAL\n");
            //[cache requestWithString:url requestor:self userData:nil];
            [WCURLHandleCreate([NSURL URLWithString:url], self, nil) loadInBackground];
        }else if(transferMode == NP_ASFILEONLY){
            KWQDebug("Stream type: NP_ASFILEONLY not yet supported\n");
        }else if(transferMode == NP_ASFILE){
            KWQDebug("Stream type: NP_ASFILE not fully supported\n");
            [WCURLHandleCreate([NSURL URLWithString:url], self, nil) loadInBackground];
        }else if(transferMode == NP_SEEK){
            KWQDebug("Stream type: NP_SEEK not yet supported\n");
        }
        transferred = TRUE;
        
    }
    [self sendUpdateEvent];
}

- (void) setWindow:(NSRect)rect
{
    NPError npErr;
    NSRect frame, frameInWindow;
    
    frame = [self frame];
    frameInWindow = [self convertRect:rect toView:nil];
    
    nPort.port = [self qdPort];
    nPort.portx = 0;
    nPort.porty = 0;   
    window.window = &nPort;
    
    window.x = 0; 
    window.y = 0;

    window.width = (uint32)frame.size.width;
    window.height = (uint32)frame.size.height;

    window.clipRect.top = (uint16)rect.origin.y; // clip rect
    window.clipRect.left = (uint16)rect.origin.x;
    window.clipRect.bottom = (uint16)rect.size.height;
    window.clipRect.right = (uint16)rect.size.width;
    
    window.type = NPWindowTypeDrawable;
    
    npErr = NPP_SetWindow(instance, &window);
    KWQDebug("NPP_SetWindow: %d rect.size.height=%d rect.size.width=%d port=%d rect.origin.x=%f rect.origin.y=%f\n", npErr, (int)rect.size.height, (int)rect.size.width, (int)nPort.port, rect.origin.x, rect.origin.y);
    KWQDebug("frame.size.height=%d frame.size.width=%d frame.origin.x=%f frame.origin.y=%f\n", (int)frame.size.height, (int)frame.size.width, frame.origin.x, frame.origin.y);
    KWQDebug("frameInWindow.size.height=%d frameInWindow.size.width=%d frameInWindow.origin.x=%f frameInWindow.origin.y=%f\n", (int)frameInWindow.size.height, (int)frameInWindow.size.width, frameInWindow.origin.x, frameInWindow.origin.y);
}

// cache methods

- (void)WCURLHandleResourceDidBeginLoading:(id)sender userData:(void *)userData
{
}

- (void)WCURLHandleResourceDidCancelLoading:(id)sender userData:(void *)userData
{
}

- (void)WCURLHandleResourceDidFinishLoading:(id)sender userData:(void *)userData
{
    NPError npErr;
    
    streamOffset = 0;
    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY){
        NPP_StreamAsFile(instance, stream, NULL);
    }
    npErr = NPP_DestroyStream(instance, stream, NPRES_DONE);
    KWQDebug("NPP_DestroyStream: %d\n", npErr);
}

- (void)WCURLHandle:(id)sender resourceDataDidBecomeAvailable:(NSData *)data userData:(void *)userData
{
    int32 bytes;
    
    bytes = NPP_WriteReady(instance, stream);
    KWQDebug("NPP_WriteReady bytes=%d\n", (int)bytes);
    
    bytes = NPP_Write(instance, stream, streamOffset, [data length], (void *)[data bytes]);
    KWQDebug("NPP_Write bytes=%d\n", (int)bytes);
    streamOffset += [data length];
}

- (void)WCURLHandle:(id)sender resourceDidFailLoadingWithResult:(int)result userData:(void *)userData
{
}

// FIXME: Remove old cache code
#if 0
-(void)cacheDataAvailable:(NSNotification *)notification
{
    id <WCURICacheData> data;
    int32 bytes;
    
    data = [notification object];
    bytes = NPP_WriteReady(instance, stream);
    KWQDebug("NPP_WriteReady bytes=%d\n", (int)bytes);
    
    bytes = NPP_Write(instance, stream, streamOffset, [data cacheDataSize], [data cacheData]);
    KWQDebug("NPP_Write bytes=%d\n", (int)bytes);
    streamOffset += [data cacheDataSize];
}

-(void)cacheFinished:(NSNotification *)notification
{
    NPError npErr;
    
    streamOffset = 0;
    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY){
        NPP_StreamAsFile(instance, stream, NULL);
    }
    npErr = NPP_DestroyStream(instance, stream, NPRES_DONE);
    KWQDebug("NPP_DestroyStream: %d\n", npErr);
}
#endif

// event methods

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
    KWQDebug("NPP_HandleEvent(getFocusEvent): %d  when: %u\n", acceptedEvent, event.when);
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
    KWQDebug("NPP_HandleEvent(loseFocusEvent): %d  when: %u\n", acceptedEvent, event.when);
    return YES;
}

-(void)sendActivateEvent
{
    EventRecord event;
    bool acceptedEvent;
    UnsignedWide msecs;
    
    event.what = activateEvt;
    event.message = (UInt32)[self qdPort];
    Microseconds(&msecs);
    event.when = (uint32)((double)UnsignedWideToUInt64(msecs) / 1000000 * 60); // microseconds to ticks
    acceptedEvent = NPP_HandleEvent(instance, &event); 
    KWQDebug("NPP_HandleEvent(activateEvent): %d  when: %u\n", acceptedEvent, event.when);
}

-(void)sendUpdateEvent
{
    EventRecord event;
    bool acceptedEvent;
    UnsignedWide msecs;
    
    event.what = updateEvt;
    event.message = (UInt32)[self qdPort];
    Microseconds(&msecs);
    event.when = (uint32)((double)UnsignedWideToUInt64(msecs) / 1000000 * 60); // microseconds to ticks
    acceptedEvent = NPP_HandleEvent(instance, &event); 
    KWQDebug("NPP_HandleEvent(updateEvt): %d  when: %u\n", acceptedEvent, event.when);
}

-(void)mouseDown:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    Point pt;
    NSPoint viewPoint;
    NSRect frame;
    
    viewPoint = [self convertPoint:[theEvent locationInWindow] fromView:[[theEvent window] contentView]];
    frame = [self frame];
    
    pt.v = (short)viewPoint.y; 
    pt.h = (short)viewPoint.x;
    event.what = mouseDown;
    event.where = pt;
    event.when = (uint32)([theEvent timestamp] * 60); // seconds to ticks
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(mouseDown): %d pt.v=%d, pt.h=%d ticks=%u\n", acceptedEvent, pt.v, pt.h, event.when);
}

-(void)mouseUp:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    Point pt;
    NSPoint viewPoint;
    NSRect frame;
    
    viewPoint = [self convertPoint:[theEvent locationInWindow] fromView:[[theEvent window] contentView]];
    frame = [self frame];
    
    pt.v = (short)viewPoint.y; 
    pt.h = (short)viewPoint.x;
    event.what = mouseUp;
    event.where = pt;
    event.when = (uint32)([theEvent timestamp] * 60); 
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d ticks=%u\n", acceptedEvent, pt.v, pt.h, event.when);
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    Point pt;
    NSPoint viewPoint;
    NSRect frame;
    
    viewPoint = [self convertPoint:[theEvent locationInWindow] fromView:[[theEvent window] contentView]];
    frame = [self frame];
    
    pt.v = (short)viewPoint.y; 
    pt.h = (short)viewPoint.x;
    event.what = osEvt;
    event.where = pt;
    event.when = (uint32)([theEvent timestamp] * 60); // seconds to ticks
    event.message = mouseMovedMessage;
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(mouseDragged): %d pt.v=%d, pt.h=%d ticks=%u\n", acceptedEvent, pt.v, pt.h, event.when);
}

//FIXME: mouseEntered and mouseExited are not being called for some reason
- (void)mouseEntered:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    KWQDebug("NPP_HandleEvent(mouseEntered)\n");
    if([theEvent trackingNumber] != trackingTag)
        return;
    event.what = adjustCursorEvent;
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(mouseEntered): %dn", acceptedEvent);
}

- (void)mouseExited:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    if([theEvent trackingNumber] != trackingTag)
        return;    
    event.what = adjustCursorEvent;
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(mouseExited): %d\n", acceptedEvent);
}

- (void)keyUp:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    event.what = keyUp;
    event.message = [[theEvent charactersIgnoringModifiers] characterAtIndex:0];
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(keyUp): %d key:%c\n", acceptedEvent, (event.message & charCodeMask));
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
    KWQDebug("NPP_HandleEvent(keyDown): %d key:%c\n", acceptedEvent, (event.message & charCodeMask));
}

// plug-in to browser calls

-(NPError)getURLNotify:(const char *)url target:(const char *)target notifyData:(void *)notifyData
{
    KWQDebug("getURLNotify\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)getURL:(const char *)url target:(const char *)target
{
    KWQDebug("getURL\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)postURLNotify:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData
{
    KWQDebug("postURLNotify\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)postURL:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file
{
    KWQDebug("postURL\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream
{
    KWQDebug("newStream\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer
{
    KWQDebug("write\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason
{
    KWQDebug("destroyStream\n");
    return NPERR_GENERIC_ERROR;
}

-(void)status:(const char *)message
{
    KWQDebug("status\n");
}

-(NPError)getValue:(NPNVariable)variable value:(void *)value
{
    KWQDebug("getValue\n");
    return NPERR_GENERIC_ERROR;
}

-(NPError)setValue:(NPPVariable)variable value:(void *)value
{
    KWQDebug("setValue\n");
    return NPERR_GENERIC_ERROR;
}

-(void)invalidateRect:(NPRect *)invalidRect
{
    KWQDebug("invalidateRect\n");
}

-(void)invalidateRegion:(NPRegion)invalidateRegion
{
    KWQDebug("invalidateRegion\n");
}

-(void)forceRedraw
{
    KWQDebug("forceRedraw\n");
}


-(void)dealloc
{
    NPError npErr;
    
    [eventSender stop];
    npErr = NPP_Destroy(instance, NULL);
    KWQDebug("NPP_Destroy: %d\n", npErr);
    [super dealloc];
}

@end
