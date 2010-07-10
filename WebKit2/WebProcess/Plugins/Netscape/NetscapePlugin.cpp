/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "NetscapePlugin.h"

#include <WebCore/GraphicsContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/KURL.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

NetscapePlugin::NetscapePlugin(PassRefPtr<NetscapePluginModule> pluginModule)
    : m_pluginModule(pluginModule)
    , m_npWindow()
    , m_isStarted(false)
    , m_inNPPNew(false)
#if PLATFORM(MAC)
    , m_drawingModel(static_cast<NPDrawingModel>(-1))
    , m_eventModel(static_cast<NPEventModel>(-1))
#endif
{
    m_npp.ndata = this;
    m_npp.pdata = 0;
    
    m_pluginModule->pluginCreated();
}

NetscapePlugin::~NetscapePlugin()
{
    ASSERT(!m_isStarted);

    m_pluginModule->pluginDestroyed();
}

PassRefPtr<NetscapePlugin> NetscapePlugin::fromNPP(NPP npp)
{
    if (npp) {
        NetscapePlugin* plugin = static_cast<NetscapePlugin*>(npp->ndata);
        ASSERT(npp == &plugin->m_npp);
        
        return plugin;
    }

    // FIXME: Return the current NetscapePlugin here.
    ASSERT_NOT_REACHED();
    return 0;
}

void NetscapePlugin::callSetWindow()
{
    m_npWindow.x = m_frameRect.x();
    m_npWindow.y = m_frameRect.y();
    m_npWindow.width = m_frameRect.width();
    m_npWindow.height = m_frameRect.height();
    m_npWindow.clipRect.top = m_clipRect.y();
    m_npWindow.clipRect.left = m_clipRect.x();
    m_npWindow.clipRect.bottom = m_clipRect.bottom();
    m_npWindow.clipRect.right = m_clipRect.right();

    m_pluginModule->pluginFuncs().setwindow(&m_npp, &m_npWindow);
}

bool NetscapePlugin::initialize(const Parameters& parameters)
{
    uint16_t mode = parameters.loadManually ? NP_FULL : NP_EMBED;
    
    m_inNPPNew = true;

    CString mimeTypeCString = parameters.mimeType.utf8();

    ASSERT(parameters.names.size() == parameters.values.size());

    Vector<CString> paramNames;
    Vector<CString> paramValues;
    for (size_t i = 0; i < parameters.names.size(); ++i) {
        paramNames.append(parameters.names[i].utf8());
        paramValues.append(parameters.values[i].utf8());
    }

    // The strings that these pointers point to are kept alive by paramNames and paramValues.
    Vector<const char*> names;
    Vector<const char*> values;
    for (size_t i = 0; i < paramNames.size(); ++i) {
        names.append(paramNames[i].data());
        values.append(paramValues[i].data());
    }
    
    NPError error = m_pluginModule->pluginFuncs().newp(const_cast<char*>(mimeTypeCString.data()), &m_npp, mode, 
                                                       names.size(), const_cast<char**>(names.data()), const_cast<char**>(values.data()), 0);
    m_inNPPNew = false;

    if (error != NPERR_NO_ERROR)
        return false;

    m_isStarted = true;

    // FIXME: This is not correct in all cases.
    m_npWindow.type = NPWindowTypeDrawable;

    if (!platformPostInitialize()) {
        destroy();
        return false;
    }

    return true;
}
    
void NetscapePlugin::destroy()
{
    ASSERT(m_isStarted);

    m_pluginModule->pluginFuncs().destroy(&m_npp, 0);
    m_isStarted = false;
}
    
void NetscapePlugin::paint(GraphicsContext* context, const IntRect& dirtyRect)
{
    platformPaint(context, dirtyRect);
}

void NetscapePlugin::geometryDidChange(const IntRect& frameRect, const IntRect& clipRect)
{
    if (m_frameRect == frameRect && m_clipRect == clipRect) {
        // Nothing to do.
        return;
    }

    m_frameRect = frameRect;
    m_clipRect = clipRect;

    callSetWindow();
}

} // namespace WebKit
