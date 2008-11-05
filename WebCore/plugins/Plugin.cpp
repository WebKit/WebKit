/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include "Plugin.h"

#include "AtomicString.h"
#include "PluginData.h"
#include "Frame.h"

namespace WebCore {

Plugin::Plugin(PluginData* pluginData, unsigned index)
    : m_pluginData(pluginData)
    , m_index(index)
{
}

Plugin::~Plugin()
{
}

String Plugin::name() const
{
    return m_pluginData->plugins()[m_index]->name;
}

String Plugin::filename() const
{
    return m_pluginData->plugins()[m_index]->file;
}

String Plugin::description() const
{
    return m_pluginData->plugins()[m_index]->desc;
}

unsigned Plugin::length() const
{
    return m_pluginData->plugins()[m_index]->mimes.size();
}

PassRefPtr<MimeType> Plugin::item(unsigned index)
{
    const Vector<PluginInfo*>& plugins = m_pluginData->plugins();
    if (index >= plugins[m_index]->mimes.size())
        return 0;

    MimeClassInfo* mime = plugins[m_index]->mimes[index];

    const Vector<MimeClassInfo*>& mimes = m_pluginData->mimes();
    for (unsigned i = 0; i < mimes.size(); ++i)
        if (mimes[i] == mime)
            return MimeType::create(m_pluginData.get(), i).get();
    return 0;
}

bool Plugin::canGetItemsForName(const AtomicString& propertyName)
{
    const Vector<MimeClassInfo*>& mimes = m_pluginData->mimes();
    for (unsigned i = 0; i < mimes.size(); ++i)
        if (mimes[i]->type == propertyName)
            return true;
    return false;
}

PassRefPtr<MimeType> Plugin::namedItem(const AtomicString& propertyName)
{
    const Vector<MimeClassInfo*>& mimes = m_pluginData->mimes();
    for (unsigned i = 0; i < mimes.size(); ++i)
        if (mimes[i]->type == propertyName)
            return MimeType::create(m_pluginData.get(), i).get();
    return 0;
}

} // namespace WebCore
