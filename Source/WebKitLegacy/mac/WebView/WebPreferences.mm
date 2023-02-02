/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "WebPreferencesInternal.h"

#import "NetworkStorageSessionMap.h"
#import "TestingFunctions.h"
#import "WebApplicationCache.h"
#import "WebFeature.h"
#import "WebFrameNetworkingContext.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitVersionChecks.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSURLExtras.h"
#import "WebPreferenceKeysPrivate.h"
#import "WebPreferencesDefinitions.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/ApplicationCacheStorage.h>
#import <WebCore/AudioSession.h>
#import <WebCore/MediaPlayerEnums.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/Settings.h>
#import <WebCore/WebCoreJITOperations.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/text/TextEncodingRegistry.h>
#import <wtf/BlockPtr.h>
#import <wtf/Compiler.h>
#import <wtf/MainThread.h>
#import <wtf/OptionSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

using namespace WebCore;

#if PLATFORM(IOS_FAMILY)
#import <WebCore/GraphicsContext.h>
#import <WebCore/WebCoreThreadMessage.h>
#endif

NSString *WebPreferencesChangedNotification = @"WebPreferencesChangedNotification";
NSString *WebPreferencesRemovedNotification = @"WebPreferencesRemovedNotification";
NSString *WebPreferencesChangedInternalNotification = @"WebPreferencesChangedInternalNotification";
NSString *WebPreferencesCacheModelChangedInternalNotification = @"WebPreferencesCacheModelChangedInternalNotification";

#define KEY(x) (_private->identifier ? [_private->identifier.get() stringByAppendingString:(x)] : (x))

enum { WebPreferencesVersion = 1 };

static RetainPtr<WebPreferences>& standardPreferences()
{
    static NeverDestroyed<RetainPtr<WebPreferences>> standardPreferences;
    return standardPreferences;
}

static RetainPtr<NSMutableDictionary>& webPreferencesInstances()
{
    static NeverDestroyed<RetainPtr<NSMutableDictionary>> webPreferencesInstances;
    return webPreferencesInstances;
}

static unsigned webPreferencesInstanceCountWithPrivateBrowsingEnabled;

template<unsigned size> static bool contains(const char* const (&array)[size], const char* item)
{
    if (!item)
        return false;
    for (auto* string : array) {
        if (equalIgnoringASCIICase(string, item))
            return true;
    }
    return false;
}

static WebCacheModel cacheModelForMainBundle(NSString *bundleIdentifier)
{
    @autoreleasepool {
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
            "com.app4mac.KidsBrowser",
            "com.app4mac.wKiosk",
            "com.freeverse.bumpercar",
            "com.omnigroup.OmniWeb5",
            "com.sunrisebrowser.Sunrise",
            "net.hmdt-web.Shiira",
        };

        const char* bundleID = [bundleIdentifier UTF8String];
        if (contains(documentViewerIDs, bundleID))
            return WebCacheModelDocumentViewer;
        if (contains(documentBrowserIDs, bundleID))
            return WebCacheModelDocumentBrowser;
        if (contains(primaryWebBrowserIDs, bundleID))
            return WebCacheModelPrimaryWebBrowser;

        bool isLinkedAgainstWebKit = WebKitLinkedOnOrAfter(0);
        if (!isLinkedAgainstWebKit)
            return WebCacheModelDocumentViewer; // Apps that don't link against WebKit probably aren't meant to be browsers.

#if !PLATFORM(IOS_FAMILY)
        bool isLegacyApp = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_CACHE_MODEL_API);
#else
        bool isLegacyApp = false;
#endif
        if (isLegacyApp)
            return WebCacheModelDocumentBrowser; // To avoid regressions in apps that depended on old WebKit's large cache.

        return WebCacheModelDocumentViewer; // To save memory.
    }
}

#if ENABLE(BUILD_FOR_TESTING)
WebCacheModel TestWebPreferencesCacheModelForMainBundle(NSString *bundleIdentifier)
{
    return cacheModelForMainBundle(bundleIdentifier);
}
#endif // ENABLE(BUILD_FOR_TESTING)

@interface WebPreferences ()
- (void)_postCacheModelChangedNotification;
@end

@interface WebPreferences (WebInternal)
+ (NSString *)_concatenateKeyWithIBCreatorID:(NSString *)key;
+ (NSString *)_IBCreatorID;
@end

enum class UpdateAfterBatchType : uint8_t {
    API         = 1 << 0,
    Internal    = 1 << 1
};

struct WebPreferencesPrivate
{
public:
    WebPreferencesPrivate()
#if PLATFORM(IOS_FAMILY)
        : readWriteQueue { adoptNS(dispatch_queue_create("com.apple.WebPreferences.ReadWriteQueue", DISPATCH_QUEUE_CONCURRENT)) }
#endif
    {
    }

#if PLATFORM(IOS_FAMILY)
    RetainPtr<dispatch_queue_t> readWriteQueue;
#endif
    RetainPtr<NSMutableDictionary> values;
    RetainPtr<NSString> identifier;
    BOOL inPrivateBrowsing { NO };
    BOOL autosaves { NO };
    BOOL automaticallyDetectsCacheModel { NO };
    unsigned numWebViews { 0 };
    unsigned updateBatchCount { 0 };
    OptionSet<UpdateAfterBatchType> updateAfterBatchType;
};

#if PLATFORM(IOS_FAMILY)
@interface WebPreferences ()
- (id)initWithIdentifier:(NSString *)anIdentifier sendChangeNotification:(BOOL)sendChangeNotification;
@end
#endif

@implementation WebPreferences

- (instancetype)init
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

#if PLATFORM(IOS_FAMILY)
- (id)initWithIdentifier:(NSString *)anIdentifier
{
    return [self initWithIdentifier:anIdentifier sendChangeNotification:YES];
}
#endif

#if PLATFORM(IOS_FAMILY)
- (instancetype)initWithIdentifier:(NSString *)anIdentifier sendChangeNotification:(BOOL)sendChangeNotification
#else
- (instancetype)initWithIdentifier:(NSString *)anIdentifier
#endif
{
    WebPreferences *instance = [[self class] _getInstanceForIdentifier:anIdentifier];
    if (instance) {
        [self release];
        return [instance retain];
    }

    self = [super init];
    if (!self)
        return nil;

    _private = new WebPreferencesPrivate;
    _private->values = adoptNS([[NSMutableDictionary alloc] init]);
    _private->identifier = adoptNS([anIdentifier copy]);
    _private->automaticallyDetectsCacheModel = YES;

    [[self class] _setInstance:self forIdentifier:_private->identifier.get()];

    [self _updatePrivateBrowsingStateTo:[self privateBrowsingEnabled]];

#if PLATFORM(IOS_FAMILY)
    if (sendChangeNotification) {
        [self _postPreferencesChangedNotification];
        [self _postCacheModelChangedNotification];
    }
#else
    [self _postPreferencesChangedNotification];
    [self _postCacheModelChangedNotification];
#endif

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super init];
    if (!self)
        return nil;

    _private = new WebPreferencesPrivate;
    _private->automaticallyDetectsCacheModel = YES;

    @try {
        id identifier = nil;
        id values = nil;
        if ([decoder allowsKeyedCoding]) {
            identifier = [decoder decodeObjectForKey:@"Identifier"];
            values = [decoder decodeObjectForKey:@"Values"];
        } else {
            int version;
            [decoder decodeValueOfObjCType:@encode(int) at:&version size:sizeof(int)];
            if (version == 1) {
                identifier = [decoder decodeObject];
                values = [decoder decodeObject];
            }
        }

        if ([identifier isKindOfClass:[NSString class]])
            _private->identifier = adoptNS([identifier copy]);
        if ([values isKindOfClass:[NSDictionary class]])
            _private->values = adoptNS([values mutableCopy]); // ensure dictionary is mutable

        LOG(Encoding, "Identifier = %@, Values = %@\n", _private->identifier.get(), _private->values.get());
    } @catch(id) {
        [self release];
        return nil;
    }

    // If we load a nib multiple times, or have instances in multiple
    // nibs with the same name, the first guy up wins.
    WebPreferences *instance = [[self class] _getInstanceForIdentifier:_private->identifier.get()];
    if (instance) {
        [self release];
        self = [instance retain];
    } else {
        [[self class] _setInstance:self forIdentifier:_private->identifier.get()];
        [self _updatePrivateBrowsingStateTo:[self privateBrowsingEnabled]];
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    if ([encoder allowsKeyedCoding]){
        [encoder encodeObject:_private->identifier.get() forKey:@"Identifier"];
#if PLATFORM(IOS_FAMILY)
        dispatch_sync(_private->readWriteQueue.get(), ^{
#endif
        [encoder encodeObject:_private->values.get() forKey:@"Values"];
        LOG (Encoding, "Identifier = %@, Values = %@\n", _private->identifier.get(), _private->values.get());
#if PLATFORM(IOS_FAMILY)
        });
#endif
    }
    else {
        int version = WebPreferencesVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:_private->identifier.get()];
#if PLATFORM(IOS_FAMILY)
        dispatch_sync(_private->readWriteQueue.get(), ^{
#endif
        [encoder encodeObject:_private->values.get()];
#if PLATFORM(IOS_FAMILY)
        });
#endif
    }
}

+ (WebPreferences *)standardPreferences
{
#if !PLATFORM(IOS_FAMILY)
    if (!standardPreferences()) {
        standardPreferences() = adoptNS([[WebPreferences alloc] initWithIdentifier:nil]);
        [standardPreferences() setAutosaves:YES];
    }
#else
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        standardPreferences() = adoptNS([[WebPreferences alloc] initWithIdentifier:nil sendChangeNotification:NO]);
        [standardPreferences() _postPreferencesChangedNotification];
        [standardPreferences() setAutosaves:YES];
    });
#endif

    return standardPreferences().get();
}

// if we ever have more than one WebPreferences object, this would move to init
+ (void)initialize
{
#if PLATFORM(MAC)
    JSC::initialize();
    WTF::initializeMainThread();
    WebCore::populateJITOperations();
#endif

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        INITIALIZE_DEFAULT_PREFERENCES_DICTIONARY_FROM_GENERATED_PREFERENCES

        @NO, WebKitUserStyleSheetEnabledPreferenceKey,
        @"", WebKitUserStyleSheetLocationPreferenceKey,
        @YES, WebKitAllowAnimatedImagesPreferenceKey,
        @YES, WebKitAllowAnimatedImageLoopingPreferenceKey,
        @"1800", WebKitBackForwardCacheExpirationIntervalKey,
        @NO, WebKitPrivateBrowsingEnabledPreferenceKey,
        @(cacheModelForMainBundle([[NSBundle mainBundle] bundleIdentifier])), WebKitCacheModelPreferenceKey,
        @YES, WebKitZoomsTextOnlyPreferenceKey,
        [NSNumber numberWithLongLong:ApplicationCacheStorage::noQuota()], WebKitApplicationCacheTotalQuota,

        // FIXME: Are these relevent to WebKitLegacy? If not, we should remove them.
        @NO, WebKitResourceLoadStatisticsEnabledPreferenceKey,
        @NO, WebKitDebugInAppBrowserPrivacyEnabledPreferenceKey,

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        @"~/Library/WebKit/MediaKeys", WebKitMediaKeysStorageDirectoryKey,
#endif

#if PLATFORM(MAC)
        @NO, WebKitRespectStandardStyleKeyEquivalentsPreferenceKey,
        @"1", WebKitPDFDisplayModePreferenceKey,
        @"0", WebKitPDFScaleFactorPreferenceKey,
        @(WebTextDirectionSubmenuAutomaticallyIncluded), WebKitTextDirectionSubmenuInclusionBehaviorPreferenceKey,
        [NSNumber numberWithLongLong:ApplicationCacheStorage::noQuota()], WebKitApplicationCacheDefaultOriginQuota,
#endif

#if PLATFORM(IOS_FAMILY)
        @NO, WebKitStorageTrackerEnabledPreferenceKey,
        @(static_cast<unsigned>(AudioSession::CategoryType::None)), WebKitAudioSessionCategoryOverride,

        // Per-Origin Quota on iOS is 25MB. When the quota is reached for a particular origin
        // the quota for that origin can be increased. See also webView:exceededApplicationCacheOriginQuotaForSecurityOrigin:totalSpaceNeeded in WebUI/WebUIDelegate.m.
        [NSNumber numberWithLongLong:(25 * 1024 * 1024)], WebKitApplicationCacheDefaultOriginQuota,

        @NO, WebKitAlwaysRequestGeolocationPermissionPreferenceKey,
        @(static_cast<int>(InterpolationQuality::Low)), WebKitInterpolationQualityPreferenceKey,
        @NO, WebKitNetworkDataUsageTrackingEnabledPreferenceKey,
        @"", WebKitNetworkInterfaceNamePreferenceKey,
#endif
        nil];

#if !PLATFORM(IOS_FAMILY)
    // This value shouldn't ever change, which is assumed in the initialization of WebKitPDFDisplayModePreferenceKey above
    ASSERT(kPDFDisplaySinglePageContinuous == 1);
#endif
    [[NSUserDefaults standardUserDefaults] registerDefaults:dict];
}

- (void)dealloc
{
    [self _updatePrivateBrowsingStateTo:NO];

    delete _private;
    [super dealloc];
}

- (NSString *)identifier
{
    return _private->identifier.get();
}

- (id)_valueForKey:(NSString *)key
{
    NSString *_key = KEY(key);
#if PLATFORM(IOS_FAMILY)
    __block id o = nil;
    dispatch_sync(_private->readWriteQueue.get(), ^{
        o = [_private->values.get() objectForKey:_key];
    });
#else
    id o = [_private->values.get() objectForKey:_key];
#endif
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
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
#endif
    [_private->values.get() setObject:value forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:value forKey:_key];
    [self _postPreferencesChangedNotification];
}

- (NSArray<NSString *> *)_stringArrayValueForKey:(NSString *)key
{
    id value = [self _valueForKey:key];
    if (![value isKindOfClass:[NSArray class]])
        return nil;

    NSArray *array = (NSArray *)value;
    for (id object in array) {
        if (![object isKindOfClass:[NSString class]])
            return nil;
    }

    return (NSArray<NSString *> *)array;
}

- (void)_setStringArrayValueForKey:(NSArray<NSString *> *)value forKey:(NSString *)key
{
    NSString *_key = KEY(key);
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
#endif
        [_private->values.get() setObject:value forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:value forKey:_key];
    [self _postPreferencesChangedNotification];
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
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
#endif
    [_private->values.get() setObject:@(value) forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setInteger:value forKey:_key];
    [self _postPreferencesChangedNotification];
}

- (unsigned int)_unsignedIntValueForKey:(NSString *)key
{
    id o = [self _valueForKey:key];
    return [o respondsToSelector:@selector(unsignedIntValue)] ? [o unsignedIntValue] : 0;
}

- (void)_setUnsignedIntValue:(unsigned int)value forKey:(NSString *)key
{
    if ([self _unsignedIntValueForKey:key] == value)
        return;
    NSString *_key = KEY(key);
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
#endif
    [_private->values.get() setObject:@(value) forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:@(value) forKey:_key];
    [self _postPreferencesChangedNotification];
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
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
#endif
    [_private->values.get() setObject:@(value) forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setFloat:value forKey:_key];
    [self _postPreferencesChangedNotification];
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
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
#endif
    [_private->values.get() setObject:@(value) forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setBool:value forKey:_key];
    [self _postPreferencesChangedNotification];
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
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
#endif
    [_private->values.get() setObject:@(value) forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithLongLong:value] forKey:_key];
    [self _postPreferencesChangedNotification];
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
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
#endif
    [_private->values.get() setObject:@(value) forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithUnsignedLongLong:value] forKey:_key];
    [self _postPreferencesChangedNotification];
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

#if !PLATFORM(IOS_FAMILY)
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
        return [NSURL fileURLWithPath:locationString isDirectory:NO];
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
#else

// These methods have had their implementations removed on iOS since it
// is wrong to have such a setting stored in preferences that, when read,
// is applied to all WebViews in a iOS process. Such a design might work
// OK for an application like Safari on Mac OS X, where the only WebViews
// in use display web content in a straightforward manner. However, it is
// wrong for iOS, where WebViews are used for various purposes, like
// text editing, text rendering, and displaying web content.
//
// I have changed the user style sheet mechanism to be a per-WebView
// setting, rather than a per-process preference. This seems to give the
// behavior we want for iOS.

- (BOOL)userStyleSheetEnabled
{
    return NO;
}

- (void)setUserStyleSheetEnabled:(BOOL)flag
{
    // no-op
}

- (NSURL *)userStyleSheetLocation
{
    return nil;
}

- (void)setUserStyleSheetLocation:(NSURL *)URL
{
    // no-op
}
#endif // PLATFORM(IOS_FAMILY)

- (BOOL)shouldPrintBackgrounds
{
    return [self _boolValueForKey: WebKitShouldPrintBackgroundsPreferenceKey];
}

- (void)setShouldPrintBackgrounds:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitShouldPrintBackgroundsPreferenceKey];
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

- (void)setAdditionalSupportedImageTypes:(NSArray<NSString*> *)imageTypes
{
    [self _setStringArrayValueForKey:imageTypes forKey:WebKitAdditionalSupportedImageTypesKey];
}

- (NSArray<NSString *> *)additionalSupportedImageTypes
{
    return [self _stringArrayValueForKey:WebKitAdditionalSupportedImageTypesKey];
}

- (void)setAutosaves:(BOOL)flag
{
    _private->autosaves = flag;
}

- (BOOL)autosaves
{
    return _private->autosaves;
}

#if !PLATFORM(IOS_FAMILY)
- (void)setTabsToLinks:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitTabToLinksPreferenceKey];
}

- (BOOL)tabsToLinks
{
    return [self _boolValueForKey:WebKitTabToLinksPreferenceKey];
}
#endif

- (void)setPrivateBrowsingEnabled:(BOOL)enabled
{
    [self _updatePrivateBrowsingStateTo:enabled];
    [self _setBoolValue:enabled forKey:WebKitPrivateBrowsingEnabledPreferenceKey];
}

- (BOOL)privateBrowsingEnabled
{
    // Changes to private browsing defaults do not have effect on existing WebPreferences, and must be done through -setPrivateBrowsingEnabled.
    // This is needed to accurately track private browsing sessions in the process.
    return _private->inPrivateBrowsing;
}

- (void)_updatePrivateBrowsingStateTo:(BOOL)enabled
{
    if (!_private) {
        ASSERT(!enabled);
        return;
    }

    if (enabled == _private->inPrivateBrowsing)
        return;
    if (enabled > _private->inPrivateBrowsing) {
        WebFrameNetworkingContext::ensurePrivateBrowsingSession();
        ++webPreferencesInstanceCountWithPrivateBrowsingEnabled;
    } else {
        ASSERT(webPreferencesInstanceCountWithPrivateBrowsingEnabled);
        --webPreferencesInstanceCountWithPrivateBrowsingEnabled;
        if (!webPreferencesInstanceCountWithPrivateBrowsingEnabled)
            WebFrameNetworkingContext::destroyPrivateBrowsingSession();
    }
    _private->inPrivateBrowsing = enabled;
}

- (void)setUsesPageCache:(BOOL)usesPageCache
{
    [self _setBoolValue:usesPageCache forKey:WebKitUsesPageCachePreferenceKey];
}

- (BOOL)usesPageCache
{
    return [self _boolValueForKey:WebKitUsesPageCachePreferenceKey];
}

- (void)_postCacheModelChangedNotification
{
#if !PLATFORM(IOS_FAMILY)
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }
#endif

    [[NSNotificationCenter defaultCenter] postNotificationName:WebPreferencesCacheModelChangedInternalNotification object:self userInfo:nil];
}

- (void)setCacheModel:(WebCacheModel)cacheModel
{
    [self _setIntegerValue:cacheModel forKey:WebKitCacheModelPreferenceKey];
    [self setAutomaticallyDetectsCacheModel:NO];
    [self _postCacheModelChangedNotification];
}

- (WebCacheModel)cacheModel
{
    return (WebCacheModel)[self _integerValueForKey:WebKitCacheModelPreferenceKey];
}


- (void)setSuppressesIncrementalRendering:(BOOL)suppressesIncrementalRendering
{
    [self _setBoolValue:suppressesIncrementalRendering forKey:WebKitSuppressesIncrementalRenderingKey];
}

- (BOOL)suppressesIncrementalRendering
{
    return [self _boolValueForKey:WebKitSuppressesIncrementalRenderingKey];
}

- (BOOL)allowsAirPlayForMediaPlayback
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    return [self _boolValueForKey:WebKitAllowsAirPlayForMediaPlaybackPreferenceKey];
#else
    return false;
#endif
}

- (void)setAllowsAirPlayForMediaPlayback:(BOOL)flag
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    [self _setBoolValue:flag forKey:WebKitAllowsAirPlayForMediaPlaybackPreferenceKey];
#endif
}

- (BOOL)isJavaEnabled
{
    return NO;
}

- (void)setJavaEnabled:(BOOL)flag
{
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

- (WebKitJavaScriptRuntimeFlags)javaScriptRuntimeFlags
{
    return static_cast<WebKitJavaScriptRuntimeFlags>([self _unsignedIntValueForKey:WebKitJavaScriptRuntimeFlagsPreferenceKey]);
}

- (void)setJavaScriptRuntimeFlags:(WebKitJavaScriptRuntimeFlags)flags
{
    [self _setUnsignedIntValue:flags forKey:WebKitJavaScriptRuntimeFlagsPreferenceKey];
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

// FIXME: applicationChromeMode is no longer needed by ToT, but is still used in Safari 8.
- (BOOL)applicationChromeModeEnabled
{
    return NO;
}

- (void)setApplicationChromeModeEnabled:(BOOL)flag
{
}

- (BOOL)domTimersThrottlingEnabled
{
    return [self _boolValueForKey:WebKitDOMTimersThrottlingEnabledPreferenceKey];
}

- (void)setDOMTimersThrottlingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitDOMTimersThrottlingEnabledPreferenceKey];
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

#if !PLATFORM(IOS_FAMILY)
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

- (BOOL)showsToolTipOverTruncatedText
{
    return [self _boolValueForKey:WebKitShowsToolTipOverTruncatedTextPreferenceKey];
}

- (void)setShowsToolTipOverTruncatedText:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShowsToolTipOverTruncatedTextPreferenceKey];
}

- (BOOL)textAreasAreResizable
{
    return [self _boolValueForKey: WebKitTextAreasAreResizablePreferenceKey];
}

- (void)setTextAreasAreResizable:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitTextAreasAreResizablePreferenceKey];
}
#endif // !PLATFORM(IOS_FAMILY)

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

- (BOOL)allowTopNavigationToDataURLs
{
    return [self _boolValueForKey: WebKitAllowTopNavigationToDataURLsPreferenceKey];
}

- (void)setAllowTopNavigationToDataURLs:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitAllowTopNavigationToDataURLsPreferenceKey];
}

- (BOOL)allowCrossOriginSubresourcesToAskForCredentials
{
    return [self _boolValueForKey:WebKitAllowCrossOriginSubresourcesToAskForCredentialsKey];
}

- (void)setAllowCrossOriginSubresourcesToAskForCredentials:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAllowCrossOriginSubresourcesToAskForCredentialsKey];
}

- (BOOL)needsStorageAccessFromFileURLsQuirk
{
    return [self _boolValueForKey: WebKitNeedsStorageAccessFromFileURLsQuirkKey];
}

-(void)setNeedsStorageAccessFromFileURLsQuirk:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitNeedsStorageAccessFromFileURLsQuirkKey];
}

- (NSTimeInterval)_backForwardCacheExpirationInterval
{
    return (NSTimeInterval)[self _floatValueForKey:WebKitBackForwardCacheExpirationIntervalKey];
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)_standalone
{
    return [self _boolValueForKey:WebKitStandalonePreferenceKey];
}

- (void)_setStandalone:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitStandalonePreferenceKey];
}

- (void)_setTelephoneNumberParsingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitTelephoneParsingEnabledPreferenceKey];
}

- (BOOL)_telephoneNumberParsingEnabled
{
    return [self _boolValueForKey:WebKitTelephoneParsingEnabledPreferenceKey];
}

- (BOOL)contentChangeObserverEnabled
{
    return [self _boolValueForKey:WebKitContentChangeObserverEnabledPreferenceKey];
}

- (void)setContentChangeObserverEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitContentChangeObserverEnabledPreferenceKey];
}
#endif

#if ENABLE(TEXT_AUTOSIZING)
- (void)_setMinimumZoomFontSize:(float)size
{
    [self _setFloatValue:size forKey:WebKitMinimumZoomFontSizePreferenceKey];
}

- (float)_minimumZoomFontSize
{
    return [self _floatValueForKey:WebKitMinimumZoomFontSizePreferenceKey];
}

- (void)_setTextAutosizingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitTextAutosizingEnabledPreferenceKey];
}

- (BOOL)_textAutosizingEnabled
{
    return [self _boolValueForKey:WebKitTextAutosizingEnabledPreferenceKey];
}
#endif

#if PLATFORM(IOS_FAMILY)
- (void)_setMaxParseDuration:(float)d
{
    [self _setFloatValue:d forKey:WebKitMaxParseDurationPreferenceKey];
}

- (float)_maxParseDuration
{
    return [self _floatValueForKey:WebKitMaxParseDurationPreferenceKey];
}

- (void)_setAllowMultiElementImplicitFormSubmission:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAllowMultiElementImplicitFormSubmissionPreferenceKey];
}

- (BOOL)_allowMultiElementImplicitFormSubmission
{
    return [self _boolValueForKey:WebKitAllowMultiElementImplicitFormSubmissionPreferenceKey];
}

- (void)_setAlwaysRequestGeolocationPermission:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAlwaysRequestGeolocationPermissionPreferenceKey];
}

- (BOOL)_alwaysRequestGeolocationPermission
{
    return [self _boolValueForKey:WebKitAlwaysRequestGeolocationPermissionPreferenceKey];
}

- (void)_setInterpolationQuality:(int)quality
{
    [self _setIntegerValue:quality forKey:WebKitInterpolationQualityPreferenceKey];
}

- (int)_interpolationQuality
{
    return [self _integerValueForKey:WebKitInterpolationQualityPreferenceKey];
}

- (BOOL)_allowPasswordEcho
{
    return [self _boolValueForKey:WebKitPasswordEchoEnabledPreferenceKey];
}

- (float)_passwordEchoDuration
{
    return [self _floatValueForKey:WebKitPasswordEchoDurationPreferenceKey];
}

#endif // PLATFORM(IOS_FAMILY)

#if !PLATFORM(IOS_FAMILY)
- (float)PDFScaleFactor
{
    return [self _floatValueForKey:WebKitPDFScaleFactorPreferenceKey];
}

- (void)setPDFScaleFactor:(float)factor
{
    [self _setFloatValue:factor forKey:WebKitPDFScaleFactorPreferenceKey];
}
#endif

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

#if !PLATFORM(IOS_FAMILY)
- (PDFDisplayMode)PDFDisplayMode
{
    PDFDisplayMode value = static_cast<PDFDisplayMode>([self _integerValueForKey:WebKitPDFDisplayModePreferenceKey]);
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
#endif

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
    auto value = static_cast<WebTextDirectionSubmenuInclusionBehavior>([self _integerValueForKey:WebKitTextDirectionSubmenuInclusionBehaviorPreferenceKey]);
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

#if PLATFORM(IOS_FAMILY)
- (BOOL)storageTrackerEnabled
{
    return [self _boolValueForKey:WebKitStorageTrackerEnabledPreferenceKey];
}

- (void)setStorageTrackerEnabled:(BOOL)storageTrackerEnabled
{
    [self _setBoolValue:storageTrackerEnabled forKey:WebKitStorageTrackerEnabledPreferenceKey];
}
#endif

- (BOOL)localStorageEnabled
{
    return [self _boolValueForKey:WebKitLocalStorageEnabledPreferenceKey];
}

- (void)setLocalStorageEnabled:(BOOL)localStorageEnabled
{
    [self _setBoolValue:localStorageEnabled forKey:WebKitLocalStorageEnabledPreferenceKey];
}

+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)ident
{
    LOG(Encoding, "requesting for %@\n", ident);

    if (!ident)
        return standardPreferences().get();

    WebPreferences *instance = [webPreferencesInstances() objectForKey:[self _concatenateKeyWithIBCreatorID:ident]];

    return instance;
}

+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)ident
{
    if (!ident)
        return;

    auto& instances = webPreferencesInstances();
    if (!instances)
        instances = adoptNS([[NSMutableDictionary alloc] init]);
    [instances setObject:instance forKey:[self _concatenateKeyWithIBCreatorID:ident]];
    LOG(Encoding, "recording %p for %@\n", instance, [self _concatenateKeyWithIBCreatorID:ident]);
}

+ (void)_checkLastReferenceForIdentifier:(id)identifier
{
    // FIXME: This won't work at all under garbage collection because retainCount returns a constant.
    // We may need to change WebPreferences API so there's an explicit way to end the lifetime of one.
    WebPreferences *instance = [webPreferencesInstances() objectForKey:identifier];
    if ([instance retainCount] == 1)
        [webPreferencesInstances() removeObjectForKey:identifier];
}

+ (void)_removeReferenceForIdentifier:(NSString *)ident
{
    if (ident)
        [self performSelector:@selector(_checkLastReferenceForIdentifier:) withObject:[self _concatenateKeyWithIBCreatorID:ident] afterDelay:0.1];
}

- (void)_startBatchingUpdates
{
    if (!_private->updateBatchCount)
        _private->updateAfterBatchType = { };

    _private->updateBatchCount++;
}

- (void)_stopBatchingUpdates
{
    ASSERT(_private->updateBatchCount > 0);
    if (_private->updateBatchCount <= 0)
        NSLog(@"ERROR: Unbalanced _startBatchingUpdates/_stopBatchingUpdates.");

    _private->updateBatchCount--;
    if (!_private->updateBatchCount) {
        if (_private->updateAfterBatchType.contains(UpdateAfterBatchType::Internal)) {
            if (_private->updateAfterBatchType.contains(UpdateAfterBatchType::API))
                [self _postPreferencesChangedNotification];
            else
                [self _postPreferencesChangedAPINotification];
        }
    }
}

- (void)_batchUpdatePreferencesInBlock:(void (^)(WebPreferences *))block
{
    [self _startBatchingUpdates];
    block(self);
    [self _stopBatchingUpdates];
}

- (void)_resetForTesting
{
    _private->values = adoptNS([[NSMutableDictionary alloc] init]);
    [self _postPreferencesChangedNotification];
}

- (void)_postPreferencesChangedNotification
{
#if !PLATFORM(IOS_FAMILY)
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }
#endif

    if (_private->updateBatchCount) {
        _private->updateAfterBatchType.add({ UpdateAfterBatchType::API, UpdateAfterBatchType::Internal });
        return;
    }

    [[NSNotificationCenter defaultCenter] postNotificationName:WebPreferencesChangedInternalNotification object:self userInfo:nil];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebPreferencesChangedNotification object:self userInfo:nil];
}

- (void)_postPreferencesChangedAPINotification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }

    if (_private->updateBatchCount) {
        _private->updateAfterBatchType.add({ UpdateAfterBatchType::Internal });
        return;
    }

    [[NSNotificationCenter defaultCenter] postNotificationName:WebPreferencesChangedNotification object:self userInfo:nil];
}

+ (CFStringEncoding)_systemCFStringEncoding
{
    return PAL::webDefaultCFStringEncoding();
}

+ (void)_setInitialDefaultTextEncodingToSystemEncoding
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:
        @{ WebKitDefaultTextEncodingNamePreferenceKey: PAL::defaultTextEncodingNameForSystemLanguage() }];
}

static RetainPtr<NSString>& classIBCreatorID()
{
    static NeverDestroyed<RetainPtr<NSString>> classIBCreatorID;
    return classIBCreatorID;
}

+ (void)_setIBCreatorID:(NSString *)string
{
    classIBCreatorID() = adoptNS([string copy]);
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

- (BOOL)acceleratedDrawingEnabled
{
    return [self _boolValueForKey:WebKitAcceleratedDrawingEnabledPreferenceKey];
}

- (void)setAcceleratedDrawingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitAcceleratedDrawingEnabledPreferenceKey];
}

- (BOOL)displayListDrawingEnabled
{
    return [self _boolValueForKey:WebKitDisplayListDrawingEnabledPreferenceKey];
}

- (void)setDisplayListDrawingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitDisplayListDrawingEnabledPreferenceKey];
}

- (BOOL)resourceLoadStatisticsEnabled
{
    return [self _boolValueForKey:WebKitResourceLoadStatisticsEnabledPreferenceKey];
}

- (void)setResourceLoadStatisticsEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitResourceLoadStatisticsEnabledPreferenceKey];
}

- (BOOL)largeImageAsyncDecodingEnabled
{
    return [self _boolValueForKey:WebKitLargeImageAsyncDecodingEnabledPreferenceKey];
}

- (void)setLargeImageAsyncDecodingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitLargeImageAsyncDecodingEnabledPreferenceKey];
}

- (BOOL)animatedImageAsyncDecodingEnabled
{
    return [self _boolValueForKey:WebKitAnimatedImageAsyncDecodingEnabledPreferenceKey];
}

- (void)setAnimatedImageAsyncDecodingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitAnimatedImageAsyncDecodingEnabledPreferenceKey];
}

- (BOOL)canvasUsesAcceleratedDrawing
{
    return [self _boolValueForKey:WebKitCanvasUsesAcceleratedDrawingPreferenceKey];
}

- (void)setCanvasUsesAcceleratedDrawing:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitCanvasUsesAcceleratedDrawingPreferenceKey];
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

- (BOOL)legacyLineLayoutVisualCoverageEnabled
{
    return [self _boolValueForKey:WebKitLegacyLineLayoutVisualCoverageEnabledPreferenceKey];
}

- (void)setLegacyLineLayoutVisualCoverageEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitLegacyLineLayoutVisualCoverageEnabledPreferenceKey];
}

- (BOOL)showRepaintCounter
{
    return [self _boolValueForKey:WebKitShowRepaintCounterPreferenceKey];
}

- (void)setShowRepaintCounter:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitShowRepaintCounterPreferenceKey];
}

- (BOOL)webAudioEnabled
{
    return [self _boolValueForKey:WebKitWebAudioEnabledPreferenceKey];
}

- (void)setWebAudioEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitWebAudioEnabledPreferenceKey];
}

- (BOOL)webGLEnabled
{
    return [self _boolValueForKey:WebKitWebGLEnabledPreferenceKey];
}

- (void)setWebGLEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitWebGLEnabledPreferenceKey];
}

- (BOOL)forceLowPowerGPUForWebGL
{
    return [self _boolValueForKey:WebKitForceWebGLUsesLowPowerPreferenceKey];
}

- (void)setForceWebGLUsesLowPower:(BOOL)forceLowPower
{
    [self _setBoolValue:forceLowPower forKey:WebKitForceWebGLUsesLowPowerPreferenceKey];
}

- (BOOL)isFrameFlatteningEnabled
{
    return [self _unsignedIntValueForKey:WebKitFrameFlatteningPreferenceKey] != WebKitFrameFlatteningDisabled;
}

- (void)setFrameFlatteningEnabled:(BOOL)flattening
{
    WebKitFrameFlattening value = flattening ? WebKitFrameFlatteningFullyEnabled : WebKitFrameFlatteningDisabled;
    [self _setUnsignedIntValue:value forKey:WebKitFrameFlatteningPreferenceKey];
}

- (WebKitFrameFlattening)frameFlattening
{
    return static_cast<WebKitFrameFlattening>([self _unsignedIntValueForKey:WebKitFrameFlatteningPreferenceKey]);
}

- (void)setFrameFlattening:(WebKitFrameFlattening)flattening
{
    [self _setUnsignedIntValue:flattening forKey:WebKitFrameFlatteningPreferenceKey];
}

- (BOOL)asyncFrameScrollingEnabled
{
    return [self _boolValueForKey:WebKitAsyncFrameScrollingEnabledPreferenceKey];
}

- (void)setAsyncFrameScrollingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitAsyncFrameScrollingEnabledPreferenceKey];
}

- (BOOL)isSpatialNavigationEnabled
{
    return [self _boolValueForKey:WebKitSpatialNavigationEnabledPreferenceKey];
}

- (void)setSpatialNavigationEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitSpatialNavigationEnabledPreferenceKey];
}

- (BOOL)hyperlinkAuditingEnabled
{
    return [self _boolValueForKey:WebKitHyperlinkAuditingEnabledPreferenceKey];
}

- (void)setHyperlinkAuditingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitHyperlinkAuditingEnabledPreferenceKey];
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

- (void)setFullScreenEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitFullScreenEnabledPreferenceKey];
}

- (BOOL)fullScreenEnabled
{
    return [self _boolValueForKey:WebKitFullScreenEnabledPreferenceKey];
}

- (void)setAsynchronousSpellCheckingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAsynchronousSpellCheckingEnabledPreferenceKey];
}

- (BOOL)asynchronousSpellCheckingEnabled
{
    return [self _boolValueForKey:WebKitAsynchronousSpellCheckingEnabledPreferenceKey];
}

+ (void)setWebKitLinkTimeVersion:(int)version
{
    setWebKitLinkTimeVersion(version);
}

- (void)setLoadsSiteIconsIgnoringImageLoadingPreference: (BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitLoadSiteIconsKey];
}

- (BOOL)loadsSiteIconsIgnoringImageLoadingPreference
{
    return [self _boolValueForKey: WebKitLoadSiteIconsKey];
}

- (void)setAVFoundationEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAVFoundationEnabledKey];
}

- (BOOL)isAVFoundationEnabled
{
    return [self _boolValueForKey:WebKitAVFoundationEnabledKey];
}

- (void)setAVFoundationNSURLSessionEnabled:(BOOL)flag
{
}

- (BOOL)isAVFoundationNSURLSessionEnabled
{
    return YES;
}

- (BOOL)isInheritURIQueryComponentEnabled
{
    return [self _boolValueForKey: WebKitEnableInheritURIQueryComponentPreferenceKey];
}

- (void)setEnableInheritURIQueryComponent:(BOOL)flag
{
    [self _setBoolValue:flag forKey: WebKitEnableInheritURIQueryComponentPreferenceKey];
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)mediaPlaybackAllowsAirPlay
{
    return [self _boolValueForKey:WebKitAllowsAirPlayForMediaPlaybackPreferenceKey];
}

- (void)setMediaPlaybackAllowsAirPlay:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAllowsAirPlayForMediaPlaybackPreferenceKey];
}

- (unsigned)audioSessionCategoryOverride
{
    return [self _unsignedIntValueForKey:WebKitAudioSessionCategoryOverride];
}

- (void)setAudioSessionCategoryOverride:(unsigned)override
{
    if (override > static_cast<unsigned>(AudioSession::CategoryType::AudioProcessing)) {
        // Clients are passing us OSTypes values from AudioToolbox/AudioSession.h,
        // which need to be translated into AudioSession::CategoryType:
        switch (override) {
        case WebKitAudioSessionCategoryAmbientSound:
            override = static_cast<unsigned>(AudioSession::CategoryType::AmbientSound);
            break;
        case WebKitAudioSessionCategorySoloAmbientSound:
            override = static_cast<unsigned>(AudioSession::CategoryType::SoloAmbientSound);
            break;
        case WebKitAudioSessionCategoryMediaPlayback:
            override = static_cast<unsigned>(AudioSession::CategoryType::MediaPlayback);
            break;
        case WebKitAudioSessionCategoryRecordAudio:
            override = static_cast<unsigned>(AudioSession::CategoryType::RecordAudio);
            break;
        case WebKitAudioSessionCategoryPlayAndRecord:
            override = static_cast<unsigned>(AudioSession::CategoryType::PlayAndRecord);
            break;
        case WebKitAudioSessionCategoryAudioProcessing:
            override = static_cast<unsigned>(AudioSession::CategoryType::AudioProcessing);
            break;
        default:
            override = static_cast<unsigned>(AudioSession::CategoryType::None);
            break;
        }
    }

    [self _setUnsignedIntValue:override forKey:WebKitAudioSessionCategoryOverride];
}

- (BOOL)networkDataUsageTrackingEnabled
{
    return [self _boolValueForKey:WebKitNetworkDataUsageTrackingEnabledPreferenceKey];
}

- (void)setNetworkDataUsageTrackingEnabled:(BOOL)trackingEnabled
{
    [self _setBoolValue:trackingEnabled forKey:WebKitNetworkDataUsageTrackingEnabledPreferenceKey];
}

- (NSString *)networkInterfaceName
{
    return [self _stringValueForKey:WebKitNetworkInterfaceNamePreferenceKey];
}

- (void)setNetworkInterfaceName:(NSString *)name
{
    [self _setStringValue:name forKey:WebKitNetworkInterfaceNamePreferenceKey];
}
#endif // PLATFORM(IOS_FAMILY)

// Deprecated. Use -videoPlaybackRequiresUserGesture and -audioPlaybackRequiresUserGesture instead.
- (BOOL)mediaPlaybackRequiresUserGesture
{
    return [self _boolValueForKey:WebKitRequiresUserGestureForMediaPlaybackPreferenceKey];
}

// Deprecated. Use -setVideoPlaybackRequiresUserGesture and -setAudioPlaybackRequiresUserGesture instead.
- (void)setMediaPlaybackRequiresUserGesture:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitRequiresUserGestureForMediaPlaybackPreferenceKey];
}

- (BOOL)videoPlaybackRequiresUserGesture
{
    return [self _boolValueForKey:WebKitRequiresUserGestureForVideoPlaybackPreferenceKey];
}

- (void)setVideoPlaybackRequiresUserGesture:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitRequiresUserGestureForVideoPlaybackPreferenceKey];
}

- (BOOL)audioPlaybackRequiresUserGesture
{
    return [self _boolValueForKey:WebKitRequiresUserGestureForAudioPlaybackPreferenceKey];
}

- (void)setAudioPlaybackRequiresUserGesture:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitRequiresUserGestureForAudioPlaybackPreferenceKey];
}

- (BOOL)overrideUserGestureRequirementForMainContent
{
    return [self _boolValueForKey:WebKitMainContentUserGestureOverrideEnabledPreferenceKey];
}

- (void)setOverrideUserGestureRequirementForMainContent:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMainContentUserGestureOverrideEnabledPreferenceKey];
}

- (BOOL)mediaPlaybackAllowsInline
{
    return [self _boolValueForKey:WebKitAllowsInlineMediaPlaybackPreferenceKey];
}

- (void)setMediaPlaybackAllowsInline:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAllowsInlineMediaPlaybackPreferenceKey];
}

- (BOOL)inlineMediaPlaybackRequiresPlaysInlineAttribute
{
    return [self _boolValueForKey:WebKitInlineMediaPlaybackRequiresPlaysInlineAttributeKey];
}

- (void)setInlineMediaPlaybackRequiresPlaysInlineAttribute:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitInlineMediaPlaybackRequiresPlaysInlineAttributeKey];
}

- (BOOL)invisibleAutoplayNotPermitted
{
    return [self _boolValueForKey:WebKitInvisibleAutoplayNotPermittedKey];
}

- (void)setInvisibleAutoplayNotPermitted:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitInvisibleAutoplayNotPermittedKey];
}

- (BOOL)mediaControlsScaleWithPageZoom
{
    return [self _boolValueForKey:WebKitMediaControlsScaleWithPageZoomPreferenceKey];
}

- (void)setMediaControlsScaleWithPageZoom:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaControlsScaleWithPageZoomPreferenceKey];
}

- (BOOL)allowsAlternateFullscreen
{
    return [self allowsPictureInPictureMediaPlayback];
}

- (void)setAllowsAlternateFullscreen:(BOOL)flag
{
    [self setAllowsPictureInPictureMediaPlayback:flag];
}

- (BOOL)allowsPictureInPictureMediaPlayback
{
    return [self _boolValueForKey:WebKitAllowsPictureInPictureMediaPlaybackPreferenceKey];
}

- (void)setAllowsPictureInPictureMediaPlayback:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAllowsPictureInPictureMediaPlaybackPreferenceKey];
}

- (BOOL)mockScrollbarsEnabled
{
    return [self _boolValueForKey:WebKitMockScrollbarsEnabledPreferenceKey];
}

- (void)setMockScrollbarsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMockScrollbarsEnabledPreferenceKey];
}

- (NSString *)pictographFontFamily
{
    return [self _stringValueForKey: WebKitPictographFontPreferenceKey];
}

- (void)setPictographFontFamily:(NSString *)family
{
    [self _setStringValue: family forKey: WebKitPictographFontPreferenceKey];
}

- (BOOL)pageCacheSupportsPlugins
{
    return [self _boolValueForKey:WebKitPageCacheSupportsPluginsPreferenceKey];
}

- (void)setPageCacheSupportsPlugins:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitPageCacheSupportsPluginsPreferenceKey];

}

#if PLATFORM(IOS_FAMILY)
- (void)_invalidateCachedPreferences
{
    dispatch_barrier_sync(_private->readWriteQueue.get(), ^{
        if (_private->values)
            _private->values = adoptNS([[NSMutableDictionary alloc] init]);
    });

    [self _updatePrivateBrowsingStateTo:[self privateBrowsingEnabled]];

    // Tell any live WebViews to refresh their preferences
    [self _postPreferencesChangedNotification];
}

- (void)_synchronizeWebStoragePolicyWithCookiePolicy
{
    // FIXME: This should be done in clients, WebKit shouldn't be making such policy decisions.

    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
    WebStorageBlockingPolicy storageBlockingPolicy;
    switch (static_cast<unsigned>(cookieAcceptPolicy)) {
    case NSHTTPCookieAcceptPolicyAlways:
        storageBlockingPolicy = WebAllowAllStorage;
        break;
    case NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
    case NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain:
        storageBlockingPolicy = WebBlockThirdPartyStorage;
        break;
    case NSHTTPCookieAcceptPolicyNever:
        storageBlockingPolicy = WebBlockAllStorage;
        break;
    default:
        ASSERT_NOT_REACHED();
        storageBlockingPolicy = WebBlockAllStorage;
        break;
    }

    [self setStorageBlockingPolicy:storageBlockingPolicy];
}
#endif

- (void)setBackspaceKeyNavigationEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitBackspaceKeyNavigationEnabledKey];
}

- (BOOL)backspaceKeyNavigationEnabled
{
    return [self _boolValueForKey:WebKitBackspaceKeyNavigationEnabledKey];
}

- (void)setWantsBalancedSetDefersLoadingBehavior:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitWantsBalancedSetDefersLoadingBehaviorKey];
}

- (BOOL)wantsBalancedSetDefersLoadingBehavior
{
    return [self _boolValueForKey:WebKitWantsBalancedSetDefersLoadingBehaviorKey];
}

- (void)setShouldDisplaySubtitles:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShouldDisplaySubtitlesPreferenceKey];
}

- (BOOL)shouldDisplaySubtitles
{
    return [self _boolValueForKey:WebKitShouldDisplaySubtitlesPreferenceKey];
}

- (void)setShouldDisplayCaptions:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShouldDisplayCaptionsPreferenceKey];
}

- (BOOL)shouldDisplayCaptions
{
    return [self _boolValueForKey:WebKitShouldDisplayCaptionsPreferenceKey];
}

- (void)setShouldDisplayTextDescriptions:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShouldDisplayTextDescriptionsPreferenceKey];
}

- (BOOL)shouldDisplayTextDescriptions
{
    return [self _boolValueForKey:WebKitShouldDisplayTextDescriptionsPreferenceKey];
}

- (void)setNotificationsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitNotificationsEnabledKey];
}

- (BOOL)notificationsEnabled
{
    return [self _boolValueForKey:WebKitNotificationsEnabledKey];
}

- (void)setShouldRespectImageOrientation:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShouldRespectImageOrientationKey];
}

- (BOOL)shouldRespectImageOrientation
{
    return [self _boolValueForKey:WebKitShouldRespectImageOrientationKey];
}

- (void)setIncrementalRenderingSuppressionTimeoutInSeconds:(NSTimeInterval)timeout
{
    [self _setFloatValue:timeout forKey:WebKitIncrementalRenderingSuppressionTimeoutInSecondsKey];
}

- (NSTimeInterval)incrementalRenderingSuppressionTimeoutInSeconds
{
    return [self _floatValueForKey:WebKitIncrementalRenderingSuppressionTimeoutInSecondsKey];
}

- (BOOL)diagnosticLoggingEnabled
{
    return [self _boolValueForKey:WebKitDiagnosticLoggingEnabledKey];
}

- (void)setDiagnosticLoggingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitDiagnosticLoggingEnabledKey];
}

- (void)setStorageBlockingPolicy:(WebStorageBlockingPolicy)storageBlockingPolicy
{
#if PLATFORM(IOS_FAMILY)
    // We don't want to write the setting out, so we just reset the default instead of storing the new setting.
    // FIXME: This code removes any defaults previously registered by client process, which is not appropriate for this method to do.
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{ WebKitStorageBlockingPolicyKey: @(storageBlockingPolicy) }];
#else
    [self _setIntegerValue:storageBlockingPolicy forKey:WebKitStorageBlockingPolicyKey];
#endif
}

- (WebStorageBlockingPolicy)storageBlockingPolicy
{
    return static_cast<WebStorageBlockingPolicy>([self _integerValueForKey:WebKitStorageBlockingPolicyKey]);
}

- (BOOL)plugInSnapshottingEnabled
{
    return NO;
}

- (void)setPlugInSnapshottingEnabled:(BOOL)enabled
{
}

- (BOOL)hiddenPageDOMTimerThrottlingEnabled
{
    return [self _boolValueForKey:WebKitHiddenPageDOMTimerThrottlingEnabledPreferenceKey];
}

- (void)setHiddenPageDOMTimerThrottlingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitHiddenPageDOMTimerThrottlingEnabledPreferenceKey];
}

- (BOOL)hiddenPageCSSAnimationSuspensionEnabled
{
    return [self _boolValueForKey:WebKitHiddenPageCSSAnimationSuspensionEnabledPreferenceKey];
}

- (void)setHiddenPageCSSAnimationSuspensionEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitHiddenPageCSSAnimationSuspensionEnabledPreferenceKey];
}

- (BOOL)lowPowerVideoAudioBufferSizeEnabled
{
    return [self _boolValueForKey:WebKitLowPowerVideoAudioBufferSizeEnabledPreferenceKey];
}

- (void)setLowPowerVideoAudioBufferSizeEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitLowPowerVideoAudioBufferSizeEnabledPreferenceKey];
}

- (BOOL)useLegacyTextAlignPositionedElementBehavior
{
    return [self _boolValueForKey:WebKitUseLegacyTextAlignPositionedElementBehaviorPreferenceKey];
}

- (void)setUseLegacyTextAlignPositionedElementBehavior:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitUseLegacyTextAlignPositionedElementBehaviorPreferenceKey];
}

- (BOOL)mediaSourceEnabled
{
    return [self _boolValueForKey:WebKitMediaSourceEnabledPreferenceKey];
}

- (void)setMediaSourceEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitMediaSourceEnabledPreferenceKey];
}

- (BOOL)sourceBufferChangeTypeEnabled
{
    return [self _boolValueForKey:WebKitSourceBufferChangeTypeEnabledPreferenceKey];
}

- (void)setSourceBufferChangeTypeEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitSourceBufferChangeTypeEnabledPreferenceKey];
}

- (BOOL)imageControlsEnabled
{
    // Image controls are no longer supported.
    return NO;
}

- (void)setImageControlsEnabled:(BOOL)enabled
{
    // Image controls are no longer supported.
    UNUSED_PARAM(enabled);
}

- (BOOL)serviceControlsEnabled
{
    return [self _boolValueForKey:WebKitServiceControlsEnabledPreferenceKey];
}

- (void)setServiceControlsEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitServiceControlsEnabledPreferenceKey];
}

- (BOOL)gamepadsEnabled
{
    return [self _boolValueForKey:WebKitGamepadsEnabledPreferenceKey];
}

- (void)setGamepadsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitGamepadsEnabledPreferenceKey];
}

- (BOOL)shouldConvertPositionStyleOnCopy
{
    return [self _boolValueForKey:WebKitShouldConvertPositionStyleOnCopyPreferenceKey];
}

- (void)setShouldConvertPositionStyleOnCopy:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitShouldConvertPositionStyleOnCopyPreferenceKey];
}

- (NSString *)mediaKeysStorageDirectory
{
    return [[self _stringValueForKey:WebKitMediaKeysStorageDirectoryKey] stringByStandardizingPath];
}

- (void)setMediaKeysStorageDirectory:(NSString *)directory
{
    [self _setStringValue:directory forKey:WebKitMediaKeysStorageDirectoryKey];
}

- (BOOL)mediaDevicesEnabled
{
    return [self _boolValueForKey:WebKitMediaDevicesEnabledPreferenceKey];
}

- (void)setMediaDevicesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaDevicesEnabledPreferenceKey];
}

- (BOOL)mediaStreamEnabled
{
    return [self _boolValueForKey:WebKitMediaStreamEnabledPreferenceKey];
}

- (void)setMediaStreamEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaStreamEnabledPreferenceKey];
}

- (BOOL)peerConnectionEnabled
{
    return [self _boolValueForKey:WebKitPeerConnectionEnabledPreferenceKey];
}

- (void)setPeerConnectionEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitPeerConnectionEnabledPreferenceKey];
}

- (BOOL)linkPreloadEnabled
{
    return [self _boolValueForKey:WebKitLinkPreloadEnabledPreferenceKey];
}

- (void)setLinkPreloadEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitLinkPreloadEnabledPreferenceKey];
}

- (BOOL)mediaPreloadingEnabled
{
    return [self _boolValueForKey:WebKitMediaPreloadingEnabledPreferenceKey];
}

- (void)setMediaPreloadingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaPreloadingEnabledPreferenceKey];
}

- (void)setMetaRefreshEnabled:(BOOL)enabled
{
    [self setHTTPEquivEnabled:enabled];
}

- (BOOL)metaRefreshEnabled
{
    return [self httpEquivEnabled];
}

- (void)setHTTPEquivEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitHTTPEquivEnabledPreferenceKey];
}

- (BOOL)httpEquivEnabled
{
    return [self _boolValueForKey:WebKitHTTPEquivEnabledPreferenceKey];
}

- (BOOL)javaScriptMarkupEnabled
{
    return [self _boolValueForKey:WebKitJavaScriptMarkupEnabledPreferenceKey];
}

- (void)setJavaScriptMarkupEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitJavaScriptMarkupEnabledPreferenceKey];
}

- (BOOL)mediaDataLoadsAutomatically
{
    return [self _boolValueForKey:WebKitMediaDataLoadsAutomaticallyPreferenceKey];
}

- (void)setMediaDataLoadsAutomatically:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaDataLoadsAutomaticallyPreferenceKey];
}

- (BOOL)attachmentElementEnabled
{
    return [self _boolValueForKey:WebKitAttachmentElementEnabledPreferenceKey];
}

- (void)setAttachmentElementEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAttachmentElementEnabledPreferenceKey];
}

- (BOOL)allowsInlineMediaPlaybackAfterFullscreen
{
    return [self _boolValueForKey:WebKitAllowsInlineMediaPlaybackAfterFullscreenPreferenceKey];
}

- (void)setAllowsInlineMediaPlaybackAfterFullscreen:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAllowsInlineMediaPlaybackAfterFullscreenPreferenceKey];
}

- (BOOL)mockCaptureDevicesEnabled
{
    return [self _boolValueForKey:WebKitMockCaptureDevicesEnabledPreferenceKey];
}

- (void)setMockCaptureDevicesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMockCaptureDevicesEnabledPreferenceKey];
}

- (BOOL)mockCaptureDevicesPromptEnabled
{
    return [self _boolValueForKey:WebKitMockCaptureDevicesPromptEnabledPreferenceKey];
}

- (void)setMockCaptureDevicesPromptEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMockCaptureDevicesPromptEnabledPreferenceKey];
}

- (BOOL)enumeratingAllNetworkInterfacesEnabled
{
    return [self _boolValueForKey:WebKitEnumeratingAllNetworkInterfacesEnabledPreferenceKey];
}

- (void)setEnumeratingAllNetworkInterfacesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitEnumeratingAllNetworkInterfacesEnabledPreferenceKey];
}

- (BOOL)iceCandidateFilteringEnabled
{
    return [self _boolValueForKey:WebKitICECandidateFilteringEnabledPreferenceKey];
}

- (void)setIceCandidateFilteringEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitICECandidateFilteringEnabledPreferenceKey];
}

- (BOOL)mediaCaptureRequiresSecureConnection
{
    return [self _boolValueForKey:WebKitMediaCaptureRequiresSecureConnectionPreferenceKey];
}

- (void)setMediaCaptureRequiresSecureConnection:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaCaptureRequiresSecureConnectionPreferenceKey];
}

- (BOOL)dataTransferItemsEnabled
{
    return [self _boolValueForKey:WebKitDataTransferItemsEnabledPreferenceKey];
}

- (void)setDataTransferItemsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitDataTransferItemsEnabledPreferenceKey];
}

- (BOOL)customPasteboardDataEnabled
{
    return [self _boolValueForKey:WebKitCustomPasteboardDataEnabledPreferenceKey];
}

- (void)setCustomPasteboardDataEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCustomPasteboardDataEnabledPreferenceKey];
}

- (BOOL)cacheAPIEnabled
{
    return [self _boolValueForKey:WebKitCacheAPIEnabledPreferenceKey];
}

- (void)setCacheAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCacheAPIEnabledPreferenceKey];
}

- (BOOL)downloadAttributeEnabled
{
    return [self _boolValueForKey:WebKitDownloadAttributeEnabledPreferenceKey];
}

- (void)setDownloadAttributeEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitDownloadAttributeEnabledPreferenceKey];
}

- (void)setDirectoryUploadEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitDirectoryUploadEnabledPreferenceKey];
}

- (BOOL)directoryUploadEnabled
{
    return [self _boolValueForKey:WebKitDirectoryUploadEnabledPreferenceKey];
}

- (BOOL)CSSOMViewScrollingAPIEnabled
{
    return [self _boolValueForKey:WebKitCSSOMViewScrollingAPIEnabledPreferenceKey];
}

- (void)setCSSOMViewScrollingAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCSSOMViewScrollingAPIEnabledPreferenceKey];
}

- (BOOL)menuItemElementEnabled
{
    return [self _boolValueForKey:WebKitMenuItemElementEnabledPreferenceKey];
}

- (void)setMenuItemElementEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMenuItemElementEnabledPreferenceKey];
}

- (BOOL)mediaUserGestureInheritsFromDocument
{
    return [self _boolValueForKey:WebKitMediaUserGestureInheritsFromDocument];
}

- (void)setMediaUserGestureInheritsFromDocument:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaUserGestureInheritsFromDocument];
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)quickLookDocumentSavingEnabled
{
    return [self _boolValueForKey:WebKitQuickLookDocumentSavingPreferenceKey];
}

- (void)setQuickLookDocumentSavingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitQuickLookDocumentSavingPreferenceKey];
}
#endif

- (NSString *)mediaContentTypesRequiringHardwareSupport
{
    return [self _stringValueForKey:WebKitMediaContentTypesRequiringHardwareSupportPreferenceKey];
}

- (void)setMediaContentTypesRequiringHardwareSupport:(NSString *)value
{
    [self _setStringValue:value forKey:WebKitMediaContentTypesRequiringHardwareSupportPreferenceKey];
}

- (BOOL)legacyEncryptedMediaAPIEnabled
{
    return [self _boolValueForKey:WebKitLegacyEncryptedMediaAPIEnabledKey];
}

- (void)setLegacyEncryptedMediaAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitLegacyEncryptedMediaAPIEnabledKey];
}

- (BOOL)encryptedMediaAPIEnabled
{
    return [self _boolValueForKey:WebKitEncryptedMediaAPIEnabledKey];
}

- (void)setEncryptedMediaAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitEncryptedMediaAPIEnabledKey];
}

- (BOOL)pictureInPictureAPIEnabled
{
    return [self _boolValueForKey:WebKitPictureInPictureAPIEnabledKey];
}

- (void)setPictureInPictureAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitPictureInPictureAPIEnabledKey];
}

- (BOOL)constantPropertiesEnabled
{
    return [self _boolValueForKey:WebKitConstantPropertiesEnabledPreferenceKey];
}

- (void)setConstantPropertiesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitConstantPropertiesEnabledPreferenceKey];
}

- (BOOL)colorFilterEnabled
{
    return [self _boolValueForKey:WebKitColorFilterEnabledPreferenceKey];
}

- (void)setColorFilterEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitColorFilterEnabledPreferenceKey];
}

- (BOOL)punchOutWhiteBackgroundsInDarkMode
{
    return [self _boolValueForKey:WebKitPunchOutWhiteBackgroundsInDarkModePreferenceKey];
}

- (void)setPunchOutWhiteBackgroundsInDarkMode:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitPunchOutWhiteBackgroundsInDarkModePreferenceKey];
}

- (BOOL)allowMediaContentTypesRequiringHardwareSupportAsFallback
{
    return [self _boolValueForKey:WebKitAllowMediaContentTypesRequiringHardwareSupportAsFallbackKey];
}

- (void)setAllowMediaContentTypesRequiringHardwareSupportAsFallback:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAllowMediaContentTypesRequiringHardwareSupportAsFallbackKey];
}

- (BOOL)mediaCapabilitiesEnabled
{
    return [self _boolValueForKey:WebKitMediaCapabilitiesEnabledPreferenceKey];
}

- (void)setMediaCapabilitiesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaCapabilitiesEnabledPreferenceKey];
}

- (BOOL)lineHeightUnitsEnabled
{
    return [self _boolValueForKey:WebKitLineHeightUnitsEnabledPreferenceKey];
}

- (void)setLineHeightUnitsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitLineHeightUnitsEnabledPreferenceKey];
}

- (BOOL)layoutFormattingContextIntegrationEnabled
{
    return [self _boolValueForKey:WebKitLayoutFormattingContextIntegrationEnabledPreferenceKey];
}

- (void)setLayoutFormattingContextIntegrationEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitLayoutFormattingContextIntegrationEnabledPreferenceKey];
}

- (BOOL)isInAppBrowserPrivacyEnabled
{
    return [self _boolValueForKey:WebKitDebugInAppBrowserPrivacyEnabledPreferenceKey];
}

- (void)setInAppBrowserPrivacyEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitDebugInAppBrowserPrivacyEnabledPreferenceKey];
}

- (BOOL)webSQLEnabled
{
    return [self _boolValueForKey:WebKitWebSQLEnabledPreferenceKey];
}

- (void)setWebSQLEnabled:(BOOL)webSQLEnabled
{
    [self _setBoolValue:webSQLEnabled forKey:WebKitWebSQLEnabledPreferenceKey];
}

@end

@implementation WebPreferences (WebInternal)

+ (NSString *)_IBCreatorID
{
    return classIBCreatorID().get();
}

+ (NSString *)_concatenateKeyWithIBCreatorID:(NSString *)key
{
    NSString *IBCreatorID = [WebPreferences _IBCreatorID];
    if (!IBCreatorID)
        return key;
    return [IBCreatorID stringByAppendingString:key];
}

@end

@implementation WebPreferences (WebPrivateFeatures)

- (BOOL)_isEnabledForFeature:(WebFeature *)feature
{
    return [self _boolValueForKey:feature.preferenceKey];
}

- (void)_setEnabled:(BOOL)value forFeature:(WebFeature *)feature
{
    [self _setBoolValue:value forKey:feature.preferenceKey];
}

@end

@implementation WebPreferences (WebPrivateTesting)

+ (void)_switchNetworkLoaderToNewTestingSession
{
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif
    NetworkStorageSessionMap::switchToNewTestingSession();
}

+ (void)_setCurrentNetworkLoaderSessionCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy
{
    auto cookieStorage = NetworkStorageSessionMap::defaultStorageSession().cookieStorage();
    RELEASE_ASSERT(cookieStorage); // Will fail when NetworkStorageSessionMap::switchToNewTestingSession() was not called beforehand.
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), policy);
}

+ (void)_clearNetworkLoaderSession:(void (^)(void))completionHandler
{
    NetworkStorageSessionMap::defaultStorageSession().deleteAllCookies([completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setBoolPreferenceForTestingWithValue:(BOOL)value forKey:(NSString *)key
{
    [self _setBoolValue:value forKey:key];
}

- (void)_setUInt32PreferenceForTestingWithValue:(uint32_t)value forKey:(NSString *)key
{
    [self _setIntegerValue:value forKey:key];
}

- (void)_setDoublePreferenceForTestingWithValue:(double)value forKey:(NSString *)key
{
    [self _setFloatValue:value forKey:key];
}

- (void)_setStringPreferenceForTestingWithValue:(NSString *)value forKey:(NSString *)key
{
    [self _setStringValue:value forKey:key];
}

@end

@implementation WebPreferences (WebPrivatePreferencesConvertedToWebFeature)

- (BOOL)userGesturePromisePropagationEnabled
{
    return [self _boolValueForKey:WebKitUserGesturePromisePropagationEnabledPreferenceKey];
}

- (void)setUserGesturePromisePropagationEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitUserGesturePromisePropagationEnabledPreferenceKey];
}

- (BOOL)requestIdleCallbackEnabled
{
    return [self _boolValueForKey:WebKitRequestIdleCallbackEnabledPreferenceKey];
}

- (void)setRequestIdleCallbackEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitRequestIdleCallbackEnabledPreferenceKey];
}

- (BOOL)highlightAPIEnabled
{
    return [self _boolValueForKey:WebKitHighlightAPIEnabledPreferenceKey];
}

- (void)setHighlightAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitHighlightAPIEnabledPreferenceKey];
}

- (BOOL)asyncClipboardAPIEnabled
{
    return [self _boolValueForKey:WebKitAsyncClipboardAPIEnabledPreferenceKey];
}

- (void)setAsyncClipboardAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAsyncClipboardAPIEnabledPreferenceKey];
}

- (BOOL)contactPickerAPIEnabled
{
    return [self _boolValueForKey:WebKitContactPickerAPIEnabledPreferenceKey];
}

- (void)setContactPickerAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitContactPickerAPIEnabledPreferenceKey];
}

- (BOOL)intersectionObserverEnabled
{
    return [self _boolValueForKey:WebKitIntersectionObserverEnabledPreferenceKey];
}

- (void)setIntersectionObserverEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitIntersectionObserverEnabledPreferenceKey];
}

- (BOOL)visualViewportAPIEnabled
{
    return [self _boolValueForKey:WebKitVisualViewportAPIEnabledPreferenceKey];
}

- (void)setVisualViewportAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitVisualViewportAPIEnabledPreferenceKey];
}

- (BOOL)syntheticEditingCommandsEnabled
{
    return [self _boolValueForKey:WebKitSyntheticEditingCommandsEnabledPreferenceKey];
}

- (void)setSyntheticEditingCommandsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitSyntheticEditingCommandsEnabledPreferenceKey];
}

- (BOOL)CSSOMViewSmoothScrollingEnabled
{
    return [self _boolValueForKey:WebKitCSSOMViewSmoothScrollingEnabledPreferenceKey];
}

- (void)setCSSOMViewSmoothScrollingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCSSOMViewSmoothScrollingEnabledPreferenceKey];
}

- (BOOL)webAnimationsCompositeOperationsEnabled
{
    return [self _boolValueForKey:WebKitWebAnimationsCompositeOperationsEnabledPreferenceKey];
}

- (void)setWebAnimationsCompositeOperationsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitWebAnimationsCompositeOperationsEnabledPreferenceKey];
}

- (BOOL)webAnimationsMutableTimelinesEnabled
{
    return [self _boolValueForKey:WebKitWebAnimationsMutableTimelinesEnabledPreferenceKey];
}

- (void)setWebAnimationsMutableTimelinesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitWebAnimationsMutableTimelinesEnabledPreferenceKey];
}

- (BOOL)maskWebGLStringsEnabled
{
    return [self _boolValueForKey:WebKitMaskWebGLStringsEnabledPreferenceKey];
}

- (void)setMaskWebGLStringsEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitMaskWebGLStringsEnabledPreferenceKey];
}

- (BOOL)serverTimingEnabled
{
    return [self _boolValueForKey:WebKitServerTimingEnabledPreferenceKey];
}

- (void)setServerTimingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitServerTimingEnabledPreferenceKey];
}

- (BOOL)CSSCustomPropertiesAndValuesEnabled
{
    return [self _boolValueForKey:WebKitCSSCustomPropertiesAndValuesEnabledPreferenceKey];
}

- (void)setCSSCustomPropertiesAndValuesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCSSCustomPropertiesAndValuesEnabledPreferenceKey];
}

- (BOOL)resizeObserverEnabled
{
    return [self _boolValueForKey:WebKitResizeObserverEnabledPreferenceKey];
}

- (void)setResizeObserverEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitResizeObserverEnabledPreferenceKey];
}

- (BOOL)privateClickMeasurementEnabled
{
    return [self _boolValueForKey:WebKitPrivateClickMeasurementEnabledPreferenceKey];
}

- (void)setPrivateClickMeasurementEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitPrivateClickMeasurementEnabledPreferenceKey];
}

- (BOOL)fetchAPIKeepAliveEnabled
{
    return [self _boolValueForKey:WebKitFetchAPIEnabledPreferenceKey];
}

- (void)setFetchAPIKeepAliveEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitFetchAPIEnabledPreferenceKey];
}

- (BOOL)genericCueAPIEnabled
{
    return [self _boolValueForKey:WebKitGenericCueAPIEnabledKey];
}

- (void)setGenericCueAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitGenericCueAPIEnabledKey];
}

- (BOOL)aspectRatioOfImgFromWidthAndHeightEnabled
{
    return [self _boolValueForKey:WebKitAspectRatioOfImgFromWidthAndHeightEnabledPreferenceKey];
}

- (void)setAspectRatioOfImgFromWidthAndHeightEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAspectRatioOfImgFromWidthAndHeightEnabledPreferenceKey];
}

- (BOOL)referrerPolicyAttributeEnabled
{
    return [self _boolValueForKey:WebKitReferrerPolicyAttributeEnabledPreferenceKey];
}

- (void)setReferrerPolicyAttributeEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitReferrerPolicyAttributeEnabledPreferenceKey];
}

- (BOOL)coreMathMLEnabled
{
    return [self _boolValueForKey:WebKitCoreMathMLEnabledPreferenceKey];
}

- (void)setCoreMathMLEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCoreMathMLEnabledPreferenceKey];
}

- (BOOL)linkPreloadResponsiveImagesEnabled
{
    return [self _boolValueForKey:WebKitLinkPreloadResponsiveImagesEnabledPreferenceKey];
}

- (void)setLinkPreloadResponsiveImagesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitLinkPreloadResponsiveImagesEnabledPreferenceKey];
}

- (BOOL)remotePlaybackEnabled
{
    return [self _boolValueForKey:WebKitRemotePlaybackEnabledPreferenceKey];
}

- (void)setRemotePlaybackEnabled:(BOOL)remotePlaybackEnabled
{
    [self _setBoolValue:remotePlaybackEnabled forKey:WebKitRemotePlaybackEnabledPreferenceKey];
}

- (BOOL)readableByteStreamAPIEnabled
{
    return [self _boolValueForKey:WebKitReadableByteStreamAPIEnabledPreferenceKey];
}

- (void)setReadableByteStreamAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitReadableByteStreamAPIEnabledPreferenceKey];
}

- (BOOL)transformStreamAPIEnabled
{
    return [self _boolValueForKey:WebKitTransformStreamAPIEnabledPreferenceKey];
}

- (void)setTransformStreamAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitTransformStreamAPIEnabledPreferenceKey];
}

- (BOOL)_mediaRecorderEnabled
{
    return [self _boolValueForKey:WebKitMediaRecorderEnabledPreferenceKey];
}

- (void)_setMediaRecorderEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaRecorderEnabledPreferenceKey];
}

- (BOOL)mediaRecorderEnabled
{
    return [self _boolValueForKey:WebKitMediaRecorderEnabledPreferenceKey];
}

- (void)setMediaRecorderEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaRecorderEnabledPreferenceKey];
}

- (BOOL)CSSIndividualTransformPropertiesEnabled
{
    return [self _boolValueForKey:WebKitCSSIndividualTransformPropertiesEnabledPreferenceKey];
}

- (void)setCSSIndividualTransformPropertiesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCSSIndividualTransformPropertiesEnabledPreferenceKey];
}

- (BOOL)_speechRecognitionEnabled
{
    return [self _boolValueForKey:WebKitSpeechRecognitionEnabledPreferenceKey];
}

- (void)_setSpeechRecognitionEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitSpeechRecognitionEnabledPreferenceKey];
}

- (WebKitPitchCorrectionAlgorithm)_pitchCorrectionAlgorithm
{
    return static_cast<WebKitPitchCorrectionAlgorithm>([self _unsignedIntValueForKey:WebKitPitchCorrectionAlgorithmPreferenceKey]);
}

- (void)_setPitchCorrectionAlgorithm:(WebKitPitchCorrectionAlgorithm)pitchCorrectionAlgorithm
{
    [self _setUnsignedIntValue:pitchCorrectionAlgorithm forKey:WebKitPitchCorrectionAlgorithmPreferenceKey];
}

@end

@implementation WebPreferences (WebPrivateDeprecated)

// The preferences in this category are deprecated and have no effect. They should
// be removed when it is considered safe to do so.

- (void)setSubpixelCSSOMElementMetricsEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitSubpixelCSSOMElementMetricsEnabledPreferenceKey];
}

- (BOOL)subpixelCSSOMElementMetricsEnabled
{
    return [self _boolValueForKey:WebKitSubpixelCSSOMElementMetricsEnabledPreferenceKey];
}

- (void)setUserTimingEnabled:(BOOL)flag
{
}

- (BOOL)userTimingEnabled
{
    return YES;
}

- (BOOL)requestAnimationFrameEnabled
{
    return YES;
}

- (void)setRequestAnimationFrameEnabled:(BOOL)enabled
{
}

- (void)setResourceTimingEnabled:(BOOL)flag
{
}

- (BOOL)resourceTimingEnabled
{
    return YES;
}

- (void)setCSSShadowPartsEnabled:(BOOL)flag
{
}

- (BOOL)cssShadowPartsEnabled
{
    return YES;
}

- (void)setIsSecureContextAttributeEnabled:(BOOL)flag
{
}

- (BOOL)isSecureContextAttributeEnabled
{
    return YES;
}

- (void)setFetchAPIEnabled:(BOOL)flag
{
}

- (BOOL)fetchAPIEnabled
{
    return YES;
}

- (void)setShadowDOMEnabled:(BOOL)flag
{
}

- (BOOL)shadowDOMEnabled
{
    return YES;
}

- (void)setCustomElementsEnabled:(BOOL)flag
{
}

- (BOOL)customElementsEnabled
{
    return YES;
}

- (BOOL)keygenElementEnabled
{
    return NO;
}

- (void)setKeygenElementEnabled:(BOOL)flag
{
}

- (void)setVideoPluginProxyEnabled:(BOOL)flag
{
}

- (BOOL)isVideoPluginProxyEnabled
{
    return NO;
}

- (void)setHixie76WebSocketProtocolEnabled:(BOOL)flag
{
}

- (BOOL)isHixie76WebSocketProtocolEnabled
{
    return NO;
}

- (void)setDiskImageCacheEnabled:(BOOL)enabled
{
}

- (void)setAccelerated2dCanvasEnabled:(BOOL)enabled
{
}

- (BOOL)accelerated2dCanvasEnabled
{
    return NO;
}

- (void)setExperimentalNotificationsEnabled:(BOOL)enabled
{
}

- (BOOL)experimentalNotificationsEnabled
{
    return NO;
}

- (BOOL)selectionAcrossShadowBoundariesEnabled
{
    return YES;
}

- (void)setSelectionAcrossShadowBoundariesEnabled:(BOOL)flag
{
}

- (BOOL)isXSSAuditorEnabled
{
    return NO;
}

- (void)setXSSAuditorEnabled:(BOOL)flag
{
}

- (BOOL)subpixelAntialiasedLayerTextEnabled
{
    return NO;
}

- (void)setSubpixelAntialiasedLayerTextEnabled:(BOOL)enabled
{
}

- (BOOL)webGL2Enabled
{
    return [self _boolValueForKey:WebKitWebGLEnabledPreferenceKey];
}

- (void)setWebGL2Enabled:(BOOL)enabled
{
}

@end
