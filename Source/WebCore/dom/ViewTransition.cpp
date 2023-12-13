/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "ViewTransition.h"

#include "CheckVisibilityOptions.h"
#include "Document.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "TypedElementDescendantIteratorInlines.h"

namespace WebCore {

static std::pair<Ref<DOMPromise>, Ref<DeferredPromise>> createPromiseAndWrapper(Document& document)
{
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(document.globalObject());
    JSC::JSLockHolder lock(globalObject.vm());
    RefPtr deferredPromise = DeferredPromise::create(globalObject);
    Ref domPromise = DOMPromise::create(globalObject, *JSC::jsCast<JSC::JSPromise*>(deferredPromise->promise()));
    return { WTFMove(domPromise), deferredPromise.releaseNonNull() };
}

ViewTransition::ViewTransition(Document& document, RefPtr<ViewTransitionUpdateCallback>&& updateCallback)
    : m_document(document)
    , m_updateCallback(WTFMove(updateCallback))
    , m_ready(createPromiseAndWrapper(document))
    , m_updateCallbackDone(createPromiseAndWrapper(document))
    , m_finished(createPromiseAndWrapper(document))
{
}

ViewTransition::~ViewTransition() = default;

Ref<ViewTransition> ViewTransition::create(Document& document, RefPtr<ViewTransitionUpdateCallback>&& updateCallback)
{
    return adoptRef(*new ViewTransition(document, WTFMove(updateCallback)));
}

DOMPromise& ViewTransition::ready()
{
    return m_ready.first.get();
}

DOMPromise& ViewTransition::updateCallbackDone()
{
    return m_updateCallbackDone.first.get();
}

DOMPromise& ViewTransition::finished()
{
    return m_finished.first.get();
}

// https://drafts.csswg.org/css-view-transitions/#skip-the-view-transition
void ViewTransition::skipViewTransition(ExceptionOr<JSC::JSValue>&& reason)
{
    if (!m_document)
        return;

    ASSERT(m_document->activeViewTransition() == this);
    ASSERT(m_phase != ViewTransitionPhase::Done);

    if (m_phase < ViewTransitionPhase::UpdateCallbackCalled) {
        protectedDocument()->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [this, weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (protectedThis)
                callUpdateCallback();
        });
    }

    // FIXME: Set rendering suppression for view transitions to false.

    if (m_document->activeViewTransition() == this)
        clearViewTransition();

    m_phase = ViewTransitionPhase::Done;

    if (reason.hasException())
        m_ready.second->reject(reason.releaseException());
    else {
        m_ready.second->rejectWithCallback([&] (auto&) {
            return reason.releaseReturnValue();
        }, RejectAsHandled::Yes);
    }

    m_updateCallbackDone.first->whenSettled([this, protectedThis = Ref { *this }] {
        switch (m_updateCallbackDone.first->status()) {
        case DOMPromise::Status::Fulfilled:
            m_finished.second->resolve();
            break;
        case DOMPromise::Status::Rejected:
            m_finished.second->rejectWithCallback([&] (auto&) {
                return m_updateCallbackDone.first->result();
            }, RejectAsHandled::Yes);
            break;
        case DOMPromise::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

// https://drafts.csswg.org/css-view-transitions/#ViewTransition-skipTransition
void ViewTransition::skipTransition()
{
    if (m_phase != ViewTransitionPhase::Done)
        skipViewTransition(Exception { ExceptionCode::AbortError, "Skipping view transition because skipTransition() was called."_s });
}

// https://drafts.csswg.org/css-view-transitions/#call-dom-update-callback-algorithm
void ViewTransition::callUpdateCallback()
{
    if (!m_document)
        return;

    ASSERT(m_phase < ViewTransitionPhase::UpdateCallbackCalled || m_phase == ViewTransitionPhase::Done);

    RefPtr<DOMPromise> callbackPromise;
    if (!m_updateCallback) {
        auto promiseAndWrapper = createPromiseAndWrapper(*m_document);
        promiseAndWrapper.second->resolve();
        callbackPromise = WTFMove(promiseAndWrapper.first);
    } else {
        auto result = m_updateCallback->handleEvent();
        callbackPromise = result.type() == CallbackResultType::Success ? result.releaseReturnValue() : nullptr;
        if (!callbackPromise || callbackPromise->isSuspended()) {
            auto promiseAndWrapper = createPromiseAndWrapper(*m_document);
            // FIXME: First case should reject with `ExceptionCode::ExistingExceptionError`.
            if (result.type() == CallbackResultType::ExceptionThrown)
                promiseAndWrapper.second->reject(ExceptionCode::TypeError);
            else
                promiseAndWrapper.second->reject();
            callbackPromise = WTFMove(promiseAndWrapper.first);
        }
    }

    if (m_phase != ViewTransitionPhase::Done)
        m_phase = ViewTransitionPhase::UpdateCallbackCalled;

    callbackPromise->whenSettled([this, weakThis = WeakPtr { *this }, callbackPromise] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        switch (callbackPromise->status()) {
        case DOMPromise::Status::Fulfilled:
            m_updateCallbackDone.second->resolve();
            break;
        case DOMPromise::Status::Rejected:
            m_updateCallbackDone.second->rejectWithCallback([&] (auto&) {
                return callbackPromise->result();
            }, RejectAsHandled::No);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

// https://drafts.csswg.org/css-view-transitions/#setup-view-transition-algorithm
void ViewTransition::setupViewTransition()
{
    if (!m_document)
        return;

    ASSERT(m_phase == ViewTransitionPhase::PendingCapture);

    m_phase = ViewTransitionPhase::CapturingOldState;

    auto checkFailure = captureOldState();
    if (checkFailure.hasException()) {
        skipViewTransition(checkFailure.releaseException());
        return;
    }

    // FIXME: Set document’s rendering suppression for view transitions to true.
    protectedDocument()->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [this, weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (m_phase == ViewTransitionPhase::Done)
            return;

        callUpdateCallback();
        m_updateCallbackDone.first->whenSettled([this, weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            switch (m_updateCallbackDone.first->status()) {
            case DOMPromise::Status::Fulfilled:
                activateViewTransition();
                break;
            case DOMPromise::Status::Rejected:
                if (m_phase == ViewTransitionPhase::Done)
                    return;
                skipViewTransition(m_updateCallbackDone.first->result());
                break;
            case DOMPromise::Status::Pending:
                ASSERT_NOT_REACHED();
                break;
            }
        });

        // FIXME: Handle timeout.
    });
}

static AtomString effectiveViewTransitionName(Element& element)
{
    CheckVisibilityOptions visibilityOptions { .contentVisibilityAuto = true };
    if (!element.checkVisibility(visibilityOptions))
        return nullAtom();
    ASSERT(element.computedStyle());
    auto transitionName = element.computedStyle()->viewTransitionName();
    if (!transitionName)
        return nullAtom();
    return transitionName->name;
}

static ExceptionOr<void> checkDuplicateViewTransitionName(const AtomString& name, ListHashSet<AtomString>& usedTransitionNames)
{
    if (usedTransitionNames.contains(name))
        return Exception { ExceptionCode::InvalidStateError, makeString("Multiple elements found with view-transition-name: "_s, name) };
    usedTransitionNames.add(name);
    return { };
}

// https://drafts.csswg.org/css-view-transitions/#capture-old-state-algorithm
ExceptionOr<void> ViewTransition::captureOldState()
{
    if (!m_document)
        return { };
    ListHashSet<AtomString> usedTransitionNames;
    Vector<Ref<Element>> captureElements;
    Ref document = *m_document;
    // FIXME: Set transition’s initial snapshot containing block size to the snapshot containing block size.
    // FIXME: Loop should probably use flat tree.
    for (Ref element : descendantsOfType<Element>(document)) {
        // FIXME: This check should also cover fragmented content.
        if (auto name = effectiveViewTransitionName(element); !name.isNull()) {
            if (auto check = checkDuplicateViewTransitionName(name, usedTransitionNames); check.hasException())
                return check.releaseException();
            // FIXME: Set element’s captured in a view transition to true.
            captureElements.append(element);
        }
    }
    // FIXME: Sort captureElements in paint order.
    for (auto& element : captureElements) {
        // FIXME: Fill in the rest of CapturedElement.
        CapturedElement capture;
        auto transitionName = element->computedStyle()->viewTransitionName();
        m_namedElements.add(transitionName->name, capture);
    }
    return { };
}

// https://drafts.csswg.org/css-view-transitions/#capture-new-state-algorithm
ExceptionOr<void> ViewTransition::captureNewState()
{
    if (!m_document)
        return { };
    ListHashSet<AtomString> usedTransitionNames;
    Ref document = *m_document;
    // FIXME: Loop should probably use flat tree.
    for (Ref element : descendantsOfType<Element>(document)) {
        if (auto name = effectiveViewTransitionName(element); !name.isNull()) {
            if (auto check = checkDuplicateViewTransitionName(name, usedTransitionNames); check.hasException())
                return check.releaseException();

            if (!m_namedElements.contains(name)) {
                CapturedElement capturedElement;
                m_namedElements.add(name, capturedElement);
            }
            m_namedElements.find(name)->newElement = element.ptr();
        }
    }
    return { };
}

// https://drafts.csswg.org/css-view-transitions/#setup-transition-pseudo-elements
void ViewTransition::setupTransitionPseudoElements()
{
    protectedDocument()->setHasViewTransitionPseudoElementTree(true);
    // FIXME: Implement step 9.

    if (RefPtr documentElement = protectedDocument()->documentElement())
        documentElement->invalidateStyleInternal();
}

// https://drafts.csswg.org/css-view-transitions/#activate-view-transition
void ViewTransition::activateViewTransition()
{
    if (m_phase == ViewTransitionPhase::Done)
        return;

    // FIXME: Set rendering suppression for view transitions to false.

    // FIXME: If transition’s initial snapshot containing block size is not equal to the snapshot containing block size, then skip the view transition for transition, and return.

    auto checkFailure = captureNewState();
    if (checkFailure.hasException()) {
        skipViewTransition(checkFailure.releaseException());
        return;
    }

    // FIXME: Set captured element flag to true.

    setupTransitionPseudoElements();
    // FIXME: Update pseudo-element styles for transition.

    m_phase = ViewTransitionPhase::Animating;
    m_ready.second->resolve();
}

// https://drafts.csswg.org/css-view-transitions/#handle-transition-frame-algorithm
void ViewTransition::handleTransitionFrame()
{
    bool hasActiveAnimations = false;

    // FIXME: Actually query the animation state.

    if (!hasActiveAnimations) {
        m_phase = ViewTransitionPhase::Done;
        clearViewTransition();
        m_finished.second->resolve();
    }

    // FIXME: If transition’s initial snapshot containing block size is not equal to the snapshot containing block size, then skip the view transition for transition, and return.
    // FIXME: Update pseudo-element styles for transition.
}

// https://drafts.csswg.org/css-view-transitions/#clear-view-transition-algorithm
void ViewTransition::clearViewTransition()
{
    if (!m_document)
        return;

    ASSERT(m_document->activeViewTransition() == this);

    // FIXME: Implement step 3.

    protectedDocument()->setHasViewTransitionPseudoElementTree(false);
    protectedDocument()->setActiveViewTransition(nullptr);

    if (RefPtr documentElement = protectedDocument()->documentElement())
        documentElement->invalidateStyleInternal();
}

}
