/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
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

#import "WebCoreSettings.h"

@implementation WebCoreSettings

+ (WebCoreSettings *)sharedSettings
{
    static WebCoreSettings *shared;
    if (!shared) {
        shared = [[WebCoreSettings alloc] init];
    }
    return shared;
}

- (void)dealloc
{
    [standardFontFamily release];
    [fixedFontFamily release];
    [serifFontFamily release];
    [sansSerifFontFamily release];
    [cursiveFontFamily release];
    [fantasyFontFamily release];

    [super dealloc];
}

- (void)_updateAllViews
{
}

- (void)setStandardFontFamily:(NSString *)s
{
    if ([standardFontFamily isEqualToString:s]) {
        return;
    }
    [standardFontFamily release];
    standardFontFamily = [s copy];
    [self _updateAllViews];
}

- (NSString *)standardFontFamily
{
    return standardFontFamily;
}

- (void)setFixedFontFamily:(NSString *)s
{
    if ([fixedFontFamily isEqualToString:s]) {
        return;
    }
    [fixedFontFamily release];
    fixedFontFamily = [s copy];
    [self _updateAllViews];
}

- (NSString *)fixedFontFamily
{
    return fixedFontFamily;
}

- (void)setSerifFontFamily:(NSString *)s
{
    if ([serifFontFamily isEqualToString:s]) {
        return;
    }
    [serifFontFamily release];
    serifFontFamily = [s copy];
    [self _updateAllViews];
}

- (NSString *)serifFontFamily
{
    return serifFontFamily;
}

- (void)setSansSerifFontFamily:(NSString *)s
{
    if ([sansSerifFontFamily isEqualToString:s]) {
        return;
    }
    [sansSerifFontFamily release];
    sansSerifFontFamily = [s copy];
    [self _updateAllViews];
}

- (NSString *)sansSerifFontFamily
{
    return sansSerifFontFamily;
}

- (void)setCursiveFontFamily:(NSString *)s
{
    if ([cursiveFontFamily isEqualToString:s]) {
        return;
    }
    [cursiveFontFamily release];
    cursiveFontFamily = [s copy];
    [self _updateAllViews];
}

- (NSString *)cursiveFontFamily
{
    return cursiveFontFamily;
}

- (void)setFantasyFontFamily:(NSString *)s
{
    if ([fantasyFontFamily isEqualToString:s]) {
        return;
    }
    [fantasyFontFamily release];
    fantasyFontFamily = [s copy];
    [self _updateAllViews];
}

- (NSString *)fantasyFontFamily
{
    return fantasyFontFamily;
}

- (void)setMinimumFontSize:(float)size
{
    if (minimumFontSize == size) {
        return;
    }
    minimumFontSize = size;
    [self _updateAllViews];
}

- (float)minimumFontSize
{
    return minimumFontSize;
}

- (void)setDefaultFontSize:(float)size
{
    if (defaultFontSize == size) {
        return;
    }
    defaultFontSize = size;
    [self _updateAllViews];
}

- (float)defaultFontSize
{
    return defaultFontSize;
}

- (void)setDefaultFixedFontSize:(float)size
{
    if (defaultFixedFontSize == size) {
        return;
    }
    defaultFixedFontSize = size;
    [self _updateAllViews];
}

- (float)defaultFixedFontSize
{
    return defaultFixedFontSize;
}

- (void)setJavaEnabled:(BOOL)enabled
{
    JavaEnabled = enabled;
}

- (BOOL)JavaEnabled
{
    return JavaEnabled;
}

- (void)setPluginsEnabled:(BOOL)enabled
{
    pluginsEnabled = enabled;
}

- (BOOL)pluginsEnabled
{
    return pluginsEnabled;
}

- (void)setJavaScriptEnabled:(BOOL)enabled
{
    JavaScriptEnabled = enabled;
}

- (BOOL)JavaScriptEnabled
{
    return JavaScriptEnabled;
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)enabled
{
    JavaScriptCanOpenWindowsAutomatically = enabled;
}

- (BOOL)JavaScriptCanOpenWindowsAutomatically
{
    return JavaScriptCanOpenWindowsAutomatically;
}

- (void)setWillLoadImagesAutomatically:(BOOL)load
{
    willLoadImagesAutomatically = load;
}

- (BOOL)willLoadImagesAutomatically
{
    return willLoadImagesAutomatically;
}

- (void)setUserStyleSheetLocation:(NSString *)s
{
    if ([userStyleSheetLocation isEqualToString:s]) {
        return;
    }
    [userStyleSheetLocation release];
    userStyleSheetLocation = [s copy];
    [self _updateAllViews];
}

- (NSString *)userStyleSheetLocation
{
    return userStyleSheetLocation;
}

@end
