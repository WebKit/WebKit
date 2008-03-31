/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PluginDatabase_H
#define PluginDatabase_H

#include "PlatformString.h"
#include "PluginPackage.h"
#include "StringHash.h"

#include <wtf/Vector.h>
#include <wtf/HashSet.h>

namespace WebCore {
    class Element;
    class Frame;
    class IntSize;
    class KURL;
    class PluginPackage;

    typedef HashSet<RefPtr<PluginPackage>, PluginPackageHash> PluginSet;
  
    class PluginDatabase {
    public:
        static PluginDatabase* installedPlugins();

        bool refresh();
        Vector<PluginPackage*> plugins() const;
        bool isMIMETypeRegistered(const String& mimeType);
        void addExtraPluginDirectory(const String&);

        static bool isPreferredPluginDirectory(const String& directory);
        static int preferredPluginCompare(const void*, const void*);

        PluginPackage* findPlugin(const KURL&, String& mimeType);

    private:
        void setPluginDirectories(const Vector<String>& directories) { m_pluginDirectories = directories; }

        void getPluginPathsInDirectories(HashSet<String>&) const;
        void getDeletedPlugins(PluginSet&) const;

        // Returns whether the plugin was actually added or not (it won't be added if it's a duplicate of an existing plugin).
        bool add(PassRefPtr<PluginPackage>);
        void remove(PluginPackage*);

        PluginPackage* pluginForMIMEType(const String& mimeType);
        String MIMETypeForExtension(const String& extension) const;

        static Vector<String> defaultPluginDirectories();

        Vector<String> m_pluginDirectories;
        HashSet<String> m_registeredMIMETypes;
        PluginSet m_plugins;
        HashMap<String, RefPtr<PluginPackage> > m_pluginsByPath;
        HashMap<String, time_t> m_pluginPathsWithTimes;
    };

} // namespace WebCore

#endif
