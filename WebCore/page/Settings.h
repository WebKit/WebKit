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

#ifndef WebCoreSettings_H
#define WebCoreSettings_H

#include "AtomicString.h"
#include "KURL.h"

namespace WebCore {

class Settings
{
public:
    enum EditableLinkBehavior {
        EditableLinkDefaultBehavior = 0,
        EditableLinkAlwaysLive,
        EditableLinkOnlyLiveWithShiftKey,
        EditableLinkLiveWhenNotFocused,
        EditableLinkNeverLive
    };

    Settings()
        : m_minimumFontSize(0)
        , m_minimumLogicalFontSize(0)
        , m_defaultFontSize(0)
        , m_defaultFixedFontSize(0)
        , m_javaEnabled(0)
        , m_willLoadImagesAutomatically(0)
        , m_privateBrowsingEnabled(0)
        , m_pluginsEnabled(0)
        , m_javaScriptEnabled(0)
        , m_javaScriptCanOpenWindowsAutomatically(0)
        , m_shouldPrintBackgrounds(false)
        , m_textAreasAreResizable(0)
        , m_shouldUseDashboardBackwardCompatibilityMode(false)
        , m_editableLinkBehavior(EditableLinkDefaultBehavior)
    {
    }

    const AtomicString& stdFontName() const { return m_stdFontName; }
    const AtomicString& fixedFontName() const { return m_fixedFontName; }
    const AtomicString& serifFontName() const { return m_serifFontName; }
    const AtomicString& sansSerifFontName() const { return m_sansSerifFontName; }
    const AtomicString& cursiveFontName() const { return m_cursiveFontName; }
    const AtomicString& fantasyFontName() const { return m_fantasyFontName; }

    int minFontSize() const { return m_minimumFontSize; }
    int minLogicalFontSize() const { return m_minimumLogicalFontSize; }
    int mediumFontSize() const { return m_defaultFontSize; }
    int mediumFixedFontSize() const { return m_defaultFixedFontSize; }

    bool autoLoadImages() const { return m_willLoadImagesAutomatically; }

    bool isJavaScriptEnabled() const { return m_javaScriptEnabled; }
    bool JavaScriptCanOpenWindowsAutomatically() const { return m_javaScriptCanOpenWindowsAutomatically; }
    bool isJavaEnabled() const { return m_javaEnabled; }
    bool isPluginsEnabled() const { return m_pluginsEnabled; }
    bool privateBrowsingEnabled() const { return m_privateBrowsingEnabled; }
    
    const String& encoding() const { return m_encoding; }

    KURL userStyleSheetLocation() const { return m_userStyleSheetLocation; }
    bool shouldPrintBackgrounds() const { return m_shouldPrintBackgrounds; }
    bool textAreasAreResizable() const { return m_textAreasAreResizable; }
    EditableLinkBehavior editableLinkBehavior() const { return m_editableLinkBehavior; }

    void setStdFontName(const AtomicString& s) { m_stdFontName = s; }
    void setFixedFontName(const AtomicString& s) { m_fixedFontName = s; }
    void setSerifFontName(const AtomicString& s) { m_serifFontName = s; }
    void setSansSerifFontName(const AtomicString& s) { m_sansSerifFontName = s; }
    void setCursiveFontName(const AtomicString& s) { m_cursiveFontName = s; }
    void setFantasyFontName(const AtomicString& s) { m_fantasyFontName = s; }
    
    void setMinFontSize(int s) { m_minimumFontSize = s; }
    void setMinLogicalFontSize(int s) { m_minimumLogicalFontSize = s; }
    void setMediumFontSize(int s) { m_defaultFontSize = s; }
    void setMediumFixedFontSize(int s) { m_defaultFixedFontSize = s; }
    
    void setAutoLoadImages(bool f) { m_willLoadImagesAutomatically = f; }
    void setIsJavaScriptEnabled(bool f) { m_javaScriptEnabled = f; }
    void setIsJavaEnabled(bool f) { m_javaEnabled = f; }
    void setArePluginsEnabled(bool f) { m_pluginsEnabled = f; }
    void setPrivateBrowsingEnabled(bool f) { m_privateBrowsingEnabled = f; }
    void setJavaScriptCanOpenWindowsAutomatically(bool f) { m_javaScriptCanOpenWindowsAutomatically = f; }

    void setEncoding(const String& s) { m_encoding = s; }

    void setUserStyleSheetLocation(const KURL& s) { m_userStyleSheetLocation = s; }
    void setShouldPrintBackgrounds(bool f) { m_shouldPrintBackgrounds = f; }
    void setTextAreasAreResizable(bool f) { m_textAreasAreResizable = f; }
    void setEditableLinkBehavior(EditableLinkBehavior e) { m_editableLinkBehavior = e; }
    
    void setShouldUseDashboardBackwardCompatibilityMode(bool b) { m_shouldUseDashboardBackwardCompatibilityMode = b; }
    bool shouldUseDashboardBackwardCompatibilityMode() const { return m_shouldUseDashboardBackwardCompatibilityMode; }
    
private:
    AtomicString m_stdFontName;
    AtomicString m_fixedFontName;
    AtomicString m_serifFontName;
    AtomicString m_sansSerifFontName;
    AtomicString m_cursiveFontName;
    AtomicString m_fantasyFontName;
    String m_encoding;
    KURL m_userStyleSheetLocation;
    
    int m_minimumFontSize;
    int m_minimumLogicalFontSize;
    int m_defaultFontSize;
    int m_defaultFixedFontSize;
    bool m_javaEnabled : 1;
    bool m_willLoadImagesAutomatically : 1;
    bool m_privateBrowsingEnabled : 1;
    bool m_pluginsEnabled : 1;
    bool m_javaScriptEnabled : 1;
    bool m_javaScriptCanOpenWindowsAutomatically : 1;
    bool m_shouldPrintBackgrounds : 1;
    bool m_textAreasAreResizable : 1;
    bool m_shouldUseDashboardBackwardCompatibilityMode : 1;
    EditableLinkBehavior m_editableLinkBehavior;
};

}

#endif
