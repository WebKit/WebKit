/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MixedContentChecker.h"

#include "BlobURL.h"
#include "DocumentInlines.h"
#include "Document.h"
#include "LegacySchemeRegistry.h"
#include "LocalFrame.h"
#include "SecurityOrigin.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static bool isNonLocalHostPotentiallyTrustworthyOrigin(RefPtr<SecurityOrigin> origin)
{
    // We currently deviate from the mixed content spec, and do not consider localhost
    // or loopback URLs as secure contexts if they do not use a secure scheme.
    // https://bugs.webkit.org/show_bug.cgi?id=171934
    if (SecurityOrigin::isLocalHostOrLoopbackIPAddress(origin->host()))
        return LegacySchemeRegistry::shouldTreatURLSchemeAsSecure(origin->protocol());

    return origin->isPotentiallyTrustworthy();
}

static RefPtr<SecurityOrigin> getActualOrigin(const Document& document)
{
    RefPtr origin = document.securityOrigin();
    // sandboxed iframes have an opaque origin so we should perform the mixed content
    // check considering the origin the iframe would have had if it were not sandboxed.
    if (origin->isOpaque())
        origin = SecurityOrigin::create(document.url());
    return origin;
}

static bool isNonLocalHostPotentiallyTrustworthyURL(const URL& url)
{
    if (!url.isValid())
        return true;

    // Secure Contexts
    // W3C Candidate Recommendation Draft, 10 November 2023
    // 3.2. Is url potentially trustworthy?

    // 1. If url is "about:blank" or "about:srcdoc", return "Potentially Trustworthy".
    // 2. If url’s scheme is "data", return "Potentially Trustworthy".
    if (url.isAboutBlank() || url.isAboutSrcDoc() || url.protocolIsData())
        return true;

    // 3. Return the result of executing § 3.1 Is origin potentially trustworthy? on url’s origin.
    // NOTE: The origin of blob: URLs is the origin of the context in which they were created.
    //       Therefore, blobs created in a trustworthy origin will themselves be potentially
    //       trustworthy.
    RefPtr<SecurityOrigin> origin;
    if (!url.protocolIsBlob())
        origin = SecurityOrigin::create(url);
    else if (RefPtr document = BlobURL::getOwnerDocument(url))
        origin = getActualOrigin(*document);
    else
        origin = SecurityOrigin::createForBlobURL(url);

    return isNonLocalHostPotentiallyTrustworthyOrigin(origin);
}

static bool prohibitsMixedSecurityContextsItself(const Document& document)
{
    RefPtr origin = getActualOrigin(document);

    // We currently deviate from the mixed content spec, and allow loading of
    // non-trustworthy resources from local documents or from documents with
    // custom schemes handled by a scheme handler. Note that we otherwise
    // consider these documents as trustworthy.
    // https://bugs.webkit.org/show_bug.cgi?id=297785
    if (origin->isLocal() || LegacySchemeRegistry::schemeIsHandledBySchemeHandler(origin->protocol()))
        return false;

    return isNonLocalHostPotentiallyTrustworthyOrigin(origin);
}

static bool prohibitsMixedSecurityContexts(const Document& document)
{
    // https://www.w3.org/TR/mixed-content/#upgrade-algorithm
    // Editor’s Draft, 23 February 2023
    // 4.3. Does settings prohibit mixed security contexts?

    if (prohibitsMixedSecurityContextsItself(document))
        return true;

    const URL& url = document.url();
    if (url.protocolIsData()) {
        RefPtr<Frame> frame = document.frame();

        do {
            frame = frame->tree().parent();
            if (!frame)
                break;

            if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get())) {
                RefPtr document = localFrame->document();

                if (prohibitsMixedSecurityContextsItself(*document))
                    return true;
            } else {
                // FIXME: <rdar://116259764> Make mixed content checks work correctly with site isolated iframes.
                break;
            }
        } while (!frame->isMainFrame());
    }

    return false;
}

static void logConsoleWarning(const LocalFrame& frame, bool blocked, const URL& target, bool isUpgradingIPAddressAndLocalhostEnabled)
{
    auto isUpgradingLocalhostDisabled = !isUpgradingIPAddressAndLocalhostEnabled && shouldTreatAsPotentiallyTrustworthy(target);
    ASCIILiteral errorString = [&] {
    if (blocked)
        return "blocked and must"_s;
    if (isUpgradingLocalhostDisabled)
        return "not upgraded to HTTPS and must be served from the local host."_s;
    return "automatically upgraded and should"_s;
    }();

    auto message = makeString((!blocked ? ""_s : "[blocked] "_s), "The page at "_s, frame.document()->url().stringCenterEllipsizedToLength(), " requested insecure content from "_s, target.stringCenterEllipsizedToLength(), ". This content was "_s, errorString, !isUpgradingLocalhostDisabled ? " be served over HTTPS.\n"_s : "\n"_s);
    frame.protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, message);
}

static bool destinationIsImageAudioOrVideo(FetchOptions::Destination destination)
{
    return destination == FetchOptions::Destination::Audio || destination == FetchOptions::Destination::Image || destination == FetchOptions::Destination::Video;
}

static bool destinationIsImageAndInitiatorIsImageset(FetchOptions::Destination destination, Initiator initiator)
{
    return destination == FetchOptions::Destination::Image && initiator == Initiator::Imageset;
}

bool MixedContentChecker::shouldUpgradeInsecureContent(LocalFrame& frame, IsUpgradable isUpgradable, const URL& url, FetchOptions::Destination destination, Initiator initiator)
{
    RefPtr document = frame.document();
    if (!document || isUpgradable != IsUpgradable::Yes)
        return false;

    // https://www.w3.org/TR/mixed-content/#upgrade-algorithm
    // Editor’s Draft, 23 February 2023
    // 4.1. Upgrade request to a potentially trustworthy URL, if appropriate

    // 4.1.1.3 Does settings prohibit mixed security contexts? returns "Does Not Restrict Mixed Security Contents" when applied to request’s client.
    if (!prohibitsMixedSecurityContexts(*document))
        return false;

    // 4.1.1.1, 4.1.1.2, 4.1.1.4, 4.1.1.5
    if (!canModifyRequest(url, destination, initiator))
        return false;

    // 4.1.2 If request’s URL’s scheme is http, set request’s URL’s scheme to https, and return.
    if (url.protocolIs("http"_s)) {
        auto shouldUpgradeIPAddressAndLocalhostForTesting = document->settings().iPAddressAndLocalhostMixedContentUpgradeTestingEnabled();
        logConsoleWarning(frame, /* blocked */ false, url, shouldUpgradeIPAddressAndLocalhostForTesting);

        return true;
    }

    return false;
}

bool MixedContentChecker::canModifyRequest(const URL& url, FetchOptions::Destination destination, Initiator initiator)
{
    // 4.1.1.1 request’s URL is a potentially trustworthy URL.
    if (isNonLocalHostPotentiallyTrustworthyURL(url))
        return false;

    // 4.1.1.2 request’s URL’s host is an IP address.
    // We diverge from the spec when it comes to the loopback address, which consider as upgradable.
    if (URL::hostIsIPAddress(url.host()) && !SecurityOrigin::isLocalHostOrLoopbackIPAddress(url.host()))
        return false;

    // 4.1.1.4 request’s destination is not "image", "audio", or "video".
    if (!destinationIsImageAudioOrVideo(destination))
        return false;

    // 4.1.1.5 request’s destination is "image" and request’s initiator is "imageset".
    if (destinationIsImageAndInitiatorIsImageset(destination, initiator))
        return false;

    return true;
}

bool MixedContentChecker::shouldBlockRequest(LocalFrame& frame, const URL& url, IsUpgradable isUpgradable)
{
    if (isUpgradable == IsUpgradable::Yes)
        return false;

    // https://www.w3.org/TR/mixed-content/#upgrade-algorithm
    // Editor’s Draft, 23 February 2023
    // 4.4. Should fetching request be blocked as mixed content?

    RefPtr document = frame.document();
    if (!document)
        return false;

    // 4.4.1.1 Does settings prohibit mixed security contexts? returns "Does Not Restrict Mixed Security Contexts" when applied to request’s client.
    if (!prohibitsMixedSecurityContexts(*frame.document()))
        return false;

    // 4.4.1.2  request’s URL is a potentially trustworthy URL.
    if (isNonLocalHostPotentiallyTrustworthyURL(url))
        return false;

    // 4.4.1.3 The user agent has been instructed to allow mixed content, as described in § 7.2 User Controls).
    // 4.4.1.4 request’s destination is "document", and request’s target browsing context has no parent browsing context.

    logConsoleWarning(frame, /* blocked */ true, url, document->settings().iPAddressAndLocalhostMixedContentUpgradeTestingEnabled());
    return true;
}

} // namespace WebCore
