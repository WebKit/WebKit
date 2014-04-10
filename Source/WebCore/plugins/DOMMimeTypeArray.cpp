/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DOMMimeTypeArray.h"

#include "DOMPlugin.h"
#include "Frame.h"
#include "Page.h"
#include "PluginData.h"
#include <wtf/text/AtomicString.h>

#if ENABLE(WEB_REPLAY)
#include "Document.h"
#include "WebReplayInputs.h"
#include <replay/InputCursor.h>
#endif

namespace WebCore {

DOMMimeTypeArray::DOMMimeTypeArray(Frame* frame)
    : DOMWindowProperty(frame)
{
}

DOMMimeTypeArray::~DOMMimeTypeArray()
{
}

unsigned DOMMimeTypeArray::length() const
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;
    return data->mimes().size();
}

PassRefPtr<DOMMimeType> DOMMimeTypeArray::item(unsigned index)
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;
    const Vector<MimeClassInfo>& mimes = data->mimes();
    if (index >= mimes.size())
        return 0;
    return DOMMimeType::create(data, m_frame, index).get();
}

bool DOMMimeTypeArray::canGetItemsForName(const AtomicString& propertyName)
{
    PluginData *data = getPluginData();
    if (!data)
        return 0;
    const Vector<MimeClassInfo>& mimes = data->mimes();
    for (unsigned i = 0; i < mimes.size(); ++i) {
        if (mimes[i].type == propertyName)
            return true;
    }
    return false;
}

PassRefPtr<DOMMimeType> DOMMimeTypeArray::namedItem(const AtomicString& propertyName)
{
    PluginData *data = getPluginData();
    if (!data)
        return 0;
    const Vector<MimeClassInfo>& mimes = data->mimes();
    for (unsigned i = 0; i < mimes.size(); ++i) {
        if (mimes[i].type == propertyName)
            return DOMMimeType::create(data, m_frame, i).get();
    }
    return 0;
}

PluginData* DOMMimeTypeArray::getPluginData() const
{
    if (!m_frame)
        return nullptr;

    Page* page = m_frame->page();
    if (!page)
        return nullptr;

    PluginData* pluginData = &page->pluginData();

#if ENABLE(WEB_REPLAY)
    if (!m_frame->document())
        return pluginData;

    InputCursor& cursor = m_frame->document()->inputCursor();
    if (cursor.isCapturing())
        cursor.appendInput<FetchPluginData>(pluginData);
    else if (cursor.isReplaying()) {
        if (FetchPluginData* input = cursor.fetchInput<FetchPluginData>())
            pluginData = input->pluginData().get();
    }
#endif

    return pluginData;
}

} // namespace WebCore
