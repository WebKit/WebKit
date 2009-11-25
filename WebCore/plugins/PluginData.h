/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef PluginData_h
#define PluginData_h

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include "PlatformString.h"

namespace WebCore {

    class Page;
    struct PluginInfo;

    struct MimeClassInfo : Noncopyable {
        String type;
        String desc;
        String suffixes;
        PluginInfo* plugin;
    };

    struct PluginInfo : Noncopyable {
        String name;
        String file;
        String desc;
        Vector<MimeClassInfo*> mimes;
    };

    // FIXME: merge with PluginDatabase in the future
    class PluginData : public RefCounted<PluginData> {
    public:
        static PassRefPtr<PluginData> create(const Page* page) { return adoptRef(new PluginData(page)); }
        ~PluginData();

        void disconnectPage() { m_page = 0; }
        const Page* page() const { return m_page; }

        const Vector<PluginInfo*>& plugins() const { return m_plugins; }
        const Vector<MimeClassInfo*>& mimes() const { return m_mimes; }

        bool supportsMimeType(const String& mimeType) const;
        String pluginNameForMimeType(const String& mimeType) const;

        static void refresh();

    private:
        PluginData(const Page*);
        void initPlugins();

        Vector<PluginInfo*> m_plugins;
        Vector<MimeClassInfo*> m_mimes;
        const Page* m_page;
    };

}

#endif
