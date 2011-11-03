/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebUndoClient.h"

#include "WKAPICast.h"
#include "WebEditCommandProxy.h"
#include "WebView.h"

namespace WebKit {

void WebUndoClient::registerEditCommand(WebView* view, PassRefPtr<WebEditCommandProxy> prpCommand, WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (!m_client.registerEditCommand)
        return;

    RefPtr<WebEditCommandProxy> command = prpCommand;
    m_client.registerEditCommand(toAPI(view), toAPI(command.release().leakRef()), (undoOrRedo == WebPageProxy::Undo) ? kWKViewUndo : kWKViewRedo, m_client.clientInfo);
}

void WebUndoClient::clearAllEditCommands(WebView* view)
{
    if (!m_client.clearAllEditCommands)
        return;
    
    m_client.clearAllEditCommands(toAPI(view), m_client.clientInfo);
}

bool WebUndoClient::canUndoRedo(WebView* view, WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (!m_client.canUndoRedo)
        return false;
    
    return m_client.canUndoRedo(toAPI(view), undoOrRedo, m_client.clientInfo);
}

void WebUndoClient::executeUndoRedo(WebView* view, WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (!m_client.executeUndoRedo)
        return;
    
    m_client.executeUndoRedo(toAPI(view), undoOrRedo, m_client.clientInfo);
}

} // namespace WebKit

