/*	
        WebPluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/npapi.h>

@class WebBaseNetscapePluginView;
@class WebDataSource;
@class WebResourceHandle;
@class WebResourceResponse;
@class WebResourceRequest;

@protocol WebDocumentRepresentation;

@interface WebNetscapePluginStream : NSObject <WebDocumentRepresentation>
{
    WebBaseNetscapePluginView *view;
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
    WebResourceResponse *response;
    NSMutableData *resourceData;
    
    NPP_NewStreamProcPtr NPP_NewStream;
    NPP_DestroyStreamProcPtr NPP_DestroyStream;
    NPP_StreamAsFileProcPtr NPP_StreamAsFile;
    NPP_WriteReadyProcPtr NPP_WriteReady;
    NPP_WriteProcPtr NPP_Write;
    NPP_URLNotifyProcPtr NPP_URLNotify;
}

- initWithURL:(NSURL *)theURL pluginPointer:(NPP)thePluginPointer notifyData:(void *)theNotifyData;

- (void)startLoad;
- (void)stop;

@end
