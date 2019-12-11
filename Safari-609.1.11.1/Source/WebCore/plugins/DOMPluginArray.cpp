/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *  Copyright (C) 2008, 2015 Apple Inc. All rights reserved.
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
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMPluginArray);

DOMPluginArray::DOMPluginArray(Navigator& navigator)
    : m_navigator(makeWeakPtr(navigator))
{
}

DOMPluginArray::~DOMPluginArray() = default;

unsigned DOMPluginArray::length() const
{
    PluginData* data = pluginData();
    if (!data)
        return 0;

    return data->publiclyVisiblePlugins().size();
}

RefPtr<DOMPlugin> DOMPluginArray::item(unsigned index)
{
    PluginData* data = pluginData();
    if (!data)
        return nullptr;

    const Vector<PluginInfo>& plugins = data->publiclyVisiblePlugins();
    if (index >= plugins.size())
        return nullptr;
    return DOMPlugin::create(data, frame(), plugins[index]);
}

RefPtr<DOMPlugin> DOMPluginArray::namedItem(const AtomString& propertyName)
{
    PluginData* data = pluginData();
    if (!data)
        return nullptr;

    for (auto& plugin : data->webVisiblePlugins()) {
        if (plugin.name == propertyName)
            return DOMPlugin::create(data, frame(), plugin);
    }
    return nullptr;
}

Vector<AtomString> DOMPluginArray::supportedPropertyNames()
{
    PluginData* data = pluginData();
    if (!data)
        return { };

    const auto& plugins = data->publiclyVisiblePlugins();

    Vector<AtomString> result;
    result.reserveInitialCapacity(plugins.size());
    for (auto& plugin : plugins)
        result.uncheckedAppend(plugin.name);

    return result;
}

void DOMPluginArray::refresh(bool reloadPages)
{
    auto* frame = this->frame();
    if (!frame)
        return;

    if (!frame->page())
        return;

    Page::refreshPlugins(reloadPages);
}

PluginData* DOMPluginArray::pluginData() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    Page* page = frame->page();
    if (!page)
        return nullptr;

    return &page->pluginData();
}

} // namespace WebCore
