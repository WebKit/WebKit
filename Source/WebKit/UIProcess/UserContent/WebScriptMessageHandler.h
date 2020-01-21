/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "APIUserContentWorld.h"
#include "WebUserContentControllerDataTypes.h"
#include <wtf/Identified.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
struct SecurityOriginData;
class SerializedScriptValue;
}

namespace API {
class UserContentWorld;
}

namespace WebKit {

class WebPageProxy;
class WebFrameProxy;
struct FrameInfoData;

class WebScriptMessageHandler : public RefCounted<WebScriptMessageHandler>, public Identified<WebScriptMessageHandler>  {
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void didPostMessage(WebPageProxy&, FrameInfoData&&, WebCore::SerializedScriptValue&) = 0;
    };

    static Ref<WebScriptMessageHandler> create(std::unique_ptr<Client>, const String& name, API::UserContentWorld&);
    virtual ~WebScriptMessageHandler();

    String name() const { return m_name; }

    API::ContentWorldBase& world() { return m_world.get(); }

    Client& client() const { return *m_client; }

private:
    WebScriptMessageHandler(std::unique_ptr<Client>, const String&, API::UserContentWorld&);

    std::unique_ptr<Client> m_client;
    String m_name;
    Ref<API::UserContentWorld> m_world;
};

} // namespace API
