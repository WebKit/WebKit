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

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "Supplementable.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;
class Document;
class UserGestureIndicator;

enum class StorageAccessWasGranted : bool {
    No,
    Yes
};

enum class StorageAccessPromptWasShown : bool {
    No,
    Yes
};

const unsigned maxNumberOfTimesExplicitlyDeniedFrameSpecificStorageAccess = 2;

class DocumentStorageAccess final : public Supplement<Document>, public CanMakeWeakPtr<DocumentStorageAccess> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DocumentStorageAccess(Document&);
    ~DocumentStorageAccess();

    static void hasStorageAccess(Document&, Ref<DeferredPromise>&&);
    static void requestStorageAccess(Document&, Ref<DeferredPromise>&&);

private:
    void hasStorageAccess(Ref<DeferredPromise>&&);
    void requestStorageAccess(Ref<DeferredPromise>&&);
    static DocumentStorageAccess* from(Document&);
    static const char* supplementName();
    bool hasFrameSpecificStorageAccess() const;
    void setWasExplicitlyDeniedFrameSpecificStorageAccess() { ++m_numberOfTimesExplicitlyDeniedFrameSpecificStorageAccess; };
    bool isAllowedToRequestFrameSpecificStorageAccess() { return m_numberOfTimesExplicitlyDeniedFrameSpecificStorageAccess < maxNumberOfTimesExplicitlyDeniedFrameSpecificStorageAccess; };
    void enableTemporaryTimeUserGesture();
    void consumeTemporaryTimeUserGesture();

    std::unique_ptr<UserGestureIndicator> m_temporaryUserGesture;
    
    Document& m_document;

    uint8_t m_numberOfTimesExplicitlyDeniedFrameSpecificStorageAccess = 0;
};

} // namespace WebCore

#endif // ENABLE(RESOURCE_LOAD_STATISTICS)
