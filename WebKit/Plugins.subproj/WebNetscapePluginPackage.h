/*
        WebNetscapePluginPackage.h
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBasePluginPackage.h>

typedef enum {
    WebCFMExecutableType,
    WebMachOExecutableType
} WebExecutableType;

@interface WebNetscapePluginPackage : WebBasePluginPackage
{
    BOOL isBundle;
    BOOL isCFM;
    
    NPPluginFuncs pluginFuncs;
    NPNetscapeFuncs browserFuncs;
    
    uint16 pluginSize;
    uint16 pluginVersion;
        
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
    NPP_GetJavaClassProcPtr NPP_GetJavaClass;
}

- (WebExecutableType)executableType;

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

