/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "SessionState.h"

#include "SessionStateConversion.h"
#include <WebCore/BackForwardFrameItemIdentifier.h>
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/SerializedScriptValue.h>

namespace WebKit {

FrameState::~FrameState()
{
    RELEASE_ASSERT(RunLoop::isMain());
}

FrameState::FrameState()
{
    RELEASE_ASSERT(RunLoop::isMain());
}

FrameState::FrameState(String&& urlString, String&& originalURLString, String&& referrer, AtomString&& target, std::optional<WebCore::FrameIdentifier> frameID, std::optional<Vector<uint8_t>>&& stateObjectData, int64_t documentSequenceNumber, int64_t itemSequenceNumber, std::optional<WTF::UUID> navigationAPIKey, WebCore::IntPoint scrollPosition, bool shouldRestoreScrollPosition, float pageScaleFactor, std::optional<HTTPBody>&& httpBody, std::optional<WebCore::BackForwardItemIdentifier> itemID, std::optional<WebCore::BackForwardFrameItemIdentifier> frameItemID, String&& title, WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, RefPtr<WebCore::SerializedScriptValue>&& sessionStateObject, bool wasCreatedByJSWithoutUserInteraction, bool wasRestoredFromSession, WebCore::IsInitialAboutBlank isInitialAboutBlank, std::optional<WebCore::PolicyContainer>&& policyContainer,
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect exposedContentRect, WebCore::IntRect unobscuredContentRect, WebCore::FloatSize minimumLayoutSizeInScrollViewCoordinates, WebCore::IntSize contentSize, bool scaleIsInitial, WebCore::FloatBoxExtent obscuredInsets,
#endif
    Vector<Ref<FrameState>>&& children, Vector<AtomString>&& documentState
)
    : urlString(WTF::move(urlString))
    , originalURLString(WTF::move(originalURLString))
    , referrer(WTF::move(referrer))
    , target(WTF::move(target))
    , frameID(frameID)
    , stateObjectData(WTF::move(stateObjectData))
    , documentSequenceNumber(documentSequenceNumber)
    , itemSequenceNumber(itemSequenceNumber)
    , navigationAPIKey(navigationAPIKey)
    , scrollPosition(scrollPosition)
    , shouldRestoreScrollPosition(shouldRestoreScrollPosition)
    , pageScaleFactor(pageScaleFactor)
    , httpBody(WTF::move(httpBody))
    , itemID(itemID)
    , frameItemID(frameItemID)
    , title(WTF::move(title))
    , shouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy)
    , sessionStateObject(WTF::move(sessionStateObject))
    , wasCreatedByJSWithoutUserInteraction(wasCreatedByJSWithoutUserInteraction)
    , wasRestoredFromSession(wasRestoredFromSession)
    , isInitialAboutBlank(isInitialAboutBlank)
    , policyContainer(WTF::move(policyContainer))
#if PLATFORM(IOS_FAMILY)
    , exposedContentRect(exposedContentRect)
    , unobscuredContentRect(unobscuredContentRect)
    , minimumLayoutSizeInScrollViewCoordinates(minimumLayoutSizeInScrollViewCoordinates)
    , contentSize(contentSize)
    , scaleIsInitial(scaleIsInitial)
    , obscuredInsets(obscuredInsets)
#endif
    , children(WTF::move(children))
    , m_documentState(WTF::move(documentState))
{
}

FrameState::FrameState(const String& urlString, const String& originalURLString, const String& referrer, const AtomString& target, std::optional<WebCore::FrameIdentifier> frameID, std::optional<Vector<uint8_t>> stateObjectData, int64_t documentSequenceNumber, int64_t itemSequenceNumber, std::optional<WTF::UUID> navigationAPIKey, WebCore::IntPoint scrollPosition, bool shouldRestoreScrollPosition, float pageScaleFactor, const std::optional<HTTPBody>& httpBody, std::optional<WebCore::BackForwardItemIdentifier> itemID, std::optional<WebCore::BackForwardFrameItemIdentifier> frameItemID, const String& title, WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, RefPtr<WebCore::SerializedScriptValue>&& sessionStateObject, bool wasCreatedByJSWithoutUserInteraction, bool wasRestoredFromSession, WebCore::IsInitialAboutBlank isInitialAboutBlank, const std::optional<WebCore::PolicyContainer>& policyContainer,
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect exposedContentRect, WebCore::IntRect unobscuredContentRect, WebCore::FloatSize minimumLayoutSizeInScrollViewCoordinates, WebCore::IntSize contentSize, bool scaleIsInitial, WebCore::FloatBoxExtent obscuredInsets,
#endif
    const Vector<Ref<FrameState>>& children, const Vector<AtomString>& documentState
)
    : urlString(urlString)
    , originalURLString(originalURLString)
    , referrer(referrer)
    , target(target)
    , frameID(frameID)
    , stateObjectData(stateObjectData)
    , documentSequenceNumber(documentSequenceNumber)
    , itemSequenceNumber(itemSequenceNumber)
    , navigationAPIKey(navigationAPIKey)
    , scrollPosition(scrollPosition)
    , shouldRestoreScrollPosition(shouldRestoreScrollPosition)
    , pageScaleFactor(pageScaleFactor)
    , httpBody(httpBody)
    , itemID(itemID)
    , frameItemID(frameItemID)
    , title(title)
    , shouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy)
    , sessionStateObject(WTF::move(sessionStateObject))
    , wasCreatedByJSWithoutUserInteraction(wasCreatedByJSWithoutUserInteraction)
    , wasRestoredFromSession(wasRestoredFromSession)
    , isInitialAboutBlank(isInitialAboutBlank)
    , policyContainer(policyContainer)
#if PLATFORM(IOS_FAMILY)
    , exposedContentRect(exposedContentRect)
    , unobscuredContentRect(unobscuredContentRect)
    , minimumLayoutSizeInScrollViewCoordinates(minimumLayoutSizeInScrollViewCoordinates)
    , contentSize(contentSize)
    , scaleIsInitial(scaleIsInitial)
    , obscuredInsets(obscuredInsets)
#endif
    , children(children)
    , m_documentState(documentState)
{
}

Ref<FrameState> FrameState::copy()
{
    return adoptRef(*new FrameState(
        urlString,
        originalURLString,
        referrer,
        target,
        frameID,
        stateObjectData,
        documentSequenceNumber,
        itemSequenceNumber,
        navigationAPIKey,
        scrollPosition,
        shouldRestoreScrollPosition,
        pageScaleFactor,
        httpBody,
        itemID,
        frameItemID,
        title,
        shouldOpenExternalURLsPolicy,
        sessionStateObject.copyRef(),
        wasCreatedByJSWithoutUserInteraction,
        wasRestoredFromSession,
        isInitialAboutBlank,
        policyContainer,
#if PLATFORM(IOS_FAMILY)
        exposedContentRect,
        unobscuredContentRect,
        minimumLayoutSizeInScrollViewCoordinates,
        contentSize,
        scaleIsInitial,
        obscuredInsets,
#endif
        children.map([](auto& child) { return child->copy(); }),
        m_documentState
    ));
}

void FrameState::replacePayloadFrom(Ref<FrameState>&& other)
{
    // frameItemID and itemID are the position of this FrameState in the BF list tree
    // owned by the surrounding WebBackForwardListFrameItem and are fixed at construction.
    // children is the parallel data path of WebBackForwardListFrameItem::m_children and
    // is maintained there. Neither is affected by a payload replacement.
    urlString = WTF::move(other->urlString);
    originalURLString = WTF::move(other->originalURLString);
    referrer = WTF::move(other->referrer);
    target = WTF::move(other->target);
    frameID = other->frameID;
    stateObjectData = WTF::move(other->stateObjectData);
    documentSequenceNumber = other->documentSequenceNumber;
    itemSequenceNumber = other->itemSequenceNumber;
    navigationAPIKey = other->navigationAPIKey;
    scrollPosition = other->scrollPosition;
    shouldRestoreScrollPosition = other->shouldRestoreScrollPosition;
    pageScaleFactor = other->pageScaleFactor;
    httpBody = WTF::move(other->httpBody);
    title = WTF::move(other->title);
    shouldOpenExternalURLsPolicy = other->shouldOpenExternalURLsPolicy;
    sessionStateObject = WTF::move(other->sessionStateObject);
    wasCreatedByJSWithoutUserInteraction = other->wasCreatedByJSWithoutUserInteraction;
    wasRestoredFromSession = other->wasRestoredFromSession;
    isInitialAboutBlank = other->isInitialAboutBlank;
    policyContainer = WTF::move(other->policyContainer);
#if PLATFORM(IOS_FAMILY)
    exposedContentRect = other->exposedContentRect;
    unobscuredContentRect = other->unobscuredContentRect;
    minimumLayoutSizeInScrollViewCoordinates = other->minimumLayoutSizeInScrollViewCoordinates;
    contentSize = other->contentSize;
    scaleIsInitial = other->scaleIsInitial;
    obscuredInsets = other->obscuredInsets;
#endif
    m_documentState = WTF::move(other->m_documentState);
}

bool FrameState::validateDocumentState(const Vector<AtomString>& documentState)
{
    for (auto& stateString : documentState) {
        if (stateString.isNull())
            continue;

        if (!stateString.is8Bit())
            continue;

        // rdar://48634553 indicates 8-bit string can be invalid.
        for (auto character : stateString.span8())
            RELEASE_ASSERT(isLatin1(character));
    }
    return true;
}

void FrameState::setDocumentState(const Vector<AtomString>& documentState, ShouldValidate shouldValidate)
{
    m_documentState = documentState;

    if (shouldValidate == ShouldValidate::Yes)
        validateDocumentState(m_documentState);
}

void FrameState::replaceChildFrameState(Ref<FrameState>&& frameState)
{
    for (auto& child : children) {
        if (child->frameID == frameState->frameID) {
            child = WTF::move(frameState);
            return;
        }
        child->replaceChildFrameState(frameState.copyRef());
    }
}

bool SessionState::isEqualForTesting(const SessionState& other) const
{
    if (!backForwardListState.isEqualForTesting(other.backForwardListState))
        return false;

    if (provisionalURL != other.provisionalURL)
        return false;

    if (isAppInitiated != other.isAppInitiated)
        return false;

    return true;
}

bool BackForwardListState::isEqualForTesting(const BackForwardListState& other) const
{
    if (items.size() != other.items.size())
        return false;

    for (size_t i = 0; i < items.size(); ++i) {
        if (!items[i].isEqualForTesting(other.items[i]))
            return false;
    }

    if (currentIndex != other.currentIndex)
        return false;

    return true;
}

bool BackForwardListItemState::isEqualForTesting(const BackForwardListItemState& other) const
{
    if (!frameState->isEqualForTesting(other.frameState.get()))
        return false;

    if (navigatedFrameID != other.navigatedFrameID)
        return false;

    return true;
}

bool FrameState::isEqualForTesting(const FrameState& other) const
{
    if (urlString != other.urlString)
        return false;

    if (originalURLString != other.originalURLString)
        return false;

    if (referrer != other.referrer)
        return false;

    if (target != other.target)
        return false;

    if (title != other.title)
        return false;

    if (children.size() != other.children.size())
        return false;

    for (size_t i = 0; i < children.size(); ++i) {
        auto& child = children[i];
        auto& otherChild = other.children[i];
        if (!child->isEqualForTesting(otherChild.get()))
            return false;
    }

    return true;
}

} // namespace WebKit
