/*
 * Copyright (C) 2005, 2006, 2007, 2011, 2012 Apple Inc. All rights reserved.
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
#import "WebFrameNetworkingContext.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitSystemBits.h"
#import "WebKitSystemInterface.h"
#import "WebKitVersionChecks.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSURLExtras.h"
#import "WebSystemInterface.h"
#import <WebCore/ApplicationCacheStorage.h>
#import <WebCore/AudioSession.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/ResourceHandle.h>
#import <runtime/InitializeThreading.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

using namespace WebCore;

#if PLATFORM(IOS)
#import <AudioToolbox/AudioSession.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/ImageSource.h>
#import <WebCore/WebCoreThreadMessage.h>
#endif

NSString *WebPreferencesChangedNotification = @"WebPreferencesChangedNotification";
NSString *WebPreferencesRemovedNotification = @"WebPreferencesRemovedNotification";
NSString *WebPreferencesChangedInternalNotification = @"WebPreferencesChangedInternalNotification";
NSString *WebPreferencesCacheModelChangedInternalNotification = @"WebPreferencesCacheModelChangedInternalNotification";

#define KEY(x) (_private->identifier ? [_private->identifier.get() stringByAppendingString:(x)] : (x))

enum { WebPreferencesVersion = 1 };

static WebPreferences *_standardPreferences;
static NSMutableDictionary *webPreferencesInstances;

static unsigned webPreferencesInstanceCountWithPrivateBrowsingEnabled;

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

        const char* bundleID = [[[NSBundle mainBundle] bundleIdentifier] UTF8String];
        if (contains(documentViewerIDs, sizeof(documentViewerIDs) / sizeof(documentViewerIDs[0]), bundleID))
            return WebCacheModelDocumentViewer;
        if (contains(documentBrowserIDs, sizeof(documentBrowserIDs) / sizeof(documentBrowserIDs[0]), bundleID))
            return WebCacheModelDocumentBrowser;
        if (contains(primaryWebBrowserIDs, sizeof(primaryWebBrowserIDs) / sizeof(primaryWebBrowserIDs[0]), bundleID))
            return WebCacheModelPrimaryWebBrowser;

        bool isLinkedAgainstWebKit = WebKitLinkedOnOrAfter(0);
        if (!isLinkedAgainstWebKit)
            return WebCacheModelDocumentViewer; // Apps that don't link against WebKit probably aren't meant to be browsers.

#if !PLATFORM(IOS)
        bool isLegacyApp = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_CACHE_MODEL_API);
#else
        bool isLegacyApp = false;
#endif
        if (isLegacyApp)
            return WebCacheModelDocumentBrowser; // To avoid regressions in apps that depended on old WebKit's large cache.

        return WebCacheModelDocumentViewer; // To save memory.
    }
}

@interface WebPreferences ()
- (void)_postCacheModelChangedNotification;
@end

@interface WebPreferences (WebInternal)
+ (NSString *)_concatenateKeyWithIBCreatorID:(NSString *)key;
+ (NSString *)_IBCreatorID;
@end

struct WebPreferencesPrivate
{
public:
    WebPreferencesPrivate()
    : autosaves(NO)
    , automaticallyDetectsCacheModel(NO)
    , numWebViews(0)
#if PLATFORM(IOS)
    , readWriteQueue(dispatch_queue_create("com.apple.WebPreferences.ReadWriteQueue", DISPATCH_QUEUE_CONCURRENT))
#endif
    {
    }

#if PLATFORM(IOS)
    ~WebPreferencesPrivate()
    {
        dispatch_release(readWriteQueue);
    }
#endif

    RetainPtr<NSMutableDictionary> values;
    RetainPtr<NSString> identifier;
    BOOL autosaves;
    BOOL automaticallyDetectsCacheModel;
    unsigned numWebViews;
#if PLATFORM(IOS)
    dispatch_queue_t readWriteQueue;
#endif
};

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

#if PLATFORM(IOS)
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

#if PLATFORM(IOS)
- (id)initWithIdentifier:(NSString *)anIdentifier
{
    return [self initWithIdentifier:anIdentifier sendChangeNotification:YES];
}
#endif

#if PLATFORM(IOS)
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

    [self _updatePrivateBrowsingStateFrom:NO to:[self privateBrowsingEnabled]];

#if PLATFORM(IOS)
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
            [decoder decodeValueOfObjCType:@encode(int) at:&version];
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
        [self _updatePrivateBrowsingStateFrom:NO to:[self privateBrowsingEnabled]];
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    if ([encoder allowsKeyedCoding]){
        [encoder encodeObject:_private->identifier.get() forKey:@"Identifier"];
#if PLATFORM(IOS)
        dispatch_sync(_private->readWriteQueue, ^{
#endif
        [encoder encodeObject:_private->values.get() forKey:@"Values"];
        LOG (Encoding, "Identifier = %@, Values = %@\n", _private->identifier.get(), _private->values.get());
#if PLATFORM(IOS)
        });
#endif
    }
    else {
        int version = WebPreferencesVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:_private->identifier.get()];
#if PLATFORM(IOS)
        dispatch_sync(_private->readWriteQueue, ^{
#endif
        [encoder encodeObject:_private->values.get()];
#if PLATFORM(IOS)
        });
#endif
    }
}

+ (WebPreferences *)standardPreferences
{
#if !PLATFORM(IOS)
    if (_standardPreferences == nil) {
        _standardPreferences = [[WebPreferences alloc] initWithIdentifier:nil];
        [_standardPreferences setAutosaves:YES];
    }
#else
    // FIXME: This check is necessary to avoid recursion (see <rdar://problem/9564337>), but it also makes _standardPreferences construction not thread safe.
    if (_standardPreferences)
        return _standardPreferences;

    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        _standardPreferences = [[WebPreferences alloc] initWithIdentifier:nil sendChangeNotification:NO];
        [_standardPreferences _postPreferencesChangedNotification];
        [_standardPreferences setAutosaves:YES];
    });
#endif

    return _standardPreferences;
}

// if we ever have more than one WebPreferences object, this would move to init
+ (void)initialize
{
#if !PLATFORM(IOS)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
#endif
#if PLATFORM(IOS)
    static unsigned defaultMaximumImageSize = 20 * 1024 * 1024;
#endif

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        @"Times",                       WebKitStandardFontPreferenceKey,
        @"Courier",                     WebKitFixedFontPreferenceKey,
        @"Times",                       WebKitSerifFontPreferenceKey,
        @"Helvetica",                   WebKitSansSerifFontPreferenceKey,
#if !PLATFORM(IOS)
        @"Apple Chancery",              WebKitCursiveFontPreferenceKey,
#else
        @"Snell Roundhand",             WebKitCursiveFontPreferenceKey,
#endif
        @"Papyrus",                     WebKitFantasyFontPreferenceKey,
#if PLATFORM(IOS)
        @"AppleColorEmoji",             WebKitPictographFontPreferenceKey,
#else
        @"Apple Color Emoji",           WebKitPictographFontPreferenceKey,
#endif
        @"0",                           WebKitMinimumFontSizePreferenceKey,
        @"9",                           WebKitMinimumLogicalFontSizePreferenceKey, 
        @"16",                          WebKitDefaultFontSizePreferenceKey,
        @"13",                          WebKitDefaultFixedFontSizePreferenceKey,
        @"ISO-8859-1",                  WebKitDefaultTextEncodingNamePreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUsesEncodingDetectorPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUserStyleSheetEnabledPreferenceKey,
        @"",                            WebKitUserStyleSheetLocationPreferenceKey,
#if !PLATFORM(IOS)
        [NSNumber numberWithBool:NO],   WebKitShouldPrintBackgroundsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitTextAreasAreResizablePreferenceKey,
#endif
        [NSNumber numberWithBool:NO],   WebKitShrinksStandaloneImagesToFitPreferenceKey,
#if !PLATFORM(IOS)
        [NSNumber numberWithBool:YES],  WebKitJavaEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitJavaScriptEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitWebSecurityEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowUniversalAccessFromFileURLsPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowFileAccessFromFileURLsPreferenceKey,
#if PLATFORM(IOS)
        [NSNumber numberWithBool:NO],   WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
#else
        [NSNumber numberWithBool:YES],  WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitPluginsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDatabasesEnabledPreferenceKey,
#if PLATFORM(IOS)
        [NSNumber numberWithBool:NO],   WebKitStorageTrackerEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitLocalStorageEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitExperimentalNotificationsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImagesPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImageLoopingPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDisplayImagesKey,
        [NSNumber numberWithBool:NO],   WebKitLoadSiteIconsKey,
        @"1800",                        WebKitBackForwardCacheExpirationIntervalKey,
#if !PLATFORM(IOS)
        [NSNumber numberWithBool:NO],   WebKitTabToLinksPreferenceKey,
#endif
        [NSNumber numberWithBool:NO],   WebKitPrivateBrowsingEnabledPreferenceKey,
#if !PLATFORM(IOS)
        [NSNumber numberWithBool:NO],   WebKitRespectStandardStyleKeyEquivalentsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowsURLsInToolTipsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowsToolTipOverTruncatedTextPreferenceKey,
        @"1",                           WebKitPDFDisplayModePreferenceKey,
        @"0",                           WebKitPDFScaleFactorPreferenceKey,
#endif
        @"0",                           WebKitUseSiteSpecificSpoofingPreferenceKey,
        [NSNumber numberWithInt:WebKitEditableLinkDefaultBehavior], WebKitEditableLinkBehaviorPreferenceKey,
#if !PLATFORM(IOS)
        [NSNumber numberWithInt:WebTextDirectionSubmenuAutomaticallyIncluded],
                                        WebKitTextDirectionSubmenuInclusionBehaviorPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitDOMPasteAllowedPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitUsesPageCachePreferenceKey,
        [NSNumber numberWithInt:cacheModelForMainBundle()], WebKitCacheModelPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitPageCacheSupportsPluginsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitDeveloperExtrasEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitJavaScriptExperimentsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAuthorAndUserStylesEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitApplicationChromeModeEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitWebArchiveDebugModeEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitLocalFileContentSniffingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitOfflineWebApplicationCacheEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitZoomsTextOnlyPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitJavaScriptCanAccessClipboardPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitXSSAuditorEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAcceleratedCompositingEnabledPreferenceKey,
        // CSS Shaders also need WebGL enabled (which is disabled by default), so we can keep it enabled for now.
        [NSNumber numberWithBool:YES], WebKitCSSCustomFilterEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitCSSRegionsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitCSSCompositingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitCSSGridLayoutEnabledPreferenceKey,
#if PLATFORM(IOS) && !PLATFORM(IOS_SIMULATOR)
        [NSNumber numberWithBool:YES],  WebKitAcceleratedDrawingEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitCanvasUsesAcceleratedDrawingPreferenceKey,
#else
        [NSNumber numberWithBool:NO],  WebKitAcceleratedDrawingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitCanvasUsesAcceleratedDrawingPreferenceKey,
#endif
        [NSNumber numberWithBool:NO],   WebKitShowDebugBordersPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowRepaintCounterPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitWebGLEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitMultithreadedWebGLEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitForceSoftwareWebGLRenderingPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitAccelerated2dCanvasEnabledPreferenceKey,
#if PLATFORM(IOS)
        [NSNumber numberWithBool:YES],  WebKitFrameFlatteningEnabledPreferenceKey,
#else
        [NSNumber numberWithBool:NO],   WebKitFrameFlatteningEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:NO],   WebKitSpatialNavigationEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitDNSPrefetchingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitFullScreenEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitAsynchronousSpellCheckingEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitHyperlinkAuditingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUsePreHTML5ParserQuirksKey,
#if !PLATFORM(IOS)
        [NSNumber numberWithBool:YES],  WebKitAVFoundationEnabledKey,
        [NSNumber numberWithBool:NO],   WebKitMediaPlaybackRequiresUserGesturePreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitMediaPlaybackAllowsInlinePreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitWebAudioEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitSuppressesIncrementalRenderingKey,
        [NSNumber numberWithBool:NO],   WebKitRegionBasedColumnsEnabledKey,
        [NSNumber numberWithBool:YES],  WebKitBackspaceKeyNavigationEnabledKey,
        [NSNumber numberWithBool:NO],   WebKitShouldDisplaySubtitlesPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShouldDisplayCaptionsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShouldDisplayTextDescriptionsPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitNotificationsEnabledKey,
        [NSNumber numberWithBool:NO],   WebKitShouldRespectImageOrientationKey,
#else
        [NSNumber numberWithBool:NO],   WebKitAVFoundationEnabledKey,
        [NSNumber numberWithBool:YES],  WebKitMediaPlaybackRequiresUserGesturePreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitMediaPlaybackAllowsInlinePreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitMediaPlaybackAllowsAirPlayPreferenceKey,
        [NSNumber numberWithUnsignedInt:AudioSession::None],  WebKitAudioSessionCategoryOverride,
        [NSNumber numberWithLongLong:WebCore::ApplicationCacheStorage::noQuota()], WebKitApplicationCacheTotalQuota,

        // Per-Origin Quota on iOS is 25MB. When the quota is reached for a particular origin
        // the quota for that origin can be increased. See also webView:exceededApplicationCacheOriginQuotaForSecurityOrigin:totalSpaceNeeded in WebUI/WebUIDelegate.m.
        [NSNumber numberWithLongLong:(25 * 1024 * 1024)], WebKitApplicationCacheDefaultOriginQuota,

        // Enable WebAudio by default in all iOS UIWebViews
        [NSNumber numberWithBool:YES],   WebKitWebAudioEnabledPreferenceKey,

        [NSNumber numberWithBool:YES],   WebKitShouldRespectImageOrientationKey,
#endif // PLATFORM(IOS)
        [NSNumber numberWithBool:YES],  WebKitRequestAnimationFrameEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitWantsBalancedSetDefersLoadingBehaviorKey,
        [NSNumber numberWithBool:NO],   WebKitDiagnosticLoggingEnabledKey,
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
        [NSNumber numberWithBool:NO],
#else
        [NSNumber numberWithBool:YES],
#endif
                                        WebKitScreenFontSubstitutionEnabledKey,
        [NSNumber numberWithInt:WebAllowAllStorage], WebKitStorageBlockingPolicyKey,
        [NSNumber numberWithBool:NO],   WebKitPlugInSnapshottingEnabledPreferenceKey,

#if PLATFORM(IOS)
        [NSNumber numberWithUnsignedInt:defaultMaximumImageSize], WebKitMaximumImageSizePreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitTelephoneParsingEnabledPreferenceKey,
        [NSNumber numberWithInt:-1],      WebKitLayoutIntervalPreferenceKey,
        [NSNumber numberWithFloat:-1.0f], WebKitMaxParseDurationPreferenceKey,
        [NSNumber numberWithBool:NO],     WebKitAllowMultiElementImplicitFormSubmissionPreferenceKey,
        [NSNumber numberWithInt:-1],      WebKitPageCacheSizePreferenceKey,
        [NSNumber numberWithInt:-1],      WebKitObjectCacheSizePreferenceKey,
        [NSNumber numberWithInt:-1],      WebKitNSURLMemoryCacheSizePreferenceKey,
        [NSNumber numberWithInt:-1],      WebKitNSURLDiskCacheSizePreferenceKey,
        [NSNumber numberWithBool:NO],     WebKitAlwaysRequestGeolocationPermissionPreferenceKey,
        [NSNumber numberWithInt:InterpolationLow], WebKitInterpolationQualityPreferenceKey,
        [NSNumber numberWithBool:YES],    WebKitPasswordEchoEnabledPreferenceKey,
        [NSNumber numberWithFloat:2.0f],  WebKitPasswordEchoDurationPreferenceKey,
        [NSNumber numberWithBool:NO],     WebKitNetworkDataUsageTrackingEnabledPreferenceKey,
        @"",                              WebKitNetworkInterfaceNamePreferenceKey,
#endif
#if ENABLE(DISK_IMAGE_CACHE)
        [NSNumber numberWithBool:YES], WebKitDiskImageCacheEnabledPreferenceKey,
        [NSNumber numberWithUnsignedInt:(100 * 1024)], WebKitDiskImageCacheMinimumImageSizePreferenceKey,
        [NSNumber numberWithUnsignedInt:(100 * 1024 * 1024)], WebKitDiskImageCacheMaximumCacheSizePreferenceKey,
#endif
#if ENABLE(IOS_TEXT_AUTOSIZING)
        [NSNumber numberWithFloat:WKGetMinimumZoomFontSize()], WebKitMinimumZoomFontSizePreferenceKey,
#endif
#if ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)
        [NSNumber numberWithBool:YES], WebKitDiskImageCacheEnabledPreferenceKey,
        [NSNumber numberWithUnsignedInt:(100 * 1024)], WebKitDiskImageCacheMinimumImageSizePreferenceKey,
        [NSNumber numberWithUnsignedInt:(100 * 1024 * 1024)], WebKitDiskImageCacheMaximumCacheSizePreferenceKey,
#endif
        [NSNumber numberWithLongLong:ApplicationCacheStorage::noQuota()], WebKitApplicationCacheTotalQuota,
        [NSNumber numberWithLongLong:ApplicationCacheStorage::noQuota()], WebKitApplicationCacheDefaultOriginQuota,
        [NSNumber numberWithBool:YES],  WebKitQTKitEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitHiddenPageDOMTimerThrottlingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitHiddenPageCSSAnimationSuspensionEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitLowPowerVideoAudioBufferSizeEnabledPreferenceKey,
        
        [NSNumber numberWithBool:NO], WebKitUseLegacyTextAlignPositionedElementBehaviorPreferenceKey,
        
#if !PLATFORM(IOS)
        [NSNumber numberWithBool:NO],   WebKitVideoPluginProxyEnabledKey,
#else
        [NSNumber numberWithBool:YES],   WebKitVideoPluginProxyEnabledKey,
#endif
#if ENABLE(MEDIA_SOURCE)
        [NSNumber numberWithBool:NO], WebKitMediaSourceEnabledPreferenceKey,
#endif
        nil];

#if !PLATFORM(IOS)
    // This value shouldn't ever change, which is assumed in the initialization of WebKitPDFDisplayModePreferenceKey above
    ASSERT(kPDFDisplaySinglePageContinuous == 1);
#endif
    [[NSUserDefaults standardUserDefaults] registerDefaults:dict];
}

- (void)dealloc
{
    [self _updatePrivateBrowsingStateFrom:[self privateBrowsingEnabled] to:NO];

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
#if PLATFORM(IOS)
    __block id o = nil;
    dispatch_sync(_private->readWriteQueue, ^{
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
#if PLATFORM(IOS)
    dispatch_barrier_sync(_private->readWriteQueue, ^{
#endif
    [_private->values.get() setObject:value forKey:_key];
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
    dispatch_barrier_sync(_private->readWriteQueue, ^{
#endif
    [_private->values.get() _webkit_setInt:value forKey:_key];
#if PLATFORM(IOS)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setInteger:value forKey:_key];
    [self _postPreferencesChangedNotification];
}

#if PLATFORM(IOS)
- (unsigned int)_unsignedIntValueForKey:(NSString *)key
{
    id o = [self _valueForKey:key];
    return [o respondsToSelector:@selector(unsignedIntValue)] ? [o unsignedIntValue] : 0;
}

- (void)_setUnsignedIntValue:(unsigned int)value forKey:(NSString *)key
{    if ([self _unsignedIntValueForKey:key] == value)
        return;
    NSString *_key = KEY(key);
    dispatch_barrier_sync(_private->readWriteQueue, ^{
    [_private->values.get() _webkit_setUnsignedInt:value forKey:_key];
    });
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithUnsignedInt:value] forKey:_key];
    [self _postPreferencesChangedNotification];
}
#endif

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
#if PLATFORM(IOS)
    dispatch_barrier_sync(_private->readWriteQueue, ^{
#endif
    [_private->values.get() _webkit_setFloat:value forKey:_key];
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
    dispatch_barrier_sync(_private->readWriteQueue, ^{
#endif
    [_private->values.get() _webkit_setBool:value forKey:_key];
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
    dispatch_barrier_sync(_private->readWriteQueue, ^{
#endif
    [_private->values.get() _webkit_setLongLong:value forKey:_key];
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
    dispatch_barrier_sync(_private->readWriteQueue, ^{
#endif
    [_private->values.get() _webkit_setUnsignedLongLong:value forKey:_key];
#if PLATFORM(IOS)
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

#if !PLATFORM(IOS)
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
#endif // PLATFORM(IOS)

#if !PLATFORM(IOS)
- (BOOL)shouldPrintBackgrounds
{
    return [self _boolValueForKey: WebKitShouldPrintBackgroundsPreferenceKey];
}

- (void)setShouldPrintBackgrounds:(BOOL)flag
{
    [self _setBoolValue: flag forKey: WebKitShouldPrintBackgroundsPreferenceKey];
}
#endif

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

#if !PLATFORM(IOS)
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
    [self _updatePrivateBrowsingStateFrom:[self privateBrowsingEnabled] to:enabled];
    [self _setBoolValue:enabled forKey:WebKitPrivateBrowsingEnabledPreferenceKey];
}

- (BOOL)privateBrowsingEnabled
{
    return [self _boolValueForKey:WebKitPrivateBrowsingEnabledPreferenceKey];
}

- (void)_updatePrivateBrowsingStateFrom:(BOOL)wasEnabled to:(BOOL)enabled
{
    if (enabled == wasEnabled)
        return;
    if (enabled > wasEnabled) {
        WebFrameNetworkingContext::ensurePrivateBrowsingSession();
        ++webPreferencesInstanceCountWithPrivateBrowsingEnabled;
    } else {
        ASSERT(webPreferencesInstanceCountWithPrivateBrowsingEnabled);
        --webPreferencesInstanceCountWithPrivateBrowsingEnabled;
        if (!webPreferencesInstanceCountWithPrivateBrowsingEnabled)
            WebFrameNetworkingContext::destroyPrivateBrowsingSession();
    }
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
#if !PLATFORM(IOS)
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

@end

@implementation WebPreferences (WebPrivate)

#if PLATFORM(IOS) && !(__IPHONE_OS_VERSION_MIN_REQUIRED >= 60000)
- (void) setWebInspectorServerEnabled:(BOOL)flag
{
}
#endif

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

- (void)setJavaScriptExperimentsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitJavaScriptExperimentsEnabledPreferenceKey];
}

- (BOOL)javaScriptExperimentsEnabled
{
    return [self _boolValueForKey:WebKitJavaScriptExperimentsEnabledPreferenceKey];
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

#if !PLATFORM(IOS)
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
#endif // !PLATFORM(IOS)

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
    return (NSTimeInterval)[self _floatValueForKey:WebKitBackForwardCacheExpirationIntervalKey];
}

#if PLATFORM(IOS)
- (size_t)_maximumImageSize
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:WebKitMaximumImageSizePreferenceKey];
}

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
#endif

#if ENABLE(IOS_TEXT_AUTOSIZING)
- (void)_setMinimumZoomFontSize:(float)size
{
    [self _setFloatValue:size forKey:WebKitMinimumZoomFontSizePreferenceKey];
}

- (float)_minimumZoomFontSize
{
    return [self _floatValueForKey:WebKitMinimumZoomFontSizePreferenceKey];
}
#endif

#if PLATFORM(IOS)
- (void)_setLayoutInterval:(int)l
{
    [self _setIntegerValue:l forKey:WebKitLayoutIntervalPreferenceKey];
}

- (int)_layoutInterval
{
    return [self _integerValueForKey:WebKitLayoutIntervalPreferenceKey];
}

- (void)_setMaxParseDuration:(float)d
{
    [self _setFloatValue:d forKey:WebKitMaxParseDurationPreferenceKey];
}

- (float)_maxParseDuration
{
    return [self _floatValueForKey:WebKitMaxParseDurationPreferenceKey];
}

- (void)_setPageCacheSize:(int)size
{
    [self _setIntegerValue:size forKey:WebKitPageCacheSizePreferenceKey];
}

- (int)_pageCacheSize
{
    return [self _integerValueForKey:WebKitPageCacheSizePreferenceKey];
}

- (void)_setObjectCacheSize:(int)size
{
    [self _setIntegerValue:size forKey:WebKitObjectCacheSizePreferenceKey];
}

- (int)_objectCacheSize
{
    return [self _integerValueForKey:WebKitObjectCacheSizePreferenceKey];
}

- (void)_setNSURLMemoryCacheSize:(int)size
{
    [self _setIntegerValue:size forKey:WebKitNSURLMemoryCacheSizePreferenceKey];
}

- (int)_NSURLMemoryCacheSize
{
    return [self _integerValueForKey:WebKitNSURLMemoryCacheSizePreferenceKey];
}

- (void)_setNSURLDiskCacheSize:(int)size
{
    [self _setIntegerValue:size forKey:WebKitNSURLDiskCacheSizePreferenceKey];
}

- (int)_NSURLDiskCacheSize
{
    return [self _integerValueForKey:WebKitNSURLDiskCacheSizePreferenceKey];
}

- (void)_setAlwaysUseBaselineOfPrimaryFont:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAlwaysUseBaselineOfPrimaryFontPreferenceKey];
}

- (BOOL)_alwaysUseBaselineOfPrimaryFont
{
    return [self _boolValueForKey:WebKitAlwaysUseBaselineOfPrimaryFontPreferenceKey];
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

- (void)_setAlwaysUseAcceleratedOverflowScroll:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAlwaysUseAcceleratedOverflowScrollPreferenceKey];
}

- (BOOL)_alwaysUseAcceleratedOverflowScroll
{
    return [self _boolValueForKey:WebKitAlwaysUseAcceleratedOverflowScrollPreferenceKey];
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

#endif // PLATFORM(IOS)

#if !PLATFORM(IOS)
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

#if !PLATFORM(IOS)
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

#if PLATFORM(IOS)
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

- (void)_postPreferencesChangedNotification
{
#if !PLATFORM(IOS)
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }
#endif

    [[NSNotificationCenter defaultCenter] postNotificationName:WebPreferencesChangedInternalNotification object:self userInfo:nil];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebPreferencesChangedNotification object:self userInfo:nil];
}

- (void)_postPreferencesChangedAPINotification
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }

    [[NSNotificationCenter defaultCenter] postNotificationName:WebPreferencesChangedNotification object:self userInfo:nil];
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

+ (void)_switchNetworkLoaderToNewTestingSession
{
    InitWebCoreSystemInterface();
    NetworkStorageSession::switchToNewTestingSession();
}

+ (void)_setCurrentNetworkLoaderSessionCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy
{
    WKSetHTTPCookieAcceptPolicy(NetworkStorageSession::defaultStorageSession().cookieStorage().get(), policy);
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

- (BOOL)cssCustomFilterEnabled
{
    return [self _boolValueForKey:WebKitCSSCustomFilterEnabledPreferenceKey];
}

- (void)setCSSCustomFilterEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitCSSCustomFilterEnabledPreferenceKey];
}

- (BOOL)cssRegionsEnabled
{
    return [self _boolValueForKey:WebKitCSSRegionsEnabledPreferenceKey];
}

- (void)setCSSRegionsEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitCSSRegionsEnabledPreferenceKey];
}

- (BOOL)cssCompositingEnabled
{
    return [self _boolValueForKey:WebKitCSSCompositingEnabledPreferenceKey];
}

- (void)setCSSCompositingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitCSSCompositingEnabledPreferenceKey];
}

- (BOOL)cssGridLayoutEnabled
{
    return [self _boolValueForKey:WebKitCSSGridLayoutEnabledPreferenceKey];
}

- (void)setCSSGridLayoutEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitCSSGridLayoutEnabledPreferenceKey];
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

- (BOOL)multithreadedWebGLEnabled
{
    return [self _boolValueForKey:WebKitMultithreadedWebGLEnabledPreferenceKey];
}

- (void)setMultithreadedWebGLEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitMultithreadedWebGLEnabledPreferenceKey];
}

- (BOOL)forceSoftwareWebGLRendering
{
    return [self _boolValueForKey:WebKitForceSoftwareWebGLRenderingPreferenceKey];
}

- (void)setForceSoftwareWebGLRendering:(BOOL)forced
{
    [self _setBoolValue:forced forKey:WebKitForceSoftwareWebGLRenderingPreferenceKey];
}

- (BOOL)accelerated2dCanvasEnabled
{
    return [self _boolValueForKey:WebKitAccelerated2dCanvasEnabledPreferenceKey];
}

- (void)setAccelerated2dCanvasEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitAccelerated2dCanvasEnabledPreferenceKey];
}

#if ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)
- (BOOL)diskImageCacheEnabled
{
    return [self _boolValueForKey:WebKitDiskImageCacheEnabledPreferenceKey];
}

- (void)setDiskImageCacheEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitDiskImageCacheEnabledPreferenceKey];
}

- (unsigned)diskImageCacheMinimumImageSize
{
    return [self _integerValueForKey:WebKitDiskImageCacheMinimumImageSizePreferenceKey];
}

- (void)setDiskImageCacheMinimumImageSize:(unsigned)minimumSize
{
    [self _setIntegerValue:minimumSize forKey:WebKitDiskImageCacheMinimumImageSizePreferenceKey];
}

- (unsigned)diskImageCacheMaximumCacheSize
{
    return [self _integerValueForKey:WebKitDiskImageCacheMaximumCacheSizePreferenceKey];
}

- (void)setDiskImageCacheMaximumCacheSize:(unsigned)maximumSize
{
    [self _setIntegerValue:maximumSize forKey:WebKitDiskImageCacheMaximumCacheSizePreferenceKey];
}

- (NSString *)_diskImageCacheSavedCacheDirectory
{
    return [[self _stringValueForKey:WebKitDiskImageCacheSavedCacheDirectoryKey] stringByStandardizingPath];
}

- (void)_setDiskImageCacheSavedCacheDirectory:(NSString *)path
{
    [self _setStringValue:[path stringByStandardizingPath] forKey:WebKitDiskImageCacheSavedCacheDirectoryKey];
}
#endif // ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)

- (BOOL)isFrameFlatteningEnabled
{
    return [self _boolValueForKey:WebKitFrameFlatteningEnabledPreferenceKey];
}

- (void)setFrameFlatteningEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitFrameFlatteningEnabledPreferenceKey];
}

- (BOOL)isSpatialNavigationEnabled
{
    return [self _boolValueForKey:WebKitSpatialNavigationEnabledPreferenceKey];
}

- (void)setSpatialNavigationEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitSpatialNavigationEnabledPreferenceKey];
}

- (BOOL)paginateDuringLayoutEnabled
{
    return [self _boolValueForKey:WebKitPaginateDuringLayoutEnabledPreferenceKey];
}

- (void)setPaginateDuringLayoutEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitPaginateDuringLayoutEnabledPreferenceKey];
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

- (void)setQTKitEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitQTKitEnabledPreferenceKey];
}

- (BOOL)isQTKitEnabled
{
    return [self _boolValueForKey:WebKitQTKitEnabledPreferenceKey];
}

- (void)setVideoPluginProxyEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitVideoPluginProxyEnabledKey];
}

- (BOOL)isVideoPluginProxyEnabled
{
    return [self _boolValueForKey:WebKitVideoPluginProxyEnabledKey];
}

- (void)setHixie76WebSocketProtocolEnabled:(BOOL)flag
{
}

- (BOOL)isHixie76WebSocketProtocolEnabled
{
    return false;
}

#if PLATFORM(IOS)
- (BOOL)mediaPlaybackAllowsAirPlay
{
    return [self _boolValueForKey:WebKitMediaPlaybackAllowsAirPlayPreferenceKey];
}

- (void)setMediaPlaybackAllowsAirPlay:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaPlaybackAllowsAirPlayPreferenceKey];
}

- (unsigned)audioSessionCategoryOverride
{
    return [self _unsignedIntValueForKey:WebKitAudioSessionCategoryOverride];
}

- (void)setAudioSessionCategoryOverride:(unsigned)override
{
    if (override > AudioSession::AudioProcessing) {
        // Clients are passing us OSTypes values from AudioToolbox/AudioSession.h,
        // which need to be translated into AudioSession::CategoryType:
        switch (override) {
        case kAudioSessionCategory_AmbientSound:
            override = AudioSession::AmbientSound;
            break;
        case kAudioSessionCategory_SoloAmbientSound:
            override = AudioSession::SoloAmbientSound;
            break;
        case kAudioSessionCategory_MediaPlayback:
            override = AudioSession::MediaPlayback;
            break;
        case kAudioSessionCategory_RecordAudio:
            override = AudioSession::RecordAudio;
            break;
        case kAudioSessionCategory_PlayAndRecord:
            override = AudioSession::PlayAndRecord;
            break;
        case kAudioSessionCategory_AudioProcessing:
            override = AudioSession::AudioProcessing;
            break;
        default:
            override = AudioSession::None;
            break;
        }
    }

    [self _setUnsignedIntValue:override forKey:WebKitAudioSessionCategoryOverride];
}

- (BOOL)networkDataUsageTrackingEnabled
{
    return [self _boolValueForKey:WebKitNetworkDataUsageTrackingEnabledPreferenceKey];
}

- (void)setNetworkDataUsageTrackingEnabled:(bool)trackingEnabled
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
#endif // PLATFORM(IOS)

- (BOOL)mediaPlaybackRequiresUserGesture
{
    return [self _boolValueForKey:WebKitMediaPlaybackRequiresUserGesturePreferenceKey];
}

- (void)setMediaPlaybackRequiresUserGesture:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaPlaybackRequiresUserGesturePreferenceKey];
}

- (BOOL)mediaPlaybackAllowsInline
{
    return [self _boolValueForKey:WebKitMediaPlaybackAllowsInlinePreferenceKey];
}

- (void)setMediaPlaybackAllowsInline:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaPlaybackAllowsInlinePreferenceKey];
}

- (BOOL)mockScrollbarsEnabled
{
    return [self _boolValueForKey:WebKitMockScrollbarsEnabledPreferenceKey];
}

- (void)setMockScrollbarsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMockScrollbarsEnabledPreferenceKey];
}

- (BOOL)seamlessIFramesEnabled
{
    return [self _boolValueForKey:WebKitSeamlessIFramesEnabledPreferenceKey];
}

- (void)setSeamlessIFramesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitSeamlessIFramesEnabledPreferenceKey];
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

#if PLATFORM(IOS)
- (void)_invalidateCachedPreferences
{
    BOOL privateBrowsingWasEnabled = [self privateBrowsingEnabled];

    dispatch_barrier_sync(_private->readWriteQueue, ^{
        if (_private->values)
            _private->values = adoptNS([[NSMutableDictionary alloc] init]);
    });

    [self _updatePrivateBrowsingStateFrom:privateBrowsingWasEnabled to:[self privateBrowsingEnabled]];

    // Tell any live WebViews to refresh their preferences
    [self _postPreferencesChangedNotification];
}

- (void)_synchronizeWebStoragePolicyWithCookiePolicy
{
    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
    WebStorageBlockingPolicy storageBlockingPolicy;
    switch (cookieAcceptPolicy) {
    case NSHTTPCookieAcceptPolicyAlways:
        storageBlockingPolicy = WebAllowAllStorage;
        break;
    case NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
        storageBlockingPolicy = WebBlockThirdPartyStorage;
        break;
    case NSHTTPCookieAcceptPolicyNever:
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

- (void)setRegionBasedColumnsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitRegionBasedColumnsEnabledKey];
}

- (BOOL)regionBasedColumnsEnabled
{
    return [self _boolValueForKey:WebKitRegionBasedColumnsEnabledKey];
}

- (void)setShouldRespectImageOrientation:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShouldRespectImageOrientationKey];
}

- (BOOL)shouldRespectImageOrientation
{
    return [self _boolValueForKey:WebKitShouldRespectImageOrientationKey];
}

- (BOOL)requestAnimationFrameEnabled
{
    return [self _boolValueForKey:WebKitRequestAnimationFrameEnabledPreferenceKey];
}

- (void)setRequestAnimationFrameEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitRequestAnimationFrameEnabledPreferenceKey];
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

static bool needsScreenFontsEnabledQuirk()
{
#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    static bool is1PasswordNeedingScreenFontsQuirk = WKExecutableWasLinkedOnOrBeforeMountainLion()
        && [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"ws.agile.1Password"];
    return is1PasswordNeedingScreenFontsQuirk;
#else
    return NO;
#endif
}

- (BOOL)screenFontSubstitutionEnabled
{
    if (needsScreenFontsEnabledQuirk())
        return YES;
    return [self _boolValueForKey:WebKitScreenFontSubstitutionEnabledKey];
}

- (void)setScreenFontSubstitutionEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitScreenFontSubstitutionEnabledKey];
}

- (void)setStorageBlockingPolicy:(WebStorageBlockingPolicy)storageBlockingPolicy
{
#if PLATFORM(IOS)
    // We don't want to write the setting out, so we just reset the default instead of storing the new setting.
    NSDictionary *dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:storageBlockingPolicy] forKey:WebKitStorageBlockingPolicyKey];
    [[NSUserDefaults standardUserDefaults] registerDefaults:dict];
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
    return [self _boolValueForKey:WebKitPlugInSnapshottingEnabledPreferenceKey];
}

- (void)setPlugInSnapshottingEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitPlugInSnapshottingEnabledPreferenceKey];
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
