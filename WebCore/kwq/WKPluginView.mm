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

@implementation WKPluginView

- initWithFrame: (NSRect) r widget: (QWidget *)w plugin: (WKPlugin *)plug url: (NSString *)location mime:(NSString *)mimeType
{
    NPError npErr;
    char cMime[200];
    NPSavedData saved;
    
    [super initWithFrame: r];
    widget = w;
    isFlipped = YES;
    instance = &instanceStruct;
    stream = &streamStruct;
    streamOffset = 0;

    mime = mimeType;
    url = location;
    plugin = plug;
    NPP_New = 		[plugin NPP_New]; // copy function pointers
    NPP_SetWindow = 	[plugin NPP_SetWindow];
    NPP_NewStream = 	[plugin NPP_NewStream];
    NPP_WriteReady = 	[plugin NPP_WriteReady];
    NPP_Write = 	[plugin NPP_Write];
    NPP_DestroyStream = [plugin NPP_DestroyStream];
    NPP_HandleEvent = 	[plugin NPP_HandleEvent];
    
    [mime getCString:cMime];
    npErr = NPP_New(cMime, instance, NP_EMBED, 0, NULL, NULL, &saved);
    KWQDebug("NPP_New: %d\n", npErr);
    transferred = FALSE;
    return self;
}

- (void)drawRect:(NSRect)rect {
    NPError npErr;
    NPWindow window;
    NP_Port nPort;
   // Point pt;
    char cMime[200], cURL[800];
    uint16 stype;
    id <WCURICache> cache;
    
    nPort.port = [self qdPort];
    nPort.portx = 0;
    nPort.porty = 0;
    
    window.window = &nPort;
    window.x = 20;
    window.y = 20;
    window.width = 640;
    window.height = 450;
    window.clipRect.top = 0;
    window.clipRect.left = 0;
    window.clipRect.bottom = 450;
    window.clipRect.right = 640;
    window.type = NPWindowTypeDrawable;
    /*SetPort(nPort.port);
    LineTo((int)rect.size.width, (int)rect.size.height);
    MoveTo(0,0);*/
    npErr = NPP_SetWindow(instance, &window);
    KWQDebug("NPP_SetWindow: %d rect.size.height=%d rect.size.width=%d port=%d\n", npErr, (int)rect.size.height, (int)rect.size.width, (int)nPort.port);
    
    if(!transferred){
        [url getCString:cURL];
        stream->url = cURL;
        stream->end = 0;
        stream->lastmodified = 0;
        stream->notifyData = NULL;
        [mime getCString:cMime];
        
        npErr = NPP_NewStream(instance, cMime, stream, TRUE, &stype);
        KWQDebug("NPP_NewStream: %d\n", npErr);
        
        cache = WCGetDefaultURICache();
        if(stype == NP_NORMAL){
            KWQDebug("Stream type: NP_NORMAL\n");
            [cache requestWithString:url requestor:self userData:nil];
        }else if(stype == NP_ASFILEONLY){
            KWQDebug("Stream type: NP_ASFILEONLY\n");
        }else if(stype == NP_ASFILE){
            KWQDebug("Stream type: NP_ASFILE\n");
        }else if(stype == NP_SEEK){
            KWQDebug("Stream type: NP_SEEK\n");
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
    npErr = NPP_DestroyStream(instance, stream, NPRES_DONE);
    KWQDebug("NPP_DestroyStream: %d\n", npErr);
}

-(BOOL)acceptsFirstResponder
{
    return true;
}

-(void)sendNullEvent
{
    EventRecord event;
    
    event.what = 0;
    KWQDebug("NPP_HandleEvent: %d\n", NPP_HandleEvent(instance, &event));
}

@end
