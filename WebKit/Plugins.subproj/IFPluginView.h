/*	
    IFPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import <npapi.h>

@class IFPluginNullEventSender;
@class IFWebDataSource;
@class IFPlugin;
@protocol IFWebController;

@interface IFPluginView : NSView
{
    IFPlugin *plugin;
    IFPluginNullEventSender *eventSender;
    unsigned argsCount;
    char **cAttributes, **cValues;
    
    id <IFWebController> webController;
    IFWebDataSource *webDataSource;
    
    NPP instance;
    NPWindow window;
    NP_Port nPort;
    NPP_t instanceStruct;

    BOOL canRestart, isHidden, isStarted, fullMode;
            
    NSString *URL, *mime;
    NSURL *baseURL;
    NSTrackingRectTag trackingTag;
    NSMutableArray *filesToErase, *activeURLHandles;
    
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

- initWithFrame:(NSRect)r plugin:(IFPlugin *)plug url:(NSString *)location mime:(NSString *)mime arguments:(NSDictionary *)arguments mode:(uint16)mode;
-(void)stop;

+(void)getCarbonEvent:(EventRecord *)carbonEvent;

@end
