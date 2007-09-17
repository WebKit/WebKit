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

#ifndef PluginDatabaseWin_H
#define PluginDatabaseWin_H

#include <wtf/Vector.h>
#include <wtf/HashSet.h>

#include "PlatformString.h"
#include "PluginPackageWin.h"
#include "StringHash.h"

namespace WebCore {
    class Element;
    class Frame;
    class IntSize;
    class KURL;
    class PluginPackageWin;
    class PluginViewWin;

    typedef HashSet<RefPtr<PluginPackageWin>, PluginPackageWinHash> PluginSet;
  
    class PluginDatabaseWin {
    public:
        static PluginDatabaseWin* installedPlugins();
        PluginViewWin* createPluginView(Frame* parentFrame, const IntSize&, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually);

        bool refresh();
        Vector<PluginPackageWin*> plugins() const;
        bool isMIMETypeRegistered(const String& mimeType) const;
        void addExtraPluginPath(const String&);
    private:
        void setPluginPaths(const Vector<String>& paths) { m_pluginPaths = paths; }
        PluginSet getPluginsInPaths() const;

        PluginPackageWin* findPlugin(const KURL& url, const String& mimeType);

        PluginPackageWin* pluginForMIMEType(const String& mimeType);
        PluginPackageWin* pluginForExtension(const String& extension);

        static Vector<String> defaultPluginPaths();

        Vector<String> m_pluginPaths;
        HashSet<String> m_registeredMIMETypes;
        PluginSet m_plugins;
    };

} // namespace WebCore

#endif
