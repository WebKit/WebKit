/*	
        WebBaseNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>
#import <WebKit/npapi.h>

@class WebNetscapePluginPackage;
@class WebResponse;

@interface WebBaseNetscapePluginStream : WebBaseResourceHandleDelegate
{
    NSURL *URL;
    NPP instance;
    uint16 transferMode;
    int32 offset;
    NPStream stream;
    char *path;
    void *notifyData;
    WebNetscapePluginPackage *plugin;
        
    NPP_NewStreamProcPtr NPP_NewStream;
    NPP_DestroyStreamProcPtr NPP_DestroyStream;
    NPP_StreamAsFileProcPtr NPP_StreamAsFile;
    NPP_WriteReadyProcPtr NPP_WriteReady;
    NPP_WriteProcPtr NPP_Write;
    NPP_URLNotifyProcPtr NPP_URLNotify;
}

- (void)setPluginPointer:(NPP)pluginPointer;
- (void)setResponse:(WebResponse *)theReponse;
- (void)receivedData:(NSData *)data;
- (void)receivedError:(NPReason)reason;
- (void)finishedLoadingWithData:(NSData *)data;

@end
