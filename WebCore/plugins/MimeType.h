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

#ifndef MimeType_h
#define MimeType_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/RefCounted.h>

#include "PluginData.h"

namespace WebCore {

    class Plugin;
    class String;

    class MimeType : public RefCounted<MimeType> {
    public:
        static PassRefPtr<MimeType> create(PassRefPtr<PluginData> pluginData, unsigned index) { return adoptRef(new MimeType(pluginData, index)); }
        ~MimeType();

        const String &type() const;
        const String &suffixes() const;
        const String &description() const;
        PassRefPtr<Plugin> enabledPlugin() const;

    private:
        MimeType(PassRefPtr<PluginData>, unsigned index);
        RefPtr<PluginData> m_pluginData;
        unsigned m_index;
    };

}

#endif
