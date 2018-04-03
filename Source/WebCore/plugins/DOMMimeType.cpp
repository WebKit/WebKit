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
#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"
#include "PluginData.h"
#include "SubframeLoader.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

DOMMimeType::DOMMimeType(RefPtr<PluginData>&& pluginData, Frame* frame, unsigned index)
    : FrameDestructionObserver(frame)
    , m_pluginData(WTFMove(pluginData))
{
    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    m_pluginData->getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);
    m_mimeClassInfo = mimes[index];
    m_pluginInfo = m_pluginData->webVisiblePlugins()[mimePluginIndices[index]];
}

DOMMimeType::~DOMMimeType() = default;

String DOMMimeType::type() const
{
    return m_mimeClassInfo.type;
}

String DOMMimeType::suffixes() const
{
    const Vector<String>& extensions = m_mimeClassInfo.extensions;

    StringBuilder builder;
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (i)
            builder.append(',');
        builder.append(extensions[i]);
    }
    return builder.toString();
}

String DOMMimeType::description() const
{
    return m_mimeClassInfo.desc;
}

RefPtr<DOMPlugin> DOMMimeType::enabledPlugin() const
{
    if (!m_frame || !m_frame->page() || !m_frame->page()->mainFrame().loader().subframeLoader().allowPlugins())
        return nullptr;

    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    m_pluginData->getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);
    return DOMPlugin::create(m_pluginData.get(), m_frame, m_pluginInfo);
}

} // namespace WebCore
