/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
#include <kwqdebug.h>
#include <khtml_settings.h>

KHTMLSettings::KHTMLSettings()
{    
    m_charSet = QFont::Latin1;
}

QString KHTMLSettings::stdFontName() const
{
    return NSSTRING_TO_QSTRING([[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitStandardFont"]);
}


QString KHTMLSettings::fixedFontName() const
{
    return NSSTRING_TO_QSTRING([[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitFixedFont"]);
}


QString KHTMLSettings::serifFontName() const
{
    return NSSTRING_TO_QSTRING([[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitSerifFont"]);
}


QString KHTMLSettings::sansSerifFontName() const
{
    return NSSTRING_TO_QSTRING([[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitSansSerifFont"]);
}


QString KHTMLSettings::cursiveFontName() const
{
    return NSSTRING_TO_QSTRING([[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitCursiveFont"]);
}


QString KHTMLSettings::fantasyFontName() const
{
    return NSSTRING_TO_QSTRING([[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitFantasyFont"]);
}


QString KHTMLSettings::settingsToCSS() const
{
    _logNotYetImplemented();
    return QString();
}

const QString &KHTMLSettings::encoding() const
{
    // FIXME: remove this hack
    static const QString *DEFAULT_ENCODING = NULL;
    _logNotYetImplemented();
    if (DEFAULT_ENCODING == NULL) {
        DEFAULT_ENCODING = new QString(NSSTRING_TO_QSTRING(@"NSISOLatin1StringEncoding"));
    }
    return *DEFAULT_ENCODING;
}

int KHTMLSettings::minFontSize() const
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitMinimumFontSize"];
}

int KHTMLSettings::mediumFontSize() const
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitMediumFontSize"];
}

QFont::CharSet KHTMLSettings::script() const
{
    return m_charSet;
}

void KHTMLSettings::setScript(QFont::CharSet c)
{
    m_charSet = c;
}

bool KHTMLSettings::changeCursor() const
{
    _logNotYetImplemented();
    return FALSE;
}

bool KHTMLSettings::isFormCompletionEnabled() const
{
    _logNotYetImplemented();
    return FALSE;
}

int KHTMLSettings::maxFormCompletionItems() const
{
    _logNotYetImplemented();
    return 0;
}

bool KHTMLSettings::isJavaEnabled(QString const&) const
{
    return isJavaEnabled();
}

bool KHTMLSettings::isJavaEnabled() const
{
    return [[[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitJavaEnabled"] boolValue];
}

bool KHTMLSettings::autoLoadImages() const
{
    _logNotYetImplemented();
    return true;
}

bool KHTMLSettings::isPluginsEnabled(QString const&) const
{
    return isPluginsEnabled();
}

bool KHTMLSettings::isPluginsEnabled() const
{
    return [[[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitPluginsEnabled"] boolValue];
}

bool KHTMLSettings::isJavaScriptEnabled(QString const&) const
{
    return isJavaScriptEnabled();
}

bool KHTMLSettings::isJavaScriptEnabled() const
{
    return [[[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitJScriptEnabled"] boolValue];
}

bool KHTMLSettings::isJavaScriptDebugEnabled() const
{
    return false;
}
