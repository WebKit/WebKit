/*	
    WebPluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/npapi.h>

@class WebDataSource;
@class WebResourceHandle;
@class WebResourceRequest;
@class WebNetscapePluginView;

@protocol WebDocumentRepresentation;

@interface WebNetscapePluginStream : NSObject <WebDocumentRepresentation>
{
    WebNetscapePluginView *view;
    NSURL *URL;
    NPP instance;
    uint16 transferMode;
    int32 offset;
    NPStream npStream;
    NSString *path;
    
    void *notifyData;
    
    BOOL isFirstChunk;

    WebResourceRequest *request;
    WebResourceHandle *resource;
    NSMutableData *resourceData;
    
    NPP_NewStreamProcPtr NPP_NewStream;
    NPP_DestroyStreamProcPtr NPP_DestroyStream;
    NPP_StreamAsFileProcPtr NPP_StreamAsFile;
    NPP_WriteReadyProcPtr NPP_WriteReady;
    NPP_WriteProcPtr NPP_Write;
    NPP_URLNotifyProcPtr NPP_URLNotify;
}

- initWithURL:(NSURL *)theURL pluginPointer:(NPP)thePluginPointer;
- initWithURL:(NSURL *)theURL pluginPointer:(NPP)thePluginPointer notifyData:(void *)theNotifyData;

- (void)startLoad;
- (void)stop;

@end
