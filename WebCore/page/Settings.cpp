/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "Settings.h"

#include "Page.h"

namespace WebCore {

Settings::Settings()
    : m_editableLinkBehavior(EditableLinkDefaultBehavior)
    , m_minimumFontSize(0)
    , m_minimumLogicalFontSize(0)
    , m_defaultFontSize(0)
    , m_defaultFixedFontSize(0)
    , m_isJavaEnabled(false)
    , m_loadsImagesAutomatically(false)
    , m_privateBrowsingEnabled(false)
    , m_arePluginsEnabled(false)
    , m_isJavaScriptEnabled(false)
    , m_javaScriptCanOpenWindowsAutomatically(false)
    , m_shouldPrintBackgrounds(false)
    , m_textAreasAreResizable(false)
    , m_usesDashboardBackwardCompatibilityMode(false)
    , m_needsAcrobatFrameReloadingQuirk(false)
{
    // A Frame may not have been created yet, so we initialize the AtomicString 
    // hash before trying to use it.
    AtomicString::init();
}

void Settings::setStandardFontFamily(const AtomicString& standardFontFamily)
{
    if (standardFontFamily == m_standardFontFamily)
        return;

    m_standardFontFamily = standardFontFamily;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setFixedFontFamily(const AtomicString& fixedFontFamily)
{
    if (m_fixedFontFamily == fixedFontFamily)
        return;
        
    m_fixedFontFamily = fixedFontFamily;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setSerifFontFamily(const AtomicString& serifFontFamily)
{
    if (m_serifFontFamily == serifFontFamily)
        return;
        
    m_serifFontFamily = serifFontFamily;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setSansSerifFontFamily(const AtomicString& sansSerifFontFamily)
{
    if (m_sansSerifFontFamily == sansSerifFontFamily)
        return;
        
    m_sansSerifFontFamily = sansSerifFontFamily; 
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setCursiveFontFamily(const AtomicString& cursiveFontFamily)
{
    if (m_cursiveFontFamily == cursiveFontFamily)
        return;
        
    m_cursiveFontFamily = cursiveFontFamily;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setFantasyFontFamily(const AtomicString& fantasyFontFamily)
{
    if (m_fantasyFontFamily == fantasyFontFamily)
        return;
        
    m_fantasyFontFamily = fantasyFontFamily;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setMinimumFontSize(int minimumFontSize)
{
    if (m_minimumFontSize == minimumFontSize)
        return;

    m_minimumFontSize = minimumFontSize;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setMinimumLogicalFontSize(int minimumLogicalFontSize)
{
    if (m_minimumLogicalFontSize == minimumLogicalFontSize)
        return;

    m_minimumLogicalFontSize = minimumLogicalFontSize;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setDefaultFontSize(int defaultFontSize)
{
    if (m_defaultFontSize == defaultFontSize)
        return;

    m_defaultFontSize = defaultFontSize;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setDefaultFixedFontSize(int defaultFontSize)
{
    if (m_defaultFontSize == defaultFontSize)
        return;

    m_defaultFixedFontSize = defaultFontSize;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_loadsImagesAutomatically = loadsImagesAutomatically;
}

void Settings::setJavaScriptEnabled(bool isJavaScriptEnabled)
{
    m_isJavaScriptEnabled = isJavaScriptEnabled;
}

void Settings::setJavaEnabled(bool isJavaEnabled)
{
    m_isJavaEnabled = isJavaEnabled;
}

void Settings::setPluginsEnabled(bool arePluginsEnabled)
{
    m_arePluginsEnabled = arePluginsEnabled;
}

void Settings::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    m_privateBrowsingEnabled = privateBrowsingEnabled;
}

void Settings::setJavaScriptCanOpenWindowsAutomatically(bool javaScriptCanOpenWindowsAutomatically)
{
    m_javaScriptCanOpenWindowsAutomatically = javaScriptCanOpenWindowsAutomatically;
}

void Settings::setDefaultTextEncodingName(const String& defaultTextEncodingName)
{
    m_defaultTextEncodingName = defaultTextEncodingName;
}

void Settings::setUserStyleSheetLocation(const KURL& userStyleSheetLocation)
{
    if (m_userStyleSheetLocation == userStyleSheetLocation)
        return;

    m_userStyleSheetLocation = userStyleSheetLocation;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setShouldPrintBackgrounds(bool shouldPrintBackgrounds)
{
    m_shouldPrintBackgrounds = shouldPrintBackgrounds;
}

void Settings::setTextAreasAreResizable(bool textAreasAreResizable)
{
    if (m_textAreasAreResizable == textAreasAreResizable)
        return;

    m_textAreasAreResizable = textAreasAreResizable;
    Page::setNeedsReapplyStylesForSettingsChange(this);
}

void Settings::setEditableLinkBehavior(EditableLinkBehavior editableLinkBehavior)
{
    m_editableLinkBehavior = editableLinkBehavior;
}

void Settings::setUsesDashboardBackwardCompatibilityMode(bool usesDashboardBackwardCompatibilityMode)
{
    m_usesDashboardBackwardCompatibilityMode = usesDashboardBackwardCompatibilityMode;
}


// FIXME: This quirk is needed because of Radar 4674537. We need to phase it out once Adobe
// can fix the bug from their end.
void Settings::setNeedsAcrobatFrameReloadingQuirk(bool shouldNotReloadIFramesForUnchangedSRC)
{
    m_needsAcrobatFrameReloadingQuirk = shouldNotReloadIFramesForUnchangedSRC;
}

} // namespace WebCore
