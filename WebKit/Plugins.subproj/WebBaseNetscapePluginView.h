/*
        WebBaseNetscapePluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

#import <npapi.h>

@class WebDataSource;
@class WebFrame;
@class WebNetscapePluginPackage;
@class WebNetscapePluginNullEventSender;
@class WebView;


@interface WebBaseNetscapePluginView : NSView
{
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
    BOOL isStarted;
    BOOL inSetWindow;
            
    NSString *MIMEType;
    NSURL *baseURL;
    NSTrackingRectTag trackingTag;
    NSMutableArray *streams;
    NSMutableDictionary *streamNotifications;
    NSTimer *nullEventTimer;
    
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

+ (WebBaseNetscapePluginView *)currentPluginView;

- (BOOL)start;
- (void)stop;
- (BOOL)isStarted;

- (WebFrame *)webFrame;
- (WebDataSource *)dataSource;
- (WebView *)controller;

- (NPP)pluginPointer;

- (void)setWindow;

- (WebNetscapePluginPackage *)plugin;
- (void)setPlugin:(WebNetscapePluginPackage *)thePlugin;
- (void)setMIMEType:(NSString *)theMIMEType;
- (void)setBaseURL:(NSURL *)theBaseURL;
- (void)setAttributes:(NSDictionary *)attributes;
- (void)setMode:(int)theMode;

@end
