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

    NSDictionary *mimeTypes;
    NSString *name;
    NSString *executablePath;
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
- (void)load;
- (void)unload;

- (NPP_NewProcPtr)NPP_New;
- (NPP_SetWindowProcPtr)NPP_SetWindow;
- (NPP_NewStreamProcPtr)NPP_NewStream;
- (NPP_WriteReadyProcPtr)NPP_WriteReady;
- (NPP_WriteProcPtr)NPP_Write;
- (NPP_DestroyStreamProcPtr)NPP_DestroyStream;
- (NPP_HandleEventProcPtr)NPP_HandleEvent;
- (NSDictionary *)mimeTypes;
- (NSString *)name;
- (NSString *)executablePath;
- (BOOL)isLoaded;
- (NSString *)description;


@end
    
NSMutableDictionary *getMimeTypesForResourceFile(SInt16 resRef);

