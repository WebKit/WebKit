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
