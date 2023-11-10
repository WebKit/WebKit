/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PaymentSession.h"

#if ENABLE(APPLE_PAY)

#include "Document.h"
#include "DocumentLoader.h"
#include "FeaturePolicy.h"
#include "FrameDestructionObserverInlines.h"
#include "SecurityOrigin.h"

namespace WebCore {

static bool isSecure(DocumentLoader& documentLoader)
{
    if (!documentLoader.response().url().protocolIs("https"_s))
        return false;

    if (!documentLoader.response().certificateInfo() || documentLoader.response().certificateInfo()->containsNonRootSHA1SignedCertificate())
        return false;

    return true;
}

ExceptionOr<void> PaymentSession::canCreateSession(Document& document)
{
    if (!isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Payment, document, LogFeaturePolicyFailure::Yes))
        return Exception { ExceptionCode::SecurityError, "Third-party iframes are not allowed to request payments unless explicitly allowed via Feature-Policy (payment)"_s };

    if (!document.frame())
        return Exception { ExceptionCode::InvalidAccessError, "Trying to start an Apple Pay session from an inactive document."_s };

    if (!isSecure(*document.loader()))
        return Exception { ExceptionCode::InvalidAccessError, "Trying to start an Apple Pay session from an insecure document."_s };

    auto& topDocument = document.topDocument();
    if (&document != &topDocument) {
        for (RefPtr ancestorDocument = document.parentDocument(); ancestorDocument != &topDocument; ancestorDocument = ancestorDocument->parentDocument()) {
            if (!isSecure(*ancestorDocument->loader()))
                return Exception { ExceptionCode::InvalidAccessError, "Trying to start an Apple Pay session from a document with an insecure parent frame."_s };
        }
    }

    return { };
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
