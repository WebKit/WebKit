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

// FIXME: remove this hack
static const QString *DEFAULT_ENCODING = NULL;

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

QFont::CharSet KHTMLSettings::charset() const
{
    return m_charSet;
}


void KHTMLSettings::setCharset( QFont::CharSet c )
{
    m_charSet = c;
}


const QString &KHTMLSettings::encoding() const
{
    _logNotYetImplemented();
    // FIXME: remove this hack
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


static QString *_availableFamiles = 0;
QString KHTMLSettings::availableFamilies()
{
    if (_availableFamiles == 0)
        _availableFamiles = new QString(NSSTRING_TO_QSTRING([[[NSFontManager sharedFontManager]
				availableFontFamilies] componentsJoinedByString:@","]));
    return *_availableFamiles;
}


QFont::CharSet KHTMLSettings::script() const
{
    return m_charSet;
}


void KHTMLSettings::setScript(QFont::CharSet c)
{
    m_charSet = c;
}


const QValueList<int> &KHTMLSettings::fontSizes() const
{
    unsigned int i;
    NSArray *fontSizeArray;

    // fetch sizes from defaults, since they might have changed. This may turn out to
    // be a performance problem, in which case we'll need to add API for refetching
    // the sizes, which would be called when we reapply styles.
    m_fontSizes.clear();
    fontSizeArray = [[NSUserDefaults standardUserDefaults] arrayForKey:@"WebKitFontSizes"];
    for(i=0; i<[fontSizeArray count]; i++){
        m_fontSizes << [[fontSizeArray objectAtIndex:i] intValue];
    }

    return m_fontSizes;
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




