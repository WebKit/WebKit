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
#include "LocalFrame.h"
#include "Page.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMPluginArray);

Ref<DOMPluginArray> DOMPluginArray::create(Navigator& navigator, Vector<Ref<DOMPlugin>>&& publiclyVisiblePlugins, Vector<Ref<DOMPlugin>>&& additionalWebVisibilePlugins)
{
    return adoptRef(*new DOMPluginArray(navigator, WTFMove(publiclyVisiblePlugins), WTFMove(additionalWebVisibilePlugins)));
}

DOMPluginArray::DOMPluginArray(Navigator& navigator, Vector<Ref<DOMPlugin>>&& publiclyVisiblePlugins, Vector<Ref<DOMPlugin>>&& additionalWebVisibilePlugins)
    : m_navigator(navigator)
    , m_publiclyVisiblePlugins(WTFMove(publiclyVisiblePlugins))
    , m_additionalWebVisibilePlugins(WTFMove(additionalWebVisibilePlugins))
{
}

DOMPluginArray::~DOMPluginArray() = default;

unsigned DOMPluginArray::length() const
{
    return m_publiclyVisiblePlugins.size();
}

RefPtr<DOMPlugin> DOMPluginArray::item(unsigned index)
{
    if (index >= m_publiclyVisiblePlugins.size())
        return nullptr;
    return m_publiclyVisiblePlugins[index].ptr();
}

RefPtr<DOMPlugin> DOMPluginArray::namedItem(const AtomString& propertyName)
{
    for (auto& plugin : m_publiclyVisiblePlugins) {
        if (plugin->name() == propertyName)
            return plugin.ptr();
    }

    for (auto& plugin : m_additionalWebVisibilePlugins) {
        if (plugin->name() == propertyName)
            return plugin.ptr();
    }

    return nullptr;
}

bool DOMPluginArray::isSupportedPropertyName(const AtomString& propertyName) const
{
    return m_publiclyVisiblePlugins.containsIf([&](auto& plugin) { return plugin->name() == propertyName; })
        || m_additionalWebVisibilePlugins.containsIf([&](auto& plugin) { return plugin->name() == propertyName; });
}

Vector<AtomString> DOMPluginArray::supportedPropertyNames() const
{
    return m_publiclyVisiblePlugins.map([](auto& plugin) {
        return AtomString { plugin->name() };
    });
}

void DOMPluginArray::refresh(bool reloadPages)
{
    if (!m_navigator)
        return;

    auto* frame = m_navigator->frame();
    if (!frame)
        return;

    if (!frame->page())
        return;

    Page::refreshPlugins(reloadPages);
}

} // namespace WebCore
