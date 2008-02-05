/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#include <wtf/Vector.h>
#include <wtf/HashSet.h>

#include "PlatformString.h"
#include "PluginPackage.h"
#include "StringHash.h"

namespace WebCore {
    class Element;
    class Frame;
    class IntSize;
    class KURL;
    class PluginPackage;
    class PluginView;

    typedef HashSet<RefPtr<PluginPackage>, PluginPackageHash> PluginSet;
  
    class PluginDatabase {
    public:
        static PluginDatabase* installedPlugins();
        PluginView* createPluginView(Frame* parentFrame, const IntSize&, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually);

        bool refresh();
        Vector<PluginPackage*> plugins() const;
        bool isMIMETypeRegistered(const String& mimeType);
        void addExtraPluginPath(const String&);
    private:
        void setPluginPaths(const Vector<String>& paths) { m_pluginPaths = paths; }
        PluginSet getPluginsInPaths() const;

        PluginPackage* findPlugin(const KURL& url, String& mimeType);
        PluginPackage* pluginForMIMEType(const String& mimeType);
        String MIMETypeForExtension(const String& extension) const;

        static Vector<String> defaultPluginPaths();

        Vector<String> m_pluginPaths;
        HashSet<String> m_registeredMIMETypes;
        PluginSet m_plugins;
    };

} // namespace WebCore

#endif
