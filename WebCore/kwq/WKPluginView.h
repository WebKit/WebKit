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
#include "kwqdebug.h"


typedef NPStream* NPS;

@interface WKPluginView : NSQuickDrawView {
    QWidget *widget;
    bool isFlipped;
    NPP instance;
    NPP_t instanceStruct;
    NPStream streamStruct;
    NPS stream;
    int32 streamOffset;
    NSString *url, *mime;
    WKPlugin *plugin;
    bool transferred;
    NPWindow window;
    NP_Port nPort;
    uint16 transferMode;
        
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

- initWithFrame: (NSRect) r widget: (QWidget *)w plugin: (WKPlugin *)plug url: (NSString *)location mime:(NSString *)mime;
-(void)drawRect:(NSRect)rect;
-(BOOL)acceptsFirstResponder;
-(void)sendNullEvents;
-(void)mouseDown:(NSEvent *)theEvent;
-(void)mouseUp:(NSEvent *)theEvent;
-(void)dealloc;

@end
