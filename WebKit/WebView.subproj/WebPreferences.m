/*        
        WebPreferences.m
        Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import "WebPreferences.h"

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

@implementation WebPreferences

+ (WebPreferences *)standardPreferences
{
    static WebPreferences *_standardPreferences = nil;

    if (_standardPreferences == nil) {
        _standardPreferences = [[WebPreferences alloc] init];
    }

    return _standardPreferences;
}

- (void)_updateWebCoreSettings
{
    WebCoreSettings *settings = [WebCoreSettings sharedSettings];

    [settings setCursiveFontFamily:[self cursiveFontFamily]];
    [settings setDefaultFixedFontSize:[self defaultFixedFontSize]];
    [settings setDefaultFontSize:[self defaultFontSize]];
    [settings setFantasyFontFamily:[self fantasyFontFamily]];
    [settings setFixedFontFamily:[self fixedFontFamily]];
    [settings setJavaEnabled:[self JavaEnabled]];
    [settings setJavaScriptEnabled:[self JavaScriptEnabled]];
    [settings setJavaScriptCanOpenWindowsAutomatically:[self JavaScriptCanOpenWindowsAutomatically]];
    [settings setMinimumFontSize:[self minimumFontSize]];
    [settings setPluginsEnabled:[self pluginsEnabled]];
    [settings setSansSerifFontFamily:[self sansSerifFontFamily]];
    [settings setSerifFontFamily:[self serifFontFamily]];
    [settings setStandardFontFamily:[self standardFontFamily]];
    [settings setWillLoadImagesAutomatically:[self willLoadImagesAutomatically]];
    
    if ([self userStyleSheetEnabled]) {
        [settings setUserStyleSheetLocation:[self userStyleSheetLocation]];
    } else {
        [settings setUserStyleSheetLocation:@""];
    }
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
        @"6",                           WebKitMinimumFontSizePreferenceKey,
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

    [[NSNotificationCenter defaultCenter]
        addObserver:[self standardPreferences]
           selector:@selector(_updateWebCoreSettings)
               name:NSUserDefaultsDidChangeNotification
             object:[NSUserDefaults standardUserDefaults]];
    
    [[self standardPreferences] _updateWebCoreSettings];

    [pool release];
}

- (NSString *)standardFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitStandardFontPreferenceKey];
}

- (void)setStandardFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitStandardFontPreferenceKey];
    [self _updateWebCoreSettings];
}

- (NSString *)fixedFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitFixedFontPreferenceKey];
}

- (void)setFixedFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitFixedFontPreferenceKey];
    [self _updateWebCoreSettings];
}

- (NSString *)serifFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitSerifFontPreferenceKey];
}

- (void)setSerifFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitSerifFontPreferenceKey];
    [self _updateWebCoreSettings];
}

- (NSString *)sansSerifFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitSansSerifFontPreferenceKey];
}

- (void)setSansSerifFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitSansSerifFontPreferenceKey];
    [self _updateWebCoreSettings];
}

- (NSString *)cursiveFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitCursiveFontPreferenceKey];
}

- (void)setCursiveFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitCursiveFontPreferenceKey];
    [self _updateWebCoreSettings];
}

- (NSString *)fantasyFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitFantasyFontPreferenceKey];
}

- (void)setFantasyFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitFantasyFontPreferenceKey];
    [self _updateWebCoreSettings];
}

- (int)defaultFontSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitDefaultFontSizePreferenceKey];
}

- (void)setDefaultFontSize:(int)size
{
    [[NSUserDefaults standardUserDefaults] setInteger:size forKey:WebKitDefaultFontSizePreferenceKey];
    [self _updateWebCoreSettings];
}

- (int)defaultFixedFontSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitDefaultFixedFontSizePreferenceKey];
}

- (void)setDefaultFixedFontSize:(int)size
{
    [[NSUserDefaults standardUserDefaults] setInteger:size forKey:WebKitDefaultFixedFontSizePreferenceKey];
    [self _updateWebCoreSettings];
}

- (int)minimumFontSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitMinimumFontSizePreferenceKey];
}

- (void)setMinimumFontSize:(int)size
{
    [[NSUserDefaults standardUserDefaults] setInteger:size forKey:WebKitMinimumFontSizePreferenceKey];
    [self _updateWebCoreSettings];
}

- (NSString *)defaultTextEncodingName
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitDefaultTextEncodingNamePreferenceKey];
}

- (void)setDefaultTextEncodingName:(NSString *)encoding
{
    [[NSUserDefaults standardUserDefaults] setObject:encoding forKey:WebKitDefaultTextEncodingNamePreferenceKey];
}

- (BOOL)userStyleSheetEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitUserStyleSheetEnabledPreferenceKey];
}

- (void)setUserStyleSheetEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitUserStyleSheetEnabledPreferenceKey];
    [self _updateWebCoreSettings];
}

- (NSString *)userStyleSheetLocation
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitUserStyleSheetLocationPreferenceKey];
}

- (void)setUserStyleSheetLocation:(NSString *)string
{
    [[NSUserDefaults standardUserDefaults] setObject:string forKey:WebKitUserStyleSheetLocationPreferenceKey];
    [self _updateWebCoreSettings];
}

- (BOOL)JavaEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitJavaEnabledPreferenceKey];
}

- (void)setJavaEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitJavaEnabledPreferenceKey];
    [self _updateWebCoreSettings];
}

- (BOOL)JavaScriptEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitJavaScriptEnabledPreferenceKey];
}

- (void)setJavaScriptEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitJavaScriptEnabledPreferenceKey];
    [self _updateWebCoreSettings];
}

- (BOOL)JavaScriptCanOpenWindowsAutomatically
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey];
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey];
    [self _updateWebCoreSettings];
}

- (BOOL)pluginsEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitPluginsEnabledPreferenceKey];
}

- (void)setPluginsEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitPluginsEnabledPreferenceKey];
    [self _updateWebCoreSettings];
}

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

- (BOOL)allowAnimatedImages
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitAllowAnimatedImagesPreferenceKey];
}

- (void)setAllowAnimatedImages:(BOOL)flag;
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitAllowAnimatedImagesPreferenceKey];
}

- (BOOL)allowAnimatedImageLooping
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitAllowAnimatedImageLoopingPreferenceKey];
}

- (void)setAllowAnimatedImageLooping: (BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitAllowAnimatedImageLoopingPreferenceKey];
}

- (void)setWillLoadImagesAutomatically: (BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitDisplayImagesKey];
    [self _updateWebCoreSettings];
}

- (BOOL)willLoadImagesAutomatically
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitDisplayImagesKey];
}

@end
