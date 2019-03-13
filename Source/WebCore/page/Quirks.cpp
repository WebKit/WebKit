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
#include "DocumentLoader.h"
#include "HTMLMetaElement.h"
#include "HTMLObjectElement.h"
#include "Settings.h"

namespace WebCore {

static inline OptionSet<AutoplayQuirk> allowedAutoplayQuirks(Document& document)
{
    auto* loader = document.loader();
    if (!loader)
        return { };

    return loader->allowedAutoplayQuirks();
}

Quirks::Quirks(Document& document)
    : m_document(makeWeakPtr(document))
{
}

Quirks::~Quirks() = default;

inline bool Quirks::needsQuirks() const
{
    return m_document && m_document->settings().needsSiteSpecificQuirks();
}

bool Quirks::shouldIgnoreInvalidSignal() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "www.thrivepatientportal.com");
}

bool Quirks::needsFormControlToBeMouseFocusable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "ceac.state.gov") || host.endsWithIgnoringASCIICase(".ceac.state.gov");
#else
    return false;
#endif
}

bool Quirks::needsAutoplayPlayPauseEvents() const
{
    if (!needsQuirks())
        return false;

    if (allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::SynthesizedPauseEvents))
        return true;

    return allowedAutoplayQuirks(m_document->topDocument()).contains(AutoplayQuirk::SynthesizedPauseEvents);
}

bool Quirks::needsSeekingSupportDisabled() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "netflix.com") || host.endsWithIgnoringASCIICase(".netflix.com");
}

bool Quirks::needsPerDocumentAutoplayBehavior() const
{
#if PLATFORM(MAC)
    ASSERT(m_document == &m_document->topDocument());
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::PerDocumentAutoplayBehavior);
#else
    return false;
#endif
}

bool Quirks::shouldAutoplayForArbitraryUserGesture() const
{
#if PLATFORM(MAC)
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::ArbitraryUserGestures);
#else
    return false;
#endif
}

bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_hasBrokenEncryptedMediaAPISupportQuirk)
        return m_hasBrokenEncryptedMediaAPISupportQuirk.value();

    auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();

    m_hasBrokenEncryptedMediaAPISupportQuirk = domain == "starz.com"
        || domain.endsWith(".starz.com")
        || domain == "youtube.com"
        || domain.endsWith(".youtube.com")
        || domain == "hulu.com"
        || domain.endsWith("hulu.com");

    return m_hasBrokenEncryptedMediaAPISupportQuirk.value();
}

bool Quirks::hasWebSQLSupportQuirk() const
{
    if (!needsQuirks())
        return false;
    
    if (m_hasWebSQLSupportQuirk)
        return m_hasWebSQLSupportQuirk.value();
    
    auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();
    
    m_hasWebSQLSupportQuirk = domain == "bostonglobe.com"
        || domain.endsWith(".bostonglobe.com")
        || domain == "latimes.com"
        || domain.endsWith(".latimes.com");
    
    return m_hasWebSQLSupportQuirk.value();
}

bool Quirks::isTouchBarUpdateSupressedForHiddenContentEditable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "docs.google.com");
#else
    return false;
#endif
}

bool Quirks::isNeverRichlyEditableForTouchBar() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    auto host = url.host();

    if (equalLettersIgnoringASCIICase(host, "twitter.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "onedrive.live.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "trix-editor.org"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.icloud.com")) {
        auto path = url.path();
        if (path.contains("notes") || url.fragmentIdentifier().contains("notes"))
            return true;
    }
#endif

    return false;
}

}
