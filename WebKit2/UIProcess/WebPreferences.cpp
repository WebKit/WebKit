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

#include "WebPreferences.h"

#include "WebContext.h"

namespace WebKit {

WebPreferences* WebPreferences::shared()
{
    static WebPreferences* sharedPreferences = WebPreferences::create().leakRef();
    return sharedPreferences;
}

WebPreferences::WebPreferences()
{
}

WebPreferences::WebPreferences(WebPreferences* preferences)
    : m_store(preferences->m_store)
{
}

WebPreferences::~WebPreferences()
{
}

void WebPreferences::addContext(WebContext* context)
{
    m_contexts.add(context);
}

void WebPreferences::removeContext(WebContext* context)
{
    m_contexts.remove(context);
}

void WebPreferences::update()
{
    for (HashSet<WebContext*>::iterator it = m_contexts.begin(), end = m_contexts.end(); it != end; ++it)
        (*it)->preferencesDidChange();
}

void WebPreferences::setJavaScriptEnabled(bool b)
{
    m_store.javaScriptEnabled = b;
    update();
}

bool WebPreferences::javaScriptEnabled() const
{
    return m_store.javaScriptEnabled;
}

void WebPreferences::setLoadsImagesAutomatically(bool b)
{
    m_store.loadsImagesAutomatically = b;
    update();
}

bool WebPreferences::loadsImagesAutomatically() const
{
    return m_store.loadsImagesAutomatically;
}

void WebPreferences::setOfflineWebApplicationCacheEnabled(bool b)
{
    m_store.offlineWebApplicationCacheEnabled = b;
    update();
}
    
bool WebPreferences::offlineWebApplicationCacheEnabled() const
{
    return m_store.offlineWebApplicationCacheEnabled;
}

void WebPreferences::setLocalStorageEnabled(bool b)
{
    m_store.localStorageEnabled = b;
    update();
}

bool WebPreferences::localStorageEnabled() const
{
    return m_store.localStorageEnabled;
}

void WebPreferences::setXSSAuditorEnabled(bool b)
{
    m_store.xssAuditorEnabled = b;
    update();
}

bool WebPreferences::xssAuditorEnabled() const
{
    return m_store.xssAuditorEnabled;
}

void WebPreferences::setFontSmoothingLevel(FontSmoothingLevel level)
{
    m_store.fontSmoothingLevel = level;
    update();
}

FontSmoothingLevel WebPreferences::fontSmoothingLevel() const
{
    return static_cast<FontSmoothingLevel>(m_store.fontSmoothingLevel);
}

} // namespace WebKit

