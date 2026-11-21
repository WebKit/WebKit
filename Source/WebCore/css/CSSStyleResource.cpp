/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSStyleResource.h"

#include "CSSURLModifiers.h"
#include "CSSURLProvenance.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "Document.h"
#include "ThreadableLoader.h"

namespace WebCore::CSS {

ResourceLoaderOptions styleResourceLoaderOptions(bool isInitiatingElementInUserAgentShadowTree)
{
    auto options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = isInitiatingElementInUserAgentShadowTree ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
    options.shouldEnableContentExtensionsCheck = isInitiatingElementInUserAgentShadowTree ? ShouldEnableContentExtensionsCheck::No : ShouldEnableContentExtensionsCheck::Yes;
    return options;
}

static void applyStyleResourceRequestOptions(ResourceLoaderOptions& options, const URLModifiers& modifiers, FetchOptions::Mode corsMode)
{
    // https://drafts.csswg.org/css-values-4/#fetch-a-style-resource

    // 3. Let req be a new request whose url is parsedUrl, whose destination is destination, mode is corsMode,
    //    origin is settingsObject's origin, credentials mode is "same-origin", use-url-credentials flag is set,
    //    client is settingsObject, and whose referrer is "client".
    //
    // The url is set when the request is created. The loader sets the destination from the type of resource
    // requested, the origin and client from its document, and the referrer from its frame. The use-url-credentials
    // flag is indicated by StoredCredentialsPolicy::Use.
    options.mode = corsMode;
    options.credentials = FetchOptions::Credentials::SameOrigin;

    // Fetch always gives data: URLs a "basic" response tainting. https://fetch.spec.whatwg.org/#main-fetch
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;

    // 4. If corsMode is "no-cors", set req's credentials mode to "include".
    if (corsMode == FetchOptions::Mode::NoCors)
        options.credentials = FetchOptions::Credentials::Include;

    // 5. Apply any URL request modifier steps that apply to this request.
    applyModifiersToLoaderOptions(modifiers, options);
}

CachedResourceRequest createStyleResourceRequest(Document& document, const WTF::URL& url, const URLModifiers& modifiers, const URLProvenance& provenance, FetchOptions::Mode corsMode, ResourceLoaderOptions options, const AtomString& initiatorType)
{
    // https://drafts.csswg.org/css-values-4/#fetch-a-style-resource

    // 1. Let parsedUrl be the result of resolving urlValue given cssRuleOrDeclaration. If that failed, return.
    // 2. Let settingsObject be cssRuleOrDeclaration's relevant settings object.
    //
    // The url is resolved when the style is parsed and the settings object is the document.

    // Set req's referrer policy to the style sheet's, as the Referrer Policy spec requires. This is done
    // before step 5 so that a referrer-policy() modifier takes precedence.
    // https://w3c.github.io/webappsec-referrer-policy/#integration-with-css
    if (provenance.styleSheetReferrerPolicy != ReferrerPolicy::EmptyString)
        options.referrerPolicy = provenance.styleSheetReferrerPolicy;

    // Steps 3 through 5.
    applyStyleResourceRequestOptions(options, modifiers, corsMode);

    CachedResourceRequest request(ResourceRequest(WTF::URL { url }), options);
    if (options.mode == FetchOptions::Mode::Cors)
        request.updateForAccessControl(document);

    // 6. If req's mode is "cors", and sheet is not null, then set req's referrer to the style resource base URL
    //    given cssRuleOrDeclaration.
    //
    // This sets the referrer in every mode, not just "cors", as the Referrer Policy spec requires.
    // https://w3c.github.io/webappsec-referrer-policy/#integration-with-css
    if (!provenance.styleSheetURL.isNull())
        request.resourceRequest().setHTTPReferrer(provenance.styleSheetURL.string());

    // 7. If sheet's origin-clean flag is set, set req's initiator type to "css".
    //
    // Otherwise the load is marked as loaded from an opaque source.
    if (provenance.isStyleSheetOriginClean)
        request.setInitiatorType(initiatorType.isNull() ? cachedResourceRequestInitiatorTypes().css : initiatorType);
    else {
        auto requestOptions = request.options();
        requestOptions.loadedFromOpaqueSource = LoadedFromOpaqueSource::Yes;
        request.setOptions(requestOptions);
    }

    return request;
}

void applyStyleResourceRequestSteps(ThreadableLoaderOptions& options, const URLModifiers& modifiers, const URLProvenance& provenance, FetchOptions::Mode corsMode)
{
    // https://drafts.csswg.org/css-values-4/#fetch-a-style-resource

    // Steps 3 through 5.
    applyStyleResourceRequestOptions(options, modifiers, corsMode);

    // 6. If req's mode is "cors", and sheet is not null, then set req's referrer to the style resource base URL
    //    given cssRuleOrDeclaration.
    //
    // Resources in workers (the only users of this function) have no associated stylesheets, so this never applies.

    // 7. If sheet's origin-clean flag is set, set req's initiator type to "css".
    //
    // Otherwise the load is marked as loaded from an opaque source.
    if (provenance.isStyleSheetOriginClean)
        options.initiatorType = cachedResourceRequestInitiatorTypes().css;
    else
        options.loadedFromOpaqueSource = LoadedFromOpaqueSource::Yes;
}

} // namespace WebCore::CSS
