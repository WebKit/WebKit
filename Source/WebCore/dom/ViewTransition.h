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

#pragma once

#include "Element.h"
#include "ExceptionOr.h"
#include "JSValueInWrappedObject.h"
#include "ViewTransitionUpdateCallback.h"
#include <wtf/CheckedRef.h>
#include <wtf/Ref.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class DOMPromise;
class DeferredPromise;

enum class ViewTransitionPhase : uint8_t {
    PendingCapture,
    UpdateCallbackCalled,
    Animating,
    Done
};

struct CapturedElement {
    // FIXME: Add the following:
    // old image (2d bitmap)
    // old width / height
    // old transform
    // old writing mode
    // old direction
    // old text-orientation
    // old mix-blend-mode
    RefPtr<Element> newElement;

    // FIXME: Also handle these:
    // group keyframes
    // group animation name rule
    // group styles rule
    // image pair isolation rule
    // image animation name rule
};

class ViewTransition : public RefCounted<ViewTransition>, public CanMakeWeakPtr<ViewTransition> {
public:
    static Ref<ViewTransition> create(Document&, RefPtr<ViewTransitionUpdateCallback>&&);
    ~ViewTransition();

    void skipTransition();
    void skipViewTransition(ExceptionOr<JSC::JSValue>&&);
    void callUpdateCallback();

    void setupViewTransition();
    void captureOldState();
    void captureNewState();
    void setupTransitionPseudoElements();
    void activateViewTransition();
    void handleTransitionFrame();
    void clearViewTransition();

    DOMPromise& ready();
    DOMPromise& updateCallbackDone();
    DOMPromise& finished();

    ViewTransitionPhase phase() const { return m_phase; }
    const HashMap<AtomString, CapturedElement>& namedElements() const { return m_namedElements; };

    RefPtr<Document> protectedDocument() const { return m_document.get(); }

private:
    ViewTransition(Document&, RefPtr<ViewTransitionUpdateCallback>&&);

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;

    HashMap<AtomString, CapturedElement> m_namedElements;
    ViewTransitionPhase m_phase { ViewTransitionPhase::PendingCapture };

    RefPtr<ViewTransitionUpdateCallback> m_updateCallback;

    using PromiseAndWrapper = std::pair<Ref<DOMPromise>, Ref<DeferredPromise>>;
    PromiseAndWrapper m_ready;
    PromiseAndWrapper m_updateCallbackDone;
    PromiseAndWrapper m_finished;

    // WeakPtr<RenderObject> m_rootPseudoElement;
    FloatSize m_initialSnapshotContainingBlockSize;
};

}
