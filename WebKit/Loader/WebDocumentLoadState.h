/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

@class WebFrameLoader;

@interface WebDocumentLoadState : NSObject
{
@public
    WebFrameLoader *frameLoader;
    
    NSData *mainResourceData;

    // A reference to actual request used to create the data source.
    // This should only be used by the resourceLoadDelegate's
    // identifierForInitialRequest:fromDatasource: method.  It is
    // not guaranteed to remain unchanged, as requests are mutable.
    NSURLRequest *originalRequest;    

    // A copy of the original request used to create the data source.
    // We have to copy the request because requests are mutable.
    NSURLRequest *originalRequestCopy;
    
    // The 'working' request for this datasource.  It may be mutated
    // several times from the original request to include additional
    // headers, cookie information, canonicalization and redirects.
    NSMutableURLRequest *request;

    NSURLResponse *response;
    
    NSError *mainDocumentError;    
    
    BOOL committed; // This data source has been committed
    BOOL stopping;
    BOOL loading; // self and webView are retained while loading
    BOOL gotFirstByte; // got first byte
}

- (id)initWithRequest:(NSURLRequest *)request;
- (void)setFrameLoader:(WebFrameLoader *)fl;
- (WebFrameLoader *)frameLoader;
- (void)setMainResourceData:(NSData *)data;
- (NSData *)mainResourceData;
- (NSURLRequest *)originalRequest;
- (NSURLRequest *)originalRequestCopy;
- (NSMutableURLRequest *)request;
- (void)setRequest:(NSURLRequest *)request;
- (void)replaceRequestURLForAnchorScrollWithURL:(NSURL *)URL;
- (BOOL)isStopping;
- (void)stopLoading;
- (void)setupForReplace;
- (void)commitIfReady;
- (void)setCommitted:(BOOL)f;
- (BOOL)isCommitted;
- (BOOL)isLoading;
- (void)setLoading:(BOOL)f;
- (void)updateLoading;
- (void)receivedData:(NSData *)data;
- (void)setupForReplaceByMIMEType:(NSString *)newMIMEType;
- (void)finishedLoading;
- (NSURLResponse *)response;
- (void)clearErrors;
- (NSError *)mainDocumentError;
- (void)mainReceivedError:(NSError *)error complete:(BOOL)isComplete;
- (void)setResponse:(NSURLResponse *)resp;

@end
