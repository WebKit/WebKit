/*
    Copyright (C) 2007 Trolltech ASA

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "PluginInfoStore.h"
#include "qdebug.h"
#if QT_VERSION < 0x040400
#include "qwebobjectplugin_p.h"
#endif
#include "NotImplemented.h"

namespace WebCore {

PluginInfo* PluginInfoStore::createPluginInfoForPluginAtIndex(unsigned i)
{
    //qDebug() << ">>>>>>>>>>> PluginInfoStore::createPluginInfoForPluginAtIndex(" << i << ")";

#if QT_VERSION < 0x040400
    QWebFactoryLoader *loader = QWebFactoryLoader::self();
    if (i > loader->m_pluginInfo.count())
        return 0;
    const QWebFactoryLoader::Info &qinfo = loader->m_pluginInfo.at(i);
    PluginInfo *info = new PluginInfo;
    info->name = qinfo.name;
    info->desc = qinfo.description;
    foreach (const QWebFactoryLoader::MimeInfo &m, qinfo.mimes) {
        MimeClassInfo *mime = new MimeClassInfo;
        mime->type = m.type;
        mime->plugin = info;
        foreach (QString ext, m.extensions)
            mime->suffixes.append(ext);
        info->mimes.append(mime);
    }
    return info;
#else
    return 0; // ### FIXME
#endif
}

unsigned PluginInfoStore::pluginCount() const
{
#if QT_VERSION < 0x040400
    //qDebug() << ">>>>>>>>>>> PluginInfoStore::count =" << QWebFactoryLoader::self()->keys().count();
    return QWebFactoryLoader::self()->keys().count();
#else
    return 0;
#endif
}

String PluginInfoStore::pluginNameForMIMEType(const String& mimeType)
{
    // FIXME: This method is stubbed out and should really return the name of a plug-in package for
    // a given MIME type.
    return String();
}
    
bool PluginInfoStore::supportsMIMEType(const WebCore::String& string)
{
#if QT_VERSION < 0x040400
    bool supports = QWebFactoryLoader::self()->supportsMimeType(string);
#else
    bool supports = false;
#endif
    //qDebug() << ">>>>>>>>>>> PluginInfoStore::supportsMIMEType(" << string << ") =" << supports;
    return supports;
}

void refreshPlugins(bool) {
    notImplemented();
}

}
