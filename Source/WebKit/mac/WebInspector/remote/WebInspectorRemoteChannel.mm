/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ENABLE(REMOTE_INSPECTOR)

#import "WebInspectorRemoteChannel.h"

#import "WebInspectorClient.h"
#import "WebInspectorClientRegistry.h"
#import "WebInspectorServerWebViewConnection.h"
#import "WebKitLogging.h"
#import <wtf/text/CString.h>

@interface WebInspectorRemoteChannel ()
- (id)initWithRemote:(WebInspectorServerWebViewConnection *)remote local:(WebInspectorClient*)local;
@end

@implementation WebInspectorRemoteChannel

+ (WebInspectorRemoteChannel *)createChannelForPageId:(unsigned)pageId connection:(WebInspectorServerWebViewConnection *)connection
{
    // Get the Inspector client. Its possible the page closed before the remote client requested it.
    WebInspectorClient *inspectorClient = [[WebInspectorClientRegistry sharedRegistry] clientForPageId:pageId];
    if (!inspectorClient) {
        LOG(RemoteInspector, "WebInspectorRemoteChannel: Page requested no longer exists: (%u)", pageId);
        return nil;
    }

    // The WebView does not allow remote inspection.
    if (!inspectorClient->canBeRemotelyInspected()) {
        LOG(RemoteInspector, "WebInspectorRemoteChannel: WebView for Page (%u) does not allow remote inspection.", pageId);
        return nil;
    }

    // The channel is released by whichever side closes the channel. The channel could be closed
    // remotely by the socket closing (the user navigating away from the remote inspector page).
    // It could be closed locally by the Page being destroyed, like the browser or tab being closed.
    // Setting up the connection can fail if there is already an existing session.
    WebInspectorRemoteChannel *channel = [[WebInspectorRemoteChannel alloc] initWithRemote:connection local:inspectorClient];
    if (!inspectorClient->setupRemoteConnection(channel)) {
        LOG(RemoteInspector, "WebInspectorRemoteChannel: Setting up a remote session failed. Probably already a local session.");
        [channel release];
        channel = nil;
        return nil;
    }

    return channel;
}

- (id)initWithRemote:(WebInspectorServerWebViewConnection *)remote local:(WebInspectorClient*)local
{
    self = [super init];
    if (!self)
        return nil;

    _remote = remote;
    _local = local;

    return self;
}

- (void)closeFromLocalSide
{
    // Local side will release. Make sure the remote side clears its reference to the channel.
    [_remote clearChannel];
}

- (void)closeFromRemoteSide
{
    // Remote side will release. Make sure the local side clears its reference to the channel.
    _local->teardownRemoteConnection(false);
}

- (void)sendMessageToFrontend:(NSString *)message
{
    [_remote sendMessageToFrontend:message];
}

- (void)sendMessageToBackend:(NSString *)message
{
    _local->sendMessageToBackend(message);
}

@end

#endif
