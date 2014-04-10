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
#include "DOMPluginArray.h"

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

DOMPluginArray::DOMPluginArray(Frame* frame)
    : DOMWindowProperty(frame)
{
}

DOMPluginArray::~DOMPluginArray()
{
}

unsigned DOMPluginArray::length() const
{
    PluginData* data = pluginData();
    if (!data)
        return 0;
    return data->plugins().size();
}

PassRefPtr<DOMPlugin> DOMPluginArray::item(unsigned index)
{
    PluginData* data = pluginData();
    if (!data)
        return 0;
    const Vector<PluginInfo>& plugins = data->plugins();
    if (index >= plugins.size())
        return 0;
    return DOMPlugin::create(data, m_frame, index).get();
}

bool DOMPluginArray::canGetItemsForName(const AtomicString& propertyName)
{
    PluginData* data = pluginData();
    if (!data)
        return 0;
    const Vector<PluginInfo>& plugins = data->plugins();
    for (unsigned i = 0; i < plugins.size(); ++i) {
        if (plugins[i].name == propertyName)
            return true;
    }
    return false;
}

PassRefPtr<DOMPlugin> DOMPluginArray::namedItem(const AtomicString& propertyName)
{
    PluginData* data = pluginData();
    if (!data)
        return 0;
    const Vector<PluginInfo>& plugins = data->plugins();
    for (unsigned i = 0; i < plugins.size(); ++i) {
        if (plugins[i].name == propertyName)
            return DOMPlugin::create(data, m_frame, i).get();
    }
    return 0;
}

void DOMPluginArray::refresh(bool reload)
{
    Page::refreshPlugins(reload);
}

PluginData* DOMPluginArray::pluginData() const
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
