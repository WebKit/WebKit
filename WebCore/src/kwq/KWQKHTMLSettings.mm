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
    m_fontSizes.clear();
    m_fontSizes << 6;
    m_fontSizes << 8;
    m_fontSizes << 10;
    m_fontSizes << 12;
    m_fontSizes << 14;
    m_fontSizes << 16;
    m_fontSizes << 18;
    m_fontSizes << 20;
    
    // set available font families...ask the system
    
    NSFontManager *sharedFontManager;
    NSArray *array;
    
    sharedFontManager = [NSFontManager sharedFontManager];
    array = [sharedFontManager availableFontFamilies];
    m_fontFamilies = NSSTRING_TO_QSTRING([array componentsJoinedByString:@","]);
}

QString KHTMLSettings::stdFontName() const
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return NSSTRING_TO_QSTRING([defaults objectForKey:@"stdFontName"]);
}


QString KHTMLSettings::fixedFontName() const
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return NSSTRING_TO_QSTRING([defaults objectForKey:@"fixedFontName"]);
}


QString KHTMLSettings::serifFontName() const
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return NSSTRING_TO_QSTRING([defaults objectForKey:@"serifFontName"]);
}


QString KHTMLSettings::sansSerifFontName() const
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return NSSTRING_TO_QSTRING([defaults objectForKey:@"sansSerifFontName"]);
}


QString KHTMLSettings::cursiveFontName() const
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return NSSTRING_TO_QSTRING([defaults objectForKey:@"cursiveFontName"]);
}


QString KHTMLSettings::fantasyFontName() const
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return NSSTRING_TO_QSTRING([defaults objectForKey:@"fantasyFontName"]);
}


QString KHTMLSettings::settingsToCSS() const
{
    _logNotYetImplemented();
    return "";
}

QFont::CharSet KHTMLSettings::charset() const
{
    _logNotYetImplemented();
}


void KHTMLSettings::setCharset( QFont::CharSet c )
{
    _logNotYetImplemented();
}


const QString &KHTMLSettings::encoding() const
{
    _logNotYetImplemented();
}


int KHTMLSettings::minFontSize() const
{
    return 6;
}


QString KHTMLSettings::availableFamilies() const
{
    return m_fontFamilies;
}


QFont::CharSet KHTMLSettings::script() const
{
    _logNotYetImplemented();
}


void KHTMLSettings::setScript( QFont::CharSet c )
{
    _logNotYetImplemented();
}


const QValueList<int> &KHTMLSettings::fontSizes() const
{
    return m_fontSizes;
}


bool KHTMLSettings::changeCursor()
{
    _logNotYetImplemented();
}


bool KHTMLSettings::isFormCompletionEnabled() const
{
    _logNotYetImplemented();
}


int KHTMLSettings::maxFormCompletionItems() const
{
    _logNotYetImplemented();
}




