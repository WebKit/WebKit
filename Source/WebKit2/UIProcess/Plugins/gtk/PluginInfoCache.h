/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PluginInfoCache_h
#define PluginInfoCache_h

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "PluginModuleInfo.h"
#include <mutex>
#include <wtf/NeverDestroyed.h>
#include <wtf/gobject/GOwnPtr.h>

namespace WebKit {

class PluginInfoCache {
    WTF_MAKE_NONCOPYABLE(PluginInfoCache);
    friend class NeverDestroyed<PluginInfoCache>;
public:
    static PluginInfoCache& shared();

    bool getPluginInfo(const String& pluginPath, PluginModuleInfo&);
    void updatePluginInfo(const String& pluginPath, const PluginModuleInfo&);

private:
    PluginInfoCache();
    ~PluginInfoCache();

    void saveToFile();
    static gboolean saveToFileIdleCallback(PluginInfoCache*);

    GOwnPtr<GKeyFile> m_cacheFile;
    GOwnPtr<char> m_cachePath;
    unsigned m_saveToFileIdleId;
    bool m_readOnlyMode;
    std::mutex m_mutex;
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif // PluginInfoCache_h
