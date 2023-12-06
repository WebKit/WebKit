/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include "ArgumentCodersCocoa.h"
#include <Foundation/Foundation.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class CoreIPCPersonNameComponents {
WTF_MAKE_FAST_ALLOCATED;
public:
    CoreIPCPersonNameComponents(NSPersonNameComponents *);

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCPersonNameComponents, void>;

    CoreIPCPersonNameComponents(const String& namePrefix, const String& givenName, const String& middleName, const String& familyName, const String& nickname, std::unique_ptr<CoreIPCPersonNameComponents>&& phoneticRepresentation)
        : m_namePrefix(namePrefix)
        , m_givenName(givenName)
        , m_middleName(middleName)
        , m_familyName(familyName)
        , m_nickname(nickname)
        , m_phoneticRepresentation(WTFMove(phoneticRepresentation))
    {
    }

    String m_namePrefix;
    String m_givenName;
    String m_middleName;
    String m_familyName;
    String m_nickname;
    std::unique_ptr<CoreIPCPersonNameComponents> m_phoneticRepresentation;
};

} // namespace WebKit

#endif // PLATFORM(COCOA)
