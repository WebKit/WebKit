/*
        WebBaseNetscapePluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

#import <WebKit/npfunctions.h>

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
    NPWindow lastSetWindow;
    NP_Port nPort;
    NP_Port lastSetPort;
    NPP_t instanceStruct;

    BOOL isStarted;
    BOOL inSetWindow;
    BOOL suspendKeyUpEvents;
    BOOL hasFocus;
    BOOL currentEventIsUserGesture;

    int32 specifiedHeight;
    int32 specifiedWidth;
            
    NSString *MIMEType;
    NSURL *baseURL;
    NSTrackingRectTag trackingTag;
    NSMutableArray *streams;
    NSMutableDictionary *pendingFrameLoads;
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
    
    EventHandlerRef keyEventHandler;
}

+ (WebBaseNetscapePluginView *)currentPluginView;

- (BOOL)start;
- (BOOL)isStarted;

- (WebFrame *)webFrame;
- (WebDataSource *)dataSource;
- (WebView *)webView;
- (NSWindow *)currentWindow;

- (NPP)pluginPointer;

- (WebNetscapePluginPackage *)plugin;
- (void)setPlugin:(WebNetscapePluginPackage *)thePlugin;
- (void)setMIMEType:(NSString *)theMIMEType;
- (void)setBaseURL:(NSURL *)theBaseURL;
- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values;
- (void)setMode:(int)theMode;

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow;
- (void)viewDidMoveToHostWindow;

/* Returns the NPObject that represents the plugin interface. */
- (void *)pluginScriptableObject;

@end
