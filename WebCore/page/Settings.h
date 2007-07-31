/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef Settings_h
#define Settings_h

#include "AtomicString.h"
#include "KURL.h"

namespace WebCore {

    class Page;

    enum EditableLinkBehavior {
        EditableLinkDefaultBehavior = 0,
        EditableLinkAlwaysLive,
        EditableLinkOnlyLiveWithShiftKey,
        EditableLinkLiveWhenNotFocused,
        EditableLinkNeverLive
    };

    class Settings
    {
    public:
        Settings(Page*);

        void setStandardFontFamily(const AtomicString&);
        const AtomicString& standardFontFamily() const { return m_standardFontFamily; }

        void setFixedFontFamily(const AtomicString&);
        const AtomicString& fixedFontFamily() const { return m_fixedFontFamily; }

        void setSerifFontFamily(const AtomicString&);
        const AtomicString& serifFontFamily() const { return m_serifFontFamily; }

        void setSansSerifFontFamily(const AtomicString&);
        const AtomicString& sansSerifFontFamily() const { return m_sansSerifFontFamily; }

        void setCursiveFontFamily(const AtomicString&);
        const AtomicString& cursiveFontFamily() const { return m_cursiveFontFamily; }

        void setFantasyFontFamily(const AtomicString&);
        const AtomicString& fantasyFontFamily() const { return m_fantasyFontFamily; }

        void setMinimumFontSize(int);
        int minimumFontSize() const { return m_minimumFontSize; }

        void setMinimumLogicalFontSize(int);
        int minimumLogicalFontSize() const { return m_minimumLogicalFontSize; }

        void setDefaultFontSize(int);
        int defaultFontSize() const { return m_defaultFontSize; }

        void setDefaultFixedFontSize(int);
        int defaultFixedFontSize() const { return m_defaultFixedFontSize; }

        void setLoadsImagesAutomatically(bool);
        bool loadsImagesAutomatically() const { return m_loadsImagesAutomatically; }

        void setJavaScriptEnabled(bool);
        bool isJavaScriptEnabled() const { return m_isJavaScriptEnabled; }

        void setJavaScriptCanOpenWindowsAutomatically(bool);
        bool JavaScriptCanOpenWindowsAutomatically() const { return m_javaScriptCanOpenWindowsAutomatically; }

        void setJavaEnabled(bool);
        bool isJavaEnabled() const { return m_isJavaEnabled; }

        void setPluginsEnabled(bool);
        bool arePluginsEnabled() const { return m_arePluginsEnabled; }

        void setPrivateBrowsingEnabled(bool);
        bool privateBrowsingEnabled() const { return m_privateBrowsingEnabled; }
        
        void setDefaultTextEncodingName(const String&);
        const String& defaultTextEncodingName() const { return m_defaultTextEncodingName; }

        void setUserStyleSheetLocation(const KURL&);
        KURL userStyleSheetLocation() const { return m_userStyleSheetLocation; }

        void setShouldPrintBackgrounds(bool);
        bool shouldPrintBackgrounds() const { return m_shouldPrintBackgrounds; }

        void setTextAreasAreResizable(bool);
        bool textAreasAreResizable() const { return m_textAreasAreResizable; }

        void setEditableLinkBehavior(EditableLinkBehavior);
        EditableLinkBehavior editableLinkBehavior() const { return m_editableLinkBehavior; }
        
        void setUsesDashboardBackwardCompatibilityMode(bool);
        bool usesDashboardBackwardCompatibilityMode() const { return m_usesDashboardBackwardCompatibilityMode; }
        
        void setNeedsAdobeFrameReloadingQuirk(bool);
        bool needsAcrobatFrameReloadingQuirk() const { return m_needsAdobeFrameReloadingQuirk; }
        
        void setDOMPasteAllowed(bool);
        bool isDOMPasteAllowed() const { return m_isDOMPasteAllowed; }
        
        void setUsesPageCache(bool);
        bool usesPageCache() const { return m_usesPageCache; }

        void setShrinksStandaloneImagesToFit(bool);
        bool shrinksStandaloneImagesToFit() const { return m_shrinksStandaloneImagesToFit; }

        void setShowsURLsInToolTips(bool);
        bool showsURLsInToolTips() const { return m_showsURLsInToolTips; }

        void setFTPDirectoryTemplatePath(const String&);
        const String& ftpDirectoryTemplatePath() const { return m_ftpDirectoryTemplatePath; }
        
        void setForceFTPDirectoryListings(bool);
        bool forceFTPDirectoryListings() const { return m_forceFTPDirectoryListings; }
        
        void setDeveloperExtrasEnabled(bool);
        bool developerExtrasEnabled() const { return m_developerExtrasEnabled; }
        
    private:
        Page* m_page;
        
        String m_defaultTextEncodingName;
        String m_ftpDirectoryTemplatePath;
        KURL m_userStyleSheetLocation;
        AtomicString m_standardFontFamily;
        AtomicString m_fixedFontFamily;
        AtomicString m_serifFontFamily;
        AtomicString m_sansSerifFontFamily;
        AtomicString m_cursiveFontFamily;
        AtomicString m_fantasyFontFamily;
        EditableLinkBehavior m_editableLinkBehavior;
        int m_minimumFontSize;
        int m_minimumLogicalFontSize;
        int m_defaultFontSize;
        int m_defaultFixedFontSize;
        bool m_isJavaEnabled : 1;
        bool m_loadsImagesAutomatically : 1;
        bool m_privateBrowsingEnabled : 1;
        bool m_arePluginsEnabled : 1;
        bool m_isJavaScriptEnabled : 1;
        bool m_javaScriptCanOpenWindowsAutomatically : 1;
        bool m_shouldPrintBackgrounds : 1;
        bool m_textAreasAreResizable : 1;
        bool m_usesDashboardBackwardCompatibilityMode : 1;
        bool m_needsAdobeFrameReloadingQuirk : 1;
        bool m_isDOMPasteAllowed : 1;
        bool m_shrinksStandaloneImagesToFit : 1;
        bool m_usesPageCache: 1;
        bool m_showsURLsInToolTips : 1;
        bool m_forceFTPDirectoryListings : 1;
        bool m_developerExtrasEnabled : 1;
    };

} // namespace WebCore

#endif // Settings_h
