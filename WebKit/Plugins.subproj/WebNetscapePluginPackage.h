/*
        WebNetscapePluginPackage.h
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebCore/WebCoreViewFactory.h>

#import <WebKit/npapi.h>
#import <WebKit/WebBasePluginPackage.h>


@interface WebNetscapePluginPackage : WebBasePluginPackage
{
    BOOL isBundle;
    BOOL isCFM;
    BOOL isLoaded;
    
    NPPluginFuncs pluginFuncs;
    NPNetscapeFuncs browserFuncs;
    
    uint16 pluginSize;
    uint16 pluginVersion;
    
    CFBundleRef bundle;
    
    CFragConnectionID connID;
    
    SInt16 resourceRef;
    
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

- (BOOL)load;
- (void)unload;

- (NPP_NewProcPtr)NPP_New;
- (NPP_DestroyProcPtr)NPP_Destroy;
- (NPP_SetWindowProcPtr)NPP_SetWindow;
- (NPP_NewStreamProcPtr)NPP_NewStream;
- (NPP_WriteReadyProcPtr)NPP_WriteReady;
- (NPP_WriteProcPtr)NPP_Write;
- (NPP_StreamAsFileProcPtr)NPP_StreamAsFile;
- (NPP_DestroyStreamProcPtr)NPP_DestroyStream;
- (NPP_HandleEventProcPtr)NPP_HandleEvent;
- (NPP_URLNotifyProcPtr)NPP_URLNotify;
- (NPP_GetValueProcPtr)NPP_GetValue;
- (NPP_SetValueProcPtr)NPP_SetValue;
- (NPP_PrintProcPtr)NPP_Print;

@end

