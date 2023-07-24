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

#include "APIObject.h"
#include "DataTaskIdentifier.h"
#include <pal/SessionID.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class NetworkProcessProxy;
class WebPageProxy;
}

namespace API {

class DataTaskClient;

class DataTask : public API::ObjectImpl<API::Object::Type::DataTask> {
public:

    template<typename... Args> static Ref<DataTask> create(Args&&... args)
    {
        return adoptRef(*new DataTask(std::forward<Args>(args)...));
    }
    ~DataTask();

    void cancel();

    WebKit::WebPageProxy* page() { return m_page.get(); }
    const WTF::URL& originalURL() const { return m_originalURL; }
    const DataTaskClient& client() const { return m_client.get(); }
    void setClient(Ref<DataTaskClient>&&);
    void networkProcessCrashed();

private:
    DataTask(WebKit::DataTaskIdentifier, WeakPtr<WebKit::WebPageProxy>&&, WTF::URL&&);

    WebKit::DataTaskIdentifier m_identifier;
    WeakPtr<WebKit::WebPageProxy> m_page;
    WTF::URL m_originalURL;
    WeakPtr<WebKit::NetworkProcessProxy> m_networkProcess;
    std::optional<PAL::SessionID> m_sessionID;
    Ref<DataTaskClient> m_client;
};

} // namespace API
