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

#if ENABLE(WEB_AUTHN)

#include <WebCore/FidoConstants.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class CtapDriver : public CanMakeWeakPtr<CtapDriver> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CtapDriver);
public:
    using ResponseCallback = Function<void(Vector<uint8_t>&&)>;

    virtual ~CtapDriver() = default;

    void setProtocol(fido::ProtocolVersion protocol) { m_protocol = protocol; }

    virtual void transact(Vector<uint8_t>&& data, ResponseCallback&&) = 0;
    virtual void cancel() { };

protected:
    CtapDriver() = default;
    fido::ProtocolVersion protocol() const { return m_protocol; }

private:
    fido::ProtocolVersion m_protocol { fido::ProtocolVersion::kCtap };
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
