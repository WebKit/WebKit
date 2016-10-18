/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include <functional>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CaptureDevice;
class Document;
class SecurityOrigin;

class MediaDevicesEnumerationRequest final : public ContextDestructionObserver, public RefCounted<MediaDevicesEnumerationRequest> {
public:
    using CompletionHandler = std::function<void(const Vector<CaptureDevice>&, const String& deviceIdentifierHashSalt, bool originHasPersistentAccess)>;

    static Ref<MediaDevicesEnumerationRequest> create(Document&, CompletionHandler&&);

    virtual ~MediaDevicesEnumerationRequest();

    void start();
    void cancel();

    WEBCORE_EXPORT void setDeviceInfo(const Vector<CaptureDevice>&, const String& deviceIdentifierHashSalt, bool originHasPersistentAccess);

    WEBCORE_EXPORT SecurityOrigin* userMediaDocumentOrigin() const;
    WEBCORE_EXPORT SecurityOrigin* topLevelDocumentOrigin() const;

private:
    MediaDevicesEnumerationRequest(ScriptExecutionContext&, CompletionHandler&&);

    // ContextDestructionObserver
    void contextDestroyed() final;

    CompletionHandler m_completionHandler;
    Vector<CaptureDevice> m_deviceList;
    String m_deviceIdentifierHashSalt;
    bool m_originHasPersistentAccess { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
