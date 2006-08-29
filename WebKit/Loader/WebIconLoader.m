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
#import <WebKit/WebIconLoader.h>

#import <JavaScriptCore/Assertions.h>
#import <WebKit/WebFrameLoader.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconDatabaseBridge.h>
#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebKitLogging.h>

#import <WebKit/WebNSURLExtras.h>

@interface WebIconLoaderPrivate : NSObject
{
@public
    NSURLRequest *initialRequest;
}

@end;

@implementation WebIconLoaderPrivate

- (void)dealloc
{
    [initialRequest release];
    [super dealloc];
}

@end;

@implementation WebIconLoader

- (id)initWithRequest:(NSURLRequest *)initialRequest;
{
    ASSERT([[WebIconDatabase sharedIconDatabase] _isEnabled]);
    [super init];
    _private = [[WebIconLoaderPrivate alloc] init];
    _private->initialRequest = [initialRequest copy];
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSURL *)URL
{
    return [_private->initialRequest URL];
}

- (void)startLoading
{
    [self loadWithRequest:_private->initialRequest];
}

- (void)stopLoading
{
    [self cancel];
}

- (void)didFinishLoading
{
    NSData *data;
        
    id response = [self response];
    if ([response isKindOfClass:[NSHTTPURLResponse class]] && ([response statusCode] < 200 || [response statusCode] > 299))
        data = nil;
    else
        data = [self resourceData];
    
    if (data) 
        [[WebIconDatabase sharedIconDatabase] _setIconData:data forIconURL:[[self URL] _web_originalDataAsString]];
    else 
        [[WebIconDatabase sharedIconDatabase] _setHaveNoIconForIconURL:[[self URL] _web_originalDataAsString]];
    
    [frameLoader _iconLoaderReceivedPageIcon:[self URL]];
    [super didFinishLoading];
}

- (NSURLRequest *)willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse;
{
    ASSERT(!reachedTerminalState);
    
    // We now allow a WebIconLoader without an associated WebDataSource for kicking off a contextless icon load,
    // so we override this special case in WebIconLoader only.  If we passed this through to the super class version,
    // it would pass back a nil Request so the icon wouldn't actually load because it is designed to be part of a page load
    if (!frameLoader) 
        return newRequest;
    return [super willSendRequest:newRequest redirectResponse:redirectResponse];
}

// We don't ever want to prompt for authentication just for a site icon, so
// override this WebBaseResourceDelegate method to refuse the challenge
- (void)didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    [[challenge sender] cancelAuthenticationChallenge:challenge];
}

- (void)didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
}

@end
