/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

MixedContentChecker::MixedContentChecker(Frame& frame)
    : m_frame(frame)
{
}

FrameLoaderClient& MixedContentChecker::client() const
{
    return m_frame.loader().client();
}

// static
bool MixedContentChecker::isMixedContent(SecurityOrigin& securityOrigin, const URL& url)
{
    if (securityOrigin.protocol() != "https")
        return false; // We only care about HTTPS security origins.

    // We're in a secure context, so |url| is mixed content if it's insecure.
    return !SecurityOrigin::isSecure(url);
}

bool MixedContentChecker::canDisplayInsecureContent(SecurityOrigin& securityOrigin, ContentType type, const URL& url, AlwaysDisplayInNonStrictMode alwaysDisplayInNonStrictMode) const
{
    if (!isMixedContent(securityOrigin, url))
        return true;

    if (!m_frame.document()->contentSecurityPolicy()->allowRunningOrDisplayingInsecureContent(url))
        return false;

    bool isStrictMode = m_frame.document()->isStrictMixedContentMode();
    if (!isStrictMode && alwaysDisplayInNonStrictMode == AlwaysDisplayInNonStrictMode::Yes)
        return true;

    bool allowed = !isStrictMode && (m_frame.settings().allowDisplayOfInsecureContent() || type == ContentType::ActiveCanWarn) && !m_frame.document()->geolocationAccessed();
    logWarning(allowed, "display", url);

    if (allowed) {
        m_frame.document()->setFoundMixedContent(SecurityContext::MixedContentType::Inactive);
        client().didDisplayInsecureContent();
    }

    return allowed;
}

bool MixedContentChecker::canRunInsecureContent(SecurityOrigin& securityOrigin, const URL& url) const
{
    if (!isMixedContent(securityOrigin, url))
        return true;

    if (!m_frame.document()->contentSecurityPolicy()->allowRunningOrDisplayingInsecureContent(url))
        return false;

    bool allowed = !m_frame.document()->isStrictMixedContentMode() && m_frame.settings().allowRunningOfInsecureContent() && !m_frame.document()->geolocationAccessed() && !m_frame.document()->secureCookiesAccessed();
    logWarning(allowed, "run", url);

    if (allowed) {
        m_frame.document()->setFoundMixedContent(SecurityContext::MixedContentType::Active);
        client().didRunInsecureContent(securityOrigin, url);
    }

    return allowed;
}

void MixedContentChecker::checkFormForMixedContent(SecurityOrigin& securityOrigin, const URL& url) const
{
    // Unconditionally allow javascript: URLs as form actions as some pages do this and it does not introduce
    // a mixed content issue.
    if (WTF::protocolIsJavaScript(url))
        return;

    if (!isMixedContent(securityOrigin, url))
        return;

    String message = makeString("The page at ", m_frame.document()->url().stringCenterEllipsizedToLength(), " contains a form which targets an insecure URL ", url.stringCenterEllipsizedToLength(), ".\n");
    m_frame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, message);

    client().didDisplayInsecureContent();
}

Optional<String> MixedContentChecker::checkForMixedContentInFrameTree(const URL& url)
{
    auto* document = m_frame.document();

    while (document) {
        RELEASE_ASSERT_WITH_MESSAGE(document->frame(), "An unparented document tried to connect to a websocket with url: %s", url.string().utf8().data());
        
        auto* frame = document->frame();
        if (isMixedContent(document->securityOrigin(), url))
            return makeString("The page at ", document->url().stringCenterEllipsizedToLength(), " was blocked from connecting insecurely to ", url.stringCenterEllipsizedToLength(), " either because the protocol is insecure or the page is embedded from an insecure page.");

        if (frame->isMainFrame())
            break;

        frame = frame->tree().parent();
        RELEASE_ASSERT_WITH_MESSAGE(frame, "Should never have a parentless non main frame");
        document = frame->document();
    }
    
    return WTF::nullopt;
}

void MixedContentChecker::logWarning(bool allowed, const String& action, const URL& target) const
{
    const char* errorString = allowed ? " was allowed to " : " was not allowed to ";
    String message = makeString((allowed ? String() : "[blocked] "), "The page at ", m_frame.document()->url().stringCenterEllipsizedToLength(), errorString, action, " insecure content from ", target.stringCenterEllipsizedToLength(), ".\n");
    m_frame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, message);
}

} // namespace WebCore
