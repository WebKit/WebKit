/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import "KWQKHTMLSettings.h"

#import "WebCoreSettings.h"

QString KHTMLSettings::stdFontName() const
{
    return QString::fromNSString([[WebCoreSettings sharedSettings] standardFontFamily]);
}

QString KHTMLSettings::fixedFontName() const
{
    return QString::fromNSString([[WebCoreSettings sharedSettings] fixedFontFamily]);
}

QString KHTMLSettings::serifFontName() const
{
    return QString::fromNSString([[WebCoreSettings sharedSettings] serifFontFamily]);
}

QString KHTMLSettings::sansSerifFontName() const
{
    return QString::fromNSString([[WebCoreSettings sharedSettings] sansSerifFontFamily]);
}

QString KHTMLSettings::cursiveFontName() const
{
    return QString::fromNSString([[WebCoreSettings sharedSettings] cursiveFontFamily]);
}

QString KHTMLSettings::fantasyFontName() const
{
    return QString::fromNSString([[WebCoreSettings sharedSettings] fantasyFontFamily]);
}

QString KHTMLSettings::settingsToCSS() const
{
    return QString::null;
}

const QString &KHTMLSettings::encoding() const
{
    static QString latin1("latin1");
    return latin1;
}

int KHTMLSettings::minFontSize() const
{
    return (int)rint([[WebCoreSettings sharedSettings] minimumFontSize]);
}

int KHTMLSettings::mediumFontSize() const
{
    return (int)rint([[WebCoreSettings sharedSettings] defaultFontSize]);
}

int KHTMLSettings::mediumFixedFontSize() const
{
    return (int)rint([[WebCoreSettings sharedSettings] defaultFixedFontSize]);
}

bool KHTMLSettings::changeCursor() const
{
    return true;
}

bool KHTMLSettings::isFormCompletionEnabled() const
{
    return false;
}

KHTMLSettings::KAnimationAdvice KHTMLSettings::showAnimations() const
{
    return KAnimationEnabled;
}

int KHTMLSettings::maxFormCompletionItems() const
{
    return 0;
}

bool KHTMLSettings::isJavaEnabled(QString const&) const
{
    return isJavaEnabled();
}

bool KHTMLSettings::isJavaEnabled() const
{
    return [[WebCoreSettings sharedSettings] JavaEnabled];
}

bool KHTMLSettings::autoLoadImages() const
{
    return [[WebCoreSettings sharedSettings] willLoadImagesAutomatically];
}

bool KHTMLSettings::isPluginsEnabled(QString const&) const
{
    return isPluginsEnabled();
}

bool KHTMLSettings::isPluginsEnabled() const
{
    return [[WebCoreSettings sharedSettings] pluginsEnabled];
}

bool KHTMLSettings::isJavaScriptEnabled(QString const&) const
{
    return isJavaScriptEnabled();
}

bool KHTMLSettings::isJavaScriptEnabled() const
{
    return [[WebCoreSettings sharedSettings] JavaScriptEnabled];
}

bool KHTMLSettings::isJavaScriptDebugEnabled() const
{
    return false;
}

QString KHTMLSettings::userStyleSheet() const
{
    return QString::fromNSString([[WebCoreSettings sharedSettings] userStyleSheetLocation]);
}
