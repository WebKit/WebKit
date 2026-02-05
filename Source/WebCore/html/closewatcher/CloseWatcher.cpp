/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "CloseWatcher.h"

#include "ContextDestructionObserverInlines.h"
#include "CloseWatcherManager.h"
#include "DocumentWindow.h"
#include "Event.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "ExceptionOr.h"
#include "KeyboardEvent.h"
#include "LocalDOMWindow.h"
#include "ScriptExecutionContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CloseWatcher);

ExceptionOr<Ref<CloseWatcher>> CloseWatcher::create(ScriptExecutionContext& context, const Options& options)
{
    RefPtr document = dynamicDowncast<Document>(context);
    if (!document->isFullyActive())
        return Exception { ExceptionCode::InvalidStateError, "Document is not fully active."_s };

    Ref watcher = CloseWatcher::establish(*document);

    if (RefPtr signal = options.signal) {
        if (signal->aborted()) {
            watcher->m_active = false;
            Ref manager = protect(document->window())->closeWatcherManager();
            manager->remove(watcher.get());
        } else {
            watcher->m_signal = signal;
            watcher->m_signalAlgorithm = signal->addAlgorithm([weakWatcher = WeakPtr { watcher.get() }](JSC::JSValue) mutable {
                if (weakWatcher)
                    weakWatcher->destroy();
            });
        }
    }

    return watcher;
}

Ref<CloseWatcher> CloseWatcher::establish(Document& document)
{
    ASSERT(document.isFullyActive());

    Ref watcher = adoptRef(*new CloseWatcher(document));
    watcher->suspendIfNeeded();

    Ref manager = protect(document.window())->closeWatcherManager();

    manager->add(watcher);
    return watcher;
}

CloseWatcher::CloseWatcher(Document& document)
    : ActiveDOMObject(document)
{
}

ScriptExecutionContext* CloseWatcher::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void CloseWatcher::requestClose()
{
    requestToClose();
}

bool CloseWatcher::requestToClose()
{
    if (!canBeClosed())
        return true;

    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    Ref manager = protect(document->window())->closeWatcherManager();
    bool canPreventClose = manager->canPreventClose() && protect(document->window())->hasHistoryActionActivation();
    Ref cancelEvent = Event::create(eventNames().cancelEvent, Event::CanBubble::No, canPreventClose ? Event::IsCancelable::Yes : Event::IsCancelable::No);
    m_isRunningCancelAction = true;
    dispatchEvent(cancelEvent);
    m_isRunningCancelAction = false;
    if (cancelEvent->defaultPrevented()) {
        protect(document->window())->consumeHistoryActionUserActivation();
        return false;
    }

    close();
    return true;
}

void CloseWatcher::close()
{
    if (!canBeClosed())
        return;

    destroy();

    Ref closeEvent = Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No);

    dispatchEvent(closeEvent);
}

bool CloseWatcher::canBeClosed() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    return isActive() && !m_isRunningCancelAction && document && document->isFullyActive();
}

void CloseWatcher::destroy()
{
    if (!isActive())
        return;

    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (document && document->window()) {
        Ref manager = protect(document->window())->closeWatcherManager();
        manager->remove(*this);
    }

    m_active = false;

    if (RefPtr signal = m_signal)
        signal->removeAlgorithm(m_signalAlgorithm);
}

void CloseWatcher::eventListenersDidChange()
{
    m_hasCancelEventListener = hasEventListeners(eventNames().cancelEvent);
    m_hasCloseEventListener = hasEventListeners(eventNames().closeEvent);
}

bool CloseWatcher::virtualHasPendingActivity() const
{
    return m_hasCancelEventListener || m_hasCloseEventListener;
}

} // namespace WebCore
