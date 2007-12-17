/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#import <WebKit/WebNetscapePlugInStreamLoaderClient.h>
#import <WebCore/NetscapePlugInStreamLoader.h>
#import <WebCore/ResourceResponse.h>

using namespace WebCore;

void WebNetscapePlugInStreamLoaderClient::didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse& theResponse)
{
    [m_stream.get() startStreamWithResponse:theResponse.nsURLResponse()];
}

void WebNetscapePlugInStreamLoaderClient::didReceiveData(NetscapePlugInStreamLoader*, const char* data, int length)
{
    NSData *nsData = [[NSData alloc] initWithBytesNoCopy:(void*)data length:length freeWhenDone:NO];
    [m_stream.get() receivedData:nsData];
    [nsData release];
}

void WebNetscapePlugInStreamLoaderClient::didFail(NetscapePlugInStreamLoader*, const ResourceError& error)
{
    [m_stream.get() destroyStreamWithError:error];
    m_stream = 0;
}

void WebNetscapePlugInStreamLoaderClient::didFinishLoading(NetscapePlugInStreamLoader*)
{
    [m_stream.get() finishedLoading];
    m_stream = 0;
}
