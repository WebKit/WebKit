/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_RTC)

#include <WebCore/MDNSRegisterError.h>
#include <WebCore/ProcessQualified.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class WebMDNSRegister : public CanMakeWeakPtr<WebMDNSRegister> {
public:
    WebMDNSRegister() = default;

    void unregisterMDNSNames(WebCore::ScriptExecutionContextIdentifier);
    void registerMDNSName(WebCore::ScriptExecutionContextIdentifier, const String& ipAddress, CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)>&&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

private:
    void finishedRegisteringMDNSName(WebCore::ScriptExecutionContextIdentifier, const String& ipAddress, String&& mdnsName, std::optional<WebCore::MDNSRegisterError>, CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)>&&);

    HashMap<WebCore::ScriptExecutionContextIdentifier, HashMap<String, String>> m_registeringDocuments;
};

} // namespace WebKit

#endif // ENABLE(WEB_RTC)
