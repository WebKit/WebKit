/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FormDataReference.h"

#if PLATFORM(COCOA)
#include "PathsBlockedForSandboxExtensions.h"
#endif

namespace IPC {

FormDataReference::FormDataReference(RefPtr<WebCore::FormData>&& data, Vector<WebKit::SandboxExtensionHandle>&& sandboxExtensionHandles)
    : m_data(WTF::move(data))
{
    if (!m_data)
        return;

    unsigned fileCount = 0;
    for (auto& element : m_data->elements()) {
        if (auto* fileData = std::get_if<WebCore::FormDataElement::EncodedFileData>(&element.data)) {
            ++fileCount;
#if PLATFORM(COCOA)
            const String& path = fileData->filename;
            if (WebKit::pathIsBlockedForSandboxExtensions(path)) {
                RELEASE_LOG(Process, "Form data file path was blocked for sandbox extension: %{private}s", path.utf8().data());
                m_data = nullptr;
                break;
            }
#else
            UNUSED_VARIABLE(fileData);
#endif // !PLATFORM(COCOA)
        }
    }

    if (!m_data)
        return;

    unsigned consumedExtensions = 0;
    for (auto& handle : sandboxExtensionHandles) {
        if (WebKit::SandboxExtension::consumePermanently(handle))
            consumedExtensions++;
    }

    if (consumedExtensions != fileCount) {
        RELEASE_LOG_ERROR(IPC, "FormDataReference: dropping body because a file sandbox extension could not be consumed");
        m_data = nullptr;
    }
}

Vector<WebKit::SandboxExtensionHandle> FormDataReference::sandboxExtensionHandles() const
{
    if (!m_data)
        return { };

    return WTF::compactMap(m_data->elements(), [](auto& element) -> std::optional<WebKit::SandboxExtensionHandle> {
        if (auto* fileData = std::get_if<WebCore::FormDataElement::EncodedFileData>(&element.data)) {
            const String& path = fileData->filename;
#if PLATFORM(COCOA)
            if (WebKit::pathIsBlockedForSandboxExtensions(path)) {
                RELEASE_LOG(Process, "Form data file path was blocked for sandbox extension: %{private}s", path.utf8().data());
                return std::nullopt;
            }
#endif // PLATFORM(COCOA)
            if (auto handle = WebKit::SandboxExtension::createHandle(path, WebKit::SandboxExtension::Type::ReadOnly))
                return { WTF::move(*handle) };
        }
        return std::nullopt;
    });
}

} // namespace IPC
