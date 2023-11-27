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

#include <WebCore/SWRegistrationDatabase.h>
#include <wtf/Forward.h>

namespace WebKit {

class ServiceWorkerStorageManager {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ServiceWorkerStorageManager(const String& path);

    void closeFiles();
    void clearAllRegistrations();
    std::optional<Vector<WebCore::ServiceWorkerContextData>> importRegistrations();
    std::optional<Vector<WebCore::ServiceWorkerScripts>> updateRegistrations(const Vector<WebCore::ServiceWorkerContextData>&, const Vector<WebCore::ServiceWorkerRegistrationKey>&);

private:
    WebCore::SWRegistrationDatabase* ensureDatabase();

    String m_path;
    std::unique_ptr<WebCore::SWRegistrationDatabase> m_database;
};

} // namespace WebKit
