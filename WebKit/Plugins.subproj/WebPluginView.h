/*	
    WebPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebKit.h>
#import <WebKit/WebFrame.h>

#import <Cocoa/Cocoa.h>
#import <npapi.h>

@class WebNetscapePluginNullEventSender;
@class WebDataSource;
@class WebNetscapePlugin;
@class WebController;
@protocol WebDocumentView;

@interface WebNetscapePluginView : NSView <WebDocumentView>
{
    WebNetscapePluginNullEventSender *eventSender;
    unsigned argsCount;
    char **cAttributes, **cValues;
    
    WebController *webController;
    WebDataSource *webDataSource;
    WebFrame *webFrame;
    
    NPP instance;
    NPWindow window;
    NP_Port nPort;
    NPP_t instanceStruct;
        
    BOOL canRestart, isHidden, isStarted, fullMode, needsLayout;
            
    NSString *mime;
    NSURL *srcURL, *baseURL;
    NSTrackingRectTag trackingTag;
    NSMutableArray *streams;
    NSMutableDictionary *notificationData;
    
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

- (id)initWithFrame:(NSRect)r plugin:(WebNetscapePlugin *)plugin URL:(NSURL *)URL baseURL:(NSURL *)baseURL mime:(NSString *)mimeType arguments:(NSDictionary *)arguments;
- (void)stop;

- (WebDataSource *)webDataSource;
- (WebController *)webController;

+ (void)getCarbonEvent:(EventRecord *)carbonEvent;
- (BOOL)sendEvent:(EventRecord *)event;
- (BOOL)sendUpdateEvent;

- (NPP)pluginInstance;
- (NPP_NewStreamProcPtr)NPP_NewStream;
- (NPP_WriteReadyProcPtr)NPP_WriteReady;
- (NPP_WriteProcPtr)NPP_Write;
- (NPP_StreamAsFileProcPtr)NPP_StreamAsFile;
- (NPP_DestroyStreamProcPtr)NPP_DestroyStream;
- (NPP_URLNotifyProcPtr)NPP_URLNotify;

@end
