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
    
    static void init() { }

    // Font settings
    static const QString &stdFontName();
    static const QString &fixedFontName();
    static const QString &serifFontName();
    static const QString &sansSerifFontName();
    static const QString &cursiveFontName();
    static const QString &fantasyFontName();
    
    static const QString &settingsToCSS() { return QString::null; }

    static const QString &encoding();

    static int minFontSize();
    static int mediumFontSize();
    static int mediumFixedFontSize();

    static bool changeCursor() { return true; }

    static bool isFormCompletionEnabled() { return false; }
    static int maxFormCompletionItems() { return 0; }

    static bool autoLoadImages();
    static KAnimationAdvice showAnimations() { return KAnimationEnabled; }

    static bool isJavaScriptEnabled();
    static bool isJavaScriptEnabled(const QString &host) { return isJavaScriptEnabled(); }
    static bool isJavaScriptDebugEnabled() { return false; }
    static bool isJavaEnabled();
    static bool isJavaEnabled(const QString &host) { return isJavaEnabled(); }
    static bool isPluginsEnabled();
    static bool isPluginsEnabled(const QString &host) { return isPluginsEnabled(); }
    
    static const QString &userStyleSheet();

    static void setStdFontName(const QString &);
    static void setFixedFontName(const QString &);
    static void setSerifFontName(const QString &);
    static void setSansSerifFontName(const QString &);
    static void setCursiveFontName(const QString &);
    static void setFantasyFontName(const QString &);
    
    static void setMinFontSize(int);
    static void setMediumFontSize(int);
    static void setMediumFixedFontSize(int);
    
    static void setAutoLoadImages(bool);
    static void setIsJavaScriptEnabled(bool);
    static void setIsJavaEnabled(bool);
    static void setArePluginsEnabled(bool);

    static void setUserStyleSheet(const QString &);
};

#endif
