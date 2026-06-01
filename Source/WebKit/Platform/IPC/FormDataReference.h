/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Decoder.h"
#include "Encoder.h"
#include "Logging.h"
#include "SandboxExtension.h"
#include <WebCore/FormData.h>

namespace IPC {

class FormDataReference {
public:
    FormDataReference() = default;
    explicit FormDataReference(RefPtr<WebCore::FormData>&& data)
        : m_data(WTF::move(data))
    {
    }

    FormDataReference(RefPtr<WebCore::FormData>&&, Vector<WebKit::SandboxExtensionHandle>&&);

    RefPtr<WebCore::FormData> data() const { return m_data.get(); }
    RefPtr<WebCore::FormData> takeData() { return WTF::move(m_data); }

    Vector<WebKit::SandboxExtensionHandle> sandboxExtensionHandles() const;

    static bool sandboxExtensionsAreSufficient(const RefPtr<WebCore::FormData>&, const Vector<WebKit::SandboxExtensionHandle>&);

private:
    RefPtr<WebCore::FormData> m_data;
};

inline bool FormDataReference::sandboxExtensionsAreSufficient(const RefPtr<WebCore::FormData>& data, const Vector<WebKit::SandboxExtensionHandle>& sandboxExtensionHandles)
{
    return sandboxExtensionHandles.size() == (data ? data->filesCount() : 0);
}

inline FormDataReference::FormDataReference(RefPtr<WebCore::FormData>&& data, Vector<WebKit::SandboxExtensionHandle>&& sandboxExtensionHandles)
    : m_data(WTF::move(data))
{
    if (m_data && !sandboxExtensionsAreSufficient(m_data, sandboxExtensionHandles)) {
        RELEASE_LOG_ERROR(IPC, "FormDataReference: dropping body with %u file element(s) but %zu sandbox extension(s)", m_data->filesCount(), sandboxExtensionHandles.size());
        m_data = nullptr;
    }

    if (m_data && !WebKit::SandboxExtension::consumePermanently(sandboxExtensionHandles)) {
        RELEASE_LOG_ERROR(IPC, "FormDataReference: dropping body because a file sandbox extension could not be consumed");
        m_data = nullptr;
    }
}

inline Vector<WebKit::SandboxExtensionHandle> FormDataReference::sandboxExtensionHandles() const
{
    if (!m_data)
        return { };

    return WTF::compactMap(m_data->elements(), [](auto& element) -> std::optional<WebKit::SandboxExtensionHandle> {
        if (auto* fileData = std::get_if<WebCore::FormDataElement::EncodedFileData>(&element.data)) {
            const String& path = fileData->filename;
            if (auto handle = WebKit::SandboxExtension::createHandle(path, WebKit::SandboxExtension::Type::ReadOnly))
                return { WTF::move(*handle) };
        }
        return std::nullopt;
    });
}

} // namespace IPC
