/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include <WebCore/CredentialRequestCoordinatorClient.h>

#if ENABLE(WEB_AUTHN)
#include <WebCore/UnvalidatedDigitalCredentialRequest.h>
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMalloc.h>
namespace WebCore {

struct DigitalCredentialsResponseData;

class WEBCORE_EXPORT DummyCredentialRequestCoordinatorClient final : public CredentialRequestCoordinatorClient {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(DummyCredentialRequestCoordinatorClient, WEBCORE_EXPORT);

public:
    DummyCredentialRequestCoordinatorClient();
    ~DummyCredentialRequestCoordinatorClient() final;

    static Ref<DummyCredentialRequestCoordinatorClient> create();

private:
    void showDigitalCredentialsPicker(Vector<WebCore::UnvalidatedDigitalCredentialRequest>&&, const DigitalCredentialsRequestData&, CompletionHandler<void(Expected<DigitalCredentialsResponseData, ExceptionData>&&)>&&);
    void dismissDigitalCredentialsPicker(CompletionHandler<void(bool)>&&) final;
    ExceptionOr<Vector<ValidatedDigitalCredentialRequest>> validateAndParseDigitalCredentialRequests(const SecurityOrigin&, const Document&, const Vector<UnvalidatedDigitalCredentialRequest>&) final;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
