/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "PluginData.h"

#include "PluginDatabase.h"
#include "PluginPackage.h"

namespace WebCore {

void PluginData::initPlugins(const Page*)
{
    PluginDatabase* db = PluginDatabase::installedPlugins();
    const Vector<PluginPackage*>& plugins = db->plugins();

    for (unsigned i = 0; i < plugins.size(); ++i) {
        PluginPackage* package = plugins[i];

        PluginInfo info;
        info.name = package->name();
        info.file = package->fileName();
        info.desc = package->description();

        const MIMEToDescriptionsMap& mimeToDescriptions = package->mimeToDescriptions();
        MIMEToDescriptionsMap::const_iterator end = mimeToDescriptions.end();
        for (MIMEToDescriptionsMap::const_iterator it = mimeToDescriptions.begin(); it != end; ++it) {
            MimeClassInfo mime;
            mime.type = it->first;
            mime.desc = it->second;
            mime.extensions = package->mimeToExtensions().get(mime.type);
            info.mimes.append(mime);
        }
        m_plugins.append(info);
    }
}

void PluginData::refresh()
{
    PluginDatabase* db = PluginDatabase::installedPlugins();
    db->refresh();
}

} // namespace WebCore
