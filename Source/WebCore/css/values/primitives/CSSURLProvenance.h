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

#pragma once

#include <WebCore/ReferrerPolicy.h>
#include <wtf/Forward.h>
#include <wtf/URL.h>

namespace WebCore::CSS {

// Implicit information from the context a url() was parsed with.
// https://drafts.csswg.org/css-values-4/#fetch-a-style-resource
struct URLProvenance {
    // The location of the style sheet containing the url().
    // https://drafts.csswg.org/cssom/#concept-css-style-sheet-location
    WTF::URL styleSheetURL { };

    // The referrer policy of the style sheet containing the url().
    // https://w3c.github.io/webappsec-referrer-policy/#integration-with-css
    ReferrerPolicy styleSheetReferrerPolicy { ReferrerPolicy::EmptyString };

    // The origin-clean flag of the style sheet containing the url().
    // https://drafts.csswg.org/cssom/#concept-css-style-sheet-origin-clean-flag
    bool isStyleSheetOriginClean { true };

    bool operator==(const URLProvenance&) const = default;
};

// MARK: - Hashing

void add(Hasher&, const URLProvenance&);

} // namespace WebCore::CSS
