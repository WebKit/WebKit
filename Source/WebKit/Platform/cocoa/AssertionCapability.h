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

#if ENABLE(EXTENSION_CAPABILITIES)

#include "ExtensionCapability.h"
#include <wtf/BlockPtr.h>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class AssertionCapability final : public ExtensionCapability {
public:
    AssertionCapability(String environmentIdentifier, String domain, String name, Function<void()>&& willInvalidateFunction = nullptr, Function<void()>&& didInvalidateFunction = nullptr);

    const String& domain() const { return m_domain; }
    const String& name() const { return m_name; }

    // ExtensionCapability
    String environmentIdentifier() const final { return m_environmentIdentifier; }
    RetainPtr<_SECapabilities> platformCapability() const final;

private:
    String m_environmentIdentifier;
    String m_domain;
    String m_name;
    BlockPtr<void()> m_willInvalidateBlock;
    BlockPtr<void()> m_didInvalidateBlock;
};

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
