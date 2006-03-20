/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "PlatformString.h"
#include "Font.h"

class KHTMLSettings
{
public:
    enum KAnimationAdvice {
        KAnimationDisabled,
        KAnimationLoopOnce,
        KAnimationEnabled
    };
    
    const WebCore::AtomicString& stdFontName() const { return m_stdFontName; }
    const WebCore::AtomicString& fixedFontName() const { return m_fixedFontName; }
    const WebCore::AtomicString& serifFontName() const { return m_serifFontName; }
    const WebCore::AtomicString& sansSerifFontName() const { return m_sansSerifFontName; }
    const WebCore::AtomicString& cursiveFontName() const { return m_cursiveFontName; }
    const WebCore::AtomicString& fantasyFontName() const { return m_fantasyFontName; }

    int minFontSize() const { return m_minimumFontSize; }
    int minLogicalFontSize() const { return m_minimumLogicalFontSize; }
    int mediumFontSize() const { return m_defaultFontSize; }
    int mediumFixedFontSize() const { return m_defaultFixedFontSize; }

    bool autoLoadImages() const { return m_willLoadImagesAutomatically; }

    bool isJavaScriptEnabled() const { return m_javaScriptEnabled; }
    bool JavaScriptCanOpenWindowsAutomatically() const { return m_javaScriptCanOpenWindowsAutomatically; }
    bool isJavaEnabled() const { return m_javaEnabled; }
    bool isPluginsEnabled() const { return m_pluginsEnabled; }
    
    const DeprecatedString& encoding() const { return m_encoding; }

    DeprecatedString userStyleSheet() const { return m_userStyleSheetLocation; }
    bool shouldPrintBackgrounds() const { return m_shouldPrintBackgrounds; }
    bool textAreasAreResizable() const { return m_textAreasAreResizable; }

    void setStdFontName(const WebCore::AtomicString& s) { m_stdFontName = s; }
    void setFixedFontName(const WebCore::AtomicString& s) { m_fixedFontName = s; }
    void setSerifFontName(const WebCore::AtomicString& s) { m_serifFontName = s; }
    void setSansSerifFontName(const WebCore::AtomicString& s) { m_sansSerifFontName = s; }
    void setCursiveFontName(const WebCore::AtomicString& s) { m_cursiveFontName = s; }
    void setFantasyFontName(const WebCore::AtomicString& s) { m_fantasyFontName = s; }
    
    void setMinFontSize(int s) { m_minimumFontSize = s; }
    void setMinLogicalFontSize(int s) { m_minimumLogicalFontSize = s; }
    void setMediumFontSize(int s) { m_defaultFontSize = s; }
    void setMediumFixedFontSize(int s) { m_defaultFixedFontSize = s; }
    
    void setAutoLoadImages(bool f) { m_willLoadImagesAutomatically = f; }
    void setIsJavaScriptEnabled(bool f) { m_javaScriptEnabled = f; }
    void setIsJavaEnabled(bool f) { m_javaEnabled = f; }
    void setArePluginsEnabled(bool f) { m_pluginsEnabled = f; }
    void setJavaScriptCanOpenWindowsAutomatically(bool f) { m_javaScriptCanOpenWindowsAutomatically = f; }

    void setEncoding(const DeprecatedString& s) { m_encoding = s; }

    void setUserStyleSheet(const DeprecatedString& s) { m_userStyleSheetLocation = s; }
    void setShouldPrintBackgrounds(bool f) { m_shouldPrintBackgrounds = f; }
    void setTextAreasAreResizable(bool f) { m_textAreasAreResizable = f; }
    
private:
    WebCore::AtomicString m_stdFontName;
    WebCore::AtomicString m_fixedFontName;
    WebCore::AtomicString m_serifFontName;
    WebCore::AtomicString m_sansSerifFontName;
    WebCore::AtomicString m_cursiveFontName;
    WebCore::AtomicString m_fantasyFontName;
    DeprecatedString m_encoding; // FIXME: TextEncoding takes a latin1 string, which String & AtomicString don't easily produce
    DeprecatedString m_userStyleSheetLocation; // FIXME: KURLs still use DeprecatedString
    
    int m_minimumFontSize;
    int m_minimumLogicalFontSize;
    int m_defaultFontSize;
    int m_defaultFixedFontSize;
    bool m_javaEnabled : 1;
    bool m_willLoadImagesAutomatically : 1;
    bool m_pluginsEnabled : 1;
    bool m_javaScriptEnabled : 1;
    bool m_javaScriptCanOpenWindowsAutomatically : 1;
    bool m_shouldPrintBackgrounds : 1;
    bool m_textAreasAreResizable : 1;
};

#endif
