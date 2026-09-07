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

#include "config.h"
#include "AnchorElementUtils.h"

#include "Document.h"
#include "Event.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "FrameDestructionObserverInlines.h"
#include "ElementInlines.h"
#include "OriginAccessPatterns.h"
#include "PingLoader.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SpaceSplitString.h"

#include <JavaScriptCore/ConsoleTypes.h>

namespace WebCore {

OptionSet<Relation> AnchorElementUtils::relationsForRelAttribute(const AtomString& relValue)
{
    // Update AnchorElementUtils::isSupportedRelToken() if more rel attributes values are supported.
    static MainThreadNeverDestroyed<const AtomString> noReferrer("noreferrer"_s);
    static MainThreadNeverDestroyed<const AtomString> noOpener("noopener"_s);
    static MainThreadNeverDestroyed<const AtomString> opener("opener"_s);

    OptionSet<Relation> relations;
    SpaceSplitString values(relValue, SpaceSplitString::ShouldFoldCase::Yes);
    if (values.contains(noReferrer))
        relations.add(Relation::NoReferrer);
    if (values.contains(noOpener))
        relations.add(Relation::NoOpener);
    if (values.contains(opener))
        relations.add(Relation::Opener);
    return relations;
}

bool AnchorElementUtils::isSupportedRelToken(Document& document, StringView token)
{
#if USE(SYSTEM_PREVIEW)
    if (equalLettersIgnoringASCIICase(token, "ar"_s))
        return document.settings().systemPreviewEnabled();
#else
    UNUSED_PARAM(document);
#endif
    return equalLettersIgnoringASCIICase(token, "noreferrer"_s)
        || equalLettersIgnoringASCIICase(token, "noopener"_s)
        || equalLettersIgnoringASCIICase(token, "opener"_s);
}

bool AnchorElementUtils::hasRel(OptionSet<Relation> linkRelations, Relation relation)
{
    return linkRelations.contains(relation);
}

AtomString AnchorElementUtils::parseDownloadAttribute(const Element& element, const URL& completedURL, const QualifiedName& downloadAttr)
{
    Ref document = element.document();
    if (!document->settings().downloadAttributeEnabled())
        return nullAtom();

    bool isSameOrigin = completedURL.protocolIsData() || protect(document->securityOrigin())->canRequest(completedURL, OriginAccessPatternsForWebProcess::singleton());
    if (isSameOrigin)
        return AtomString { ResourceResponse::sanitizeSuggestedFilename(element.attributeWithoutSynchronization(downloadAttr)) };

    if (element.hasAttributeWithoutSynchronization(downloadAttr))
        document->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "The download attribute on anchor was ignored because its href URL has a different security origin."_s);

    return nullAtom();
}

void AnchorElementUtils::sendPings(const Element& element, const QualifiedName& pingAttr, const URL& destinationURL)
{
    const auto& pingValue = element.attributeWithoutSynchronization(pingAttr);
    if (pingValue.isNull())
        return;

    Ref document = element.document();
    RefPtr frame = document->frame();
    if (!frame)
        return;

    SpaceSplitString pingURLs(pingValue, SpaceSplitString::ShouldFoldCase::No);
    for (auto& pingURL : pingURLs)
        PingLoader::sendPing(*frame, document->encodingParseURL(pingURL), destinationURL);
}

ReferrerPolicy AnchorElementUtils::effectiveReferrerPolicy(OptionSet<Relation> relations, ReferrerPolicy explicitPolicy)
{
    if (relations.contains(Relation::NoReferrer))
        return ReferrerPolicy::NoReferrer;
    return explicitPolicy;
}

void AnchorElementUtils::navigateHyperlink(
    Element& sourceElement,
    Event& event,
    const URL& completedURL,
    const AtomString& target,
    OptionSet<Relation> relations,
    ReferrerPolicy referrerPolicy,
    const AtomString& downloadAttribute,
    std::optional<PrivateClickMeasurement>&& privateClickMeasurement)
{
    Ref document = sourceElement.document();
    RefPtr frame = document->frame();
    if (!frame)
        return;

    NewFrameOpenerPolicy newFrameOpenerPolicy = NewFrameOpenerPolicy::Allow;
    if (relations.contains(Relation::NoOpener)
        || relations.contains(Relation::NoReferrer)
        || (!relations.contains(Relation::Opener) && isBlankTargetFrameName(target) && !completedURL.protocolIsJavaScript())) {
        newFrameOpenerPolicy = NewFrameOpenerPolicy::Suppress;
    }

    frame->loader().changeLocation(
        completedURL,
        target,
        &event,
        referrerPolicy,
        document->shouldOpenExternalURLsPolicyToPropagate(),
        newFrameOpenerPolicy,
        downloadAttribute,
        WTF::move(privateClickMeasurement),
        NavigationHistoryBehavior::Auto,
        &sourceElement);
}

} // namespace WebCore
