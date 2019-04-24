/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#pragma once

#include "ContentSecurityPolicy.h"
#include "ContentSecurityPolicyHash.h"
#include "ContentSecurityPolicyMediaListDirective.h"
#include "ContentSecurityPolicySourceListDirective.h"
#include <wtf/URL.h>

namespace WebCore {

class Frame;

class ContentSecurityPolicyDirectiveList {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<ContentSecurityPolicyDirectiveList> create(ContentSecurityPolicy&, const String&, ContentSecurityPolicyHeaderType, ContentSecurityPolicy::PolicyFrom);
    ContentSecurityPolicyDirectiveList(ContentSecurityPolicy&, ContentSecurityPolicyHeaderType);

    const String& header() const { return m_header; }
    ContentSecurityPolicyHeaderType headerType() const { return m_headerType; }

    const ContentSecurityPolicyDirective* violatedDirectiveForUnsafeEval() const;
    const ContentSecurityPolicyDirective* violatedDirectiveForUnsafeInlineScript() const;
    const ContentSecurityPolicyDirective* violatedDirectiveForUnsafeInlineStyle() const;

    const ContentSecurityPolicyDirective* violatedDirectiveForScriptHash(const ContentSecurityPolicyHash&) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForStyleHash(const ContentSecurityPolicyHash&) const;

    const ContentSecurityPolicyDirective* violatedDirectiveForScriptNonce(const String&) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForStyleNonce(const String&) const;

    const ContentSecurityPolicyDirective* violatedDirectiveForBaseURI(const URL&) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForChildContext(const URL&, bool didReceiveRedirectResponse) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForConnectSource(const URL&, bool didReceiveRedirectResponse) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForFont(const URL&, bool didReceiveRedirectResponse) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForFormAction(const URL&, bool didReceiveRedirectResponse) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForFrame(const URL&, bool didReceiveRedirectResponse) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForFrameAncestor(const Frame&) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForFrameAncestorOrigins(const Vector<RefPtr<SecurityOrigin>>&) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForImage(const URL&, bool didReceiveRedirectResponse) const;
#if ENABLE(APPLICATION_MANIFEST)
    const ContentSecurityPolicyDirective* violatedDirectiveForManifest(const URL&, bool didReceiveRedirectResponse) const;
#endif
    const ContentSecurityPolicyDirective* violatedDirectiveForMedia(const URL&, bool didReceiveRedirectResponse) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForObjectSource(const URL&, bool didReceiveRedirectResponse, ContentSecurityPolicySourceListDirective::ShouldAllowEmptyURLIfSourceListIsNotNone) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForPluginType(const String& type, const String& typeAttribute) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForScript(const URL&, bool didReceiveRedirectResponse) const;
    const ContentSecurityPolicyDirective* violatedDirectiveForStyle(const URL&, bool didReceiveRedirectResponse) const;

    const ContentSecurityPolicyDirective* defaultSrc() const { return m_defaultSrc.get(); }

    bool hasBlockAllMixedContentDirective() const { return m_hasBlockAllMixedContentDirective; }
    bool hasFrameAncestorsDirective() const { return !!m_frameAncestors; }

    const String& evalDisabledErrorMessage() const { return m_evalDisabledErrorMessage; }
    const String& webAssemblyDisabledErrorMessage() const { return m_webAssemblyDisabledErrorMessage; }
    bool isReportOnly() const { return m_reportOnly; }
    const Vector<String>& reportURIs() const { return m_reportURIs; }

    // FIXME: Remove this once we teach ContentSecurityPolicyDirectiveList how to log an arbitrary console message.
    const ContentSecurityPolicy& policy() const { return m_policy; }

private:
    void parse(const String&, ContentSecurityPolicy::PolicyFrom);

    bool parseDirective(const UChar* begin, const UChar* end, String& name, String& value);
    void parseReportURI(const String& name, const String& value);
    void parsePluginTypes(const String& name, const String& value);
    void addDirective(const String& name, const String& value);
    void applySandboxPolicy(const String& name, const String& sandboxPolicy);
    void setUpgradeInsecureRequests(const String& name);
    void setBlockAllMixedContentEnabled(const String& name);

    template <class CSPDirectiveType>
    void setCSPDirective(const String& name, const String& value, std::unique_ptr<CSPDirectiveType>&);

    ContentSecurityPolicySourceListDirective* operativeDirective(ContentSecurityPolicySourceListDirective*) const;

    void setEvalDisabledErrorMessage(const String& errorMessage) { m_evalDisabledErrorMessage = errorMessage; }
    void setWebAssemblyDisabledErrorMessage(const String& errorMessage) { m_webAssemblyDisabledErrorMessage = errorMessage; }

    // FIXME: Make this a const reference once we teach applySandboxPolicy() to store its policy as opposed to applying it directly onto ContentSecurityPolicy.
    ContentSecurityPolicy& m_policy;

    String m_header;
    ContentSecurityPolicyHeaderType m_headerType;

    bool m_reportOnly { false };
    bool m_haveSandboxPolicy { false };
    bool m_upgradeInsecureRequests { false };
    bool m_hasBlockAllMixedContentDirective { false };

    std::unique_ptr<ContentSecurityPolicyMediaListDirective> m_pluginTypes;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_baseURI;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_connectSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_childSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_defaultSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_fontSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_formAction;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_frameAncestors;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_frameSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_imgSrc;
#if ENABLE(APPLICATION_MANIFEST)
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_manifestSrc;
#endif
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_mediaSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_objectSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_scriptSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_styleSrc;
    
    Vector<String> m_reportURIs;
    
    String m_evalDisabledErrorMessage;
    String m_webAssemblyDisabledErrorMessage;
};

} // namespace WebCore
