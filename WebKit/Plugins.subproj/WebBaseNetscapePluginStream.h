/*	
        WebBaseNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>
#import <WebKit/npapi.h>

@class WebNetscapePluginPackage;
@class NSURLResponse;

@interface WebBaseNetscapePluginStream : NSObject
{
    NSMutableData *deliveryData;
    NSURL *URL;
    NPP instance;
    uint16 transferMode;
    int32 offset;
    NPStream stream;
    char *path;
    void *notifyData;
    WebNetscapePluginPackage *plugin;
    NPReason reason;
        
    NPP_NewStreamProcPtr NPP_NewStream;
    NPP_DestroyStreamProcPtr NPP_DestroyStream;
    NPP_StreamAsFileProcPtr NPP_StreamAsFile;
    NPP_WriteReadyProcPtr NPP_WriteReady;
    NPP_WriteProcPtr NPP_Write;
    NPP_URLNotifyProcPtr NPP_URLNotify;
}

- (void)setPluginPointer:(NPP)pluginPointer;
- (void)setNotifyData:(void *)theNotifyData;

- (void)startStreamWithURL:(NSURL *)theURL 
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
