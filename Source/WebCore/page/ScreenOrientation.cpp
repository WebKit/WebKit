/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ScreenOrientation.h"

#include "DOMWindow.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FullscreenManager.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ScreenOrientation);

Ref<ScreenOrientation> ScreenOrientation::create(Document* document)
{
    auto screenOrientation = adoptRef(*new ScreenOrientation(document));
    screenOrientation->suspendIfNeeded();
    return screenOrientation;
}

ScreenOrientation::ScreenOrientation(Document* document)
    : ActiveDOMObject(document)
{
    if (shouldListenForChangeNotification()) {
        if (auto* manager = this->manager())
            manager->addObserver(*this);
    }
}

ScreenOrientation::~ScreenOrientation()
{
    if (auto* manager = this->manager())
        manager->removeObserver(*this);
}

Document* ScreenOrientation::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

ScreenOrientationManager* ScreenOrientation::manager() const
{
    auto* document = this->document();
    if (!document)
        return nullptr;
    auto* page = document->page();
    return page ? page->screenOrientationManager() : nullptr;
}

static bool isSupportedLockType(ScreenOrientationLockType lockType)
{
    switch (lockType) {
    case ScreenOrientationLockType::Any:
    case ScreenOrientationLockType::Natural:
    case ScreenOrientationLockType::Portrait:
    case ScreenOrientationLockType::Landscape:
        return true;
    default:
        return false;
    }
}

void ScreenOrientation::lock(LockType lockType, Ref<DeferredPromise>&& promise)
{
    auto* document = this->document();
    if (!document || !document->isFullyActive()) {
        promise->reject(Exception { InvalidStateError, "Document is not fully active."_s });
        return;
    }

    auto* manager = this->manager();
    if (!manager) {
        promise->reject(Exception { InvalidStateError, "No browsing context"_s });
        return;
    }

    // FIXME: Add support for the sandboxed orientation lock browsing context flag.
    if (!document->isSameOriginAsTopDocument()) {
        promise->reject(Exception { SecurityError, "Only first party documents can lock the screen orientation"_s });
        return;
    }

    if (document->page() && !document->page()->isVisible()) {
        promise->reject(Exception { SecurityError, "Only visible documents can lock the screen orientation"_s });
        return;
    }

    if (document->settings().fullscreenRequirementForScreenOrientationLockingEnabled()) {
#if ENABLE(FULLSCREEN_API)
        if (!document->fullscreenManager().isFullscreen()) {
#else
        if (true) {
#endif
            promise->reject(Exception { SecurityError, "Locking the screen orientation is only allowed when in fullscreen"_s });
            return;
        }
    }
    if (!isSupportedLockType(lockType)) {
        promise->reject(Exception { NotSupportedError, "Lock type should be one of { \"any\", \"natural\", \"portrait\", \"landscape\" }"_s });
        return;
    }
    if (auto previousPromise = manager->takeLockPromise()) {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [previousPromise = WTFMove(previousPromise)]() mutable {
            previousPromise->reject(Exception { AbortError, "A new lock request was started"_s });
        });
    }
    manager->setLockPromise(*this, WTFMove(promise));
    manager->lock(lockType, [this, protectedThis = makePendingActivity(*this)](std::optional<Exception>&& exception) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, exception = WTFMove(exception)]() mutable {
            auto* manager = this->manager();
            if (!manager)
                return;

            auto promise = manager->takeLockPromise();
            if (!promise)
                return;

            if (exception)
                promise->reject(WTFMove(*exception));
            else
                promise->resolve();
        });
    });
}

ExceptionOr<void> ScreenOrientation::unlock()
{
    auto* document = this->document();
    if (!document || !document->isFullyActive())
        return Exception { InvalidStateError, "Document is not fully active."_s };

    if (!document->isSameOriginAsTopDocument())
        return { };

    if (document->page() && !document->page()->isVisible())
        return Exception { SecurityError, "Only visible documents can unlock the screen orientation"_s };

    if (auto* manager = this->manager())
        manager->unlock();
    return { };
}

auto ScreenOrientation::type() const -> Type
{
    auto* manager = this->manager();
    if (!manager)
        return Type::PortraitPrimary;
    return manager->currentOrientation();
}

uint16_t ScreenOrientation::angle() const
{
    auto* manager = this->manager();
    if (!manager)
        return 0;

    // The angle should depend on the device's natural orientation. We currently
    // consider Portrait as the natural orientation.
    switch (manager->currentOrientation()) {
    case Type::PortraitPrimary:
        return 0;
    case Type::PortraitSecondary:
        return 180;
    case Type::LandscapePrimary:
        return 90;
    case Type::LandscapeSecondary:
        return 270;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void ScreenOrientation::visibilityStateChanged()
{
    auto* document = this->document();
    if (!document)
        return;
    auto* manager = this->manager();
    if (!manager)
        return;

    if (shouldListenForChangeNotification())
        manager->addObserver(*this);
    else
        manager->removeObserver(*this);
}

bool ScreenOrientation::shouldListenForChangeNotification() const
{
    auto* document = this->document();
    if (!document || !document->frame())
        return false;
    return document->visibilityState() == VisibilityState::Visible;
}

void ScreenOrientation::screenOrientationDidChange(ScreenOrientationType)
{
    queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().changeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

const char* ScreenOrientation::activeDOMObjectName() const
{
    return "ScreenOrientation";
}

void ScreenOrientation::suspend(ReasonForSuspension)
{
    if (auto* manager = this->manager())
        manager->removeObserver(*this);
}

void ScreenOrientation::resume()
{
    if (!shouldListenForChangeNotification())
        return;
    if (auto* manager = this->manager())
        manager->addObserver(*this);
}

void ScreenOrientation::stop()
{
    auto* manager = this->manager();
    if (!manager)
        return;

    manager->removeObserver(*this);
    if (manager->lockRequester() == this)
        manager->takeLockPromise()->reject(Exception { AbortError, "Document is no longer fully active"_s });
}

bool ScreenOrientation::virtualHasPendingActivity() const
{
    return m_hasChangeEventListener;
}

void ScreenOrientation::eventListenersDidChange()
{
    m_hasChangeEventListener = hasEventListeners(eventNames().changeEvent);
}

} // namespace WebCore
