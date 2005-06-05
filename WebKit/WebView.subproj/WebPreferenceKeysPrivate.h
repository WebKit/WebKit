/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// These are private because callers should be using the cover methods. They are in
// a Private (as opposed to Internal) header file because Safari uses some of them
// for managed preferences.
#define WebKitLogLevelPreferenceKey @"WebKitLogLevel"
#define WebKitStandardFontPreferenceKey @"WebKitStandardFont"
#define WebKitFixedFontPreferenceKey @"WebKitFixedFont"
#define WebKitSerifFontPreferenceKey @"WebKitSerifFont"
#define WebKitSansSerifFontPreferenceKey @"WebKitSansSerifFont"
#define WebKitCursiveFontPreferenceKey @"WebKitCursiveFont"
#define WebKitFantasyFontPreferenceKey @"WebKitFantasyFont"
#define WebKitMinimumFontSizePreferenceKey @"WebKitMinimumFontSize"
#define WebKitMinimumLogicalFontSizePreferenceKey @"WebKitMinimumLogicalFontSize"
#define WebKitDefaultFontSizePreferenceKey @"WebKitDefaultFontSize"
#define WebKitDefaultFixedFontSizePreferenceKey @"WebKitDefaultFixedFontSize"
#define WebKitDefaultTextEncodingNamePreferenceKey @"WebKitDefaultTextEncodingName"
#define WebKitUserStyleSheetEnabledPreferenceKey @"WebKitUserStyleSheetEnabledPreferenceKey"
#define WebKitUserStyleSheetLocationPreferenceKey @"WebKitUserStyleSheetLocationPreferenceKey"
#define WebKitShouldPrintBackgroundsPreferenceKey @"WebKitShouldPrintBackgroundsPreferenceKey"
#define WebKitTextAreasAreResizablePreferenceKey @"WebKitTextAreasAreResizable"
#define WebKitJavaEnabledPreferenceKey @"WebKitJavaEnabled"
#define WebKitJavaScriptEnabledPreferenceKey @"WebKitJavaScriptEnabled"
#define WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey @"WebKitJavaScriptCanOpenWindowsAutomatically"
#define WebKitPluginsEnabledPreferenceKey @"WebKitPluginsEnabled"
#define WebKitAllowAnimatedImagesPreferenceKey @"WebKitAllowAnimatedImagesPreferenceKey"
#define WebKitAllowAnimatedImageLoopingPreferenceKey @"WebKitAllowAnimatedImageLoopingPreferenceKey"
#define WebKitDisplayImagesKey @"WebKitDisplayImagesKey"
#define WebKitPageCacheSizePreferenceKey @"WebKitPageCacheSizePreferenceKey"
#define WebKitObjectCacheSizePreferenceKey @"WebKitObjectCacheSizePreferenceKey"
#define WebKitBackForwardCacheExpirationIntervalKey @"WebKitBackForwardCacheExpirationIntervalKey"
#define WebKitTabToLinksPreferenceKey @"WebKitTabToLinksPreferenceKey"
#define WebKitPrivateBrowsingEnabledPreferenceKey @"WebKitPrivateBrowsingEnabled"

// These are private both because callers should be using the cover methods and because the
// cover methods themselves are private.
#define WebKitRespectStandardStyleKeyEquivalentsPreferenceKey @"WebKitRespectStandardStyleKeyEquivalents"
#define WebKitShowsURLsInToolTipsPreferenceKey @"WebKitShowsURLsInToolTips"
