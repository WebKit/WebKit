/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AutoFillPopupMenuClient.h"

#include "HTMLInputElement.h"
#include "WebNode.h"
#include "WebString.h"
#include "WebVector.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

unsigned AutoFillPopupMenuClient::getSuggestionsCount() const
{
    return m_names.size() + ((m_separatorIndex == -1) ? 0 : 1);
}

WebString AutoFillPopupMenuClient::getSuggestion(unsigned listIndex) const
{
    if (listIndex == m_separatorIndex)
        return WebString();

    if (m_separatorIndex != -1 && listIndex > m_separatorIndex)
        --listIndex;

    // FIXME: Modify the PopupMenu to add the label in gray right-justified.
    ASSERT(listIndex >= 0 && listIndex < m_names.size());

    WebString suggestion = m_names[listIndex];
    if (m_labels[listIndex].isEmpty())
        return suggestion;

    return suggestion + String(" (") + m_labels[listIndex] + String(")");
}

void AutoFillPopupMenuClient::removeSuggestionAtIndex(unsigned listIndex)
{
    // FIXME: Do we want to remove AutoFill suggestions?
    ASSERT(listIndex >= 0 && listIndex < m_names.size());
    m_names.remove(listIndex);
    m_labels.remove(listIndex);
}

void AutoFillPopupMenuClient::valueChanged(unsigned listIndex, bool fireEvents)
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    if (m_separatorIndex != -1 && listIndex > m_separatorIndex)
        --listIndex;

    ASSERT(listIndex >= 0 && listIndex < m_names.size());

    webView->client()->didAcceptAutoFillSuggestion(WebNode(getTextField()),
                                                   m_names[listIndex],
                                                   m_labels[listIndex],
                                                   listIndex);
}

void AutoFillPopupMenuClient::selectionChanged(unsigned listIndex, bool fireEvents)
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    if (m_separatorIndex != -1 && listIndex > m_separatorIndex)
        --listIndex;

    ASSERT(listIndex >= 0 && listIndex < m_names.size());

    webView->client()->didSelectAutoFillSuggestion(WebNode(getTextField()),
                                                   m_names[listIndex],
                                                   m_labels[listIndex]);
}

void AutoFillPopupMenuClient::selectionCleared()
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    webView->client()->didClearAutoFillSelection(WebNode(getTextField()));
}

void AutoFillPopupMenuClient::popupDidHide()
{
    // FIXME: Refactor this method, as selectionCleared() and popupDidHide()
    // share the exact same functionality.
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    webView->client()->didClearAutoFillSelection(WebNode(getTextField()));
}

bool AutoFillPopupMenuClient::itemIsSeparator(unsigned listIndex) const
{
    return (m_separatorIndex != -1 && m_separatorIndex == listIndex);
}

void AutoFillPopupMenuClient::initialize(
    HTMLInputElement* textField,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(separatorIndex < static_cast<int>(names.size()));

    // The suggestions must be set before initializing the
    // SuggestionsPopupMenuClient.
    setSuggestions(names, labels, separatorIndex);

    SuggestionsPopupMenuClient::initialize(textField, -1);
}

void AutoFillPopupMenuClient::setSuggestions(const WebVector<WebString>& names,
                                             const WebVector<WebString>& labels,
                                             int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(separatorIndex < static_cast<int>(names.size()));

    m_names.clear();
    m_labels.clear();
    for (size_t i = 0; i < names.size(); ++i) {
        m_names.append(names[i]);
        m_labels.append(labels[i]);
    }

    m_separatorIndex = separatorIndex;

    // Try to preserve selection if possible.
    if (getSelectedIndex() >= static_cast<int>(names.size()))
        setSelectedIndex(-1);
}

} // namespace WebKit
