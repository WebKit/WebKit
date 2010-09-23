/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#import "WebPreferencesPrivate.h"
#import "WebPreferenceKeysPrivate.h"

#import "WebApplicationCache.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitSystemBits.h"
#import "WebKitSystemInterface.h"
#import "WebKitVersionChecks.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSURLExtras.h"
#import <WebCore/ApplicationCacheStorage.h>

NSString *WebPreferencesChangedNotification = @"WebPreferencesChangedNotification";
NSString *WebPreferencesRemovedNotification = @"WebPreferencesRemovedNotification";

#define KEY(x) (_private->identifier ? [_private->identifier stringByAppendingString:(x)] : (x))

enum { WebPreferencesVersion = 1 };

static WebPreferences *_standardPreferences;
static NSMutableDictionary *webPreferencesInstances;

static bool contains(const char* const array[], int count, const char* item)
{
    if (!item)
        return false;

    for (int i = 0; i < count; i++)
        if (!strcasecmp(array[i], item))
            return true;
    return false;
}

static WebCacheModel cacheModelForMainBundle(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // Apps that probably need the small setting
    static const char* const documentViewerIDs[] = {
        "Microsoft/com.microsoft.Messenger",
        "com.adiumX.adiumX", 
        "com.alientechnology.Proteus",
        "com.apple.Dashcode",
        "com.apple.iChat", 
        "com.barebones.bbedit", 
        "com.barebones.textwrangler",
        "com.barebones.yojimbo",
        "com.equinux.iSale4",
        "com.growl.growlframework",
        "com.intrarts.PandoraMan",
        "com.karelia.Sandvox",
        "com.macromates.textmate",
        "com.realmacsoftware.rapidweaverpro",
        "com.red-sweater.marsedit",
        "com.yahoo.messenger3",
        "de.codingmonkeys.SubEthaEdit",
        "fi.karppinen.Pyro",
        "info.colloquy", 
        "kungfoo.tv.ecto",
    };

    // Apps that probably need the medium setting
    static const char* const documentBrowserIDs[] = {
        "com.apple.Dictionary",
        "com.apple.Xcode",
        "com.apple.dashboard.client", 
        "com.apple.helpviewer",
        "com.culturedcode.xyle",
        "com.macrabbit.CSSEdit",
        "com.panic.Coda",
        "com.ranchero.NetNewsWire",
        "com.thinkmac.NewsLife",
        "org.xlife.NewsFire",
        "uk.co.opencommunity.vienna2",
    };

    // Apps that probably need the large setting
    static const char* const primaryWebBrowserIDs[] = {
        "com.app4mac.KidsBrowser"
        "com.app4mac.wKiosk",
        "com.freeverse.bumpercar",
        "com.omnigroup.OmniWeb5",
        "com.sunrisebrowser.Sunrise",
        "net.hmdt-web.Shiira",
    };

    WebCacheModel cacheModel;

    const char* bundleID = [[[NSBundle mainBundle] bundleIdentifier] UTF8String];
    if (contains(documentViewerIDs, sizeof(documentViewerIDs) / sizeof(documentViewerIDs[0]), bundleID))
        cacheModel = WebCacheModelDocumentViewer;
    else if (contains(documentBrowserIDs, sizeof(documentBrowserIDs) / sizeof(documentBrowserIDs[0]), bundleID))
        cacheModel = WebCacheModelDocumentBrowser;
    else if (contains(primaryWebBrowserIDs, sizeof(primaryWebBrowserIDs) / sizeof(primaryWebBrowserIDs[0]), bundleID))
        cacheModel = WebCacheModelPrimaryWebBrowser;
    else {
        bool isLegacyApp = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_CACHE_MODEL_API);
        if (isLegacyApp)
            cacheModel = WebCacheModelDocumentBrowser; // To avoid regressions in apps that depended on old WebKit's large cache.
        else
            cacheModel = WebCacheModelDocumentViewer; // To save memory.
    }

    [pool drain];

    return cacheModel;
}

@interface WebPreferencesPrivate : NSObject
{
@public
    NSMutableDictionary *values;
    NSString *identifier;
    NSString *IBCreatorID;
    BOOL autosaves;
    BOOL automaticallyDetectsCacheModel;
    unsigned numWebViews;
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
- (void)_setLongLongValue:(long long)value forKey:(NSString *)key;
- (long long)_longLongValueForKey:(NSString *)key;
- (void)_setUnsignedLongLongValue:(unsigned long long)value forKey:(NSString *)key;
- (unsigned long long)_unsignedLongLongValueForKey:(NSString *)key;
@end

@implementation WebPreferences

- (id)init
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
    _private->automaticallyDetectsCacheModel = YES;

    [[self class] _setInstance:self forIdentifier:_private->identifier];

    [self _postPreferencesChangesNotification];

    return self;
}

- (id)initWithCoder:(NSCoder *)decoder
{
    self = [super init];
    if (!self)
        return nil;

    _private = [[WebPreferencesPrivate alloc] init];
    _private->IBCreatorID = [[WebPreferences _IBCreatorID] retain];
    _private->automaticallyDetectsCacheModel = YES;

    @try {
        id identifier = nil;
        id values = nil;
        if ([decoder allowsKeyedCoding]) {
            identifier = [decoder decodeObjectForKey:@"Identifier"];
            values = [decoder decodeObjectForKey:@"Values"];
        } else {
            int version;
            [decoder decodeValueOfObjCType:@encode(int) at:&version];
            if (version == 1) {
                identifier = [decoder decodeObject];
                values = [decoder decodeObject];
            }
        }

        if ([identifier isKindOfClass:[NSString class]])
            _private->identifier = [identifier copy];
        if ([values isKindOfClass:[NSDictionary class]])
            _private->values = [values mutableCopy]; // ensure dictionary is mutable

        LOG(Encoding, "Identifier = %@, Values = %@\n", _private->identifier, _private->values);
    } @catch(id) {
        [self release];
        return nil;
    }

    // If we load a nib multiple times, or have instances in multiple
    // nibs with the same name, the first guy up wins.
    WebPreferences *instance = [[self class] _getInstanceForIdentifier:_private->identifier];
    if (instance) {
        [self release];
        self = [instance retain];
    } else {
        [[self class] _setInstance:self forIdentifier:_private->identifier];
    }

    return self;
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
    }

    return _standardPreferences;
}

// if we ever have more than one WebPreferences object, this would move to init
+ (void)initialize
{
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
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
        [NSNumber numberWithBool:NO],   WebKitUsesEncodingDetectorPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUserStyleSheetEnabledPreferenceKey,
        @"",                            WebKitUserStyleSheetLocationPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShouldPrintBackgroundsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitTextAreasAreResizablePreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShrinksStandaloneImagesToFitPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitWebSecurityEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowUniversalAccessFromFileURLsPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowFileAccessFromFileURLsPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitPluginsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDatabasesEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitLocalStorageEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitExperimentalNotificationsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImagesPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImageLoopingPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDisplayImagesKey,
        @"1800",                        WebKitBackForwardCacheExpirationIntervalKey,
        [NSNumber numberWithBool:NO],   WebKitTabToLinksPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitPrivateBrowsingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitRespectStandardStyleKeyEquivalentsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowsURLsInToolTipsPreferenceKey,
        @"1",                           WebKitPDFDisplayModePreferenceKey,
        @"0",                           WebKitPDFScaleFactorPreferenceKey,
        @"0",                           WebKitUseSiteSpecificSpoofingPreferenceKey,
        [NSNumber numberWithInt:WebKitEditableLinkDefaultBehavior], WebKitEditableLinkBehaviorPreferenceKey,
        [NSNumber numberWithInt:WebKitEditingMacBehavior], WebKitEditingBehaviorPreferenceKey,
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
        [NSNumber numberWithInt:WebTextDirectionSubmenuAutomaticallyIncluded],
#else
        [NSNumber numberWithInt:WebTextDirectionSubmenuNeverIncluded],
#endif
                                        WebKitTextDirectionSubmenuInclusionBehaviorPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitDOMPasteAllowedPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitUsesPageCachePreferenceKey,
        [NSNumber numberWithInt:cacheModelForMainBundle()], WebKitCacheModelPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitDeveloperExtrasEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAuthorAndUserStylesEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitApplicationChromeModeEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitWebArchiveDebugModeEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitLocalFileContentSniffingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitOfflineWebApplicationCacheEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitZoomsTextOnlyPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitJavaScriptCanAccessClipboardPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitXSSAuditorEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAcceleratedCompositingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowDebugBordersPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowRepaintCounterPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitWebGLEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitAccelerated2dCanvasEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUsesProxiedOpenPanelPreferenceKey,
        [NSNumber numberWithUnsignedInt:4], WebKitPluginAllowedRunTimePreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitFrameFlatteningEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDNSPrefetchingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitFullScreenEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitMemoryInfoEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitHyperlinkAuditingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUsePreHTML5ParserQuirksKey,
        [NSNumber numberWithLongLong:WebCore::ApplicationCacheStorage::noQuota()], WebKitApplicationCacheTotalQuota,
        [NSNumber numberWithLongLong:WebCore::ApplicationCacheStorage::noQuota()], WebKitApplicationCacheDefaultOriginQuota,
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
    return [o respondsToSelector:@selector(floatValue)] ? [o floatValue] : 0.0f;
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

- (long long)_longLongValueForKey:(NSString *)key
{
    id o = [self _valueForKey:key];
    return [o respondsToSelector:@selector(longLongValue)] ? [o longLongValue] : 0;
}

- (void)_setLongLongValue:(long long)value forKey:(NSString *)key
{
    if ([self _longLongValueForKey:key] == value)
        return;
    NSString *_key = KEY(key);
    [_private->values _webkit_setLongLong:value forKey:_key];
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithLongLong:value] forKey:_key];
    [self _postPreferencesChangesNotification];
}

- (unsigned long long)_unsignedLongLongValueForKey:(NSString *)key
{
    id o = [self _valueForKey:key];
    return [o respondsToSelector:@selector(unsignedLongLongValue)] ? [o unsignedLongLongValue] : 0;
}

- (void)_setUnsignedLongLongValue:(unsigned long long)value forKey:(NSString *)key
{
    if ([self _unsignedLongLongValueForKey:key] == value)
        return;
    NSString *_key = KEY(key);
    [_private->values _webkit_setUnsignedLongLong:value forKey:_key];
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithUnsignedLongLong:value] forKey:_key];
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

    if (!locationString)
        locationString = @"";

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

- (void)setAllowsAnimatedImages:(BOOL)flag
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

- (void)setAutosaves:(BOOL)flag
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

- (void)setUsesPageCache:(BOOL)usesPageCache
{
    [self _setBoolValue:usesPageCache forKey:WebKitUsesPageCachePreferenceKey];
}

- (BOOL)usesPageCache
{
    return [self _boolValueForKey:WebKitUsesPageCachePreferenceKey];
}

- (void)setCacheModel:(WebCacheModel)cacheModel
{
    [self _setIntegerValue:cacheModel forKey:WebKitCacheModelPreferenceKey];
    [self setAutomaticallyDetectsCacheModel:NO];
}

- (WebCacheModel)cacheModel
{
    return [self _integerValueForKey:WebKitCacheModelPreferenceKey];
}

@end

@implementation WebPreferences (WebPrivate)

- (BOOL)isDNSPrefetchingEnabled
{
    return [self _boolValueForKey:WebKitDNSPrefetchingEnabledPreferenceKey];
}

- (void)setDNSPrefetchingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitDNSPrefetchingEnabledPreferenceKey];
}

- (BOOL)developerExtrasEnabled
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:@"DisableWebKitDeveloperExtras"])
        return NO;
#ifdef NDEBUG
    if ([defaults boolForKey:@"WebKitDeveloperExtras"] || [defaults boolForKey:@"IncludeDebugMenu"])
        return YES;
    return [self _boolValueForKey:WebKitDeveloperExtrasEnabledPreferenceKey];
#else
    return YES; // always enable in debug builds
#endif
}

- (void)setDeveloperExtrasEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitDeveloperExtrasEnabledPreferenceKey];
}

- (BOOL)authorAndUserStylesEnabled
{
    return [self _boolValueForKey:WebKitAuthorAndUserStylesEnabledPreferenceKey];
}

- (void)setAuthorAndUserStylesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAuthorAndUserStylesEnabledPreferenceKey];
}

- (BOOL)applicationChromeModeEnabled
{
    return [self _boolValueForKey:WebKitApplicationChromeModeEnabledPreferenceKey];
}

- (void)setApplicationChromeModeEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitApplicationChromeModeEnabledPreferenceKey];
}

- (BOOL)webArchiveDebugModeEnabled
{
    return [self _boolValueForKey:WebKitWebArchiveDebugModeEnabledPreferenceKey];
}

- (void)setWebArchiveDebugModeEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitWebArchiveDebugModeEnabledPreferenceKey];
}

- (BOOL)localFileContentSniffingEnabled
{
    return [self _boolValueForKey:WebKitLocalFileContentSniffingEnabledPreferenceKey];
}

- (void)setLocalFileContentSniffingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitLocalFileContentSniffingEnabledPreferenceKey];
}

- (BOOL)offlineWebApplicationCacheEnabled
{
    return [self _boolValueForKey:WebKitOfflineWebApplicationCacheEnabledPreferenceKey];
}

- (void)setOfflineWebApplicationCacheEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitOfflineWebApplicationCacheEnabledPreferenceKey];
}

- (BOOL)zoomsTextOnly
{
    return [self _boolValueForKey:WebKitZoomsTextOnlyPreferenceKey];
}

- (void)setZoomsTextOnly:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitZoomsTextOnlyPreferenceKey];
}

- (BOOL)javaScriptCanAccessClipboard
{
    return [self _boolValueForKey:WebKitJavaScriptCanAccessClipboardPreferenceKey];
}

- (void)setJavaScriptCanAccessClipboard:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitJavaScriptCanAccessClipboardPreferenceKey];
}

- (BOOL)isXSSAuditorEnabled
{
    return [self _boolValueForKey:WebKitXSSAuditorEnabledPreferenceKey];
}

- (void)setXSSAuditorEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitXSSAuditorEnabledPreferenceKey];
}

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

- (BOOL)shrinksStandaloneImagesToFit
{
    return [self _boolValueForKey:WebKitShrinksStandaloneImagesToFitPreferenceKey];
}

- (void)setShrinksStandaloneImagesToFit:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShrinksStandaloneImagesToFitPreferenceKey];
}

- (BOOL)automaticallyDetectsCacheModel
{
    return _private->automaticallyDetectsCacheModel;
}

- (void)setAutomaticallyDetectsCacheModel:(BOOL)automaticallyDetectsCacheModel
{
    _private->automaticallyDetectsCacheModel = automaticallyDetectsCacheModel;
}

- (BOOL)usesEncodingDetector
{
    return [self _boolValueForKey: WebKitUsesEncodingDetectorPreferenceKey];
}

- (void)setUsesEncodingDetector:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitUsesEncodingDetectorPreferenceKey];
}

- (BOOL)isWebSecurityEnabled
{
    return [self _boolValueForKey: WebKitWebSecurityEnabledPreferenceKey];
}

- (void)setWebSecurityEnabled:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitWebSecurityEnabledPreferenceKey];
}

- (BOOL)allowUniversalAccessFromFileURLs
{
    return [self _boolValueForKey: WebKitAllowUniversalAccessFromFileURLsPreferenceKey];
}

- (void)setAllowUniversalAccessFromFileURLs:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitAllowUniversalAccessFromFileURLsPreferenceKey];
}

- (BOOL)allowFileAccessFromFileURLs
{
    return [self _boolValueForKey: WebKitAllowFileAccessFromFileURLsPreferenceKey];
}

- (void)setAllowFileAccessFromFileURLs:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitAllowFileAccessFromFileURLsPreferenceKey];
}

- (NSTimeInterval)_backForwardCacheExpirationInterval
{
    // FIXME: There's probably no good reason to read from the standard user defaults instead of self.
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

- (int64_t)applicationCacheTotalQuota
{
    return [self _longLongValueForKey:WebKitApplicationCacheTotalQuota];
}

- (void)setApplicationCacheTotalQuota:(int64_t)quota
{
    [self _setLongLongValue:quota forKey:WebKitApplicationCacheTotalQuota];

    // Application Cache Preferences are stored on the global cache storage manager, not in Settings.
    [WebApplicationCache setMaximumSize:quota];
}

- (int64_t)applicationCacheDefaultOriginQuota
{
    return [self _longLongValueForKey:WebKitApplicationCacheDefaultOriginQuota];
}

- (void)setApplicationCacheDefaultOriginQuota:(int64_t)quota
{
    [self _setLongLongValue:quota forKey:WebKitApplicationCacheDefaultOriginQuota];
}

- (PDFDisplayMode)PDFDisplayMode
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

- (WebKitEditableLinkBehavior)editableLinkBehavior
{
    WebKitEditableLinkBehavior value = static_cast<WebKitEditableLinkBehavior> ([self _integerValueForKey:WebKitEditableLinkBehaviorPreferenceKey]);
    if (value != WebKitEditableLinkDefaultBehavior &&
        value != WebKitEditableLinkAlwaysLive &&
        value != WebKitEditableLinkNeverLive &&
        value != WebKitEditableLinkOnlyLiveWithShiftKey &&
        value != WebKitEditableLinkLiveWhenNotFocused) {
        // ensure that a valid result is returned
        value = WebKitEditableLinkDefaultBehavior;
    }
    
    return value;
}

- (void)setEditableLinkBehavior:(WebKitEditableLinkBehavior)behavior
{
    [self _setIntegerValue:behavior forKey:WebKitEditableLinkBehaviorPreferenceKey];
}

- (WebTextDirectionSubmenuInclusionBehavior)textDirectionSubmenuInclusionBehavior
{
    WebTextDirectionSubmenuInclusionBehavior value = static_cast<WebTextDirectionSubmenuInclusionBehavior>([self _integerValueForKey:WebKitTextDirectionSubmenuInclusionBehaviorPreferenceKey]);
    if (value != WebTextDirectionSubmenuNeverIncluded &&
        value != WebTextDirectionSubmenuAutomaticallyIncluded &&
        value != WebTextDirectionSubmenuAlwaysIncluded) {
        // Ensure that a valid result is returned.
        value = WebTextDirectionSubmenuNeverIncluded;
    }
    return value;
}

- (void)setTextDirectionSubmenuInclusionBehavior:(WebTextDirectionSubmenuInclusionBehavior)behavior
{
    [self _setIntegerValue:behavior forKey:WebKitTextDirectionSubmenuInclusionBehaviorPreferenceKey];
}

- (BOOL)_useSiteSpecificSpoofing
{
    return [self _boolValueForKey:WebKitUseSiteSpecificSpoofingPreferenceKey];
}

- (void)_setUseSiteSpecificSpoofing:(BOOL)newValue
{
    [self _setBoolValue:newValue forKey:WebKitUseSiteSpecificSpoofingPreferenceKey];
}

- (BOOL)databasesEnabled
{
    return [self _boolValueForKey:WebKitDatabasesEnabledPreferenceKey];
}

- (void)setDatabasesEnabled:(BOOL)databasesEnabled
{
    [self _setBoolValue:databasesEnabled forKey:WebKitDatabasesEnabledPreferenceKey];
}

- (BOOL)localStorageEnabled
{
    return [self _boolValueForKey:WebKitLocalStorageEnabledPreferenceKey];
}

- (void)setLocalStorageEnabled:(BOOL)localStorageEnabled
{
    [self _setBoolValue:localStorageEnabled forKey:WebKitLocalStorageEnabledPreferenceKey];
}

- (BOOL)experimentalNotificationsEnabled
{
    return [self _boolValueForKey:WebKitExperimentalNotificationsEnabledPreferenceKey];
}

- (void)setExperimentalNotificationsEnabled:(BOOL)experimentalNotificationsEnabled
{
    [self _setBoolValue:experimentalNotificationsEnabled forKey:WebKitExperimentalNotificationsEnabledPreferenceKey];
}

+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)ident
{
    LOG(Encoding, "requesting for %@\n", ident);

    if (!ident)
        return _standardPreferences;
    
    WebPreferences *instance = [webPreferencesInstances objectForKey:[self _concatenateKeyWithIBCreatorID:ident]];

    return instance;
}

+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)ident
{
    if (!webPreferencesInstances)
        webPreferencesInstances = [[NSMutableDictionary alloc] init];
    if (ident) {
        [webPreferencesInstances setObject:instance forKey:[self _concatenateKeyWithIBCreatorID:ident]];
        LOG(Encoding, "recording %p for %@\n", instance, [self _concatenateKeyWithIBCreatorID:ident]);
    }
}

+ (void)_checkLastReferenceForIdentifier:(id)identifier
{
    // FIXME: This won't work at all under garbage collection because retainCount returns a constant.
    // We may need to change WebPreferences API so there's an explicit way to end the lifetime of one.
    WebPreferences *instance = [webPreferencesInstances objectForKey:identifier];
    if ([instance retainCount] == 1)
        [webPreferencesInstances removeObjectForKey:identifier];
}

+ (void)_removeReferenceForIdentifier:(NSString *)ident
{
    if (ident)
        [self performSelector:@selector(_checkLastReferenceForIdentifier:) withObject:[self _concatenateKeyWithIBCreatorID:ident] afterDelay:0.1];
}

- (void)_postPreferencesChangesNotification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }

    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebPreferencesChangedNotification object:self
                    userInfo:nil];
}

+ (CFStringEncoding)_systemCFStringEncoding
{
    return WKGetWebDefaultCFStringEncoding();
}

+ (void)_setInitialDefaultTextEncodingToSystemEncoding
{
    NSString *systemEncodingName = (NSString *)CFStringConvertEncodingToIANACharSetName([self _systemCFStringEncoding]);

    // CFStringConvertEncodingToIANACharSetName() returns cp949 for kTextEncodingDOSKorean AKA "extended EUC-KR" AKA windows-949.
    // ICU uses this name for a different encoding, so we need to change the name to a value that actually gives us windows-949.
    // In addition, this value must match what is used in Safari, see <rdar://problem/5579292>.
    // On some OS versions, the result is CP949 (uppercase).
    if ([systemEncodingName _webkit_isCaseInsensitiveEqualToString:@"cp949"])
        systemEncodingName = @"ks_c_5601-1987";
    [[NSUserDefaults standardUserDefaults] registerDefaults:
        [NSDictionary dictionaryWithObject:systemEncodingName forKey:WebKitDefaultTextEncodingNamePreferenceKey]];
}

static NSString *classIBCreatorID = nil;

+ (void)_setIBCreatorID:(NSString *)string
{
    NSString *old = classIBCreatorID;
    classIBCreatorID = [string copy];
    [old release];
}

- (BOOL)isDOMPasteAllowed
{
    return [self _boolValueForKey:WebKitDOMPasteAllowedPreferenceKey];
}

- (void)setDOMPasteAllowed:(BOOL)DOMPasteAllowed
{
    [self _setBoolValue:DOMPasteAllowed forKey:WebKitDOMPasteAllowedPreferenceKey];
}

- (NSString *)_localStorageDatabasePath
{
    return [[self _stringValueForKey:WebKitLocalStorageDatabasePathPreferenceKey] stringByStandardizingPath];
}

- (void)_setLocalStorageDatabasePath:(NSString *)path
{
    [self _setStringValue:[path stringByStandardizingPath] forKey:WebKitLocalStorageDatabasePathPreferenceKey];
}

- (NSString *)_ftpDirectoryTemplatePath
{
    return [[self _stringValueForKey:WebKitFTPDirectoryTemplatePath] stringByStandardizingPath];
}

- (void)_setFTPDirectoryTemplatePath:(NSString *)path
{
    [self _setStringValue:[path stringByStandardizingPath] forKey:WebKitFTPDirectoryTemplatePath];
}

- (BOOL)_forceFTPDirectoryListings
{
    return [self _boolValueForKey:WebKitForceFTPDirectoryListings];
}

- (void)_setForceFTPDirectoryListings:(BOOL)force
{
    [self _setBoolValue:force forKey:WebKitForceFTPDirectoryListings];
}

- (BOOL)acceleratedCompositingEnabled
{
    return [self _boolValueForKey:WebKitAcceleratedCompositingEnabledPreferenceKey];
}

- (void)setAcceleratedCompositingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitAcceleratedCompositingEnabledPreferenceKey];
}

- (BOOL)showDebugBorders
{
    return [self _boolValueForKey:WebKitShowDebugBordersPreferenceKey];
}

- (void)setShowDebugBorders:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitShowDebugBordersPreferenceKey];
}

- (BOOL)showRepaintCounter
{
    return [self _boolValueForKey:WebKitShowRepaintCounterPreferenceKey];
}

- (void)setShowRepaintCounter:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitShowRepaintCounterPreferenceKey];
}

- (BOOL)webGLEnabled
{
    return [self _boolValueForKey:WebKitWebGLEnabledPreferenceKey];
}

- (void)setWebGLEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitWebGLEnabledPreferenceKey];
}

- (BOOL)accelerated2dCanvasEnabled
{
    return [self _boolValueForKey:WebKitAccelerated2dCanvasEnabledPreferenceKey];
}

- (void)setAccelerated2dCanvasEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitAccelerated2dCanvasEnabledPreferenceKey];
}

- (BOOL)usesProxiedOpenPanel
{
    return [self _boolValueForKey:WebKitUsesProxiedOpenPanelPreferenceKey];
}

- (void)setUsesProxiedOpenPanel:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitUsesProxiedOpenPanelPreferenceKey];
}

- (unsigned)pluginAllowedRunTime
{
    return [self _integerValueForKey:WebKitPluginAllowedRunTimePreferenceKey];
}

- (void)setPluginAllowedRunTime:(unsigned)allowedRunTime
{
    return [self _setIntegerValue:allowedRunTime forKey:WebKitPluginAllowedRunTimePreferenceKey];
}

- (BOOL)isFrameFlatteningEnabled
{
    return [self _boolValueForKey:WebKitFrameFlatteningEnabledPreferenceKey];
}

- (void)setFrameFlatteningEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitFrameFlatteningEnabledPreferenceKey];
}

- (BOOL)paginateDuringLayoutEnabled
{
    return [self _boolValueForKey:WebKitPaginateDuringLayoutEnabledPreferenceKey];
}

- (void)setPaginateDuringLayoutEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitPaginateDuringLayoutEnabledPreferenceKey];
}

- (BOOL)memoryInfoEnabled
{
    return [self _boolValueForKey:WebKitMemoryInfoEnabledPreferenceKey];
}

- (void)setMemoryInfoEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMemoryInfoEnabledPreferenceKey];
}

- (BOOL)hyperlinkAuditingEnabled
{
    return [self _boolValueForKey:WebKitHyperlinkAuditingEnabledPreferenceKey];
}

- (void)setHyperlinkAuditingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitHyperlinkAuditingEnabledPreferenceKey];
}

- (WebKitEditingBehavior)editingBehavior
{
    return static_cast<WebKitEditingBehavior>([self _integerValueForKey:WebKitEditingBehaviorPreferenceKey]);
}

- (void)setEditingBehavior:(WebKitEditingBehavior)behavior
{
    [self _setIntegerValue:behavior forKey:WebKitEditingBehaviorPreferenceKey];
}

- (BOOL)usePreHTML5ParserQuirks
{
    return [self _boolValueForKey:WebKitUsePreHTML5ParserQuirksKey];
}

- (void)setUsePreHTML5ParserQuirks:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitUsePreHTML5ParserQuirksKey];
}

- (void)didRemoveFromWebView
{
    ASSERT(_private->numWebViews);
    if (--_private->numWebViews == 0)
        [[NSNotificationCenter defaultCenter]
            postNotificationName:WebPreferencesRemovedNotification
                          object:self
                        userInfo:nil];
}

- (void)willAddToWebView
{
    ++_private->numWebViews;
}

- (void)_setPreferenceForTestWithValue:(NSString *)value forKey:(NSString *)key
{
    [self _setStringValue:value forKey:key];
}

- (void)setFullScreenEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitFullScreenEnabledPreferenceKey];
}

- (BOOL)fullScreenEnabled
{
    return [self _boolValueForKey:WebKitFullScreenEnabledPreferenceKey];
}

+ (void)setWebKitLinkTimeVersion:(int)version
{
    setWebKitLinkTimeVersion(version);
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
