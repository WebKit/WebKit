/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must reproduce the above copyright
 *    notice, this list of conditions and the following conditions
 *    the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following conditions and the
 *    following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES SUBSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF LIABILITY, WHETHER
 * IN OUT OF THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * ASSOCIATED WITH THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/QualifiedName.h>
#include <WebCore/ReferrerPolicy.h>

#include <wtf/OptionSet.h>
#include <wtf/URL.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class Element;
class Event;

// Link relation bitmask values.
enum class Relation : uint8_t {
    NoReferrer = 1 << 0,
    NoOpener = 1 << 1,
    Opener = 1 << 2,
};

OptionSet<Relation> relationsForRelAttribute(const AtomString& relValue);
bool isSupportedRelToken(const Document&, StringView token);
bool hasRel(OptionSet<Relation> linkRelations, Relation);

AtomString parseDownloadAttribute(const Element&, const URL& completedURL, const QualifiedName& downloadAttr);

void sendPings(const Element&, const QualifiedName& pingAttr, const URL& destinationURL);

ReferrerPolicy effectiveReferrerPolicy(OptionSet<Relation>, ReferrerPolicy explicitPolicy);

void navigateHyperlink(
    Element& sourceElement,
    Event&,
    const URL& completedURL,
    const AtomString& target,
    OptionSet<Relation> relations,
    ReferrerPolicy,
    const AtomString& downloadAttribute = { },
    std::optional<PrivateClickMeasurement>&& = std::nullopt);

} // namespace WebCore
