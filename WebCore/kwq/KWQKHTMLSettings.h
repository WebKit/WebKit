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

#ifndef KHTML_SETTINGS_H_
#define KHTML_SETTINGS_H_

#include "QString.h"
#include "KWQStringList.h"
#include "KWQFont.h"

class KHTMLSettings
{
public:
    enum KAnimationAdvice {
        KAnimationDisabled,
        KAnimationLoopOnce,
        KAnimationEnabled
    };
    
    KHTMLSettings() { }
    
    const QString &stdFontName() const { return _stdFontName; }
    const QString &fixedFontName() const { return _fixedFontName; }
    const QString &serifFontName() const { return _serifFontName; }
    const QString &sansSerifFontName() const { return _sansSerifFontName; }
    const QString &cursiveFontName() const { return _cursiveFontName; }
    const QString &fantasyFontName() const { return _fantasyFontName; }

    int minFontSize() const { return _minimumFontSize; }
    int minLogicalFontSize() const { return _minimumLogicalFontSize; }
    int mediumFontSize() const { return _defaultFontSize; }
    int mediumFixedFontSize() const { return _defaultFixedFontSize; }

    static bool changeCursor() { return true; }

    static bool isFormCompletionEnabled() { return false; }
    static int maxFormCompletionItems() { return 0; }

    bool autoLoadImages() const { return _willLoadImagesAutomatically; }
    static KAnimationAdvice showAnimations() { return KAnimationEnabled; }

    bool isJavaScriptEnabled() const { return _JavaScriptEnabled; }
    bool JavaScriptCanOpenWindowsAutomatically() const { return _JavaScriptCanOpenWindowsAutomatically; }
    bool isJavaScriptEnabled(const QString &host) const { return _JavaScriptEnabled; }
    bool isJavaEnabled() const { return _JavaEnabled; }
    bool isJavaEnabled(const QString &host) const { return _JavaEnabled; }
    bool isPluginsEnabled() const { return _pluginsEnabled; }
    bool isPluginsEnabled(const QString &host) const { return _pluginsEnabled; }
    
    const QString &encoding() const { return _encoding; }

    const QString &userStyleSheet() const { return _userStyleSheetLocation; }
    bool shouldPrintBackgrounds() const { return _shouldPrintBackgrounds; }
    bool textAreasAreResizable() const { return _textAreasAreResizable; }

    void setStdFontName(const QString &s) { _stdFontName = s; }
    void setFixedFontName(const QString &s) { _fixedFontName = s; }
    void setSerifFontName(const QString &s) { _serifFontName = s; }
    void setSansSerifFontName(const QString &s) { _sansSerifFontName = s; }
    void setCursiveFontName(const QString &s) { _cursiveFontName = s; }
    void setFantasyFontName(const QString &s) { _fantasyFontName = s; }
    
    void setMinFontSize(int s) { _minimumFontSize = s; }
    void setMinLogicalFontSize(int s) { _minimumLogicalFontSize = s; }
    void setMediumFontSize(int s) { _defaultFontSize = s; }
    void setMediumFixedFontSize(int s) { _defaultFixedFontSize = s; }
    
    void setAutoLoadImages(bool f) { _willLoadImagesAutomatically = f; }
    void setIsJavaScriptEnabled(bool f) { _JavaScriptEnabled = f; }
    void setIsJavaEnabled(bool f) { _JavaEnabled = f; }
    void setArePluginsEnabled(bool f) { _pluginsEnabled = f; }
    void setJavaScriptCanOpenWindowsAutomatically(bool f) { _JavaScriptCanOpenWindowsAutomatically = f; }

    void setEncoding(const QString &s) { _encoding = s; }

    void setUserStyleSheet(const QString &s) { _userStyleSheetLocation = s; }
    void setShouldPrintBackgrounds(bool f) { _shouldPrintBackgrounds = f; }
    void setTextAreasAreResizable(bool f) { _textAreasAreResizable = f; }
    
private:
    QString _stdFontName;
    QString _fixedFontName;
    QString _serifFontName;
    QString _sansSerifFontName;
    QString _cursiveFontName;
    QString _fantasyFontName;
    QString _encoding;
    QString _userStyleSheetLocation;
    
    int _minimumFontSize;
    int _minimumLogicalFontSize;
    int _defaultFontSize;
    int _defaultFixedFontSize;
    uint _JavaEnabled:1;
    uint _willLoadImagesAutomatically:1;
    uint _pluginsEnabled:1;
    uint _JavaScriptEnabled:1;
    uint _JavaScriptCanOpenWindowsAutomatically:1;
    uint _shouldPrintBackgrounds:1;
    uint _textAreasAreResizable:1;
    
};

#endif
