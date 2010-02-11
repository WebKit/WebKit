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
#include "WebString.h"
#include "WebVector.h"

using namespace WebCore;

namespace WebKit {

unsigned AutoFillPopupMenuClient::getSuggestionsCount() const
{
    return m_names.size();
}

WebString AutoFillPopupMenuClient::getSuggestion(unsigned listIndex) const
{
    // FIXME: Modify the PopupMenu to add the label in gray right-justified.
    ASSERT(listIndex >= 0 && listIndex < m_names.size());
    return m_names[listIndex] + String(" (") + m_labels[listIndex] + String(")");
}

void AutoFillPopupMenuClient::removeSuggestionAtIndex(unsigned listIndex)
{
    // FIXME: Do we want to remove AutoFill suggestions?
    ASSERT(listIndex >= 0 && listIndex < m_names.size());
    m_names.remove(listIndex);
    m_labels.remove(listIndex);
}

void AutoFillPopupMenuClient::initialize(
    HTMLInputElement* textField,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    int defaultSuggestionIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(defaultSuggestionIndex < static_cast<int>(names.size()));

    // The suggestions must be set before initializing the
    // SuggestionsPopupMenuClient.
    setSuggestions(names, labels);

    SuggestionsPopupMenuClient::initialize(textField, defaultSuggestionIndex);
}

void AutoFillPopupMenuClient::setSuggestions(const WebVector<WebString>& names,
                                             const WebVector<WebString>& labels)
{
    ASSERT(names.size() == labels.size());

    m_names.clear();
    m_labels.clear();
    for (size_t i = 0; i < names.size(); ++i) {
        m_names.append(names[i]);
        m_labels.append(labels[i]);
    }

    // Try to preserve selection if possible.
    if (getSelectedIndex() >= static_cast<int>(names.size()))
        setSelectedIndex(-1);
}

} // namespace WebKit
