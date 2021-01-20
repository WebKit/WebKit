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
#include "DOMMimeType.h"

#include "DOMPlugin.h"
#include "Navigator.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<DOMMimeType> DOMMimeType::create(Navigator& navigator, const MimeClassInfo& info, DOMPlugin& enabledPlugin)
{
    return adoptRef(*new DOMMimeType(navigator, info, enabledPlugin));
}

DOMMimeType::DOMMimeType(Navigator& navigator, const MimeClassInfo& info, DOMPlugin& enabledPlugin)
    : m_navigator(makeWeakPtr(navigator))
    , m_info(info)
    , m_enabledPlugin(makeWeakPtr(enabledPlugin))
{
}

DOMMimeType::~DOMMimeType() = default;

String DOMMimeType::type() const
{
    return m_info.type;
}

String DOMMimeType::suffixes() const
{
    StringBuilder builder;
    for (size_t i = 0; i < m_info.extensions.size(); ++i) {
        if (i)
            builder.append(',');
        builder.append(m_info.extensions[i]);
    }
    return builder.toString();
}

String DOMMimeType::description() const
{
    return m_info.desc;
}

RefPtr<DOMPlugin> DOMMimeType::enabledPlugin() const
{
    return m_enabledPlugin.get();
}

} // namespace WebCore
