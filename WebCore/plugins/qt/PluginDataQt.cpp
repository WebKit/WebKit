/*
    Copyright (C) 2008 Trolltech ASA

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "PluginData.h"

#include "ChromeClientQt.h"
#include "Page.h"
#include <qwebpage.h>
#include <qwebpluginfactory.h>

namespace WebCore {

void PluginData::initPlugins()
{
#if QT_VERSION >= 0x040400
    QWebPage* webPage = static_cast<ChromeClientQt*>(m_page->chrome()->client())->m_webPage;
    QWebPluginFactory* factory = webPage->pluginFactory();
    if (!factory)
        return;

    QList<QWebPluginFactory::Plugin> plugins = factory->plugins();
    for (int i = 0; i < plugins.count(); ++i) {
        const QWebPluginFactory::Plugin& plugin = plugins.at(i);

        PluginInfo* info = new PluginInfo;
        info->name = plugin.name;
        info->desc = plugin.description;

        for (int j = 0; j < plugin.mimeTypes.count(); ++j) {
            const QWebPluginFactory::MimeType& mimeType = plugin.mimeTypes.at(j);

            MimeClassInfo* mimeInfo = new MimeClassInfo;
            mimeInfo->type = mimeType.name;
            mimeInfo->desc = mimeType.description;
            mimeInfo->suffixes = mimeType.fileExtensions.join(QLatin1String("; "));

            info->mimes.append(mimeInfo);
        }

        m_plugins.append(info);
    }
#endif
}

void PluginData::refresh()
{
    // nothing to do
}

};
