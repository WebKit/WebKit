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

static int minimumFontSize;
static int defaultFontSize;
static int defaultFixedFontSize;
static bool JavaEnabled;
static bool willLoadImagesAutomatically;
static bool pluginsEnabled;
static bool JavaScriptEnabled;

const QString &KHTMLSettings::stdFontName()
{
    static QString name;
    return name;
}

const QString &KHTMLSettings::fixedFontName()
{
    static QString name;
    return name;
}

const QString &KHTMLSettings::serifFontName()
{
    static QString name;
    return name;
}

const QString &KHTMLSettings::sansSerifFontName()
{
    static QString name;
    return name;
}

const QString &KHTMLSettings::cursiveFontName()
{
    static QString name;
    return name;
}

const QString &KHTMLSettings::fantasyFontName()
{
    static QString name;
    return name;
}

const QString &KHTMLSettings::encoding()
{
    static QString latin1("latin1");
    return latin1;
}

int KHTMLSettings::minFontSize()
{
    return minimumFontSize;
}

int KHTMLSettings::mediumFontSize()
{
    return defaultFontSize;
}

int KHTMLSettings::mediumFixedFontSize()
{
    return defaultFixedFontSize;
}

bool KHTMLSettings::isJavaEnabled()
{
    return JavaEnabled;
}

bool KHTMLSettings::autoLoadImages()
{
    return willLoadImagesAutomatically;
}

bool KHTMLSettings::isPluginsEnabled()
{
    return pluginsEnabled;
}

bool KHTMLSettings::isJavaScriptEnabled()
{
    return JavaScriptEnabled;
}

const QString &KHTMLSettings::userStyleSheet()
{
    static QString location;
    return location;
}

void KHTMLSettings::setStdFontName(const QString &n)
{
    const_cast<QString &>(stdFontName()) = n;
}

void KHTMLSettings::setFixedFontName(const QString &n)
{
    const_cast<QString &>(fixedFontName()) = n;
}

void KHTMLSettings::setSerifFontName(const QString &n)
{
    const_cast<QString &>(serifFontName()) = n;
}

void KHTMLSettings::setSansSerifFontName(const QString &n)
{
    const_cast<QString &>(sansSerifFontName()) = n;
}

void KHTMLSettings::setCursiveFontName(const QString &n)
{
    const_cast<QString &>(cursiveFontName()) = n;
}

void KHTMLSettings::setFantasyFontName(const QString &n)
{
    const_cast<QString &>(fantasyFontName()) = n;
}

void KHTMLSettings::setMinFontSize(int i)
{
    minimumFontSize = i;
}

void KHTMLSettings::setMediumFontSize(int i)
{
    defaultFontSize = i;
}

void KHTMLSettings::setMediumFixedFontSize(int i)
{
    defaultFixedFontSize = i;
}

void KHTMLSettings::setAutoLoadImages(bool b)
{
    willLoadImagesAutomatically = b;
}

void KHTMLSettings::setIsJavaScriptEnabled(bool b)
{
    JavaScriptEnabled = b;
}

void KHTMLSettings::setIsJavaEnabled(bool b)
{
    JavaEnabled = b;
}

void KHTMLSettings::setArePluginsEnabled(bool b)
{
    pluginsEnabled = b;
}

void KHTMLSettings::setUserStyleSheet(const QString &n)
{
    const_cast<QString &>(userStyleSheet()) = n;
}
