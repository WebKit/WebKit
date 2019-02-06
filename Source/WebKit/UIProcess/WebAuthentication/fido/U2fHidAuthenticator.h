/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include "Authenticator.h"
#include <wtf/RunLoop.h>

namespace apdu {
class ApduResponse;
}

namespace WebKit {

class CtapHidDriver;

class U2fHidAuthenticator final : public Authenticator {
public:
    static Ref<U2fHidAuthenticator> create(std::unique_ptr<CtapHidDriver>&& driver)
    {
        return adoptRef(*new U2fHidAuthenticator(WTFMove(driver)));
    }

private:
    explicit U2fHidAuthenticator(std::unique_ptr<CtapHidDriver>&&);

    void makeCredential() final;
    void checkExcludeList(size_t index);
    void issueRegisterCommand();
    void getAssertion() final;
    void issueSignCommand(size_t index);

    enum class CommandType : uint8_t {
        RegisterCommand,
        CheckOnlyCommand,
        BogusCommand,
        SignCommand
    };
    void issueNewCommand(Vector<uint8_t>&& command, CommandType);
    void retryLastCommand() { issueCommand(m_lastCommand, m_lastCommandType); }
    void issueCommand(const Vector<uint8_t>& command, CommandType);
    void responseReceived(Vector<uint8_t>&& response, CommandType);
    void continueRegisterCommandAfterResponseReceived(apdu::ApduResponse&&);
    void continueCheckOnlyCommandAfterResponseReceived(apdu::ApduResponse&&);
    void continueBogusCommandAfterResponseReceived(apdu::ApduResponse&&);
    void continueSignCommandAfterResponseReceived(apdu::ApduResponse&&);

    std::unique_ptr<CtapHidDriver> m_driver;
    RunLoop::Timer<U2fHidAuthenticator> m_retryTimer;
    Vector<uint8_t> m_lastCommand;
    CommandType m_lastCommandType;
    size_t m_nextListIndex { 0 };
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
