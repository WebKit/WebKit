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

#import "KWQKHTMLSettings.h"

const QString &KHTMLSettings::stdFontName() const
{
    return _stdFontName;
}

const QString &KHTMLSettings::fixedFontName() const
{
    return _fixedFontName;
}

const QString &KHTMLSettings::serifFontName() const
{
    return _fixedFontName;
}

const QString &KHTMLSettings::sansSerifFontName() const
{
    return _sansSerifFontName;
}

const QString &KHTMLSettings::cursiveFontName() const
{
    return _cursiveFontName;
}

const QString &KHTMLSettings::fantasyFontName() const
{
    return _fantasyFontName;
}

const QString &KHTMLSettings::encoding()
{
    static QString latin1("latin1");
    return latin1;
}

int KHTMLSettings::minFontSize() const
{
    return _minimumFontSize;
}

int KHTMLSettings::mediumFontSize() const
{
    return _defaultFontSize;
}

int KHTMLSettings::mediumFixedFontSize() const
{
    return _defaultFixedFontSize;
}

bool KHTMLSettings::isJavaEnabled() const
{
    return _JavaEnabled;
}

bool KHTMLSettings::autoLoadImages() const
{
    return _willLoadImagesAutomatically;
}

bool KHTMLSettings::isPluginsEnabled() const
{
    return _pluginsEnabled;
}

bool KHTMLSettings::isJavaScriptEnabled() const
{
    return _JavaScriptEnabled;
}

bool KHTMLSettings::JavaScriptCanOpenWindowsAutomatically() const
{
    return _JavaScriptCanOpenWindowsAutomatically;
}

const QString &KHTMLSettings::userStyleSheet()
{
    return _userStyleSheetLocation;
}

void KHTMLSettings::setStdFontName(const QString &n)
{
    _stdFontName = n;
}

void KHTMLSettings::setFixedFontName(const QString &n)
{
    _fixedFontName = n;
}

void KHTMLSettings::setSerifFontName(const QString &n)
{
    _serifFontName = n;
}

void KHTMLSettings::setSansSerifFontName(const QString &n)
{
    _sansSerifFontName = n;
}

void KHTMLSettings::setCursiveFontName(const QString &n)
{
    _cursiveFontName = n;
}

void KHTMLSettings::setFantasyFontName(const QString &n)
{
    _fantasyFontName = n;
}

void KHTMLSettings::setMinFontSize(int i)
{
    _minimumFontSize = i;
}

void KHTMLSettings::setMediumFontSize(int i)
{
    _defaultFontSize = i;
}

void KHTMLSettings::setMediumFixedFontSize(int i)
{
    _defaultFixedFontSize = i;
}

void KHTMLSettings::setAutoLoadImages(bool b)
{
    _willLoadImagesAutomatically = b;
}

void KHTMLSettings::setIsJavaScriptEnabled(bool b)
{
    _JavaScriptEnabled = b;
}

void KHTMLSettings::setIsJavaEnabled(bool b)
{
    _JavaEnabled = b;
}

void KHTMLSettings::setArePluginsEnabled(bool b)
{
    _pluginsEnabled = b;
}

void KHTMLSettings::setJavaScriptCanOpenWindowsAutomatically(bool b)
{
    _JavaScriptCanOpenWindowsAutomatically = b;
}

void KHTMLSettings::setUserStyleSheet(const QString &n)
{
    _userStyleSheetLocation = n;
}
