/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "NavigationRequester.h"

#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "ProcessIdentifier.h"
#include "SecurityOrigin.h"
#include "Site.h"

namespace WebCore {

bool shouldNavigationLoseFrameSpecificStorageAccess(const NavigationRequester& requester, FrameIdentifier navigatedFrame, const URL& fromURL, const URL& toURL)
{
    if (!requester.frameID)
        return false;

    // A different, cross-site frame initiating the navigation drops access. A self-navigation
    // drops access only when it crosses origins.
    // https://privacycg.github.io/storage-access/#navigation
    // FIXME: The algorithm also drops access when the navigation went through a cross-origin
    // redirect; that needs redirect information not available here and is not implemented.
    if (*requester.frameID != navigatedFrame)
        return Site(requester.url) != Site(fromURL);
    return !SecurityOrigin::create(fromURL)->isSameOriginAs(SecurityOrigin::create(toURL));
}

NavigationRequester NavigationRequester::from(Document& document)
{
    RefPtr frame = document.frame();
    RefPtr parentFrame = frame ? frame->tree().parent() : nullptr;

    return {
        document.url().isEmpty() ? aboutBlankURL() : document.url(),
        document.securityOrigin(),
        document.topOrigin(),
        document.policyContainer(),
        document.frameID(),
        frame ? std::make_optional(frame->tree().top().frameID()) : std::nullopt,
        document.pageID(),
        document.identifier(),
        document.sandboxFlags(),
        frame ? frame->sandboxFlagsFromSandboxAttributeNotCSP() : SandboxFlags { },
        document.hasLoadedThirdPartyScript(),
        document.hasLoadedThirdPartyFrame(),
        frame ? frame->hasHadUserInteraction() : false,
        [&] {
            RefPtr parentOrigin = parentFrame ? parentFrame->frameDocumentSecurityOrigin() : nullptr;
            return parentOrigin && parentOrigin->isSameOriginDomain(protect(document.topOrigin()));
        }(),
        Process::identifier()
    };
}

} // namespace WebCore
