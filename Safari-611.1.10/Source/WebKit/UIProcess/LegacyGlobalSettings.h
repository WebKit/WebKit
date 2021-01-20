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

#include "CacheModel.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class LegacyGlobalSettings {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static LegacyGlobalSettings& singleton();

    void setCacheModel(CacheModel);
    CacheModel cacheModel() const { return m_cacheModel; }

    const HashSet<String>& schemesToRegisterAsSecure() { return m_schemesToRegisterAsSecure; }
    void registerURLSchemeAsSecure(const String& scheme) { m_schemesToRegisterAsSecure.add(scheme); }

    const HashSet<String>& schemesToRegisterAsBypassingContentSecurityPolicy() { return m_schemesToRegisterAsBypassingContentSecurityPolicy; }
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme) { m_schemesToRegisterAsBypassingContentSecurityPolicy.add(scheme); }

    const HashSet<String>& schemesToRegisterAsLocal() { return m_schemesToRegisterAsLocal; }
    void registerURLSchemeAsLocal(const String& scheme) { m_schemesToRegisterAsLocal.add(scheme); }

    const HashSet<String>& schemesToRegisterAsNoAccess() { return m_schemesToRegisterAsNoAccess; }
    void registerURLSchemeAsNoAccess(const String& scheme) { m_schemesToRegisterAsNoAccess.add(scheme); }

private:
    friend class NeverDestroyed<LegacyGlobalSettings>;
    LegacyGlobalSettings();
    
    CacheModel m_cacheModel { CacheModel::PrimaryWebBrowser };
    HashSet<String> m_schemesToRegisterAsSecure;
    HashSet<String> m_schemesToRegisterAsBypassingContentSecurityPolicy;
    HashSet<String> m_schemesToRegisterAsLocal;
    HashSet<String> m_schemesToRegisterAsNoAccess;
};

} // namespace WebKit
