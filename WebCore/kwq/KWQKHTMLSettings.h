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

#include "KWQString.h"
#include "KWQStringList.h"
#include "KWQFont.h"
#include "KWQMap.h"

class KHTMLSettings
{
public:
    enum KAnimationAdvice {
        KAnimationDisabled,
        KAnimationLoopOnce,
        KAnimationEnabled
    };
    
    KHTMLSettings() {};
    
    // Font settings
    const QString &stdFontName() const;
    const QString &fixedFontName() const;
    const QString &serifFontName() const;
    const QString &sansSerifFontName() const;
    const QString &cursiveFontName() const;
    const QString &fantasyFontName() const;
    
    static const QString &settingsToCSS() { return QString::null; }

    static const QString &encoding();

    int minFontSize() const;
    int mediumFontSize() const;
    int mediumFixedFontSize() const;

    static bool changeCursor() { return true; }

    static bool isFormCompletionEnabled() { return false; }
    static int maxFormCompletionItems() { return 0; }

    bool autoLoadImages() const;
    static KAnimationAdvice showAnimations() { return KAnimationEnabled; }

    bool isJavaScriptEnabled() const;
    bool JavaScriptCanOpenWindowsAutomatically() const;
    bool isJavaScriptEnabled(const QString &host) const { return isJavaScriptEnabled(); }
    bool isJavaScriptDebugEnabled() const { return false; }
    bool isJavaEnabled() const;
    bool isJavaEnabled(const QString &host) const { return isJavaEnabled(); }
    bool isPluginsEnabled() const;
    bool isPluginsEnabled(const QString &host) const { return isPluginsEnabled(); }
    
    const QString &userStyleSheet();

    void setStdFontName(const QString &);
    void setFixedFontName(const QString &);
    void setSerifFontName(const QString &);
    void setSansSerifFontName(const QString &);
    void setCursiveFontName(const QString &);
    void setFantasyFontName(const QString &);
    
    void setMinFontSize(int);
    void setMediumFontSize(int);
    void setMediumFixedFontSize(int);
    
    void setAutoLoadImages(bool);
    void setIsJavaScriptEnabled(bool);
    void setIsJavaEnabled(bool);
    void setArePluginsEnabled(bool);
    void setJavaScriptCanOpenWindowsAutomatically(bool);

    void setUserStyleSheet(const QString &);

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
    int _defaultFontSize;
    int _defaultFixedFontSize;
    uint _JavaEnabled:1;
    uint _willLoadImagesAutomatically:1;
    uint _pluginsEnabled:1;
    uint _JavaScriptEnabled:1;
    uint _JavaScriptCanOpenWindowsAutomatically:1;
    
};

#endif
