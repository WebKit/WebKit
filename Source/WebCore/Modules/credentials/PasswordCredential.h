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

#include "DOMFormData.h"
#include "SiteBoundCredential.h"
#include "URLSearchParams.h"
#include <wtf/Variant.h>

namespace WebCore {

class HTMLFormElement;

class PasswordCredential : public SiteBoundCredential {
public:
    struct Data : public SiteBoundCredentialData {
        String password;
    };

    static Ref<PasswordCredential> create(const Data& data) { return adoptRef(*new PasswordCredential(data)); }
    static Ref<PasswordCredential> create(const HTMLFormElement& form) { return adoptRef(*new PasswordCredential(form)); }

    void setIdName(String&& idName) { m_idName = WTFMove(idName); }
    const String& idName() const { return m_idName; }

    void setPasswordName(String&& passwordName) { m_passwordName = WTFMove(passwordName); }
    const String& passwordName() const { return m_passwordName; }

    using CredentialBodyType = std::optional<Variant<RefPtr<DOMFormData>, RefPtr<URLSearchParams>>>;
    void setAdditionalData(CredentialBodyType&& additionalData) { m_additionalData = WTFMove(additionalData); }
    const CredentialBodyType& additionalData() const { return m_additionalData; }

private:
    PasswordCredential(const Data&);
    PasswordCredential(const HTMLFormElement&);

    String m_idName;
    String m_passwordName;
    CredentialBodyType m_additionalData;
};

} // namespace WebCore
