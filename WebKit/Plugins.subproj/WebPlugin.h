/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <Foundation/Foundation.h>
#import "npapi.h"
#import <WebCore/WebCoreViewFactory.h>

@interface WebNetscapePlugin : NSObject <WebCorePluginInfo>
{
    NSMutableDictionary *MIMEToExtensions;
    NSMutableDictionary *extensionToMIME;
    NSMutableDictionary *MIMEToDescription;
    
    NSString *name;
    NSString *path;
    NSString *filename;
    NSString *pluginDescription;

    BOOL isLoaded;
    BOOL isBundle;
    BOOL isCFM;
    
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

- initWithPath:(NSString *)pluginPath;
- (BOOL)load;
- (void)unload;
- (NSString *)path;
- (BOOL)isLoaded;
- (NSString *)description;
- (NSDictionary *)extensionToMIMEDictionary;

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

