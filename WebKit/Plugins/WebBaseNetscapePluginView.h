/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Cocoa/Cocoa.h>

#import <WebKit/npfunctions.h>
#import <WebKit/npapi.h>

@class WebDataSource;
@class WebFrame;
@class WebNetscapePluginPackage;
@class WebNetscapePluginNullEventSender;
@class WebView;

typedef union PluginPort {
#ifndef NP_NO_QUICKDRAW
    NP_Port qdPort;
#endif        
    NP_CGContext cgPort;
} PluginPort;

@interface WebBaseNetscapePluginView : NSView
{
    WebNetscapePluginPackage *plugin;
    
    int mode;
    
    unsigned argsCount;
    char **cAttributes;
    char **cValues;
        
    NPP instance;
    NPP_t instanceStruct;
    NPWindow window;
    NPWindow lastSetWindow;
    PluginPort nPort;
    PluginPort lastSetPort;
    NPDrawingModel drawingModel;

    BOOL isStarted;
    BOOL inSetWindow;
    BOOL suspendKeyUpEvents;
    BOOL hasFocus;
    BOOL currentEventIsUserGesture;
    BOOL isTransparent;
    BOOL isCompletelyObscured;
    
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
- (void)stop;

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
