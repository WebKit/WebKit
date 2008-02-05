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

#ifndef PluginPackage_H
#define PluginPackage_H

#include <winsock2.h>
#include <windows.h>

#include "Timer.h"
#include "StringHash.h"
#include "PlatformString.h"
#include "npfunctions.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    typedef HashMap<String, String> MIMEToDescriptionsMap;
    typedef HashMap<String, Vector<String> > MIMEToExtensionsMap;

    class PluginPackage : public RefCounted<PluginPackage> {
    public:
        ~PluginPackage();
        static PluginPackage* createPackage(const String& path, const FILETIME& lastModified);
        
        String name() const { return m_name; }
        String description() const { return m_description; }
        String fileName() const { return m_fileName; }
        String parentDirectory() const { return m_parentDirectory; }

        const MIMEToDescriptionsMap& mimeToDescriptions() const { return m_mimeToDescriptions; }
        const MIMEToExtensionsMap& mimeToExtensions() const { return m_mimeToExtensions; }

        unsigned PluginPackage::hash() const;
        static bool equal(const PluginPackage& a, const PluginPackage& b);

        bool load();
        void unload();
        void unloadWithoutShutdown();

        const NPPluginFuncs* pluginFuncs() const { return &m_pluginFuncs; }

        int compareFileVersion(unsigned compareVersionMS, unsigned compareVersionLS) const;
    private:
        PluginPackage(const String& path, const FILETIME& lastModified);
        bool fetchInfo();
        void storeFileVersion(LPVOID versionInfoData);
        bool isPluginBlacklisted();

        bool m_isLoaded;
        int m_loadCount;

        DWORD m_fileVersionMS;
        DWORD m_fileVersionLS;

        String m_description;
        String m_path;
        String m_fileName;
        String m_name;
        String m_parentDirectory;

        MIMEToDescriptionsMap m_mimeToDescriptions;
        MIMEToExtensionsMap m_mimeToExtensions;

        HMODULE m_module;
        FILETIME m_lastModified;

        NPP_ShutdownProcPtr m_NPP_Shutdown;
        NPPluginFuncs m_pluginFuncs;
        NPNetscapeFuncs m_browserFuncs;

        void freeLibrarySoon();
        void freeLibraryTimerFired(Timer<PluginPackage>*);
        Timer<PluginPackage> m_freeLibraryTimer;
    };

    struct PluginPackageHash {
        static unsigned hash(const int key) { return reinterpret_cast<PluginPackage*>(key)->hash(); }
        static unsigned hash(const RefPtr<PluginPackage>& key) { return key->hash(); }

        static bool equal(const int a, const int b) { return equal(reinterpret_cast<PluginPackage*>(a), reinterpret_cast<PluginPackage*>(b)); }
        static bool equal(const RefPtr<PluginPackage>& a, const RefPtr<PluginPackage>& b) { return PluginPackage::equal(*a.get(), *b.get()); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

} // namespace WebCore

// FIXME: This is a workaround for a bug in WTF, where it's impossible to use a custom Hash function but with default traits.
// It should be possible to do this without a StorageTraits specialization.
namespace WTF {
    template<> struct HashKeyStorageTraits<WebCore::PluginPackageHash, HashTraits<RefPtr<WebCore::PluginPackage> > > {
        typedef IntTypes<sizeof(RefPtr<WebCore::PluginPackage>)>::SignedType IntType;
        typedef WebCore::PluginPackageHash Hash;
        typedef HashTraits<IntType> Traits;
    };
}


#endif
