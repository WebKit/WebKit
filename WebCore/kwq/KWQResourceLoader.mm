/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#import "KWQResourceLoader.h"

#import "TransferJob.h"
#import "KURL.h"
#import "loader.h"
#import <kxmlcore/Assertions.h>

using namespace WebCore;

@implementation KWQResourceLoader

- (id)initWithJob:(TransferJob *)job;
{
    [super init];

    _job = job;

    job->setLoader(self);

    return self;
}

- (void)setHandle:(id <WebCoreResourceHandle>)handle
{
    ASSERT(_handle == nil);
    _handle = [handle retain];
}

- (void)receivedResponse:(NSURLResponse *)response
{
    ASSERT(response);
    ASSERT(_job);
    _job->receivedResponse(response);
}

- (void)redirectedToURL:(NSURL *)url
{
    ASSERT(url);
    ASSERT(_job);
    if (TransferJobClient* client = _job->client())
        client->receivedRedirect(_job, KURL(url));
}

- (void)addData:(NSData *)data
{
    ASSERT(data);
    ASSERT(_job);
    if (TransferJobClient* client = _job->client())
        client->receivedData(_job, (const char *)[data bytes], [data length]);
}

- (void)jobWillBeDeallocated
{
    id <WebCoreResourceHandle> handle = _handle;
    _job = 0;
    _handle = nil;

    [handle cancel];
    [handle release];
}

- (void)finishJobAndHandle:(NSData *)data
{
    TransferJob* job = _job;
    id <WebCoreResourceHandle> handle = _handle;
    _job = 0;
    _handle = nil;

    if (job) {
        if (TransferJobClient* client = job->client()) {
            client->receivedAllData(job, data);
            client->receivedAllData(job);
        }
        delete job;
    }
    [handle release];
}

- (void)jobCanceledLoad
{
    [_handle cancel];
}

- (void)cancel
{
    if (_job) {
        _job->setError(1);
    }
    [self finishJobAndHandle:nil];
}

- (void)reportError
{
    ASSERT(_job);
    _job->setError(1);
    [self finishJobAndHandle:nil];
}

- (void)finishWithData:(NSData *)data
{
    ASSERT(_job);
    ASSERT(_handle);
    [self finishJobAndHandle:data];
}

@end
