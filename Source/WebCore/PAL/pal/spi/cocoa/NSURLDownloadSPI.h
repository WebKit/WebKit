/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)

#import <Foundation/NSURLDownload.h>

#if USE(APPLE_INTERNAL_SDK)
#import <Foundation/NSURLDownloadPrivate.h>
#endif

#else

@class NSString;
@class NSURLAuthenticationChallenge;
@class NSURLDownload;
@class NSURLProtectionSpace;
@class NSURLRequest;
@class NSURLResponse;

#ifndef WebDownload_h
/* Also defined in <WebKit/WebDownload.h>. */
@interface NSURLDownload : NSObject
@end
#endif

@protocol NSURLDownloadDelegate <NSObject>
@optional
- (void)downloadDidBegin:(NSURLDownload *)download;
- (NSURLRequest *)download:(NSURLDownload *)download willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse;
- (BOOL)download:(NSURLDownload *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace;
- (void)download:(NSURLDownload *)download didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;
- (BOOL)downloadShouldUseCredentialStorage:(NSURLDownload *)download;
- (void)download:(NSURLDownload *)download didReceiveResponse:(NSURLResponse *)response;
- (void)download:(NSURLDownload *)download willResumeWithResponse:(NSURLResponse *)response fromByte:(long long)startingByte;
- (void)download:(NSURLDownload *)download didReceiveDataOfLength:(NSUInteger)length;
- (BOOL)download:(NSURLDownload *)download shouldDecodeSourceDataOfMIMEType:(NSString *)encodingType;
- (void)download:(NSURLDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename;
- (void)download:(NSURLDownload *)download didCreateDestination:(NSString *)path;
- (void)downloadDidFinish:(NSURLDownload *)download;
- (void)download:(NSURLDownload *)download didFailWithError:(NSError *)error;
@end

@interface NSURLDownload ()
- (instancetype)initWithRequest:(NSURLRequest *)request delegate:(id <NSURLDownloadDelegate>)delegate;
- (instancetype)initWithResumeData:(NSData *)resumeData delegate:(id <NSURLDownloadDelegate>)delegate path:(NSString *)path;
- (void)cancel;
- (void)setDestination:(NSString *)path allowOverwrite:(BOOL)allowOverwrite;
@property (readonly, copy) NSURLRequest *request;
@property (readonly, copy) NSData *resumeData;
@property BOOL deletesFileUponFailure;
@end

#endif

#if !USE(APPLE_INTERNAL_SDK)

@class NSURLConnectionDelegateProxy;

@interface NSURLDownload ()
+ (id)_downloadWithRequest:(NSURLRequest *)request delegate:(id)delegate directory:(NSString *)directory;
+ (id)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)response delegate:(id)delegate proxy:(id)proxy;
- (id)_initWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)response delegate:(id)delegate proxy:(NSURLConnectionDelegateProxy *)proxy;
- (id)_initWithRequest:(NSURLRequest *)request delegate:(id)delegate directory:(NSString *)directory;
@end

#endif
