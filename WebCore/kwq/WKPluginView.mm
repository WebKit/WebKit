//
//  WKPluginView.m
//  
//
//  Created by Chris Blumenberg on Thu Dec 13 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "WKPluginView.h"
#include <WCURICacheData.h>
#include <WCURICache.h>
#include <Carbon/Carbon.h> 


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
    npErr = NPP_New(cMime, instance, NP_EMBED, [arguments count], cAttributes, cValues, &saved); // need to pass attributes to plug-in
    KWQDebug("NPP_New: %d\n", npErr);
    transferred = FALSE;
    [[self window] setAcceptsMouseMovedEvents:YES];
    [self performSelector:@selector(sendNullEvents) withObject:nil afterDelay:0];
    return self;
}

- (void)drawRect:(NSRect)rect {
    NPError npErr;
    char cMime[200], cURL[800];
    id <WCURICache> cache;
    NSRect frame;
    
    frame = [self frame];
    nPort.port = [self qdPort];
    nPort.portx = (int32)rect.origin.x;
    nPort.porty = (int32)rect.origin.y;
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
    
    if(!transferred){
        [url getCString:cURL];
        stream->url = cURL;
        stream->end = 0;
        stream->lastmodified = 0;
        stream->notifyData = NULL;
        [mime getCString:cMime];
        
        npErr = NPP_NewStream(instance, cMime, stream, FALSE, &transferMode);
        KWQDebug("NPP_NewStream: %d\n", npErr);
        
        cache = WCGetDefaultURICache();
        if(transferMode == NP_NORMAL){
            KWQDebug("Stream type: NP_NORMAL\n");
            transferMode = NP_ASFILEONLY;
            [cache requestWithString:url requestor:self userData:nil];
        }else if(transferMode == NP_ASFILEONLY){
            KWQDebug("Stream type: NP_ASFILEONLY not yet supported\n");
            transferMode = NP_ASFILEONLY;
        }else if(transferMode == NP_ASFILE){
            KWQDebug("Stream type: NP_ASFILE not fully supported\n");
            transferMode = NP_ASFILE;
            [cache requestWithString:url requestor:self userData:nil];
        }else if(transferMode == NP_SEEK){
            KWQDebug("Stream type: NP_SEEK not yet supported\n");
            transferMode = NP_SEEK;
        }
        transferred = TRUE;
    }
}


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

-(BOOL)acceptsFirstResponder
{
    return true;
}

// event methods

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
    [self performSelector:@selector(sendNullEvents) withObject:nil afterDelay:0];
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

- (void)mouseEntered:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    event.what = adjustCursorEvent;
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(mouseEntered): %dn", acceptedEvent);
}

- (void)mouseExited:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    event.what = adjustCursorEvent;
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(mouseExited): %d\n", acceptedEvent);
}

- (void)mouseMoved:(NSEvent *)theEvent{
    KWQDebug("mouseMoved\n");
}

- (void)keyUp:(NSEvent *)theEvent
{
 
    EventRecord event;
    bool acceptedEvent;

    event.what = keyUp;
    event.message = [theEvent keyCode];
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(keyUp): %d key:%d\n", acceptedEvent, event.message);
}

- (void)keyDown:(NSEvent *)theEvent
{
    EventRecord event;
    bool acceptedEvent;
    
    event.what = keyDown;
    event.message = [theEvent keyCode];
    event.when = (uint32)([theEvent timestamp] * 60);
    acceptedEvent = NPP_HandleEvent(instance, &event);
    KWQDebug("NPP_HandleEvent(keyDown): %d key:%d\n", acceptedEvent, event.message);
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
    //[self cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendNullEvents) object:nil]; //compiler can't find this method!
    npErr = NPP_Destroy(instance, NULL);
    KWQDebug("NPP_Destroy: %d\n", npErr);
    [super dealloc];
}



@end
