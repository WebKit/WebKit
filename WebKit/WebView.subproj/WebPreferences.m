/*        
        WebPreferences.m
        Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebPreferencesPrivate.h>

#import <Foundation/NSDictionary_NSURLExtras.h>

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

#define KEY(x) [(_private->identifier?_private->identifier:@"") stringByAppendingString:x]

enum { WebPreferencesVersion = 1 };

@interface WebPreferencesPrivate : NSObject
{
@public
    NSMutableDictionary *values;
    NSString *identifier;
    BOOL autosaves;
}
@end

@implementation WebPreferencesPrivate
- (void)dealloc
{
    [values release];
    [identifier release];
    [super dealloc];
}
@end

@implementation WebPreferences

- init
{
    return [self initWithIdentifier:nil];
}

- (id)initWithIdentifier:(NSString *)anIdentifier
{
    [super init];
    
    if (anIdentifier == nil)
        anIdentifier = @"";
        
    _private = [[WebPreferencesPrivate alloc] init];
    
    WebPreferences *instance = [[self class] _getInstanceForIdentifier:anIdentifier];
    if (instance){
        [self release];
        return instance;
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
    
    if ([decoder allowsKeyedCoding]){
        _private->identifier = [[decoder decodeObjectForKey:@"Identifier"] retain];
        _private->values = [[decoder decodeObjectForKey:@"Values"] retain];
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
    static WebPreferences *_standardPreferences = nil;

    if (_standardPreferences == nil) {
        _standardPreferences = [[WebPreferences alloc] init];
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
        @"16",                          WebKitDefaultFontSizePreferenceKey,
        @"13",                          WebKitDefaultFixedFontSizePreferenceKey,
        @"latin1",                      WebKitDefaultTextEncodingNamePreferenceKey,
        @"1.00",                        WebKitInitialTimedLayoutDelayPreferenceKey,
        @"4096",                        WebKitInitialTimedLayoutSizePreferenceKey,
        @"1.00",                        WebKitResourceTimedLayoutDelayPreferenceKey,
        @"4",                           WebKitPageCacheSizePreferenceKey,
        @"4194304",                     WebKitObjectCacheSizePreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitInitialTimedLayoutEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitResourceTimedLayoutEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUserStyleSheetEnabledPreferenceKey,
        @"",                            WebKitUserStyleSheetLocationPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitPluginsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImagesPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImageLoopingPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDisplayImagesKey,
        nil];

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

- (NSString *)_stringValueForKey: (NSString *)key
{
    NSString *_key = KEY(key);
    NSString *s = [_private->values objectForKey:_key];
    if (s)
        return s;
    return [[NSUserDefaults standardUserDefaults] stringForKey:_key];
}

- (void)_setStringValue: (NSString *)value forKey: (NSString *)key
{
    NSString *_key = KEY(key);
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:value forKey:_key];

    [_private->values setObject: value forKey:_key];

    [self _postPreferencesChangesNotification];
}

- (int)_integerValueForKey: (NSString *)key
{
    NSString *_key = KEY(key);
    NSNumber *n = [_private->values objectForKey:_key];
    if (n)
        return [n intValue];
    return [[NSUserDefaults standardUserDefaults] integerForKey:_key];
}

- (void)_setIntegerValue: (int)value forKey: (NSString *)key
{
    NSString *_key = KEY(key);
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setInteger:value forKey:_key];

    [_private->values _web_setInt: value forKey: _key];

    [self _postPreferencesChangesNotification];
}

- (int)_boolValueForKey: (NSString *)key
{
    NSString *_key = KEY(key);
    NSNumber *n = [_private->values objectForKey:_key];
    if (n)
        return [n boolValue];
    return [[NSUserDefaults standardUserDefaults] integerForKey:_key];
}

- (void)_setBoolValue: (BOOL)value forKey: (NSString *)key
{
    NSString *_key = KEY(key);
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setBool:value forKey:_key];

    [_private->values _web_setBool: value forKey: _key];

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

- (NSURL *)userStyleSheetLocation
{
    return [NSURL URLWithString:[self _stringValueForKey: WebKitUserStyleSheetLocationPreferenceKey]];
}

- (void)setUserStyleSheetLocation:(NSURL *)URL
{
    [self _setStringValue: [URL absoluteString] forKey: WebKitUserStyleSheetLocationPreferenceKey];
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

static NSMutableDictionary *webPreferencesInstances = nil;

+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)ident
{
    WebPreferences *instance = [webPreferencesInstances objectForKey:ident];
    return instance;
}

+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)ident
{
    if (!webPreferencesInstances)
        webPreferencesInstances = [[NSMutableDictionary alloc] init];
    [webPreferencesInstances setObject:instance forKey:ident];
}

+ (void)_removeReferenceForIdentifier:(NSString *)ident
{
    [webPreferencesInstances performSelector:@selector(_web_checkLastReferenceForIdentifier:) withObject:ident afterDelay:.1];
}

- (void)_postPreferencesChangesNotification
{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebPreferencesChangedNotification object:self
                    userInfo:nil];
}

+ (NSArray *)_userDefaultsKeysForIB
{
    return [NSArray arrayWithObjects:
        WebKitStandardFontPreferenceKey,
        WebKitFixedFontPreferenceKey,
        WebKitSerifFontPreferenceKey,
        WebKitSansSerifFontPreferenceKey,
        WebKitCursiveFontPreferenceKey,
        WebKitFantasyFontPreferenceKey,
        WebKitMinimumFontSizePreferenceKey,
        WebKitDefaultFontSizePreferenceKey,
        WebKitDefaultFixedFontSizePreferenceKey,
        WebKitDefaultTextEncodingNamePreferenceKey,
        WebKitJavaEnabledPreferenceKey,
        WebKitJavaScriptEnabledPreferenceKey,
        WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
        WebKitPluginsEnabledPreferenceKey,
        WebKitAllowAnimatedImagesPreferenceKey,
        WebKitAllowAnimatedImageLoopingPreferenceKey,
        WebKitDisplayImagesKey,
        nil
    ];
}

@end

@implementation NSMutableDictionary (WebPrivate)
- (void)_web_checkLastReferenceForIdentifier:(NSString *)identifier
{
    WebPreferences *instance = [webPreferencesInstances objectForKey:identifier];
    if ([instance retainCount] == 1)
        [webPreferencesInstances removeObjectForKey:identifier];
}
@end

