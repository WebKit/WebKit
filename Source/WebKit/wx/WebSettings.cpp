/*
 * Copyright (C) 2009 Kevin Ollivier.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSettings.h"

#include "PlatformString.h"
#include "Settings.h"

namespace WebKit {

void WebSettings::SetDefaultFixedFontSize(int size)
{
    if (m_settings)
        m_settings->setDefaultFixedFontSize(size);
}
    
int WebSettings::GetDefaultFixedFontSize() const
{
    if (m_settings)
        return m_settings->defaultFixedFontSize();
        
    return 0;
}
    
void WebSettings::SetDefaultFontSize(int size)
{
    if (m_settings)
        m_settings->setDefaultFontSize(size);
}

int WebSettings::GetDefaultFontSize() const
{
    if (m_settings)
        return m_settings->defaultFontSize();
        
    return 0;
}
    
void WebSettings::SetMinimumFontSize(int size)
{
    if (m_settings)
        m_settings->setMinimumFontSize(size);
}

int WebSettings::GetMinimumFontSize() const
{
    if (m_settings)
        return m_settings->minimumFontSize();
        
    return 0;
}
    
void WebSettings::SetLoadsImagesAutomatically(bool loadAutomatically)
{
    if (m_settings)
        m_settings->setLoadsImagesAutomatically(loadAutomatically);
}
    
bool WebSettings::LoadsImagesAutomatically() const
{
    if (m_settings)
        return m_settings->loadsImagesAutomatically();
        
    return false;
}
    
void WebSettings::SetJavaScriptEnabled(bool enabled)
{
    if (m_settings)
        m_settings->setScriptEnabled(enabled);
}

bool WebSettings::IsJavaScriptEnabled() const
{
    if (m_settings)
        return m_settings->isScriptEnabled();
        
    return false;
}
    
void WebSettings::SetLocalStoragePath(const wxString& path)
{
    if (m_settings)
        m_settings->setLocalStorageDatabasePath(path);
}

wxString WebSettings::GetLocalStoragePath() const
{
    if (m_settings)
        return m_settings->localStorageDatabasePath();
        
    return wxEmptyString;
}

void WebSettings::SetEditableLinkBehavior(wxEditableLinkBehavior behavior)
{
    WebCore::EditableLinkBehavior webCoreBehavior;
    if (m_settings) {
        switch (behavior) {
        case wxEditableLinkAlwaysLive:
            webCoreBehavior = WebCore::EditableLinkAlwaysLive;
            break;
        case wxEditableLinkOnlyLiveWithShiftKey:
            webCoreBehavior = WebCore::EditableLinkOnlyLiveWithShiftKey;
            break;
        case wxEditableLinkLiveWhenNotFocused:
            webCoreBehavior = WebCore::EditableLinkLiveWhenNotFocused;
            break;
        case wxEditableLinkNeverLive:
            webCoreBehavior = WebCore::EditableLinkNeverLive;
            break;
        default:
            webCoreBehavior = WebCore::EditableLinkDefaultBehavior;
        }
        m_settings->setEditableLinkBehavior(webCoreBehavior);
    }
}

wxEditableLinkBehavior WebSettings::GetEditableLinkBehavior() const
{
    wxEditableLinkBehavior behavior = wxEditableLinkDefaultBehavior;
    if (m_settings) {
        WebCore::EditableLinkBehavior webCoreBehavior = m_settings->editableLinkBehavior();
        switch (webCoreBehavior) {
        case WebCore::EditableLinkAlwaysLive:
            behavior = wxEditableLinkAlwaysLive;
            break;
        case WebCore::EditableLinkOnlyLiveWithShiftKey:
            behavior = wxEditableLinkOnlyLiveWithShiftKey;
            break;
        case WebCore::EditableLinkLiveWhenNotFocused:
            behavior = wxEditableLinkLiveWhenNotFocused;
            break;
        case WebCore::EditableLinkNeverLive:
            behavior = wxEditableLinkNeverLive;
            break;
        default:
            behavior = wxEditableLinkDefaultBehavior;
        }
    }
    return behavior;
}

void WebSettings::SetPluginsEnabled(bool enabled)
{
    if (m_settings)
        m_settings->setPluginsEnabled(enabled);
}  

bool WebSettings::ArePluginsEnabled() const
{
    if (m_settings)
        return m_settings->arePluginsEnabled();
        
    return false;
}

void WebSettings::SetPrivateBrowsingEnabled(bool enabled)
{
    if (m_settings)
        m_settings->setPrivateBrowsingEnabled(enabled);
}

bool WebSettings::PrivateBrowsingEnabled()
{
    if (m_settings)
        m_settings->privateBrowsingEnabled();

    return false;
}

void WebSettings::SetUsesPageCache(bool enabled)
{
    if (m_settings)
        m_settings->setUsesPageCache(enabled);
}

bool WebSettings::UsesPageCache()
{
    if (m_settings)
        m_settings->usesPageCache();
    
    return false;
}

void WebSettings::SetOfflineWebApplicationCacheEnabled(bool enabled)
{
    if (m_settings)
        m_settings->setOfflineWebApplicationCacheEnabled(enabled);
}

bool WebSettings::OfflineWebApplicationCacheEnabled()
{
    if (m_settings)
        m_settings->offlineWebApplicationCacheEnabled();
    
    return false;
}
    
}
