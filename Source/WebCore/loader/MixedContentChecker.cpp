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

#include "ContentFilter.h"
#include "Document.h"
#include "FrameLoader.h"
#include "LegacySchemeRegistry.h"
#include "LocalFrameInlines.h"
#include "LocalFrameLoaderClient.h"
#include "SecurityOrigin.h"

namespace WebCore {

static bool isDocumentSecure(const Frame& frame)
{
    // FIXME: Use document.isDocumentSecure(), instead of comparing against "https" scheme, when all ports stop using loopback in LayoutTests
    // sandboxed iframes have an opaque origin so we should perform the mixed content check considering the origin
    // the iframe would have had if it were not sandboxed.
    if (RefPtr origin = frame.frameDocumentSecurityOrigin())
        return origin->protocol() == "https"_s || (origin->isOpaque() && frame.frameURLProtocol() == "https"_s);

    return false;
}

static bool isDataContextSecure(const Frame& frame)
{
    RefPtr currentFrame = frame;

    while (currentFrame) {
        RefPtr localFrame = dynamicDowncast<const LocalFrame>(currentFrame);
        RefPtr<Document> document;
        if (localFrame)
            document = localFrame->document();

        if (isDocumentSecure(*currentFrame))
            return true;

        RefPtr parentFrame = currentFrame->tree().parent();
        if (!parentFrame && localFrame)
            parentFrame = localFrame->loader().client().provisionalParentFrame();
        currentFrame = parentFrame;
    }

    return false;
}

static bool isMixedContent(const Frame& frame, const URL& url)
{
    if (isDocumentSecure(frame) || (frame.frameURLProtocol() == "data"_s && isDataContextSecure(frame)))
        return !SecurityOrigin::isSecure(url);

    return false;
}

static bool NODELETE destinationIsImageAudioOrVideo(FetchOptions::Destination destination)
{
    return destination == FetchOptions::Destination::Audio || destination == FetchOptions::Destination::Image || destination == FetchOptions::Destination::Video;
}

static bool NODELETE destinationIsImageAndInitiatorIsImageset(FetchOptions::Destination destination, Initiator initiator)
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
    // 4.1. Upgrade a mixed content request to a potentially trustworthy URL, if appropriate
    if (!isMixedContent(frame, url))
        return false;

    // 4.1 The request's URL is not upgraded in the following cases.
    if (!canModifyRequest(url, destination, initiator))
        return false;

    frame.reportMixedContentViolation(false, url);
    return true;
}

bool MixedContentChecker::canModifyRequest(const URL& url, FetchOptions::Destination destination, Initiator initiator)
{
    // 4.1.1 request’s URL is a potentially trustworthy URL.
    if (url.protocolIs("https"_s))
        return false;
    // 4.1.2 request’s URL’s host is an IP address.
    if (URL::hostIsIPAddress(url.host()) && !shouldTreatAsPotentiallyTrustworthy(url))
        return false;
    // 4.1.4 request’s destination is not "image", "audio", or "video".
    if (!destinationIsImageAudioOrVideo(destination))
        return false;
    // 4.1.5 request’s destination is "image" and request’s initiator is "imageset".
    auto schemeIsHandledBySchemeHandler = LegacySchemeRegistry::schemeIsHandledBySchemeHandler(url.protocol());
    if (!schemeIsHandledBySchemeHandler && destinationIsImageAndInitiatorIsImageset(destination, initiator))
        return false;
    return true;
}

bool MixedContentChecker::shouldBlockRequest(Frame& frame, const URL& url, IsUpgradable isUpgradable)
{
    RefPtr<Document> document;
    if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame))
        document = localFrame->document();

#if ENABLE(CONTENT_FILTERING) && HAVE(WEBCONTENTRESTRICTIONS)
    if (url == ContentFilter::blockedPageURL())
        return false;
#endif

    if (!isMixedContent(frame, url))
        return false;
    if ((LegacySchemeRegistry::schemeIsHandledBySchemeHandler(url.protocol()) || shouldTreatAsPotentiallyTrustworthy(url)) && isUpgradable == IsUpgradable::Yes)
        return false;
    frame.reportMixedContentViolation(true, url);
    return true;
}

} // namespace WebCore
