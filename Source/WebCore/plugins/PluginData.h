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

struct MimeClassInfo {
    String type;
    String desc;
    Vector<String> extensions;

    MimeClassInfo isolatedCopy()
    {
        MimeClassInfo clone;
        clone.type = type.isolatedCopy();
        clone.desc = desc.isolatedCopy();
        for (unsigned i = 0; i < extensions.size(); ++i)
            clone.extensions.append(extensions[i].isolatedCopy());
        return clone;
    }
};

inline bool operator==(const MimeClassInfo& a, const MimeClassInfo& b)
{
    return a.type == b.type && a.desc == b.desc && a.extensions == b.extensions;
}

struct PluginInfo {
    String name;
    String file;
    String desc;
    Vector<MimeClassInfo> mimes;

    PluginInfo isolatedCopy()
    {
        PluginInfo clone;
        clone.name = name.isolatedCopy();
        clone.file = file.isolatedCopy();
        clone.desc = desc.isolatedCopy();
        for (unsigned i = 0; i < mimes.size(); ++i)
            clone.mimes.append(mimes[i].isolatedCopy());
        return clone;
    }
};

// FIXME: merge with PluginDatabase in the future
class PluginData : public RefCounted<PluginData> {
public:
    static PassRefPtr<PluginData> create(const Page* page) { return adoptRef(new PluginData(page)); }

    const Vector<PluginInfo>& plugins() const { return m_plugins; }
    const Vector<MimeClassInfo>& mimes() const { return m_mimes; }
    const Vector<size_t>& mimePluginIndices() const { return m_mimePluginIndices; }
    
    bool supportsMimeType(const String& mimeType) const;
    String pluginNameForMimeType(const String& mimeType) const;

    static void refresh();

private:
    PluginData(const Page*);
    void initPlugins(const Page*);

    Vector<PluginInfo> m_plugins;
    Vector<MimeClassInfo> m_mimes;
    Vector<size_t> m_mimePluginIndices;
};

}

#endif
