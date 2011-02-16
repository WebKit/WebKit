/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPluginSiteDataManager.h"

#include "ImmutableArray.h"
#include "WebContext.h"
#include "WebProcessMessages.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebPluginSiteDataManager> WebPluginSiteDataManager::create(WebContext* webContext)
{
    return adoptRef(new WebPluginSiteDataManager(webContext));
}

WebPluginSiteDataManager::WebPluginSiteDataManager(WebContext* webContext)
    : m_webContext(webContext)
{
}

WebPluginSiteDataManager::~WebPluginSiteDataManager()
{
}

void WebPluginSiteDataManager::invalidate()
{
    invalidateCallbackMap(m_arrayCallbacks);
}

void WebPluginSiteDataManager::getSitesWithData(PassRefPtr<ArrayCallback> prpCallback)
{
    RefPtr<ArrayCallback> callback = prpCallback;

    if (!m_webContext) {
        callback->invalidate();
        return;
    }
        
#if ENABLE(PLUGIN_PROCESS)
    // FIXME: Implement.
    callback->invalidate();
#else
    if (!m_webContext->hasValidProcess()) {
        callback->invalidate();
        return;
    }
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    Vector<String> pluginPaths;
    m_webContext->pluginInfoStore()->getPluginPaths(pluginPaths);
    m_webContext->process()->send(Messages::WebProcess::GetSitesWithPluginData(pluginPaths, callbackID), 0);
#endif
}

void WebPluginSiteDataManager::didGetSitesWithData(const Vector<String>& sites, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    Vector<RefPtr<APIObject> > sitesWK(sites.size());

    for (size_t i = 0; i < sites.size(); ++i)
        sitesWK[i] = WebString::create(sites[i]);

    RefPtr<ImmutableArray> resultArray = ImmutableArray::adopt(sitesWK);
    callback->performCallbackWithReturnValue(resultArray.get());
}

void WebPluginSiteDataManager::clearSiteData(ImmutableArray* sites, uint64_t flags, uint64_t maxAgeInSeconds, PassRefPtr<VoidCallback> prpCallback)
{
    RefPtr<VoidCallback> callback = prpCallback;
    if (!m_webContext) {
        callback->invalidate();
        return;
    }

    // If the array is empty, don't do anything.
    if (sites && !sites->size()) {
        callback->performCallback();
        return;
    }

#if ENABLE(PLUGIN_PROCESS)
    // FIXME: Implement.
    callback->invalidate();
#else
    if (!m_webContext->hasValidProcess()) {
        callback->invalidate();
        return;
    }
    uint64_t callbackID = callback->callbackID();
    m_voidCallbacks.set(callbackID, callback.release());

    Vector<String> sitesVector;
    if (sites) {
        for (size_t i = 0; i < sites->size(); ++i) {
            if (WebString* site = sites->at<WebString>(i))
                sitesVector.append(site->string());
        }
    }

    Vector<String> pluginPaths;
    m_webContext->pluginInfoStore()->getPluginPaths(pluginPaths);
    m_webContext->process()->send(Messages::WebProcess::ClearPluginSiteData(pluginPaths, sitesVector, flags, maxAgeInSeconds, callbackID), 0);
#endif
}

void WebPluginSiteDataManager::didClearSiteData(uint64_t callbackID)
{
    RefPtr<VoidCallback> callback = m_voidCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallback();
}

} // namespace WebKit

