/*	
        WebBaseNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/npfunctions.h>

@class WebNetscapePluginPackage;
@class NSURLResponse;

@interface WebBaseNetscapePluginStream : NSObject
{
    NSMutableData *deliveryData;
    NSURL *requestURL;
    NSURL *responseURL;
    NSString *MIMEType;
    
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
- (NSError *)errorForReason:(NPReason)theReason;

- (id)initWithRequestURL:(NSURL *)theRequestURL
           pluginPointer:(NPP)thePluginPointer
              notifyData:(void *)theNotifyData
        sendNotification:(BOOL)flag;

- (void)setRequestURL:(NSURL *)theRequestURL;
- (void)setResponseURL:(NSURL *)theResponseURL;
- (void)setPluginPointer:(NPP)pluginPointer;

- (uint16)transferMode;

- (void)startStreamResponseURL:(NSURL *)theResponseURL
         expectedContentLength:(long long)expectedContentLength
              lastModifiedDate:(NSDate *)lastModifiedDate
                      MIMEType:(NSString *)MIMEType;
- (void)startStreamWithResponse:(NSURLResponse *)r;

// cancelLoadWithError cancels the NSURLConnection and informs WebKit of the load error.
// This method is overriden by subclasses.
- (void)cancelLoadWithError:(NSError *)error;

// destroyStreamWithError tells the plug-in that the load is completed (error == nil) or ended in error.
- (void)destroyStreamWithError:(NSError *)error;

// cancelLoadAndDestoryStreamWithError calls cancelLoadWithError: then destroyStreamWithError:.
- (void)cancelLoadAndDestroyStreamWithError:(NSError *)error;

- (void)receivedData:(NSData *)data;
- (void)finishedLoadingWithData:(NSData *)data;

@end
