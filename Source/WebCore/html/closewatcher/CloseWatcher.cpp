/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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

#include "CloseWatcherManager.h"
#include "Event.h"
#include "EventNames.h"
#include "KeyboardEvent.h"
#include "LocalDOMWindow.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CloseWatcher);

RefPtr<CloseWatcher> CloseWatcher::create(Document& document)
{
    if (!document.isFullyActive())
        return nullptr;

    return CloseWatcher::establish(document);
}

ExceptionOr<Ref<CloseWatcher>> CloseWatcher::create(ScriptExecutionContext& context, const CloseWatcher::Options& options)
{
    RefPtr document = dynamicDowncast<Document>(&context);
    if (!document || !document->isFullyActive())
        return Exception { ExceptionCode::InvalidStateError, "CloseWatchers cannot be created in a detached window."_s };

    Ref watcher = CloseWatcher::establish(*document);

    if (options.signal) {
        if (options.signal->aborted())
            watcher->m_closed = true;
        else {
            watcher->m_signal = options.signal;
            watcher->m_signalAlgorithm = options.signal->addAlgorithm([weakWatcher = WeakPtr { watcher.get() }](JSC::JSValue) mutable {
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

    Ref<CloseWatcher> watcher = adoptRef(*new CloseWatcher(document));
    watcher->suspendIfNeeded();

    auto& manager = document.domWindow()->closeWatcherManager();

    manager.add(watcher);
    return watcher;
}

CloseWatcher::CloseWatcher(Document& document)
    : ActiveDOMObject(document)
{ }

void CloseWatcher::requestClose()
{
    requestToClose();
}

bool CloseWatcher::requestToClose()
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (IsClosed() || m_isRunningCancelAction || !document || !document->isFullyActive())
        return true;

    auto& manager = document->domWindow()->closeWatcherManager();
    bool canPreventClose = manager.canPreventClose() && document->domWindow()->hasHistoryActionActivation();
    Ref cancelEvent = Event::create(eventNames().cancelEvent, Event::CanBubble::No, canPreventClose ? Event::IsCancelable::Yes : Event::IsCancelable::No);
    m_isRunningCancelAction = true;
    dispatchEvent(cancelEvent);
    m_isRunningCancelAction = false;
    if (cancelEvent->defaultPrevented()) {
        document->domWindow()->consumeHistoryActionUserActivation();
        return false;
    }

    close();
    return true;
}

void CloseWatcher::close()
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (IsClosed() || m_isRunningCancelAction || !document || !document->isFullyActive())
        return;

    destroy();

    Ref closeEvent = Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No);

    dispatchEvent(closeEvent);
}

void CloseWatcher::destroy()
{
    if (IsClosed())
        return;

    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (document && document->domWindow())
        document->domWindow()->closeWatcherManager().remove(*this);

    m_closed = true;

    if (m_signal)
        m_signal->removeAlgorithm(m_signalAlgorithm);
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
