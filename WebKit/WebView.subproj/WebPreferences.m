/*        
        WebPreferences.m
        Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import "WebPreferences.h"

#import <WebFoundation/WebNSDictionaryExtras.h>

#import <WebCore/WebCoreSettings.h>

// These are private because callers should be using the cover methods
#define WebKitLogLevelPreferenceKey @"WebKitLogLevel"
#define WebKitStandardFontPreferenceKey @"WebKitStandardFont"
#define WebKitFixedFontPreferenceKey @"WebKitFixedFont"
#define WebKitSerifFontPreferenceKey @"WebKitSerifFont"
#define WebKitSansSerifFontPreferenceKey @"WebKitSansSerifFont"
#define WebKitCursiveFontPreferenceKey @"WebKitCursiveFont"
#define WebKitFantasyFontPreferenceKey @"WebKitFantasyFont"
#define WebKitMinimumFontSizePreferenceKey @"WebKitMinimumFontSize"
#define WebKitDefaultFontSizePreferenceKey @"WebKitDefaultFontSize"
#define WebKitDefaultFixedFontSizePreferenceKey @"WebKitDefaultFixedFontSize"
#define WebKitDefaultTextEncodingNamePreferenceKey @"WebKitDefaultTextEncodingName"
#define WebKitUserStyleSheetEnabledPreferenceKey @"WebKitUserStyleSheetEnabledPreferenceKey"
#define WebKitUserStyleSheetLocationPreferenceKey @"WebKitUserStyleSheetLocationPreferenceKey"
#define WebKitJavaEnabledPreferenceKey @"WebKitJavaEnabled"
#define WebKitJavaScriptEnabledPreferenceKey @"WebKitJavaScriptEnabled"
#define WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey @"WebKitJavaScriptCanOpenWindowsAutomatically"
#define WebKitPluginsEnabledPreferenceKey @"WebKitPluginsEnabled"
#define WebKitInitialTimedLayoutDelayPreferenceKey @"WebKitInitialTimedLayoutDelay"
#define WebKitInitialTimedLayoutSizePreferenceKey @"WebKitInitialTimedLayoutSize"
#define WebKitInitialTimedLayoutEnabledPreferenceKey @"WebKitInitialTimedLayoutEnabled"
#define WebKitResourceTimedLayoutEnabledPreferenceKey @"WebKitResourceTimedLayoutEnabled"
#define WebKitResourceTimedLayoutDelayPreferenceKey @"WebKitResourceTimedLayoutDelay"
#define WebKitAllowAnimatedImagesPreferenceKey @"WebKitAllowAnimatedImagesPreferenceKey"
#define WebKitAllowAnimatedImageLoopingPreferenceKey @"WebKitAllowAnimatedImageLoopingPreferenceKey"
#define WebKitDisplayImagesKey @"WebKitDisplayImagesKey"
#define WebKitPageCacheSizePreferenceKey @"WebKitPageCacheSizePreferenceKey"
#define WebKitObjectCacheSizePreferenceKey @"WebKitObjectCacheSizePreferenceKey"

NSString *WebPreferencesChangedNotification = @"WebPreferencesChangedNotification";

@implementation WebPreferences

+ (WebPreferences *)standardPreferences
{
    static WebPreferences *_standardPreferences = nil;

    if (_standardPreferences == nil) {
        _standardPreferences = [[WebPreferences alloc] init];
    }

    return _standardPreferences;
}

- (void)_postPreferencesChangesNotification
{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebPreferencesChangedNotification object:self
                    userInfo:nil];
}

// if we ever have more than one WebPreferences object, this would move to init
+ (void)load
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        @"0x0",                         WebKitLogLevelPreferenceKey,
        @"Lucida Grande",               WebKitStandardFontPreferenceKey,
        @"Courier",                     WebKitFixedFontPreferenceKey,
        @"Times New Roman",             WebKitSerifFontPreferenceKey,
        @"Lucida Grande",               WebKitSansSerifFontPreferenceKey,
        @"Apple Chancery",              WebKitCursiveFontPreferenceKey,
        @"Papyrus",                     WebKitFantasyFontPreferenceKey,
        @"9",                           WebKitMinimumFontSizePreferenceKey,
        @"14",                          WebKitDefaultFontSizePreferenceKey,
        @"14", 				WebKitDefaultFixedFontSizePreferenceKey,
	@"latin1", 		        WebKitDefaultTextEncodingNamePreferenceKey,
        @"1.00",                        WebKitInitialTimedLayoutDelayPreferenceKey,
        @"4096",                        WebKitInitialTimedLayoutSizePreferenceKey,
        @"1.00",                        WebKitResourceTimedLayoutDelayPreferenceKey,
        @"4",                           WebKitPageCacheSizePreferenceKey,
        @"4194304",                     WebKitObjectCacheSizePreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitInitialTimedLayoutEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitResourceTimedLayoutEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUserStyleSheetEnabledPreferenceKey,
        @"",                    	WebKitUserStyleSheetLocationPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitPluginsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImagesPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImageLoopingPreferenceKey,
        [NSNumber numberWithBool:YES],	WebKitDisplayImagesKey,
        nil];

    [[NSUserDefaults standardUserDefaults] registerDefaults:dict];

/*
    [[NSNotificationCenter defaultCenter]
        addObserver:[self standardPreferences]
           selector:@selector(_postPreferencesChangesNotification)
               name:NSUserDefaultsDidChangeNotification
             object:[NSUserDefaults standardUserDefaults]];
*/    
    [[self standardPreferences] _postPreferencesChangesNotification];

    [pool release];
}

- (NSString *)_stringValueForKey: (NSString *)key
{
    NSString *s = [values objectForKey:key];
    if (s)
        return s;
    return [[NSUserDefaults standardUserDefaults] stringForKey:key];
}

- (void)_setStringValue: (NSString *)value forKey: (NSString *)key
{
    if (self == [WebPreferences standardPreferences])
        [[NSUserDefaults standardUserDefaults] setObject:value forKey:key];
    else
        [values setObject: value forKey: key];
    [self _postPreferencesChangesNotification];
}

- (int)_integerValueForKey: (NSString *)key
{
    NSNumber *n = [values objectForKey:key];
    if (n)
        return [n intValue];
    return [[NSUserDefaults standardUserDefaults] integerForKey:key];
}

- (void)_setIntegerValue: (int)value forKey: (NSString *)key
{
    if (self == [WebPreferences standardPreferences])
        [[NSUserDefaults standardUserDefaults] setInteger:value forKey:key];
    else
        [values _web_setInt: value forKey: key];
    [self _postPreferencesChangesNotification];
}

- (int)_boolValueForKey: (NSString *)key
{
    NSNumber *n = [values objectForKey:key];
    if (n)
        return [n boolValue];
    return [[NSUserDefaults standardUserDefaults] integerForKey:key];
}

- (void)_setBoolValue: (BOOL)value forKey: (NSString *)key
{
    if (self == [WebPreferences standardPreferences])
        [[NSUserDefaults standardUserDefaults] setBool:value forKey:key];
    else
        [values _web_setBool: value forKey: key];
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
    return [self _setIntegerValue: size forKey: WebKitDefaultFontSizePreferenceKey];
}

- (int)defaultFixedFontSize
{
    return [self _integerValueForKey: WebKitDefaultFixedFontSizePreferenceKey];
}

- (void)setDefaultFixedFontSize:(int)size
{
    return [self _setIntegerValue: size forKey: WebKitDefaultFixedFontSizePreferenceKey];
}

- (int)minimumFontSize
{
    return [self _integerValueForKey: WebKitMinimumFontSizePreferenceKey];
}

- (void)setMinimumFontSize:(int)size
{
    return [self _setIntegerValue: size forKey: WebKitMinimumFontSizePreferenceKey];
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

- (NSString *)userStyleSheetLocation
{
    return [self _stringValueForKey: WebKitUserStyleSheetLocationPreferenceKey];
}

- (void)setUserStyleSheetLocation:(NSString *)string
{
    [self _setStringValue: string forKey: WebKitUserStyleSheetLocationPreferenceKey];
}

- (BOOL)JavaEnabled
{
    return [self _boolValueForKey: WebKitJavaEnabledPreferenceKey];
}

- (void)setJavaEnabled:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitJavaEnabledPreferenceKey];
}

- (BOOL)JavaScriptEnabled
{
    return [self _boolValueForKey: WebKitJavaScriptEnabledPreferenceKey];
}

- (void)setJavaScriptEnabled:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitJavaScriptEnabledPreferenceKey];
}

- (BOOL)JavaScriptCanOpenWindowsAutomatically
{
    return [self _boolValueForKey: WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey];
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey];
}

- (BOOL)pluginsEnabled
{
    return [self _boolValueForKey: WebKitPluginsEnabledPreferenceKey];
}

- (void)setPluginsEnabled:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitPluginsEnabledPreferenceKey];
}

- (BOOL)allowAnimatedImages
{
    return [self _boolValueForKey: WebKitAllowAnimatedImagesPreferenceKey];
}

- (void)setAllowAnimatedImages:(BOOL)flag;
{
    [self _setBoolValue: flag forKey: WebKitAllowAnimatedImagesPreferenceKey];
}

- (BOOL)allowAnimatedImageLooping
{
    return [self _boolValueForKey: WebKitAllowAnimatedImageLoopingPreferenceKey];
}

- (void)setAllowAnimatedImageLooping: (BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitAllowAnimatedImageLoopingPreferenceKey];
}

- (void)setWillLoadImagesAutomatically: (BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitDisplayImagesKey];
}

- (BOOL)willLoadImagesAutomatically
{
    return [self _boolValueForKey: WebKitDisplayImagesKey];
}

@end

@implementation WebPreferences (WebPrivate)

- (NSTimeInterval)_initialTimedLayoutDelay
{
    return (NSTimeInterval)[[NSUserDefaults standardUserDefaults] floatForKey:WebKitInitialTimedLayoutDelayPreferenceKey];
}

- (int)_initialTimedLayoutSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitInitialTimedLayoutDelayPreferenceKey];
}

- (int)_pageCacheSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitPageCacheSizePreferenceKey];
}

- (int)_objectCacheSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitObjectCacheSizePreferenceKey];
}

- (BOOL)_initialTimedLayoutEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitInitialTimedLayoutEnabledPreferenceKey];
}

- (NSTimeInterval)_resourceTimedLayoutDelay
{
    return [[NSUserDefaults standardUserDefaults] floatForKey:WebKitResourceTimedLayoutDelayPreferenceKey];
}

- (BOOL)_resourceTimedLayoutEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitResourceTimedLayoutEnabledPreferenceKey];
}

@end
