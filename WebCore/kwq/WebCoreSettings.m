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

- (void)setStandardFontFamily:(NSString *)s
{
    NSString *c = [s copy];
    [standardFontFamily release];
    standardFontFamily = c;
}

- (NSString *)standardFontFamily
{
    return standardFontFamily;
}

- (void)setFixedFontFamily:(NSString *)s
{
    NSString *c = [s copy];
    [fixedFontFamily release];
    fixedFontFamily = c;
}

- (NSString *)fixedFontFamily
{
    return fixedFontFamily;
}

- (void)setSerifFontFamily:(NSString *)s
{
    NSString *c = [s copy];
    [serifFontFamily release];
    serifFontFamily = c;
}

- (NSString *)serifFontFamily
{
    return serifFontFamily;
}

- (void)setSansSerifFontFamily:(NSString *)s
{
    NSString *c = [s copy];
    [sansSerifFontFamily release];
    sansSerifFontFamily = c;
}

- (NSString *)sansSerifFontFamily
{
    return sansSerifFontFamily;
}

- (void)setCursiveFontFamily:(NSString *)s
{
    NSString *c = [s copy];
    [cursiveFontFamily release];
    cursiveFontFamily = c;
}

- (NSString *)cursiveFontFamily
{
    return cursiveFontFamily;
}

- (void)setFantasyFontFamily:(NSString *)s
{
    NSString *c = [s copy];
    [fantasyFontFamily release];
    fantasyFontFamily = c;
}

- (NSString *)fantasyFontFamily
{
    return fantasyFontFamily;
}

- (void)setMinimumFontSize:(float)size
{
    minimumFontSize = size;
}

- (float)minimumFontSize
{
    return minimumFontSize;
}

- (void)setDefaultFontSize:(float)size
{
    defaultFontSize = size;
}

- (float)defaultFontSize
{
    return defaultFontSize;
}

- (void)setFixedFontSize:(float)size
{
  fixedFontSize = size;
}

- (float)fixedFontSize
{
  return fixedFontSize;
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

@end
