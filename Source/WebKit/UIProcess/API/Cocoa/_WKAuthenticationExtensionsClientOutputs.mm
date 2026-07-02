/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "_WKAuthenticationExtensionsClientOutputs.h"

#import <wtf/RetainPtr.h>

@implementation _WKAuthenticationExtensionsClientOutputs {
    RetainPtr<NSData> _prfFirst;
    RetainPtr<NSData> _prfSecond;
    RetainPtr<_WKAuthenticationExtensionsLargeBlobOutputs> _largeBlob;
}

- (instancetype)initWithAppid:(BOOL)appid
{
    if (!(self = [super init]))
        return nil;

    _appid = appid;
    _prfEnabled = NO;
    return self;
}

- (instancetype)initWithAppid:(BOOL)appid prfEnabled:(BOOL)prfEnabled prfFirst:(NSData *)prfFirst prfSecond:(NSData *)prfSecond
{
    if (!(self = [super init]))
        return nil;

    _appid = appid;
    _prfEnabled = prfEnabled;
    _prfFirst = prfFirst;
    _prfSecond = prfSecond;
    return self;
}

- (instancetype)initWithAppid:(BOOL)appid prfEnabled:(BOOL)prfEnabled prfFirst:(NSData *)prfFirst prfSecond:(NSData *)prfSecond largeBlob:(_WKAuthenticationExtensionsLargeBlobOutputs *)largeBlob
{
    if (!(self = [self initWithAppid:appid prfEnabled:prfEnabled prfFirst:prfFirst prfSecond:prfSecond]))
        return nil;

    _largeBlob = largeBlob;
    return self;
}

- (NSData *)prfFirst
{
    return _prfFirst.get();
}

- (NSData *)prfSecond
{
    return _prfSecond.get();
}

- (id<_WKAuthenticationExtensionsLargeBlobOutputsStaging>)largeBlob
{
    return _largeBlob.get();
}

- (void)dealloc
{
    [super dealloc];
}

@end

@implementation _WKAuthenticationExtensionsLargeBlobOutputs {
    RetainPtr<NSData> _blob;
}

- (instancetype)initWithSupported:(BOOL)supported blob:(NSData *)blob written:(BOOL)written
{
    if (!(self = [super init]))
        return nil;

    _supported = supported;
    _blob = blob;
    _written = written;
    return self;
}

- (NSData *)blob
{
    return _blob.get();
}

- (void)dealloc
{
    [super dealloc];
}

@end
