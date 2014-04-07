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
#include "InjectedBundlePageFormClient.h"

#include "APIArray.h"
#include "ImmutableDictionary.h"
#include "InjectedBundleNodeHandle.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLTextAreaElement.h>

using namespace WebCore;

namespace WebKit {

InjectedBundlePageFormClient::InjectedBundlePageFormClient(const WKBundlePageFormClientBase* client)
{
    initialize(client);
}

void InjectedBundlePageFormClient::didFocusTextField(WebPage* page, HTMLInputElement* inputElement, WebFrame* frame)
{
    if (!m_client.didFocusTextField)
        return;

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    m_client.didFocusTextField(toAPI(page), toAPI(nodeHandle.get()), toAPI(frame), m_client.base.clientInfo);
}

void InjectedBundlePageFormClient::textFieldDidBeginEditing(WebPage* page, HTMLInputElement* inputElement, WebFrame* frame)
{
    if (!m_client.textFieldDidBeginEditing)
        return;

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    m_client.textFieldDidBeginEditing(toAPI(page), toAPI(nodeHandle.get()), toAPI(frame), m_client.base.clientInfo);
}

void InjectedBundlePageFormClient::textFieldDidEndEditing(WebPage* page, HTMLInputElement* inputElement, WebFrame* frame)
{
    if (!m_client.textFieldDidEndEditing)
        return;

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    m_client.textFieldDidEndEditing(toAPI(page), toAPI(nodeHandle.get()), toAPI(frame), m_client.base.clientInfo);
}

void InjectedBundlePageFormClient::textDidChangeInTextField(WebPage* page, HTMLInputElement* inputElement, WebFrame* frame)
{
    if (!m_client.textDidChangeInTextField)
        return;

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    m_client.textDidChangeInTextField(toAPI(page), toAPI(nodeHandle.get()), toAPI(frame), m_client.base.clientInfo);
}

void InjectedBundlePageFormClient::textDidChangeInTextArea(WebPage* page, HTMLTextAreaElement* textAreaElement, WebFrame* frame)
{
    if (!m_client.textDidChangeInTextArea)
        return;

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(textAreaElement);
    m_client.textDidChangeInTextArea(toAPI(page), toAPI(nodeHandle.get()), toAPI(frame), m_client.base.clientInfo);
}

static WKInputFieldActionType toWKInputFieldActionType(API::InjectedBundle::FormClient::InputFieldAction action)
{
    switch (action) {
    case API::InjectedBundle::FormClient::InputFieldAction::MoveUp:
        return WKInputFieldActionTypeMoveUp;
    case API::InjectedBundle::FormClient::InputFieldAction::MoveDown:
        return WKInputFieldActionTypeMoveDown;
    case API::InjectedBundle::FormClient::InputFieldAction::Cancel:
        return WKInputFieldActionTypeCancel;
    case API::InjectedBundle::FormClient::InputFieldAction::InsertTab:
        return WKInputFieldActionTypeInsertTab;
    case API::InjectedBundle::FormClient::InputFieldAction::InsertBacktab:
        return WKInputFieldActionTypeInsertBacktab;
    case API::InjectedBundle::FormClient::InputFieldAction::InsertNewline:
        return WKInputFieldActionTypeInsertNewline;
    case API::InjectedBundle::FormClient::InputFieldAction::InsertDelete:
        return WKInputFieldActionTypeInsertDelete;
    }

    ASSERT_NOT_REACHED();
    return WKInputFieldActionTypeCancel;
}

bool InjectedBundlePageFormClient::shouldPerformActionInTextField(WebPage* page, HTMLInputElement* inputElement, API::InjectedBundle::FormClient::InputFieldAction actionType, WebFrame* frame)
{
    if (!m_client.shouldPerformActionInTextField)
        return false;

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    return m_client.shouldPerformActionInTextField(toAPI(page), toAPI(nodeHandle.get()), toWKInputFieldActionType(actionType), toAPI(frame), m_client.base.clientInfo);
}

void InjectedBundlePageFormClient::willSendSubmitEvent(WebPage* page, HTMLFormElement* formElement, WebFrame* frame, WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values)
{
    if (!m_client.willSendSubmitEvent)
        return;

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(formElement);

    ImmutableDictionary::MapType map;
    for (size_t i = 0; i < values.size(); ++i)
        map.set(values[i].first, API::String::create(values[i].second));
    auto textFieldsMap = ImmutableDictionary::create(std::move(map));

    m_client.willSendSubmitEvent(toAPI(page), toAPI(nodeHandle.get()), toAPI(frame), toAPI(sourceFrame), toAPI(textFieldsMap.get()), m_client.base.clientInfo);
}

void InjectedBundlePageFormClient::willSubmitForm(WebPage* page, HTMLFormElement* formElement, WebFrame* frame, WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values, RefPtr<API::Object>& userData)
{
    if (!m_client.willSubmitForm)
        return;

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::getOrCreate(formElement);

    ImmutableDictionary::MapType map;
    for (size_t i = 0; i < values.size(); ++i)
        map.set(values[i].first, API::String::create(values[i].second));
    auto textFieldsMap = ImmutableDictionary::create(std::move(map));

    WKTypeRef userDataToPass = 0;
    m_client.willSubmitForm(toAPI(page), toAPI(nodeHandle.get()), toAPI(frame), toAPI(sourceFrame), toAPI(textFieldsMap.get()), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageFormClient::didAssociateFormControls(WebPage* page, const Vector<RefPtr<WebCore::Element>>& elements)
{
    if (!m_client.didAssociateFormControls)
        return;

    Vector<RefPtr<API::Object>> elementHandles;
    elementHandles.reserveInitialCapacity(elements.size());

    for (const auto& element : elements)
        elementHandles.uncheckedAppend(InjectedBundleNodeHandle::getOrCreate(element.get()));

    m_client.didAssociateFormControls(toAPI(page), toAPI(API::Array::create(std::move(elementHandles)).get()), m_client.base.clientInfo);
}

bool InjectedBundlePageFormClient::shouldNotifyOnFormChanges(WebPage* page)
{
    if (!m_client.shouldNotifyOnFormChanges)
        return false;

    return m_client.shouldNotifyOnFormChanges(toAPI(page), m_client.base.clientInfo);
}

} // namespace WebKit
