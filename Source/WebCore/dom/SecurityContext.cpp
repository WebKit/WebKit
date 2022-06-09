/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "SecurityContext.h"

#include "ContentSecurityPolicy.h"
#include "HTMLParserIdioms.h"
#include "PolicyContainer.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

SecurityContext::SecurityContext() = default;

SecurityContext::~SecurityContext() = default;

void SecurityContext::setSecurityOriginPolicy(RefPtr<SecurityOriginPolicy>&& securityOriginPolicy)
{
    m_securityOriginPolicy = WTFMove(securityOriginPolicy);
    m_haveInitializedSecurityOrigin = true;
}

SecurityOrigin* SecurityContext::securityOrigin() const
{
    if (!m_securityOriginPolicy)
        return nullptr;

    return &m_securityOriginPolicy->origin();
}

void SecurityContext::setContentSecurityPolicy(std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy)
{
    m_contentSecurityPolicy = WTFMove(contentSecurityPolicy);
}

bool SecurityContext::isSecureTransitionTo(const URL& url) const
{
    // If we haven't initialized our security origin by now, this is probably
    // a new window created via the API (i.e., that lacks an origin and lacks
    // a place to inherit the origin from).
    if (!haveInitializedSecurityOrigin())
        return true;

    return securityOriginPolicy()->origin().isSameOriginDomain(SecurityOrigin::create(url).get());
}

void SecurityContext::enforceSandboxFlags(SandboxFlags mask, SandboxFlagsSource source)
{
    if (source != SandboxFlagsSource::CSP)
        m_creationSandboxFlags |= mask;
    m_sandboxFlags |= mask;

    // The SandboxOrigin is stored redundantly in the security origin.
    if (isSandboxed(SandboxOrigin) && securityOriginPolicy() && !securityOriginPolicy()->origin().isUnique())
        setSecurityOriginPolicy(SecurityOriginPolicy::create(SecurityOrigin::createUnique()));
}

bool SecurityContext::isSupportedSandboxPolicy(StringView policy)
{
    static const char* const supportedPolicies[] = {
        "allow-forms", "allow-same-origin", "allow-scripts", "allow-top-navigation", "allow-pointer-lock", "allow-popups", "allow-popups-to-escape-sandbox", "allow-top-navigation-by-user-activation", "allow-modals", "allow-storage-access-by-user-activation"
    };

    for (auto* supportedPolicy : supportedPolicies) {
        if (equalIgnoringASCIICase(policy, supportedPolicy))
            return true;
    }
    return false;
}

// Keep SecurityContext::isSupportedSandboxPolicy() in sync when updating this function.
SandboxFlags SecurityContext::parseSandboxPolicy(const String& policy, String& invalidTokensErrorMessage)
{
    // http://www.w3.org/TR/html5/the-iframe-element.html#attr-iframe-sandbox
    // Parse the unordered set of unique space-separated tokens.
    SandboxFlags flags = SandboxAll;
    unsigned length = policy.length();
    unsigned start = 0;
    unsigned numberOfTokenErrors = 0;
    StringBuilder tokenErrors;
    while (true) {
        while (start < length && isHTMLSpace(policy[start]))
            ++start;
        if (start >= length)
            break;
        unsigned end = start + 1;
        while (end < length && !isHTMLSpace(policy[end]))
            ++end;

        // Turn off the corresponding sandbox flag if it's set as "allowed".
        String sandboxToken = policy.substring(start, end - start);
        if (equalLettersIgnoringASCIICase(sandboxToken, "allow-same-origin"))
            flags &= ~SandboxOrigin;
        else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-forms"))
            flags &= ~SandboxForms;
        else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-scripts")) {
            flags &= ~SandboxScripts;
            flags &= ~SandboxAutomaticFeatures;
        } else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-top-navigation")) {
            flags &= ~SandboxTopNavigation;
            flags &= ~SandboxTopNavigationByUserActivation;
        } else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-popups"))
            flags &= ~SandboxPopups;
        else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-pointer-lock"))
            flags &= ~SandboxPointerLock;
        else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-popups-to-escape-sandbox"))
            flags &= ~SandboxPropagatesToAuxiliaryBrowsingContexts;
        else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-top-navigation-by-user-activation"))
            flags &= ~SandboxTopNavigationByUserActivation;
        else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-modals"))
            flags &= ~SandboxModals;
        else if (equalLettersIgnoringASCIICase(sandboxToken, "allow-storage-access-by-user-activation"))
            flags &= ~SandboxStorageAccessByUserActivation;
        else {
            if (numberOfTokenErrors)
                tokenErrors.append(", '");
            else
                tokenErrors.append('\'');
            tokenErrors.append(sandboxToken, '\'');
            numberOfTokenErrors++;
        }

        start = end + 1;
    }

    if (numberOfTokenErrors) {
        if (numberOfTokenErrors > 1)
            tokenErrors.append(" are invalid sandbox flags.");
        else
            tokenErrors.append(" is an invalid sandbox flag.");
        invalidTokensErrorMessage = tokenErrors.toString();
    }

    return flags;
}

const CrossOriginOpenerPolicy& SecurityContext::crossOriginOpenerPolicy() const
{
    static NeverDestroyed<CrossOriginOpenerPolicy> coop;
    return coop;
}

PolicyContainer SecurityContext::policyContainer() const
{
    return {
        crossOriginEmbedderPolicy(),
        crossOriginOpenerPolicy()
    };
}

}
