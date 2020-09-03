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
#include "InjectedBundlePageEditorClient.h"

#include "APIArray.h"
#include "APIData.h"
#include "InjectedBundleCSSStyleDeclarationHandle.h"
#include "InjectedBundleNodeHandle.h"
#include "InjectedBundleRangeHandle.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WKData.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WebPage.h"
#include <WebCore/DocumentFragment.h>
#include <WebCore/StyleProperties.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

static RefPtr<InjectedBundleCSSStyleDeclarationHandle> createHandle(const StyleProperties& style)
{
    return InjectedBundleCSSStyleDeclarationHandle::getOrCreate(&style.mutableCopy()->ensureCSSStyleDeclaration());
}

InjectedBundlePageEditorClient::InjectedBundlePageEditorClient(const WKBundlePageEditorClientBase& client)
{
    initialize(&client);
}

bool InjectedBundlePageEditorClient::shouldBeginEditing(WebPage& page, const SimpleRange& range)
{
    if (m_client.shouldBeginEditing)
        return m_client.shouldBeginEditing(toAPI(&page), toAPI(createHandle(range).get()), m_client.base.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldEndEditing(WebPage& page, const SimpleRange& range)
{
    if (m_client.shouldEndEditing)
        return m_client.shouldEndEditing(toAPI(&page), toAPI(createHandle(range).get()), m_client.base.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldInsertNode(WebPage& page, Node& node, const Optional<SimpleRange>& rangeToReplace, EditorInsertAction action)
{
    if (m_client.shouldInsertNode) {
        RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(node);
        return m_client.shouldInsertNode(toAPI(&page), toAPI(nodeHandle.get()), toAPI(createHandle(rangeToReplace).get()), toAPI(action), m_client.base.clientInfo);
    }
    return true;
}

bool InjectedBundlePageEditorClient::shouldInsertText(WebPage& page, const String& text, const Optional<SimpleRange>& rangeToReplace, EditorInsertAction action)
{
    if (m_client.shouldInsertText)
        return m_client.shouldInsertText(toAPI(&page), toAPI(text.impl()), toAPI(createHandle(rangeToReplace).get()), toAPI(action), m_client.base.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldDeleteRange(WebPage& page, const Optional<WebCore::SimpleRange>& range)
{
    if (m_client.shouldDeleteRange)
        return m_client.shouldDeleteRange(toAPI(&page), toAPI(createHandle(range).get()), m_client.base.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldChangeSelectedRange(WebPage& page, const Optional<SimpleRange>& fromRange, const Optional<SimpleRange>& toRange, Affinity affinity, bool stillSelecting)
{
    if (m_client.shouldChangeSelectedRange)
        return m_client.shouldChangeSelectedRange(toAPI(&page), toAPI(createHandle(fromRange).get()), toAPI(createHandle(toRange).get()), toAPI(affinity), stillSelecting, m_client.base.clientInfo);
    return true;
}

bool InjectedBundlePageEditorClient::shouldApplyStyle(WebPage& page, const StyleProperties& style, const Optional<SimpleRange>& range)
{
    if (m_client.shouldApplyStyle)
        return m_client.shouldApplyStyle(toAPI(&page), toAPI(createHandle(style).get()), toAPI(createHandle(range).get()), m_client.base.clientInfo);
    return true;
}

void InjectedBundlePageEditorClient::didBeginEditing(WebPage& page, const String& notificationName)
{
    if (m_client.didBeginEditing)
        m_client.didBeginEditing(toAPI(&page), toAPI(notificationName.impl()), m_client.base.clientInfo);
}

void InjectedBundlePageEditorClient::didEndEditing(WebPage& page, const String& notificationName)
{
    if (m_client.didEndEditing)
        m_client.didEndEditing(toAPI(&page), toAPI(notificationName.impl()), m_client.base.clientInfo);
}

void InjectedBundlePageEditorClient::didChange(WebPage& page, const String& notificationName)
{
    if (m_client.didChange)
        m_client.didChange(toAPI(&page), toAPI(notificationName.impl()), m_client.base.clientInfo);
}

void InjectedBundlePageEditorClient::didChangeSelection(WebPage& page, const String& notificationName)
{
    if (m_client.didChangeSelection)
        m_client.didChangeSelection(toAPI(&page), toAPI(notificationName.impl()), m_client.base.clientInfo);
}

void InjectedBundlePageEditorClient::willWriteToPasteboard(WebPage& page, const Optional<SimpleRange>& range)
{
    if (m_client.willWriteToPasteboard)
        m_client.willWriteToPasteboard(toAPI(&page), toAPI(createHandle(range).get()), m_client.base.clientInfo);
}

void InjectedBundlePageEditorClient::getPasteboardDataForRange(WebPage& page, const Optional<SimpleRange>& range, Vector<String>& pasteboardTypes, Vector<RefPtr<SharedBuffer>>& pasteboardData)
{
    if (m_client.getPasteboardDataForRange) {
        WKArrayRef types = nullptr;
        WKArrayRef data = nullptr;
        m_client.getPasteboardDataForRange(toAPI(&page), toAPI(createHandle(range).get()), &types, &data, m_client.base.clientInfo);
        auto typesArray = adoptRef(toImpl(types));
        auto dataArray = adoptRef(toImpl(data));

        pasteboardTypes.clear();
        pasteboardData.clear();

        if (!typesArray || !dataArray)
            return;

        ASSERT(typesArray->size() == dataArray->size());

        for (auto type : typesArray->elementsOfType<API::String>())
            pasteboardTypes.append(type->string());

        for (auto item : dataArray->elementsOfType<API::Data>()) {
            auto buffer = SharedBuffer::create(item->bytes(), item->size());
            pasteboardData.append(WTFMove(buffer));
        }
    }
}

bool InjectedBundlePageEditorClient::performTwoStepDrop(WebPage& page, DocumentFragment& fragment, const SimpleRange& destination, bool isMove)
{
    if (!m_client.performTwoStepDrop)
        return false;

    auto nodeHandle = InjectedBundleNodeHandle::getOrCreate(&fragment);
    return m_client.performTwoStepDrop(toAPI(&page), toAPI(nodeHandle.get()), toAPI(createHandle(destination).get()), isMove, m_client.base.clientInfo);
}

void InjectedBundlePageEditorClient::didWriteToPasteboard(WebPage& page)
{
    if (m_client.didWriteToPasteboard)
        m_client.didWriteToPasteboard(toAPI(&page), m_client.base.clientInfo);
}

} // namespace WebKit
