//
//  WKPlugins.h
//  
//
//  Created by Chris Blumenberg on Tue Dec 11 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "npapi.h"

@interface WKPlugin : NSObject {

    NSMutableArray *mimeTypes;
    NSString *name, *executablePath, *filename, *pluginDescription;
    BOOL isLoaded;
    NPPluginFuncs pluginFuncs;
    NPNetscapeFuncs browserFuncs;
    uint16 pluginSize;
    uint16 pluginVersion;
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

- (BOOL)initializeWithPath:(NSString *)plugin;
- (BOOL)getPluginInfoForResourceFile:(SInt16)resRef;
- (void)load;
- (void)unload;
- (NSArray *)mimeTypes;
- (NSString *)name;
- (NSString *)filename;
- (NSString *)executablePath;
- (BOOL)isLoaded;
- (NSString *)description;
- (NSString *)pluginDescription;


- (NPP_NewProcPtr)NPP_New;
- (NPP_DestroyProcPtr)NPP_Destroy;
- (NPP_SetWindowProcPtr)NPP_SetWindow;
- (NPP_NewStreamProcPtr)NPP_NewStream;
- (NPP_WriteReadyProcPtr)NPP_WriteReady;
- (NPP_WriteProcPtr)NPP_Write;
- (NPP_StreamAsFileProcPtr)NPP_StreamAsFile;
- (NPP_DestroyStreamProcPtr)NPP_DestroyStream;
- (NPP_HandleEventProcPtr)NPP_HandleEvent;

@end

