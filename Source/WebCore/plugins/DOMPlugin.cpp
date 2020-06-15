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
#include "DOMPlugin.h"

#include "DOMMimeType.h"
#include "Navigator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMPlugin);

Ref<DOMPlugin> DOMPlugin::create(Navigator& navigator, const PluginInfo& info)
{
    return adoptRef(*new DOMPlugin(navigator, info));
}

static Vector<Ref<DOMMimeType>> makeMimeTypes(Navigator& navigator, const PluginInfo& info, DOMPlugin& self)
{
    Vector<Ref<DOMMimeType>> types;
    types.reserveInitialCapacity(info.mimes.size());
    for (auto& type : info.mimes)
        types.uncheckedAppend(DOMMimeType::create(navigator, type, self));

    std::sort(types.begin(), types.end(), [](const Ref<DOMMimeType>& a, const Ref<DOMMimeType>& b) {
        return codePointCompareLessThan(a->type(), b->type());
    });

    return types;
}

DOMPlugin::DOMPlugin(Navigator& navigator, const PluginInfo& info)
    : m_navigator(makeWeakPtr(navigator))
    , m_info(info)
    , m_mimeTypes(makeMimeTypes(navigator, info, *this))
{
}

DOMPlugin::~DOMPlugin() = default;

String DOMPlugin::name() const
{
    return m_info.name;
}

String DOMPlugin::filename() const
{
    return m_info.file;
}

String DOMPlugin::description() const
{
    return m_info.desc;
}

unsigned DOMPlugin::length() const
{
    return m_mimeTypes.size();
}

RefPtr<DOMMimeType> DOMPlugin::item(unsigned index)
{
    if (index >= m_mimeTypes.size())
        return nullptr;
    return m_mimeTypes[index].ptr();
}

RefPtr<DOMMimeType> DOMPlugin::namedItem(const AtomString& propertyName)
{
    for (auto& type : m_mimeTypes) {
        if (type->type() == propertyName)
            return type.ptr();
    }
    return nullptr;
}

Vector<AtomString> DOMPlugin::supportedPropertyNames()
{
    Vector<AtomString> result;
    result.reserveInitialCapacity(m_mimeTypes.size());
    for (auto& type : m_mimeTypes)
        result.uncheckedAppend(type->type());
    return result;
}

} // namespace WebCore
