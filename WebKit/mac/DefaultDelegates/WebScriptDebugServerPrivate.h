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

#import "WebScriptDebugServer.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
#define WebNSUInteger unsigned int
#else
#define WebNSUInteger NSUInteger
#endif

@interface WebScriptDebugServer : NSObject <WebScriptDebugServer> {
@private
    NSConnection *serverConnection;
    NSString *serverName;
    NSMutableSet *listeners;
    BOOL inCallback;
    BOOL paused;
    BOOL step;
}
+ (WebScriptDebugServer *)sharedScriptDebugServer;
+ (unsigned)listenerCount;

- (void)attachScriptDebuggerToAllWebViews;
- (void)detachScriptDebuggerFromAllWebViews;

- (void)webView:(WebView *)webView didLoadMainResourceForDataSource:(WebDataSource *)dataSource;

- (void)webView:(WebView *)webView       didParseSource:(NSString *)source
                                         baseLineNumber:(WebNSUInteger)lineNumber
                                                fromURL:(NSURL *)url
                                               sourceId:(int)sid
                                            forWebFrame:(WebFrame *)webFrame;

- (void)webView:(WebView *)webView  failedToParseSource:(NSString *)source
                                         baseLineNumber:(WebNSUInteger)lineNumber
                                                fromURL:(NSURL *)url
                                              withError:(NSError *)error
                                            forWebFrame:(WebFrame *)webFrame;

- (void)webView:(WebView *)webView    didEnterCallFrame:(WebScriptCallFrame *)frame
                                               sourceId:(int)sid
                                                   line:(int)lineno
                                            forWebFrame:(WebFrame *)webFrame;

- (void)webView:(WebView *)webView willExecuteStatement:(WebScriptCallFrame *)frame
                                               sourceId:(int)sid
                                                   line:(int)lineno
                                            forWebFrame:(WebFrame *)webFrame;

- (void)webView:(WebView *)webView   willLeaveCallFrame:(WebScriptCallFrame *)frame
                                               sourceId:(int)sid
                                                   line:(int)lineno
                                            forWebFrame:(WebFrame *)webFrame;
@end

#undef WebNSUInteger
