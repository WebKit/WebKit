/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "UserGestureIndicator.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "ResourceLoadObserver.h"
#include "SecurityOrigin.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static RefPtr<UserGestureToken>& currentToken()
{
    ASSERT(isMainThread());
    static NeverDestroyed<RefPtr<UserGestureToken>> token;
    return token;
}

UserGestureToken::UserGestureToken(ProcessingUserGestureState state, UserGestureType gestureType, Document* document)
    : m_state(state)
    , m_gestureType(gestureType)
{
    if (!document || !processingUserGesture())
        return;

    // User gesture is valid for the document that received the user gesture, all of its ancestors
    // as well as all same-origin documents on the page.
    m_documentsImpactedByUserGesture.add(*document);

    auto* documentFrame = document->frame();
    if (!documentFrame)
        return;

    for (auto* ancestorFrame = documentFrame->tree().parent(); ancestorFrame; ancestorFrame = ancestorFrame->tree().parent()) {
        auto* localAncestor = dynamicDowncast<LocalFrame>(ancestorFrame);
        if (!localAncestor)
            continue;
        if (auto* ancestorDocument = localAncestor->document())
            m_documentsImpactedByUserGesture.add(*ancestorDocument);
    }

    auto& documentOrigin = document->securityOrigin();
    for (AbstractFrame* frame = &documentFrame->tree().top(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* frameDocument = localFrame->document();
        if (frameDocument && documentOrigin.isSameOriginDomain(frameDocument->securityOrigin()))
            m_documentsImpactedByUserGesture.add(*frameDocument);
    }
}

UserGestureToken::~UserGestureToken()
{
    for (auto& observer : m_destructionObservers)
        observer(*this);
}

static Seconds maxIntervalForUserGestureForwardingForFetch { 10 };
const Seconds& UserGestureToken::maximumIntervalForUserGestureForwardingForFetch()
{
    return maxIntervalForUserGestureForwardingForFetch;
}

void UserGestureToken::setMaximumIntervalForUserGestureForwardingForFetchForTesting(Seconds value)
{
    maxIntervalForUserGestureForwardingForFetch = WTFMove(value);
}

bool UserGestureToken::isValidForDocument(const Document& document) const
{
    return m_documentsImpactedByUserGesture.contains(document);
}

UserGestureIndicator::UserGestureIndicator(std::optional<ProcessingUserGestureState> state, Document* document, UserGestureType gestureType, ProcessInteractionStyle processInteractionStyle)
    : m_previousToken { currentToken() }
{
    ASSERT(isMainThread());

    if (state)
        currentToken() = UserGestureToken::create(state.value(), gestureType, document);

    if (state && document && currentToken()->processingUserGesture()) {
        document->updateLastHandledUserGestureTimestamp(currentToken()->startTime());
        if (processInteractionStyle == ProcessInteractionStyle::Immediate)
            ResourceLoadObserver::shared().logUserInteractionWithReducedTimeResolution(document->topDocument());
        document->topDocument().setUserDidInteractWithPage(true);
        if (auto* frame = document->frame()) {
            if (!frame->hasHadUserInteraction()) {
                for (AbstractFrame *ancestor = frame; ancestor; ancestor = ancestor->tree().parent()) {
                    auto* localAncestor = dynamicDowncast<LocalFrame>(ancestor);
                    if (!localAncestor)
                        continue;
                    localAncestor->setHasHadUserInteraction();
                }
            }
        }

        if (auto* window = document->domWindow())
            window->notifyActivated(currentToken()->startTime());
    }
}

UserGestureIndicator::UserGestureIndicator(RefPtr<UserGestureToken> token, UserGestureToken::GestureScope scope, UserGestureToken::IsPropagatedFromFetch isPropagatedFromFetch)
{
    // Silently ignore UserGestureIndicators on non main threads.
    if (!isMainThread())
        return;

    // It is only safe to use currentToken() on the main thread.
    m_previousToken = currentToken();

    if (token) {
        token->setScope(scope);
        token->setIsPropagatedFromFetch(isPropagatedFromFetch);
        currentToken() = token;
    }
}

UserGestureIndicator::~UserGestureIndicator()
{
    if (!isMainThread())
        return;
    
    if (auto token = currentToken()) {
        token->resetDOMPasteAccess();
        token->resetScope();
        token->resetIsPropagatedFromFetch();
    }

    currentToken() = m_previousToken;
}

RefPtr<UserGestureToken> UserGestureIndicator::currentUserGesture()
{
    if (!isMainThread())
        return nullptr;

    return currentToken();
}

bool UserGestureIndicator::processingUserGesture(const Document* document)
{
    if (!isMainThread())
        return false;

    if (!currentToken() || !currentToken()->processingUserGesture())
        return false;

    return !document || currentToken()->isValidForDocument(*document);
}

bool UserGestureIndicator::processingUserGestureForMedia()
{
    if (!isMainThread())
        return false;

    return currentToken() ? currentToken()->processingUserGestureForMedia() : false;
}

}
