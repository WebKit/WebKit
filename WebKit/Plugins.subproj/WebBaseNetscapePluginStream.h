/*	
        WebBaseNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/npfunctions.h>

@class WebNetscapePluginPackage;
@class NSURLResponse;

#define WEB_REASON_PLUGIN_CANCELLED -1

@interface WebBaseNetscapePluginStream : NSObject
{
    NSMutableData *deliveryData;
    NSURL *requestURL;
    NSURL *responseURL;
    NPP instance;
    uint16 transferMode;
    int32 offset;
    NPStream stream;
    char *path;
    BOOL sendNotification;
    void *notifyData;
    WebNetscapePluginPackage *plugin;
    NPReason reason;
    BOOL isTerminated;
        
    NPP_NewStreamProcPtr NPP_NewStream;
    NPP_DestroyStreamProcPtr NPP_DestroyStream;
    NPP_StreamAsFileProcPtr NPP_StreamAsFile;
    NPP_WriteReadyProcPtr NPP_WriteReady;
    NPP_WriteProcPtr NPP_Write;
    NPP_URLNotifyProcPtr NPP_URLNotify;
}

+ (NPReason)reasonForError:(NSError *)error;

- (id)initWithRequestURL:(NSURL *)theRequestURL
           pluginPointer:(NPP)thePluginPointer
              notifyData:(void *)theNotifyData
        sendNotification:(BOOL)flag;

- (void)setRequestURL:(NSURL *)theRequestURL;
- (void)setResponseURL:(NSURL *)theResponseURL;
- (void)setPluginPointer:(NPP)pluginPointer;

- (void)startStreamResponseURL:(NSURL *)theResponseURL
         expectedContentLength:(long long)expectedContentLength
              lastModifiedDate:(NSDate *)lastModifiedDate
                      MIMEType:(NSString *)MIMEType;
- (void)startStreamWithResponse:(NSURLResponse *)r;
- (void)receivedData:(NSData *)data;
- (void)finishedLoadingWithData:(NSData *)data;
- (void)receivedError:(NSError *)error;
- (void)cancelWithReason:(NPReason)theReason;
- (uint16)transferMode;

@end
