/*
        WebBaseNetscapePluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

#import <npapi.h>

@class WebController;
@class WebDataSource;
@class WebFrame;
@class WebNetscapePluginPackage;
@class WebNetscapePluginNullEventSender;


@interface WebBaseNetscapePluginView : NSView
{
    WebNetscapePluginNullEventSender *eventSender;

    WebNetscapePluginPackage *plugin;
    
    int mode;
    
    unsigned argsCount;
    char **cAttributes;
    char **cValues;
        
    NPP instance;
    NPWindow window;
    NP_Port nPort;
    NPP_t instanceStruct;

    BOOL canRestart;
    BOOL isHidden;
    BOOL isStarted;
            
    NSString *MIMEType;
    NSURL *baseURL;
    NSTrackingRectTag trackingTag;
    NSMutableArray *streams;
    NSMutableDictionary *streamNotifications;
    
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
}

- (void)start;
- (void)stop;

- (WebFrame *)webFrame;
- (WebDataSource *)dataSource;
- (WebController *)controller;

+ (void)getCarbonEvent:(EventRecord *)carbonEvent;
- (BOOL)sendEvent:(EventRecord *)event;
- (BOOL)sendUpdateEvent;

- (NPP)pluginPointer;

- (void)setUpWindowAndPort;

- (WebNetscapePluginPackage *)plugin;
- (void)setPlugin:(WebNetscapePluginPackage *)thePlugin;
- (void)setMIMEType:(NSString *)theMIMEType;
- (void)setBaseURL:(NSURL *)theBaseURL;
- (void)setAttributes:(NSDictionary *)attributes;
- (void)setMode:(int)theMode;

@end
