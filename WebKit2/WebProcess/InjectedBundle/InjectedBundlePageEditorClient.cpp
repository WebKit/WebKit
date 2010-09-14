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

#include "InjectedBundlePageEditorClient.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "InjectedBundleNodeHandle.h"
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

InjectedBundlePageEditorClient::InjectedBundlePageEditorClient()
{
    initialize(0);
}

void InjectedBundlePageEditorClient::initialize(WKBundlePageEditorClient* client)
{
    if (client && !client->version)
        m_client = *client;
    else 
        memset(&m_client, 0, sizeof(m_client));
}

bool InjectedBundlePageEditorClient::shouldBeginEditing(WebPage* page, Range* range)
{
    if (m_client.shouldBeginEditing)
        return m_client.shouldBeginEditing(toRef(page), toRef(range), m_client.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldEndEditing(WebPage* page, Range* range)
{
    if (m_client.shouldEndEditing)
        return m_client.shouldEndEditing(toRef(page), toRef(range), m_client.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldInsertNode(WebPage* page, Node* node, Range* rangeToReplace, EditorInsertAction action)
{
    if (m_client.shouldInsertNode) {
        RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(node);
        return m_client.shouldInsertNode(toRef(page), toRef(nodeHandle.get()), toRef(rangeToReplace), toWK(action), m_client.clientInfo);
    }
    return true;
}

bool InjectedBundlePageEditorClient::shouldInsertText(WebPage* page, StringImpl* text, Range* rangeToReplace, EditorInsertAction action)
{
    if (m_client.shouldInsertText)
        return m_client.shouldInsertText(toRef(page), toRef(text), toRef(rangeToReplace), toWK(action), m_client.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldDeleteRange(WebPage* page, Range* range)
{
    if (m_client.shouldDeleteRange)
        return m_client.shouldDeleteRange(toRef(page), toRef(range), m_client.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldChangeSelectedRange(WebPage* page, Range* fromRange, Range* toRange, EAffinity affinity, bool stillSelecting)
{
    if (m_client.shouldChangeSelectedRange)
        return m_client.shouldChangeSelectedRange(toRef(page), toRef(fromRange), toRef(toRange), toWK(affinity), stillSelecting, m_client.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldApplyStyle(WebPage* page, CSSStyleDeclaration* style, Range* range)
{
    if (m_client.shouldApplyStyle)
        return m_client.shouldApplyStyle(toRef(page), toRef(style), toRef(range), m_client.clientInfo);
    return true;
}

void InjectedBundlePageEditorClient::didBeginEditing(WebPage* page, StringImpl* notificationName)
{
    if (m_client.didBeginEditing)
        m_client.didBeginEditing(toRef(page), toRef(notificationName), m_client.clientInfo);
}

void InjectedBundlePageEditorClient::didEndEditing(WebPage* page, StringImpl* notificationName)
{
    if (m_client.didEndEditing)
        m_client.didEndEditing(toRef(page), toRef(notificationName), m_client.clientInfo);
}

void InjectedBundlePageEditorClient::didChange(WebPage* page, StringImpl* notificationName)
{
    if (m_client.didChange)
        m_client.didChange(toRef(page), toRef(notificationName), m_client.clientInfo);
}

void InjectedBundlePageEditorClient::didChangeSelection(WebPage* page, StringImpl* notificationName)
{
    if (m_client.didChangeSelection)
        m_client.didChangeSelection(toRef(page), toRef(notificationName), m_client.clientInfo);
}

} // namespace WebKit
