/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "FrameLoaderClient.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

class FrameLoadRequest;
class IntSize;
class SecurityOriginData;

enum class RenderAsTextFlag : uint16_t;

struct MessageWithMessagePorts;

class RemoteFrameClient : public FrameLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual void frameDetached() = 0;
    virtual void sizeDidChange(IntSize) = 0;
    virtual void postMessageToRemote(FrameIdentifier source, const String& sourceOrigin, FrameIdentifier target, std::optional<SecurityOriginData> targetOrigin, const MessageWithMessagePorts&) = 0;
    virtual void changeLocation(FrameLoadRequest&&) = 0;
    virtual String renderTreeAsText(size_t baseIndent, OptionSet<RenderAsTextFlag>) = 0;
    virtual void broadcastFrameRemovalToOtherProcesses() = 0;
    virtual void closePage() = 0;
    virtual void focus() = 0;
    virtual void unfocus() = 0;
    virtual ~RemoteFrameClient() { }
};

}
