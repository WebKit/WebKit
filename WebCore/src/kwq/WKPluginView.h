//
//  WKPluginView.h
//  
//
//  Created by Chris Blumenberg on Thu Dec 13 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <AppKit/AppKit.h>
#include <qwidget.h>
#import <WKPlugin.h>
#include "npapi.h"


typedef NPStream* NPS;

@interface WKPluginViewNullEventSender : NSObject{
    NPP instance;
    NPP_HandleEventProcPtr NPP_HandleEvent;
}

-(id)initializeWithNPP:(NPP)pluginInstance functionPointer:(NPP_HandleEventProcPtr)HandleEventFunction;
-(void)sendNullEvents;
-(void)stop;
@end

@interface WKPluginView : NSQuickDrawView {
    QWidget *widget;
    WKPlugin *plugin;
    WKPluginViewNullEventSender *eventSender;
    
    NPP instance;
    NPP_t instanceStruct;
    NPStream streamStruct;
    NPS stream;
    NPWindow window;
    NP_Port nPort;
    
    int32 streamOffset;
    uint16 transferMode;
    char **cAttributes, **cValues;
    bool isFlipped, transferred, hidden;
            
    NSString *url, *mime;
    NSTrackingRectTag trackingTag;
    
    NPP_NewProcPtr NPP_New;
    NPP_DestroyProcPtr NPP_Destroy;
    NPP_SetWindowProcPtr NPP_SetWindow;
    NPP_NewStreamProcPtr NPP_NewStream;
    NPP_DestroyStreamProcPtr NPP_DestroyStream;
    NPP_StreamAsFileProcPtr NPP_StreamAsFile;
    NPP_WriteReadyProcPtr NPP_WriteReady;
    NPP_WriteProcPtr NPP_Write;
    NPP_PrintProcPtr NPP_Print;
    NPP_HandleEventProcPtr NPP_HandleEvent;
    NPP_URLNotifyProcPtr NPP_URLNotify;
    NPP_GetValueProcPtr NPP_GetValue;
    NPP_SetValueProcPtr NPP_SetValue;
    NPP_ShutdownProcPtr NPP_Shutdown; 
}

- initWithFrame: (NSRect) r widget: (QWidget *)w plugin: (WKPlugin *)plug url: (NSString *)location mime:(NSString *)mime arguments:(NSDictionary *)arguments;
-(void)drawRect:(NSRect)rect;
-(void) setWindow:(NSRect)rect;
-(BOOL)acceptsFirstResponder;
-(BOOL)becomeFirstResponder;
-(BOOL)resignFirstResponder;
-(void)sendActivateEvent;
-(void)sendUpdateEvent;
-(void)mouseDown:(NSEvent *)theEvent;
-(void)mouseUp:(NSEvent *)theEvent;
-(void)mouseDragged:(NSEvent *)theEvent;
-(void)mouseEntered:(NSEvent *)theEvent;
-(void)mouseExited:(NSEvent *)theEvent;
-(void)keyDown:(NSEvent *)theEvent;
-(void)keyUp:(NSEvent *)theEvent;
-(void)dealloc;

// plug-in to browser calls
-(NPError)getURLNotify:(const char *)url target:(const char *)target notifyData:(void *)notifyData;
-(NPError)getURL:(const char *)url target:(const char *)target;
-(NPError)postURLNotify:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData;
-(NPError)postURL:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file;
-(NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream;
-(NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer;
-(NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason;
-(void)status:(const char *)message;
-(NPError)getValue:(NPNVariable)variable value:(void *)value;
-(NPError)setValue:(NPPVariable)variable value:(void *)value;
-(void)invalidateRect:(NPRect *)invalidRect;
-(void)invalidateRegion:(NPRegion)invalidateRegion;
-(void)forceRedraw;

@end

