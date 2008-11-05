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
#include "PluginArray.h"

#include "AtomicString.h"
#include "Frame.h"
#include "Page.h"
#include "Plugin.h"
#include "PluginData.h"

namespace WebCore {

PluginArray::PluginArray(Frame* frame)
    : m_frame(frame)
{
}

PluginArray::~PluginArray()
{
}

unsigned PluginArray::length() const
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;
    return data->plugins().size();
}

PassRefPtr<Plugin> PluginArray::item(unsigned index)
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;
    const Vector<PluginInfo*>& plugins = data->plugins();
    if (index >= plugins.size())
        return 0;
    return Plugin::create(data, index).get();
}

bool PluginArray::canGetItemsForName(const AtomicString& propertyName)
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;
    const Vector<PluginInfo*>& plugins = data->plugins();
    for (unsigned i = 0; i < plugins.size(); ++i) {
        if (plugins[i]->name == propertyName)
            return true;
    }
    return false;
}

PassRefPtr<Plugin> PluginArray::namedItem(const AtomicString& propertyName)
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;
    const Vector<PluginInfo*>& plugins = data->plugins();
    for (unsigned i = 0; i < plugins.size(); ++i) {
        if (plugins[i]->name == propertyName)
            return Plugin::create(data, i).get();
    }
    return 0;
}

void PluginArray::refresh(bool reload)
{
    Page::refreshPlugins(reload);
}

PluginData* PluginArray::getPluginData() const
{
    if (!m_frame)
        return 0;
    Page* p = m_frame->page();
    if (!p)
        return 0;
    return p->pluginData();
}

} // namespace WebCore
