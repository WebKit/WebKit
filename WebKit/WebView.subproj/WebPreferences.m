/*        
        WebPreferences.mm
        Copyright 2001, Apple, Inc. All rights reserved.
*/
#import "WebPreferences.h"

// These are private because callers should be using the cover methods
#define        WebKitLogLevelPreferenceKey                @"WebKitLogLevel"
#define        WebKitStandardFontPreferenceKey                @"WebKitStandardFont"
#define        WebKitFixedFontPreferenceKey                @"WebKitFixedFont"
#define        WebKitSerifFontPreferenceKey                @"WebKitSerifFont"
#define        WebKitSansSerifFontPreferenceKey        @"WebKitSansSerifFont"
#define        WebKitCursiveFontPreferenceKey                @"WebKitCursiveFont"
#define        WebKitFantasyFontPreferenceKey                @"WebKitFantasyFont"
#define        WebKitMinimumFontSizePreferenceKey        @"WebKitMinimumFontSize"
#define        WebKitDefaultFontSizePreferenceKey        @"WebKitDefaultFontSize"
#define	       WebKitDefaultFixedFontSizePreferenceKey		 @"WebKitDefaultFixedFontSize"
#define	       WebKitDefaultTextEncodingPreferenceKey	 @"WebKitDefaultTextEncoding"
#define	       WebKitUserStyleSheetEnabledPreferenceKey @"WebKitUserStyleSheetEnabledPreferenceKey"
#define	       WebKitUserStyleSheetLocationPreferenceKey @"WebKitUserStyleSheetLocationPreferenceKey"
#define        WebKitJavaEnabledPreferenceKey                @"WebKitJavaEnabled"
#define        WebKitJavaScriptEnabledPreferenceKey        @"WebKitJavaScriptEnabled"
#define        WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey        @"WebKitJavaScriptCanOpenWindowsAutomatically"
#define        WebKitPluginsEnabledPreferenceKey        @"WebKitPluginsEnabled"
#define        WebKitInitialTimedLayoutDelayPreferenceKey        @"WebKitInitialTimedLayoutDelay"
#define        WebKitInitialTimedLayoutSizePreferenceKey        @"WebKitInitialTimedLayoutSize"
#define        WebKitInitialTimedLayoutEnabledPreferenceKey        @"WebKitInitialTimedLayoutEnabled"
#define        WebKitResourceTimedLayoutEnabledPreferenceKey        @"WebKitResourceTimedLayoutEnabled"
#define        WebKitResourceTimedLayoutDelayPreferenceKey        @"WebKitResourceTimedLayoutDelay"
#define        WebKitAllowAnimatedImagesPreferenceKey        @"WebKitAllowAnimatedImagesPreferenceKey"
#define        WebKitAllowAnimatedImageLoopingPreferenceKey        @"WebKitAllowAnimatedImageLoopingPreferenceKey"
#define        WebKitDisplayImagesKey				@"WebKitDisplayImagesKey"

@implementation WebPreferences

static WebPreferences *_standardPreferences = nil;

+ (WebPreferences *)standardPreferences
{
    if (_standardPreferences == nil) {
        _standardPreferences = [[WebPreferences alloc] init];
    }

    return _standardPreferences;
}

// if we ever have more than one WebPreferences object, this would move to init
+ (void)load
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSNumber *displayImages = [NSNumber numberWithBool:TRUE];
    NSNumber *pluginsEnabled = [NSNumber numberWithBool:TRUE];
    NSNumber *userStyleSheetEnabled = [NSNumber numberWithBool:FALSE];
    NSNumber *javaEnabled = [NSNumber numberWithBool:FALSE];
    NSNumber *javaScriptEnabled = [NSNumber numberWithBool:TRUE];
    NSNumber *javaScriptCanOpenWindows = [NSNumber numberWithBool:FALSE];
    NSNumber *timedLayoutEnabled = [NSNumber numberWithBool:TRUE];
    NSNumber *resourceTimedLayoutEnabled = [NSNumber numberWithBool:TRUE];
    NSNumber *allowAnimatedImages = [NSNumber numberWithBool:TRUE];
    NSNumber *allowAnimatedImageLooping = [NSNumber numberWithBool:TRUE];
    NSNumber *defaultTextEncoding = [NSNumber numberWithInt:(int)kCFStringEncodingWindowsLatin1];

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
	defaultTextEncoding, 		WebKitDefaultTextEncodingPreferenceKey,
        @"1.00",                        WebKitInitialTimedLayoutDelayPreferenceKey,
        @"4096",                        WebKitInitialTimedLayoutSizePreferenceKey,
        @"1.00",                        WebKitResourceTimedLayoutDelayPreferenceKey,
        timedLayoutEnabled,             WebKitInitialTimedLayoutEnabledPreferenceKey,
        resourceTimedLayoutEnabled,     WebKitResourceTimedLayoutEnabledPreferenceKey,
        userStyleSheetEnabled,          WebKitUserStyleSheetEnabledPreferenceKey,
        @"",                    	WebKitUserStyleSheetLocationPreferenceKey,
        javaEnabled,                    WebKitJavaEnabledPreferenceKey,
        javaScriptEnabled,              WebKitJavaScriptEnabledPreferenceKey,
        javaScriptCanOpenWindows,       WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
        pluginsEnabled,                 WebKitPluginsEnabledPreferenceKey,
        allowAnimatedImages,            WebKitAllowAnimatedImagesPreferenceKey,
        allowAnimatedImageLooping,      WebKitAllowAnimatedImageLoopingPreferenceKey,
        displayImages,			WebKitDisplayImagesKey,
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

- (int)defaultFontSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitDefaultFontSizePreferenceKey];
}

- (void)setDefaultFontSize:(int)size
{
    [[NSUserDefaults standardUserDefaults] setInteger:size forKey:WebKitDefaultFontSizePreferenceKey];
}

- (int)defaultFixedFontSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitDefaultFixedFontSizePreferenceKey];
}

- (void)setDefaultFixedFontSize:(int)size
{
    [[NSUserDefaults standardUserDefaults] setInteger:size forKey:WebKitDefaultFixedFontSizePreferenceKey];
}

- (int)minimumFontSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitMinimumFontSizePreferenceKey];
}

- (void)setMinimumFontSize:(int)size
{
    [[NSUserDefaults standardUserDefaults] setInteger:size forKey:WebKitMinimumFontSizePreferenceKey];
}

- (CFStringEncoding)defaultTextEncoding
{
    return (CFStringEncoding)[[NSUserDefaults standardUserDefaults] integerForKey:WebKitDefaultTextEncodingPreferenceKey];
}

- (void)setDefaultTextEncoding:(CFStringEncoding)encoding
{
    [[NSUserDefaults standardUserDefaults] setInteger:(int)encoding forKey:WebKitDefaultTextEncodingPreferenceKey];
}

- (BOOL)userStyleSheetEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitUserStyleSheetEnabledPreferenceKey];
}

- (void)setUserStyleSheetEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitUserStyleSheetEnabledPreferenceKey];
}

- (NSString *)userStyleSheetLocation
{
    return [[NSUserDefaults standardUserDefaults] stringForKey:WebKitUserStyleSheetLocationPreferenceKey];
}

- (void)setUserStyleSheetLocation:(NSString *)string
{
    [[NSUserDefaults standardUserDefaults] setObject:string forKey:WebKitUserStyleSheetLocationPreferenceKey];
}

- (BOOL)javaEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitJavaEnabledPreferenceKey];
}

- (void)setJavaEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitJavaEnabledPreferenceKey];
}

- (BOOL)javaScriptEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitJavaScriptEnabledPreferenceKey];
}

- (void)setJavaScriptEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitJavaScriptEnabledPreferenceKey];
}

- (BOOL)javaScriptCanOpenWindowsAutomatically
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey];
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey];
}

- (BOOL)pluginsEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitPluginsEnabledPreferenceKey];
}

- (void)setPluginsEnabled:(BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitPluginsEnabledPreferenceKey];
}


- (NSTimeInterval)_initialTimedLayoutDelay
{
    return (NSTimeInterval)[[NSUserDefaults standardUserDefaults] floatForKey:WebKitInitialTimedLayoutDelayPreferenceKey];
}

- (int)_initialTimedLayoutSize
{
    int size = [[NSUserDefaults standardUserDefaults] integerForKey:WebKitInitialTimedLayoutDelayPreferenceKey];
    return size;
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


- (void)setDisplayImages: (BOOL)flag
{
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebKitDisplayImagesKey];
}

- (BOOL)displayImages
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKitDisplayImagesKey];
}

@end
