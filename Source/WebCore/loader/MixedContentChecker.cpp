/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "SecurityOrigin.h"

namespace WebCore {

static bool isMixedContent(const Document& document, const URL& url)
{
    // sandboxed iframes have an opaque origin so we should perform the mixed content check considering the origin
    // the iframe would have had if it were not sandboxed.
    if (document.securityOrigin().protocol() == "https"_s || (document.securityOrigin().isOpaque() && document.url().protocolIs("https"_s)))
        return !SecurityOrigin::isSecure(url);

    return false;
}

static bool foundMixedContentInFrameTree(const LocalFrame& frame, const URL& url)
{
    auto* document = frame.document();

    while (document) {
        if (isMixedContent(*document, url))
            return true;

        auto* frame = document->frame();
        if (!frame || frame->isMainFrame())
            break;

        auto* abstractParentFrame = frame->tree().parent();
        RELEASE_ASSERT_WITH_MESSAGE(abstractParentFrame, "Should never have a parentless non main frame");
        if (auto* parentFrame = dynamicDowncast<LocalFrame>(abstractParentFrame))
            document = parentFrame->document();
    }

    return false;
}


static void logWarning(const LocalFrame& frame, bool allowed, ASCIILiteral action, const URL& target)
{
    const char* errorString = allowed ? " was allowed to " : " was not allowed to ";
    auto message = makeString((allowed ? "" : "[blocked] "), "The page at ", frame.document()->url().stringCenterEllipsizedToLength(), errorString, action, " insecure content from ", target.stringCenterEllipsizedToLength(), ".\n");
    frame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, message);
}

bool MixedContentChecker::frameAndAncestorsCanDisplayInsecureContent(LocalFrame& frame, ContentType type, const URL& url)
{
    if (!foundMixedContentInFrameTree(frame, url))
        return true;

    if (!frame.document()->contentSecurityPolicy()->allowRunningOrDisplayingInsecureContent(url))
        return false;

    bool allowed = !frame.document()->isStrictMixedContentMode() && (frame.settings().allowDisplayOfInsecureContent() || type == ContentType::ActiveCanWarn) && !frame.document()->geolocationAccessed();
    logWarning(frame, allowed, "display"_s, url);

    if (allowed) {
        frame.document()->setFoundMixedContent(SecurityContext::MixedContentType::Inactive);
        frame.loader().client().didDisplayInsecureContent();
    }

    return allowed;
}

bool MixedContentChecker::frameAndAncestorsCanRunInsecureContent(LocalFrame& frame, SecurityOrigin& securityOrigin, const URL& url, ShouldLogWarning shouldLogWarning)
{
    if (!foundMixedContentInFrameTree(frame, url))
        return true;

    if (!frame.document()->contentSecurityPolicy()->allowRunningOrDisplayingInsecureContent(url))
        return false;

    bool allowed = !frame.document()->isStrictMixedContentMode() && frame.settings().allowRunningOfInsecureContent() && !frame.document()->geolocationAccessed() && !frame.document()->secureCookiesAccessed();
    if (LIKELY(shouldLogWarning == ShouldLogWarning::Yes))
        logWarning(frame, allowed, "run"_s, url);

    if (allowed) {
        frame.document()->setFoundMixedContent(SecurityContext::MixedContentType::Active);
        frame.loader().client().didRunInsecureContent(securityOrigin, url);
    }

    return allowed;
}

void MixedContentChecker::checkFormForMixedContent(LocalFrame& frame, const URL& url)
{
    // Unconditionally allow javascript: URLs as form actions as some pages do this and it does not introduce
    // a mixed content issue.
    if (url.protocolIsJavaScript())
        return;

    if (!isMixedContent(*frame.document(), url))
        return;

    auto message = makeString("The page at ", frame.document()->url().stringCenterEllipsizedToLength(), " contains a form which targets an insecure URL ", url.stringCenterEllipsizedToLength(), ".\n");
    frame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, message);

    frame.loader().client().didDisplayInsecureContent();
}

} // namespace WebCore
