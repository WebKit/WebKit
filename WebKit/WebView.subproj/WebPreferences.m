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

#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebPreferenceKeysPrivate.h>

#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSDictionaryExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKitSystemInterface.h>

#import <WebCore/WebCoreSettings.h>

NSString *WebPreferencesChangedNotification = @"WebPreferencesChangedNotification";

#define KEY(x) (_private->identifier ? [_private->identifier stringByAppendingString:(x)] : (x))

enum { WebPreferencesVersion = 1 };

@interface WebPreferencesPrivate : NSObject
{
@public
    NSMutableDictionary *values;
    NSString *identifier;
    NSString *IBCreatorID;
    BOOL autosaves;
}
@end

@implementation WebPreferencesPrivate
- (void)dealloc
{
    [values release];
    [identifier release];
    [IBCreatorID release];
    [super dealloc];
}
@end

@interface WebPreferences (WebInternal)
+ (NSString *)_concatenateKeyWithIBCreatorID:(NSString *)key;
+ (NSString *)_IBCreatorID;
@end

@interface WebPreferences (WebForwardDeclarations)
// This pseudo-category is needed so these methods can be used from within other category implementations
// without being in the public header file.
- (BOOL)_boolValueForKey:(NSString *)key;
- (void)_setBoolValue:(BOOL)value forKey:(NSString *)key;
- (int)_integerValueForKey:(NSString *)key;
- (void)_setIntegerValue:(int)value forKey:(NSString *)key;
- (float)_floatValueForKey:(NSString *)key;
- (void)_setFloatValue:(float)value forKey:(NSString *)key;
@end

@implementation WebPreferences

- init
{
    // Create fake identifier
    static int instanceCount = 1;
    NSString *fakeIdentifier;
    
    // At least ensure that identifier hasn't been already used.  
    fakeIdentifier = [NSString stringWithFormat:@"WebPreferences%d", instanceCount++];
    while ([[self class] _getInstanceForIdentifier:fakeIdentifier]){
        fakeIdentifier = [NSString stringWithFormat:@"WebPreferences%d", instanceCount++];
    }
    
    return [self initWithIdentifier:fakeIdentifier];
}

static WebPreferences *_standardPreferences = nil;

- (id)initWithIdentifier:(NSString *)anIdentifier
{
    self = [super init];
    if (!self)
        return nil;

    _private = [[WebPreferencesPrivate alloc] init];
    _private->IBCreatorID = [[WebPreferences _IBCreatorID] retain];

    WebPreferences *instance = [[self class] _getInstanceForIdentifier:anIdentifier];
    if (instance){
        [self release];
        return [instance retain];
    }

    _private->values = [[NSMutableDictionary alloc] init];
    _private->identifier = [anIdentifier copy];
    
    [[self class] _setInstance:self forIdentifier:_private->identifier];

    [[NSNotificationCenter defaultCenter]
       postNotificationName:WebPreferencesChangedNotification object:self userInfo:nil];

    return self;
}

- (id)initWithCoder:(NSCoder *)decoder
{
    volatile id result = nil;

NS_DURING

    int version;

    _private = [[WebPreferencesPrivate alloc] init];
    _private->IBCreatorID = [[WebPreferences _IBCreatorID] retain];
    
    if ([decoder allowsKeyedCoding]){
        _private->identifier = [[decoder decodeObjectForKey:@"Identifier"] retain];
        _private->values = [[decoder decodeObjectForKey:@"Values"] retain];
        LOG (Encoding, "Identifier = %@, Values = %@\n", _private->identifier, _private->values);
    }
    else {
        [decoder decodeValueOfObjCType:@encode(int) at:&version];
        if (version == 1){
            _private->identifier = [[decoder decodeObject] retain];
            _private->values = [[decoder decodeObject] retain];
        }
    }
    
    // If we load a nib multiple times, or have instances in multiple
    // nibs with the same name, the first guy up wins.
    WebPreferences *instance = [[self class] _getInstanceForIdentifier:_private->identifier];
    if (instance){
        [self release];
        result = [instance retain];
    }
    else {
        [[self class] _setInstance:self forIdentifier:_private->identifier];
        result = self;
    }
    
NS_HANDLER

    result = nil;
    [self release];
    
NS_ENDHANDLER

    return result;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    if ([encoder allowsKeyedCoding]){
        [encoder encodeObject:_private->identifier forKey:@"Identifier"];
        [encoder encodeObject:_private->values forKey:@"Values"];
        LOG (Encoding, "Identifier = %@, Values = %@\n", _private->identifier, _private->values);
    }
    else {
        int version = WebPreferencesVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:_private->identifier];
        [encoder encodeObject:_private->values];
    }
}

+ (WebPreferences *)standardPreferences
{
    if (_standardPreferences == nil) {
        _standardPreferences = [[WebPreferences alloc] initWithIdentifier:nil];
        [_standardPreferences setAutosaves:YES];
        [_standardPreferences _postPreferencesChangesNotification];
    }

    return _standardPreferences;
}

// if we ever have more than one WebPreferences object, this would move to init
+ (void)initialize
{
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        @"0x0",                         WebKitLogLevelPreferenceKey,
        @"Times",                       WebKitStandardFontPreferenceKey,
        @"Courier",                     WebKitFixedFontPreferenceKey,
        @"Times",                       WebKitSerifFontPreferenceKey,
        @"Helvetica",                   WebKitSansSerifFontPreferenceKey,
        @"Apple Chancery",              WebKitCursiveFontPreferenceKey,
        @"Papyrus",                     WebKitFantasyFontPreferenceKey,
        @"1",                           WebKitMinimumFontSizePreferenceKey,
        @"9",                           WebKitMinimumLogicalFontSizePreferenceKey, 
        @"16",                          WebKitDefaultFontSizePreferenceKey,
        @"13",                          WebKitDefaultFixedFontSizePreferenceKey,
        @"ISO-8859-1",                  WebKitDefaultTextEncodingNamePreferenceKey,
        @"4",                           WebKitPageCacheSizePreferenceKey,
        @"8388608",                     WebKitObjectCacheSizePreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUserStyleSheetEnabledPreferenceKey,
        @"",                            WebKitUserStyleSheetLocationPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShouldPrintBackgroundsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitTextAreasAreResizablePreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitPluginsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImagesPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImageLoopingPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDisplayImagesKey,
        @"1800",                        WebKitBackForwardCacheExpirationIntervalKey,
        [NSNumber numberWithBool:NO],   WebKitTabToLinksPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitPrivateBrowsingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitRespectStandardStyleKeyEquivalentsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowsURLsInToolTipsPreferenceKey,
        @"1",                           WebKitPDFDisplayModePreferenceKey,
        @"1.0",                         WebKitPDFScaleFactorPreferenceKey,
        nil];

    // This value shouldn't ever change, which is assumed in the initialization of WebKitPDFDisplayModePreferenceKey above
    ASSERT(kPDFDisplaySinglePageContinuous == 1);
    [[NSUserDefaults standardUserDefaults] registerDefaults:dict];
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSString *)identifier
{
    return _private->identifier;
}

- (id)_valueForKey:(NSString *)key
{
    NSString *_key = KEY(key);
    id o = [_private->values objectForKey:_key];
    if (o)
        return o;
    o = [[NSUserDefaults standardUserDefaults] objectForKey:_key];
    if (!o && key != _key)
        o = [[NSUserDefaults standardUserDefaults] objectForKey:key];
    return o;
}

- (NSString *)_stringValueForKey:(NSString *)key
{
    id s = [self _valueForKey:key];
    return [s isKindOfClass:[NSString class]] ? (NSString *)s : nil;
}

- (void)_setStringValue:(NSString *)value forKey:(NSString *)key
{
    if ([[self _stringValueForKey:key] isEqualToString:value])
        return;
    NSString *_key = KEY(key);
    [_private->values setObject:value forKey:_key];
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:value forKey:_key];
    [self _postPreferencesChangesNotification];
}

- (int)_integerValueForKey:(NSString *)key
{
    id o = [self _valueForKey:key];
    return [o respondsToSelector:@selector(intValue)] ? [o intValue] : 0;
}

- (void)_setIntegerValue:(int)value forKey:(NSString *)key
{
    if ([self _integerValueForKey:key] == value)
        return;
    NSString *_key = KEY(key);
    [_private->values _webkit_setInt:value forKey:_key];
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setInteger:value forKey:_key];
    [self _postPreferencesChangesNotification];
}

- (float)_floatValueForKey:(NSString *)key
{
    id o = [self _valueForKey:key];
    return [o respondsToSelector:@selector(floatValue)] ? [o floatValue] : 0.0;
}

- (void)_setFloatValue:(float)value forKey:(NSString *)key
{
    if ([self _floatValueForKey:key] == value)
        return;
    NSString *_key = KEY(key);
    [_private->values _webkit_setFloat:value forKey:_key];
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setFloat:value forKey:_key];
    [self _postPreferencesChangesNotification];
}

- (BOOL)_boolValueForKey:(NSString *)key
{
    return [self _integerValueForKey:key] != 0;
}

- (void)_setBoolValue:(BOOL)value forKey:(NSString *)key
{
    if ([self _boolValueForKey:key] == value)
        return;
    NSString *_key = KEY(key);
    [_private->values _webkit_setBool:value forKey:_key];
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setBool:value forKey:_key];
    [self _postPreferencesChangesNotification];
}

- (NSString *)standardFontFamily
{
    return [self _stringValueForKey: WebKitStandardFontPreferenceKey];
}

- (void)setStandardFontFamily:(NSString *)family
{
    [self _setStringValue: family forKey: WebKitStandardFontPreferenceKey];
}

- (NSString *)fixedFontFamily
{
    return [self _stringValueForKey: WebKitFixedFontPreferenceKey];
}

- (void)setFixedFontFamily:(NSString *)family
{
    [self _setStringValue: family forKey: WebKitFixedFontPreferenceKey];
}

- (NSString *)serifFontFamily
{
    return [self _stringValueForKey: WebKitSerifFontPreferenceKey];
}

- (void)setSerifFontFamily:(NSString *)family 
{
    [self _setStringValue: family forKey: WebKitSerifFontPreferenceKey];
}

- (NSString *)sansSerifFontFamily
{
    return [self _stringValueForKey: WebKitSansSerifFontPreferenceKey];
}

- (void)setSansSerifFontFamily:(NSString *)family
{
    [self _setStringValue: family forKey: WebKitSansSerifFontPreferenceKey];
}

- (NSString *)cursiveFontFamily
{
    return [self _stringValueForKey: WebKitCursiveFontPreferenceKey];
}

- (void)setCursiveFontFamily:(NSString *)family
{
    [self _setStringValue: family forKey: WebKitCursiveFontPreferenceKey];
}

- (NSString *)fantasyFontFamily
{
    return [self _stringValueForKey: WebKitFantasyFontPreferenceKey];
}

- (void)setFantasyFontFamily:(NSString *)family
{
    [self _setStringValue: family forKey: WebKitFantasyFontPreferenceKey];
}

- (int)defaultFontSize
{
    return [self _integerValueForKey: WebKitDefaultFontSizePreferenceKey];
}

- (void)setDefaultFontSize:(int)size
{
    [self _setIntegerValue: size forKey: WebKitDefaultFontSizePreferenceKey];
}

- (int)defaultFixedFontSize
{
    return [self _integerValueForKey: WebKitDefaultFixedFontSizePreferenceKey];
}

- (void)setDefaultFixedFontSize:(int)size
{
    [self _setIntegerValue: size forKey: WebKitDefaultFixedFontSizePreferenceKey];
}

- (int)minimumFontSize
{
    return [self _integerValueForKey: WebKitMinimumFontSizePreferenceKey];
}

- (void)setMinimumFontSize:(int)size
{
    [self _setIntegerValue: size forKey: WebKitMinimumFontSizePreferenceKey];
}

- (int)minimumLogicalFontSize
{
  return [self _integerValueForKey: WebKitMinimumLogicalFontSizePreferenceKey];
}

- (void)setMinimumLogicalFontSize:(int)size
{
  [self _setIntegerValue: size forKey: WebKitMinimumLogicalFontSizePreferenceKey];
}

- (NSString *)defaultTextEncodingName
{
    return [self _stringValueForKey: WebKitDefaultTextEncodingNamePreferenceKey];
}

- (void)setDefaultTextEncodingName:(NSString *)encoding
{
    [self _setStringValue: encoding forKey: WebKitDefaultTextEncodingNamePreferenceKey];
}

- (BOOL)userStyleSheetEnabled
{
    return [self _boolValueForKey: WebKitUserStyleSheetEnabledPreferenceKey];
}

- (void)setUserStyleSheetEnabled:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitUserStyleSheetEnabledPreferenceKey];
}

- (NSURL *)userStyleSheetLocation
{
    NSString *locationString = [self _stringValueForKey: WebKitUserStyleSheetLocationPreferenceKey];
    
    if ([locationString _webkit_looksLikeAbsoluteURL]) {
        return [NSURL _web_URLWithDataAsString:locationString];
    } else {
        locationString = [locationString stringByExpandingTildeInPath];
        return [NSURL fileURLWithPath:locationString];
    }
}

- (void)setUserStyleSheetLocation:(NSURL *)URL
{
    NSString *locationString;
    
    if ([URL isFileURL]) {
        locationString = [[URL path] _web_stringByAbbreviatingWithTildeInPath];
    } else {
        locationString = [URL _web_originalDataAsString];
    }
    
    [self _setStringValue:locationString forKey: WebKitUserStyleSheetLocationPreferenceKey];
}

- (BOOL)shouldPrintBackgrounds
{
    return [self _boolValueForKey: WebKitShouldPrintBackgroundsPreferenceKey];
}

- (void)setShouldPrintBackgrounds:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitShouldPrintBackgroundsPreferenceKey];
}

- (BOOL)isJavaEnabled
{
    return [self _boolValueForKey: WebKitJavaEnabledPreferenceKey];
}

- (void)setJavaEnabled:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitJavaEnabledPreferenceKey];
}

- (BOOL)isJavaScriptEnabled
{
    return [self _boolValueForKey: WebKitJavaScriptEnabledPreferenceKey];
}

- (void)setJavaScriptEnabled:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitJavaScriptEnabledPreferenceKey];
}

- (BOOL)javaScriptCanOpenWindowsAutomatically
{
    return [self _boolValueForKey: WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey];
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey];
}

- (BOOL)arePlugInsEnabled
{
    return [self _boolValueForKey: WebKitPluginsEnabledPreferenceKey];
}

- (void)setPlugInsEnabled:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitPluginsEnabledPreferenceKey];
}

- (BOOL)allowsAnimatedImages
{
    return [self _boolValueForKey: WebKitAllowAnimatedImagesPreferenceKey];
}

- (void)setAllowsAnimatedImages:(BOOL)flag;
{
    [self _setBoolValue: flag forKey: WebKitAllowAnimatedImagesPreferenceKey];
}

- (BOOL)allowsAnimatedImageLooping
{
    return [self _boolValueForKey: WebKitAllowAnimatedImageLoopingPreferenceKey];
}

- (void)setAllowsAnimatedImageLooping: (BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitAllowAnimatedImageLoopingPreferenceKey];
}

- (void)setLoadsImagesAutomatically: (BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitDisplayImagesKey];
}

- (BOOL)loadsImagesAutomatically
{
    return [self _boolValueForKey: WebKitDisplayImagesKey];
}

- (void)setAutosaves:(BOOL)flag;
{
    _private->autosaves = flag;
}

- (BOOL)autosaves
{
    return _private->autosaves;
}

- (void)setTabsToLinks:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitTabToLinksPreferenceKey];
}

- (BOOL)tabsToLinks
{
    return [self _boolValueForKey:WebKitTabToLinksPreferenceKey];
}

- (void)setPrivateBrowsingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitPrivateBrowsingEnabledPreferenceKey];
}

- (BOOL)privateBrowsingEnabled
{
    return [self _boolValueForKey:WebKitPrivateBrowsingEnabledPreferenceKey];
}

@end

@implementation WebPreferences (WebPrivate)

- (BOOL)respectStandardStyleKeyEquivalents
{
    return [self _boolValueForKey:WebKitRespectStandardStyleKeyEquivalentsPreferenceKey];
}

- (void)setRespectStandardStyleKeyEquivalents:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitRespectStandardStyleKeyEquivalentsPreferenceKey];
}

- (BOOL)showsURLsInToolTips
{
    return [self _boolValueForKey:WebKitShowsURLsInToolTipsPreferenceKey];
}

- (void)setShowsURLsInToolTips:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShowsURLsInToolTipsPreferenceKey];
}

- (BOOL)textAreasAreResizable
{
    return [self _boolValueForKey: WebKitTextAreasAreResizablePreferenceKey];
}

- (void)setTextAreasAreResizable:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitTextAreasAreResizablePreferenceKey];
}

- (int)_pageCacheSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitPageCacheSizePreferenceKey];
}

- (int)_objectCacheSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitObjectCacheSizePreferenceKey];
}

- (NSTimeInterval)_backForwardCacheExpirationInterval
{
    return (NSTimeInterval)[[NSUserDefaults standardUserDefaults] floatForKey:WebKitBackForwardCacheExpirationIntervalKey];
}

- (float)PDFScaleFactor
{
    return [self _floatValueForKey:WebKitPDFScaleFactorPreferenceKey];
}

- (void)setPDFScaleFactor:(float)factor
{
    [self _setFloatValue:factor forKey:WebKitPDFScaleFactorPreferenceKey];
}

- (PDFDisplayMode)PDFDisplayMode;
{
    PDFDisplayMode value = [self _integerValueForKey:WebKitPDFDisplayModePreferenceKey];
    if (value != kPDFDisplaySinglePage && value != kPDFDisplaySinglePageContinuous && value != kPDFDisplayTwoUp && value != kPDFDisplayTwoUpContinuous) {
        // protect against new modes from future versions of OS X stored in defaults
        value = kPDFDisplaySinglePageContinuous;
    }
    return value;
}

- (void)setPDFDisplayMode:(PDFDisplayMode)mode
{
    [self _setIntegerValue:mode forKey:WebKitPDFDisplayModePreferenceKey];
}

static NSMutableDictionary *webPreferencesInstances = nil;

+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)ident
{
        LOG (Encoding, "requesting for %@\n", ident);

    if (!ident){
        if(_standardPreferences)
            return _standardPreferences;
        return nil;
    }    
    
    WebPreferences *instance = [webPreferencesInstances objectForKey:[self _concatenateKeyWithIBCreatorID:ident]];

    return instance;
}

+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)ident
{
    if (!webPreferencesInstances)
        webPreferencesInstances = [[NSMutableDictionary alloc] init];
    if (ident) {
        [webPreferencesInstances setObject:instance forKey:[self _concatenateKeyWithIBCreatorID:ident]];
        LOG (Encoding, "recording %p for %@\n", instance, [self _concatenateKeyWithIBCreatorID:ident]);
    }
}

+ (void)_removeReferenceForIdentifier:(NSString *)ident
{
    if (ident != nil) {
        [webPreferencesInstances performSelector:@selector(_web_checkLastReferenceForIdentifier:) withObject: [self _concatenateKeyWithIBCreatorID:ident] afterDelay:.1];
    }
}

- (void)_postPreferencesChangesNotification
{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebPreferencesChangedNotification object:self
                    userInfo:nil];
}

+ (CFStringEncoding)_systemCFStringEncoding
{
#if OMIT_TIGER_FEATURES
    CFStringEncoding encoding = CFStringGetSystemEncoding();

    // Map from system encodings to the appropriate default web encoding.
    // Web pages that are not labeled will be decoded assuming this encoding,
    // so it's important that this be the most likely encoding to encounter
    // on a web page. MacArabic, MacHebrew, and MacRoman are not common on
    // the web, so instead we map to the most common similar encoding actually used.
    switch (encoding) {
        case kCFStringEncodingMacArabic:
            encoding = kCFStringEncodingDOSArabic;
            break;
        case kCFStringEncodingMacHebrew:
            encoding = kCFStringEncodingDOSHebrew;
            break;
        case kCFStringEncodingMacRoman:
            encoding = kCFStringEncodingISOLatin1;
            break;
    }

    // We must not use any encoding that has no IANA character set name.
    if (CFStringConvertEncodingToIANACharSetName(encoding) == NULL)
        encoding = kCFStringEncodingISOLatin1;
#else
	CFStringEncoding encoding = WKGetWebDefaultCFStringEncoding();
#endif

    return encoding;
}

+ (void)_setInitialDefaultTextEncodingToSystemEncoding
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:
        [NSDictionary dictionaryWithObject:(NSString *)CFStringConvertEncodingToIANACharSetName([self _systemCFStringEncoding])
                                    forKey:WebKitDefaultTextEncodingNamePreferenceKey]];
}

static NSString *classIBCreatorID = nil;

+ (void)_setIBCreatorID:(NSString *)string
{
    NSString *old = classIBCreatorID;
    classIBCreatorID = [string copy];
    [old release];
}

@end

@implementation WebPreferences (WebInternal)

+ (NSString *)_IBCreatorID
{
    return classIBCreatorID;
}

+ (NSString *)_concatenateKeyWithIBCreatorID:(NSString *)key
{
    NSString *IBCreatorID = [WebPreferences _IBCreatorID];
    if (!IBCreatorID)
        return key;
    return [IBCreatorID stringByAppendingString:key];
}

@end

@implementation NSMutableDictionary (WebInternal)

- (void)_web_checkLastReferenceForIdentifier:(NSString *)identifier
{
    WebPreferences *instance = [webPreferencesInstances objectForKey:identifier];
    if ([instance retainCount] == 1)
        [webPreferencesInstances removeObjectForKey:identifier];
}

@end
