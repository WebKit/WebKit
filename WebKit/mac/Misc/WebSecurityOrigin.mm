/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#import "WebSecurityOriginPrivate.h"

#import "WebSecurityOriginInternal.h"
#import <WebCore/SecurityOriginData.h>

@interface WebSecurityOriginPrivate : NSObject {
@public
    NSString *protocol;
    NSString *domain;
    unsigned short port;
}
- (id)initWithProtocol:(NSString *)protocol domain:(NSString *)domain port:(unsigned short)port;
@end

@implementation WebSecurityOriginPrivate

- (id)initWithProtocol:(NSString *)theProtocol domain:(NSString *)theDomain port:(unsigned short)thePort
{
    self = [super init];
    if (!self)
        return nil;
    
    protocol = [theProtocol copy];
    domain = [theDomain copy];
    port = thePort;

    return self;
}

- (void)dealloc
{
    [protocol release];
    [domain release];
    [super dealloc];
}

@end

@implementation WebSecurityOrigin


- (id)initWithProtocol:(NSString *)protocol domain:(NSString *)domain
{
    return [self initWithProtocol:protocol domain:domain port:0];
}

- (id)initWithProtocol:(NSString *)protocol domain:(NSString *)domain port:(unsigned short)port
{
    self = [super init];
    if (!self)
        return nil;
    
    _private = [[WebSecurityOriginPrivate alloc] initWithProtocol:protocol domain:domain port:port];

    return self;
}

- (NSString*)protocol
{
    return [[_private->protocol retain] autorelease];
}

- (NSString*)domain
{
    return [[_private->domain retain] autorelease];
}

- (unsigned short)port
{
    return _private->port;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

@end

@implementation WebSecurityOrigin (WebInternal)

- (id)_initWithWebCoreSecurityOriginData:(WebCore::SecurityOriginData *)securityOriginData
{
    ASSERT(securityOriginData);
    return [self initWithProtocol:securityOriginData->protocol() domain:securityOriginData->host() port:securityOriginData->port()];
}

@end
