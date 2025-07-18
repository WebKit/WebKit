/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "_WKNodeInfoInternal.h"

#import "WKFrameInfoInternal.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>

@implementation _WKNodeInfo

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKNodeInfo.class, self))
        return;
    _info->API::NodeInfo::~NodeInfo();
    [super dealloc];
}

- (void)contentFrameInfo:(void (^)(WKFrameInfo *))completionHandler
{
    // FIXME: This should probably proactively fetch and just expose a property.
    auto& frameID = _info->info().contentFrameIdentifier;
    if (!frameID)
        return completionHandler(nil);
    RefPtr webFrame = WebKit::WebFrameProxy::webFrame(*frameID);
    if (!webFrame)
        return completionHandler(nil);
    webFrame->getFrameInfo([completionHandler = makeBlockPtr(completionHandler), page = RefPtr { webFrame->page() }] (std::optional<WebKit::FrameInfoData>&& data) mutable {
        if (!data)
            return completionHandler(nil);
        Ref frameInfo = API::FrameInfo::create(WTFMove(*data), WTFMove(page));
        RetainPtr frameInfoData = wrapper(frameInfo);
        completionHandler(frameInfoData.get());
    });
}

- (API::Object&)_apiObject
{
    return *_info;
}

@end
