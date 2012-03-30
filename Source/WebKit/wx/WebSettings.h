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
 
#ifndef WebSettings_h
#define WebSettings_h

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "WebKitDefines.h"

namespace WebCore {
class Settings;
}

namespace WebKit {

enum wxEditableLinkBehavior {
    wxEditableLinkDefaultBehavior,
    wxEditableLinkAlwaysLive,
    wxEditableLinkOnlyLiveWithShiftKey,
    wxEditableLinkLiveWhenNotFocused,
    wxEditableLinkNeverLive
};

/**
    @class WebSettings
    
    This class is used to control the configurable aspects of the WebKit engine.
    
    Do not instantiate this object directly. Instead, create a WebView and
    call its WebView::GetWebSettings() method to get and change that WebView's settings.
    
*/

class WXDLLIMPEXP_WEBKIT WebSettings: public wxObject {
public:
    WebSettings(WebCore::Settings* settings) :
        wxObject(),
        m_settings(settings)
    { }
    
    WebSettings() : wxObject() { }
    
    virtual ~WebSettings() { }

    /**
        Sets the default font size for fixed fonts.
    */
    void SetDefaultFixedFontSize(int size);
    
    /**
        Returns the default font size for fixed fonts.
    */
    int GetDefaultFixedFontSize() const;
    
    /**
        Sets the default font size for fonts.
    */
    void SetDefaultFontSize(int size);

    /**
        Returns the default font size for fonts.
    */
    int GetDefaultFontSize() const;
    
    /**
        Sets the minimum acceptable font size.
    */
    void SetMinimumFontSize(int size);

    /**
        Returns the minimum acceptable font size.
    */
    int GetMinimumFontSize() const;
    
    /**
        Sets whether or not images are loaded automatically. (e.g. in email 
        programs you may wish to not load images until you confirm it is not SPAM)
    */
    void SetLoadsImagesAutomatically(bool);
    
    /**
        Returns whether or not images are loaded automatically.
    */
    bool LoadsImagesAutomatically() const;
    
    /**
        Sets whether or not the WebView runs JavaScript code.
    */
    void SetJavaScriptEnabled(bool);

    /**
        Returns whether or not the WebView runs JavaScript code.
    */
    bool IsJavaScriptEnabled() const;
    
    /**
        Sets the path where local data will be stored.
    */
    void SetLocalStoragePath(const wxString& path);

    /**
        Returns the path where local data will be stored.
    */
    wxString GetLocalStoragePath() const;
    
    /**
        Sets how links are handled when the WebView is in editing mode. 
    */    
    void SetEditableLinkBehavior(wxEditableLinkBehavior behavior);
    
    /**
        Returns how links are handled when the WebView is in editing mode. 
    */   
    wxEditableLinkBehavior GetEditableLinkBehavior() const;
    
    /**
        Sets whether or not web pages can load plugins.
    */
    void SetPluginsEnabled(bool);
    
    /**
        Returns whether or not web pages can load plugins.
    */    
    bool ArePluginsEnabled() const;
    
    /**
        Enables or disables private browsing mode. When on, wxWebKit will avoid storing
        any sort of persistent data or storage on disk.
    */
    void SetPrivateBrowsingEnabled(bool);
    
    /**
        Returns the current state of private browsing mode.
    */
    bool PrivateBrowsingEnabled();
    
    /**
        Sets whether or not wxWebKit caches pages.
    */
    void SetUsesPageCache(bool);
    
    /**
        Returns whether or not wxWebKit caches pages.
    */
    bool UsesPageCache();
    
    /**
        Sets whether or not wxWebKit uses the offfline web application cache.
    */
    void SetOfflineWebApplicationCacheEnabled(bool);
    
    /**
        Returns whether or not wxWebKit uses the offline web application cache.
    */
    bool OfflineWebApplicationCacheEnabled();
    
private:
    WebCore::Settings* m_settings;

};

}

#endif // WebSettings_h
