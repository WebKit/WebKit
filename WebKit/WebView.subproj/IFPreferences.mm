/*	
        IFPreferences.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import "IFPreferences.h"

#import <WebKit/WebKitDebug.h>

// These are private because callers should be using the cover methods
#define	WebKitLogLevelPreferenceKey		@"WebKitLogLevel"
#define	WebKitStandardFontPreferenceKey		@"WebKitStandardFont"
#define	WebKitFixedFontPreferenceKey		@"WebKitFixedFont"
#define	WebKitSerifFontPreferenceKey		@"WebKitSerifFont"
#define	WebKitSansSerifFontPreferenceKey	@"WebKitSansSerifFont"
#define	WebKitCursiveFontPreferenceKey		@"WebKitCursiveFont"
#define	WebKitFantasyFontPreferenceKey		@"WebKitFantasyFont"
#define	WebKitMinimumFontSizePreferenceKey	@"WebKitMinimumFontSize"
#define	WebKitFontSizesPreferenceKey		@"WebKitFontSizes"
#define	WebKitJavaEnabledPreferenceKey		@"WebKitJavaEnabled"
#define	WebKitJScriptEnabledPreferenceKey	@"WebKitJScriptEnabled"
#define	WebKitPluginsEnabledPreferenceKey	@"WebKitPluginsEnabled"

@implementation IFPreferences

static IFPreferences *_standardPreferences = nil;

+ (IFPreferences *)standardPreferences
{
    if (_standardPreferences == nil) {
        _standardPreferences = [[IFPreferences alloc] init];
    }

    return _standardPreferences;
}

// if we ever have more than one IFPreferences object, this would move to init
+ (void)load
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSArray *fontSizeArray = [NSArray arrayWithObjects:@"6", @"8", @"9", @"11", @"14", @"16", @"20", nil];
    NSNumber *pluginsEnabled = [NSNumber numberWithBool:TRUE];
    NSNumber *javaEnabled = [NSNumber numberWithBool:FALSE];
    NSNumber *jScriptEnabled = [NSNumber numberWithBool:TRUE];

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        @"0x0", 		WebKitLogLevelPreferenceKey,
        @"Georgia", 		WebKitStandardFontPreferenceKey,
        @"Monaco",	  	WebKitFixedFontPreferenceKey,
        @"Georgia", 		WebKitSerifFontPreferenceKey,
        @"Arial", 		WebKitSansSerifFontPreferenceKey,
        @"Apple Chancery", 	WebKitCursiveFontPreferenceKey,
        @"Papyrus", 		WebKitFantasyFontPreferenceKey,
        @"6", 			WebKitMinimumFontSizePreferenceKey,
        fontSizeArray,		WebKitFontSizesPreferenceKey,
        javaEnabled,		WebKitJavaEnabledPreferenceKey,
        jScriptEnabled,		WebKitJScriptEnabledPreferenceKey,
        pluginsEnabled,		WebKitPluginsEnabledPreferenceKey,
        nil];

    [[NSUserDefaults standardUserDefaults] registerDefaults:dict];

    [pool release];
}

- (NSString *)standardFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitStandardFontPreferenceKey];
}

- (void)setStandardFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitStandardFontPreferenceKey];
}

- (NSString *)fixedFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitFixedFontPreferenceKey];
}

- (void)setFixedFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitFixedFontPreferenceKey];
}

- (NSString *)serifFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitSerifFontPreferenceKey];
}

- (void)setSerifFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitSerifFontPreferenceKey];
}

- (NSString *)sansSerifFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitSansSerifFontPreferenceKey];
}

- (void)setSansSerifFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitSansSerifFontPreferenceKey];
}

- (NSString *)cursiveFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitCursiveFontPreferenceKey];
}

- (void)setCursiveFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitCursiveFontPreferenceKey];
}

- (NSString *)fantasyFontFamily
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitFantasyFontPreferenceKey];
}

- (void)setFantasyFontFamily:(NSString *)family
{
    [[NSUserDefaults standardUserDefaults] setObject:family forKey:WebKitFantasyFontPreferenceKey];
}

- (NSArray *)fontSizes
{
    return [[NSUserDefaults standardUserDefaults] arrayForKey:WebKitFontSizesPreferenceKey];
}

- (void)setFontSizes:(NSArray *)sizes
{
    [[NSUserDefaults standardUserDefaults] setObject:sizes forKey:WebKitFontSizesPreferenceKey];
}

- (int)minimumFontSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitMinimumFontSizePreferenceKey];
}

- (void)setMinimumFontSize:(int)size
{
    [[NSUserDefaults standardUserDefaults] setInteger:size forKey:WebKitMinimumFontSizePreferenceKey];
}


- (BOOL)javaEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitJavaEnabledPreferenceKey];
}

- (void)setJavaEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitJavaEnabledPreferenceKey];
}

- (BOOL)jScriptEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitJScriptEnabledPreferenceKey];
}

- (void)setJScriptEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitJScriptEnabledPreferenceKey];
}

- (BOOL)pluginsEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitPluginsEnabledPreferenceKey];
}

- (void)setPluginsEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitPluginsEnabledPreferenceKey];
}


@end
