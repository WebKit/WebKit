/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "WebEditCommandProxy.h"

#include "MessageSenderInlines.h"
#include "UndoOrRedo.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/LocalizedStrings.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

WebEditCommandProxy::WebEditCommandProxy(WebUndoStepID commandID, String&& label, WebPageProxy& page, WebProcessProxy& process, WebCore::PageIdentifier pageIDInProcess)
    : m_commandID(commandID)
    , m_label(WTF::move(label))
    , m_page(page)
    , m_process(process)
    , m_pageIDInProcess(pageIDInProcess)
{
    page.addEditCommand(*this);
}

WebEditCommandProxy::~WebEditCommandProxy()
{
    if (RefPtr page = m_page.get())
        page->removeEditCommand(*this);
}

RefPtr<WebProcessProxy> WebEditCommandProxy::process() const
{
    return m_process.get();
}

void WebEditCommandProxy::unapply()
{
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

    // The undo step only exists in the WebPage that registered it. If that process is gone, or its
    // WebPage was closed while the process stayed alive for some other page, the step can neither be
    // unapplied nor re-registered for redo, so leave the platform undo stack alone instead of offering
    // a redo for an operation that never happened.
    RefPtr process = m_process.get();
    if (!process || !process->canSendMessage() || !page->hasWebPageInProcess(*process, m_pageIDInProcess))
        return;

    // Send the request asynchronously. While it is unacknowledged it is also handed back in the reply to
    // any ExecuteUndoRedo from this process, so that execCommand() applies it before returning; its
    // sequence number lets the web process discard whichever copy arrives second.
    auto processIdentifier = process->coreProcessIdentifier();
    auto sequence = page->addPendingUndoRedo(m_commandID, UndoOrRedo::Undo, processIdentifier);
    process->sendWithAsyncReply(Messages::WebPage::UnapplyEditCommand(sequence, m_commandID), [weakPage = WeakPtr { *page }, commandID = m_commandID, processIdentifier]() {
        if (RefPtr page = weakPage.get())
            page->removePendingUndoRedo(commandID, processIdentifier);
    }, m_pageIDInProcess);

    page->registerEditCommand(*this, UndoOrRedo::Redo);
}

void WebEditCommandProxy::reapply()
{
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

    RefPtr process = m_process.get();
    if (!process || !process->canSendMessage() || !page->hasWebPageInProcess(*process, m_pageIDInProcess))
        return;

    auto processIdentifier = process->coreProcessIdentifier();
    auto sequence = page->addPendingUndoRedo(m_commandID, UndoOrRedo::Redo, processIdentifier);
    process->sendWithAsyncReply(Messages::WebPage::ReapplyEditCommand(sequence, m_commandID), [weakPage = WeakPtr { *page }, commandID = m_commandID, processIdentifier]() {
        if (RefPtr page = weakPage.get())
            page->removePendingUndoRedo(commandID, processIdentifier);
    }, m_pageIDInProcess);

    page->registerEditCommand(*this, UndoOrRedo::Undo);
}

} // namespace WebKit
