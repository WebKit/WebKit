/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "Quirks.h"

#include "Document.h"
#include "Settings.h"

namespace WebCore {

Quirks::Quirks(Document& document)
    : m_document(makeWeakPtr(document))
{
}

Quirks::~Quirks() = default;

bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
    if (!m_document || !m_document->settings().needsSiteSpecificQuirks())
        return false;

    if (m_hasBrokenEncryptedMediaAPISupportQuirk)
        return m_hasBrokenEncryptedMediaAPISupportQuirk.value();

    auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();

    m_hasBrokenEncryptedMediaAPISupportQuirk = domain == "amazon.com"
        || domain.endsWith(".amazon.com")
        || domain == "primevideo.com"
        || domain.endsWith(".primevideo.com")
        || domain == "starz.com"
        || domain.endsWith(".starz.com")
        || domain == "hulu.com"
        || domain.endsWith("hulu.com");

    return m_hasBrokenEncryptedMediaAPISupportQuirk.value();
}

}
