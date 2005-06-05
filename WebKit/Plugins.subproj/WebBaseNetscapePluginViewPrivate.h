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

#import <WebBaseNetscapePluginView.h>

@class NSURLRequest;

@interface WebBaseNetscapePluginView (WebNPPCallbacks)

- (NPError)loadRequest:(NSURLRequest *)request inTarget:(NSString *)target withNotifyData:(void *)notifyData sendNotification:(BOOL)sendNotification;
- (NPError)getURLNotify:(const char *)URL target:(const char *)target notifyData:(void *)notifyData;
- (NPError)getURL:(const char *)URL target:(const char *)target;
- (NPError)postURLNotify:(const char *)URL target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData;
- (NPError)postURL:(const char *)URL target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file;
- (NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream;
- (NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer;
- (NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason;
- (void)status:(const char *)message;
- (const char *)userAgent;
- (void)invalidateRect:(NPRect *)invalidRect;
- (void)invalidateRegion:(NPRegion)invalidateRegion;
- (void)forceRedraw;
- (NPError)getVariable:(NPNVariable)variable value:(void *)value;

@end
