/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#if ENABLE(TRACKING_PREVENTION)

#include "RegistrableDomain.h"
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

enum class StorageAccessScope : bool {
    PerFrame,
    PerPage
};

enum class StorageAccessQuickResult : bool {
    Grant,
    Reject
};

struct RequestStorageAccessResult {
    StorageAccessWasGranted wasGranted;
    StorageAccessPromptWasShown promptWasShown;
    StorageAccessScope scope;
    RegistrableDomain topFrameDomain;
    RegistrableDomain subFrameDomain;
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<RequestStorageAccessResult> decode(Decoder&);
};

const unsigned maxNumberOfTimesExplicitlyDeniedStorageAccess = 2;

class DocumentStorageAccess final : public Supplement<Document>, public CanMakeWeakPtr<DocumentStorageAccess> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DocumentStorageAccess(Document&);
    ~DocumentStorageAccess();

    static void hasStorageAccess(Document&, Ref<DeferredPromise>&&);
    static bool hasStorageAccessForDocumentQuirk(Document&);

    static void requestStorageAccess(Document&, Ref<DeferredPromise>&&);
    static void requestStorageAccessForDocumentQuirk(Document&, CompletionHandler<void(StorageAccessWasGranted)>&&);
    static void requestStorageAccessForNonDocumentQuirk(Document& hostingDocument, RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&&);

private:
    std::optional<bool> hasStorageAccessQuickCheck();
    void hasStorageAccess(Ref<DeferredPromise>&&);
    bool hasStorageAccessQuirk();

    std::optional<StorageAccessQuickResult> requestStorageAccessQuickCheck();
    void requestStorageAccess(Ref<DeferredPromise>&&);
    void requestStorageAccessForDocumentQuirk(CompletionHandler<void(StorageAccessWasGranted)>&&);
    void requestStorageAccessForNonDocumentQuirk(RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&&);
    void requestStorageAccessQuirk(RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&&);

    static DocumentStorageAccess* from(Document&);
    static const char* supplementName();
    bool hasFrameSpecificStorageAccess() const;
    void setWasExplicitlyDeniedFrameSpecificStorageAccess() { ++m_numberOfTimesExplicitlyDeniedFrameSpecificStorageAccess; };
    bool isAllowedToRequestStorageAccess() { return m_numberOfTimesExplicitlyDeniedFrameSpecificStorageAccess < maxNumberOfTimesExplicitlyDeniedStorageAccess; };
    void enableTemporaryTimeUserGesture();
    void consumeTemporaryTimeUserGesture();

    std::unique_ptr<UserGestureIndicator> m_temporaryUserGesture;
    
    Document& m_document;

    uint8_t m_numberOfTimesExplicitlyDeniedFrameSpecificStorageAccess = 0;

    StorageAccessScope m_storageAccessScope = StorageAccessScope::PerPage;
};

template<class Encoder>
void RequestStorageAccessResult::encode(Encoder& encoder) const
{
    encoder << wasGranted << promptWasShown << scope << topFrameDomain << subFrameDomain;
}

template<class Decoder>
std::optional<RequestStorageAccessResult> RequestStorageAccessResult::decode(Decoder& decoder)
{
    std::optional<StorageAccessWasGranted> wasGranted;
    decoder >> wasGranted;
    if (!wasGranted)
        return std::nullopt;

    std::optional<StorageAccessPromptWasShown> promptWasShown;
    decoder >> promptWasShown;
    if (!promptWasShown)
        return std::nullopt;

    std::optional<StorageAccessScope> scope;
    decoder >> scope;
    if (!scope)
        return std::nullopt;

    std::optional<RegistrableDomain> topFrameDomain;
    decoder >> topFrameDomain;
    if (!topFrameDomain)
        return std::nullopt;

    std::optional<RegistrableDomain> subFrameDomain;
    decoder >> subFrameDomain;
    if (!subFrameDomain)
        return std::nullopt;

    return { { WTFMove(*wasGranted), WTFMove(*promptWasShown), WTFMove(*scope), WTFMove(*topFrameDomain), WTFMove(*subFrameDomain) } };
}

} // namespace WebCore

#endif // ENABLE(TRACKING_PREVENTION)
