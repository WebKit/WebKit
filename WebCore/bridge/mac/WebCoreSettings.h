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

#import <Foundation/Foundation.h>

#ifdef __cplusplus
class KHTMLSettings;
#else
@class KHTMLSettings;
#endif

@interface WebCoreSettings : NSObject
{
    NSString *standardFontFamily;
    NSString *fixedFontFamily;
    NSString *serifFontFamily;
    NSString *sansSerifFontFamily;
    NSString *cursiveFontFamily;
    NSString *fantasyFontFamily;
    float minimumFontSize;
    float minimumLogicalFontSize;
    float defaultFontSize;
    float defaultFixedFontSize;
    BOOL JavaEnabled;
    BOOL pluginsEnabled;
    BOOL JavaScriptEnabled;
    BOOL JavaScriptCanOpenWindowsAutomatically;
    BOOL willLoadImagesAutomatically;
    BOOL shouldPrintBackgrounds;
    BOOL textAreasAreResizable;
    NSString *userStyleSheetLocation;
    NSString *defaultTextEncoding;
    
    KHTMLSettings *settings;
}

- (void)setStandardFontFamily:(NSString *)family;
- (NSString *)standardFontFamily;

- (void)setFixedFontFamily:(NSString *)family;
- (NSString *)fixedFontFamily;

- (void)setSerifFontFamily:(NSString *)family;
- (NSString *)serifFontFamily;

- (void)setSansSerifFontFamily:(NSString *)family;
- (NSString *)sansSerifFontFamily;

- (void)setCursiveFontFamily:(NSString *)family;
- (NSString *)cursiveFontFamily;

- (void)setFantasyFontFamily:(NSString *)family;
- (NSString *)fantasyFontFamily;

- (void)setMinimumFontSize:(float)size;
- (float)minimumFontSize;

- (void)setMinimumLogicalFontSize:(float)size;
- (float)minimumLogicalFontSize;

- (void)setDefaultFontSize:(float)size;
- (float)defaultFontSize;

- (void)setDefaultFixedFontSize:(float)size;
- (float)defaultFixedFontSize;

- (void)setJavaEnabled:(BOOL)enabled;
- (BOOL)JavaEnabled;

- (void)setPluginsEnabled:(BOOL)enabled;
- (BOOL)pluginsEnabled;

- (void)setJavaScriptEnabled:(BOOL)enabled;
- (BOOL)JavaScriptEnabled;

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)enabled;
- (BOOL)JavaScriptCanOpenWindowsAutomatically;

- (void)setWillLoadImagesAutomatically:(BOOL)load;
- (BOOL)willLoadImagesAutomatically;

- (void)setUserStyleSheetLocation:(NSString *)location;
- (NSString *)userStyleSheetLocation;

- (void)setShouldPrintBackgrounds:(BOOL)enabled;
- (BOOL)shouldPrintBackgrounds;

- (void)setTextAreasAreResizable:(BOOL)resizable;
- (BOOL)textAreasAreResizable;

- (void)setDefaultTextEncoding:(NSString *)encoding;
- (NSString *)defaultTextEncoding;

- (KHTMLSettings *)settings;

@end
