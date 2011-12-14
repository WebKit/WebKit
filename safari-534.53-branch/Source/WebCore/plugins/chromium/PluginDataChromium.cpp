/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PluginDataChromium.h"

#include "PlatformBridge.h"

namespace WebCore {

class PluginCache {
public:
    PluginCache() : m_loaded(false), m_refresh(false) {}
    ~PluginCache() { reset(false); }

    void reset(bool refresh)
    {
        m_plugins.clear();
        m_loaded = false;
        m_refresh = refresh;
    }

    const Vector<PluginInfo>& plugins()
    {
        if (!m_loaded) {
            PlatformBridge::plugins(m_refresh, &m_plugins);
            m_loaded = true;
            m_refresh = false;
        }
        return m_plugins;
    }

private:
    Vector<PluginInfo> m_plugins;
    bool m_loaded;
    bool m_refresh;
};

static PluginCache pluginCache;

void PluginData::initPlugins(const Page*)
{
    const Vector<PluginInfo>& plugins = pluginCache.plugins();
    for (size_t i = 0; i < plugins.size(); ++i)
        m_plugins.append(plugins[i]);
}

void PluginData::refresh()
{
    pluginCache.reset(true);
    pluginCache.plugins();  // Force the plugins to be reloaded now.
}

String getPluginMimeTypeFromExtension(const String& extension)
{
    const Vector<PluginInfo>& plugins = pluginCache.plugins();
    for (size_t i = 0; i < plugins.size(); ++i) {
        for (size_t j = 0; j < plugins[i].mimes.size(); ++j) {
            const MimeClassInfo& mime = plugins[i].mimes[j];
            const Vector<String>& extensions = mime.extensions;
            for (size_t k = 0; k < extensions.size(); ++k) {
                if (extension == extensions[k])
                    return mime.type;
            }
        }
    }
    return String();
}

} // namespace WebCore
