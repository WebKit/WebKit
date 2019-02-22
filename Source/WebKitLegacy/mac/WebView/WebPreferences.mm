/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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

#import "WebPreferencesPrivate.h"
#import "WebPreferenceKeysPrivate.h"

#import "NetworkStorageSessionMap.h"
#import "WebApplicationCache.h"
#import "WebFrameNetworkingContext.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitVersionChecks.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSURLExtras.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/ApplicationCacheStorage.h>
#import <WebCore/AudioSession.h>
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/Settings.h>
#import <WebCore/TextEncodingRegistry.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

using namespace WebCore;

#if HAVE(AUDIO_TOOLBOX_AUDIO_SESSION)
#import <AudioToolbox/AudioSession.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <WebCore/Device.h>
#import <WebCore/GraphicsContext.h>
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
    : inPrivateBrowsing(NO)
    , autosaves(NO)
    , automaticallyDetectsCacheModel(NO)
    , numWebViews(0)
#if PLATFORM(IOS_FAMILY)
    , readWriteQueue(dispatch_queue_create("com.apple.WebPreferences.ReadWriteQueue", DISPATCH_QUEUE_CONCURRENT))
#endif
    {
    }

#if PLATFORM(IOS_FAMILY)
    ~WebPreferencesPrivate()
    {
        dispatch_release(readWriteQueue);
    }
#endif

    RetainPtr<NSMutableDictionary> values;
    BOOL inPrivateBrowsing;
    RetainPtr<NSString> identifier;
    BOOL autosaves;
    BOOL automaticallyDetectsCacheModel;
    unsigned numWebViews;
#if PLATFORM(IOS_FAMILY)
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
        [self _updatePrivateBrowsingStateTo:[self privateBrowsingEnabled]];
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    if ([encoder allowsKeyedCoding]){
        [encoder encodeObject:_private->identifier.get() forKey:@"Identifier"];
#if PLATFORM(IOS_FAMILY)
        dispatch_sync(_private->readWriteQueue, ^{
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
        dispatch_sync(_private->readWriteQueue, ^{
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
#if !PLATFORM(IOS_FAMILY)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
    bool attachmentElementEnabled = MacApplication::isAppleMail();
#else
    bool allowsInlineMediaPlayback = WebCore::deviceClass() == MGDeviceClassiPad;
    bool allowsInlineMediaPlaybackAfterFullscreen = WebCore::deviceClass() != MGDeviceClassiPad;
    bool requiresPlaysInlineAttribute = !allowsInlineMediaPlayback;
    bool attachmentElementEnabled = IOSApplication::isMobileMail();
#endif

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        @"Times",                       WebKitStandardFontPreferenceKey,
        @"Courier",                     WebKitFixedFontPreferenceKey,
        @"Times",                       WebKitSerifFontPreferenceKey,
        @"Helvetica",                   WebKitSansSerifFontPreferenceKey,
#if !PLATFORM(IOS_FAMILY)
        @"Apple Chancery",              WebKitCursiveFontPreferenceKey,
#else
        @"Snell Roundhand",             WebKitCursiveFontPreferenceKey,
#endif
        @"Papyrus",                     WebKitFantasyFontPreferenceKey,
#if PLATFORM(IOS_FAMILY)
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
#if !PLATFORM(IOS_FAMILY)
        [NSNumber numberWithBool:NO],   WebKitShouldPrintBackgroundsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitTextAreasAreResizablePreferenceKey,
#endif
        [NSNumber numberWithBool:NO],   WebKitShrinksStandaloneImagesToFitPreferenceKey,
#if !PLATFORM(IOS_FAMILY)
        [NSNumber numberWithBool:YES],  WebKitJavaEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitJavaScriptEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitJavaScriptMarkupEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitWebSecurityEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowUniversalAccessFromFileURLsPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowFileAccessFromFileURLsPreferenceKey,
#if PLATFORM(IOS_FAMILY)
        [NSNumber numberWithBool:NO],   WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
#else
        [NSNumber numberWithBool:YES],  WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitPluginsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDatabasesEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitHTTPEquivEnabledPreferenceKey,

#if PLATFORM(IOS_FAMILY)
        [NSNumber numberWithBool:NO],   WebKitStorageTrackerEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitLocalStorageEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitExperimentalNotificationsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImagesPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAllowAnimatedImageLoopingPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDisplayImagesKey,
        [NSNumber numberWithBool:NO],   WebKitLoadSiteIconsKey,
        @"1800",                        WebKitBackForwardCacheExpirationIntervalKey,
#if !PLATFORM(IOS_FAMILY)
        [NSNumber numberWithBool:NO],   WebKitTabToLinksPreferenceKey,
#endif
        [NSNumber numberWithBool:NO],   WebKitPrivateBrowsingEnabledPreferenceKey,
#if !PLATFORM(IOS_FAMILY)
        [NSNumber numberWithBool:NO],   WebKitRespectStandardStyleKeyEquivalentsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowsURLsInToolTipsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowsToolTipOverTruncatedTextPreferenceKey,
        @"1",                           WebKitPDFDisplayModePreferenceKey,
        @"0",                           WebKitPDFScaleFactorPreferenceKey,
#endif
        @"0",                           WebKitUseSiteSpecificSpoofingPreferenceKey,
        [NSNumber numberWithInt:WebKitEditableLinkDefaultBehavior], WebKitEditableLinkBehaviorPreferenceKey,
#if !PLATFORM(IOS_FAMILY)
        [NSNumber numberWithInt:WebTextDirectionSubmenuAutomaticallyIncluded],
                                        WebKitTextDirectionSubmenuInclusionBehaviorPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitDOMPasteAllowedPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitUsesPageCachePreferenceKey,
        [NSNumber numberWithInt:cacheModelForMainBundle()], WebKitCacheModelPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitPageCacheSupportsPluginsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitDeveloperExtrasEnabledPreferenceKey,
        [NSNumber numberWithUnsignedInt:0], WebKitJavaScriptRuntimeFlagsPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAuthorAndUserStylesEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitDOMTimersThrottlingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitWebArchiveDebugModeEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitLocalFileContentSniffingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitOfflineWebApplicationCacheEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitZoomsTextOnlyPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitJavaScriptCanAccessClipboardPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitXSSAuditorEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAcceleratedCompositingEnabledPreferenceKey,

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
#define DEFAULT_SUBPIXEL_ANTIALIASED_LAYER_TEXT_ENABLED YES
#else
#define DEFAULT_SUBPIXEL_ANTIALIASED_LAYER_TEXT_ENABLED NO
#endif
        [NSNumber numberWithBool:DEFAULT_SUBPIXEL_ANTIALIASED_LAYER_TEXT_ENABLED], WebKitSubpixelAntialiasedLayerTextEnabledPreferenceKey,

        [NSNumber numberWithBool:NO],   WebKitDisplayListDrawingEnabledPreferenceKey,
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
        [NSNumber numberWithBool:YES],  WebKitAcceleratedDrawingEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitCanvasUsesAcceleratedDrawingPreferenceKey,
#else
        [NSNumber numberWithBool:NO],  WebKitAcceleratedDrawingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitCanvasUsesAcceleratedDrawingPreferenceKey,
#endif
        [NSNumber numberWithBool:NO],   WebKitShowDebugBordersPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitSimpleLineLayoutEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitSimpleLineLayoutDebugBordersEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShowRepaintCounterPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitWebGLEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitForceSoftwareWebGLRenderingPreferenceKey,
        [NSNumber numberWithBool:YES],   WebKitForceWebGLUsesLowPowerPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitAccelerated2dCanvasEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitSubpixelCSSOMElementMetricsEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitResourceLoadStatisticsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitLargeImageAsyncDecodingEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitAnimatedImageAsyncDecodingEnabledPreferenceKey,
#if PLATFORM(IOS_FAMILY)
        [NSNumber numberWithUnsignedInt:static_cast<uint32_t>(FrameFlattening::FullyEnabled)], WebKitFrameFlatteningPreferenceKey,
#else
        [NSNumber numberWithUnsignedInt:static_cast<uint32_t>(FrameFlattening::Disabled)], WebKitFrameFlatteningPreferenceKey,
#endif
        [NSNumber numberWithBool:NO], WebKitAsyncFrameScrollingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitSpatialNavigationEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],  WebKitDNSPrefetchingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitFullScreenEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitAsynchronousSpellCheckingEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitHyperlinkAuditingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitUsePreHTML5ParserQuirksKey,
        [NSNumber numberWithBool:YES],  WebKitAVFoundationEnabledKey,
        [NSNumber numberWithBool:YES],  WebKitAVFoundationNSURLSessionEnabledKey,
        [NSNumber numberWithBool:NO],   WebKitSuppressesIncrementalRenderingKey,
        [NSNumber numberWithBool:attachmentElementEnabled], WebKitAttachmentElementEnabledPreferenceKey,
#if !PLATFORM(IOS_FAMILY)
        [NSNumber numberWithBool:YES],  WebKitAllowsInlineMediaPlaybackPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitAllowsInlineMediaPlaybackAfterFullscreenPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitInlineMediaPlaybackRequiresPlaysInlineAttributeKey,
        [NSNumber numberWithBool:YES],  WebKitMediaControlsScaleWithPageZoomPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitWebAudioEnabledPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitBackspaceKeyNavigationEnabledKey,
        [NSNumber numberWithBool:NO],   WebKitShouldDisplaySubtitlesPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShouldDisplayCaptionsPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitShouldDisplayTextDescriptionsPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitNotificationsEnabledKey,
        [NSNumber numberWithBool:NO],   WebKitShouldRespectImageOrientationKey,
        [NSNumber numberWithBool:YES],  WebKitMediaDataLoadsAutomaticallyPreferenceKey,
#else
        [NSNumber numberWithBool:allowsInlineMediaPlayback],   WebKitAllowsInlineMediaPlaybackPreferenceKey,
        [NSNumber numberWithBool:allowsInlineMediaPlaybackAfterFullscreen],   WebKitAllowsInlineMediaPlaybackAfterFullscreenPreferenceKey,
        [NSNumber numberWithBool:requiresPlaysInlineAttribute], WebKitInlineMediaPlaybackRequiresPlaysInlineAttributeKey,
        [NSNumber numberWithBool:NO],   WebKitMediaControlsScaleWithPageZoomPreferenceKey,
        [NSNumber numberWithUnsignedInt:AudioSession::None],  WebKitAudioSessionCategoryOverride,
        [NSNumber numberWithBool:NO],   WebKitMediaDataLoadsAutomaticallyPreferenceKey,
#if HAVE(AVKIT)
        [NSNumber numberWithBool:YES],  WebKitAVKitEnabled,
#endif
        [NSNumber numberWithBool:YES],  WebKitRequiresUserGestureForMediaPlaybackPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitRequiresUserGestureForVideoPlaybackPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitRequiresUserGestureForAudioPlaybackPreferenceKey,
        [NSNumber numberWithLongLong:WebCore::ApplicationCacheStorage::noQuota()], WebKitApplicationCacheTotalQuota,

        // Per-Origin Quota on iOS is 25MB. When the quota is reached for a particular origin
        // the quota for that origin can be increased. See also webView:exceededApplicationCacheOriginQuotaForSecurityOrigin:totalSpaceNeeded in WebUI/WebUIDelegate.m.
        [NSNumber numberWithLongLong:(25 * 1024 * 1024)], WebKitApplicationCacheDefaultOriginQuota,

        // Enable WebAudio by default in all iOS UIWebViews
        [NSNumber numberWithBool:YES],   WebKitWebAudioEnabledPreferenceKey,

        [NSNumber numberWithBool:YES],   WebKitShouldRespectImageOrientationKey,
#endif // PLATFORM(IOS_FAMILY)
#if ENABLE(WIRELESS_TARGET_PLAYBACK)
        [NSNumber numberWithBool:YES],  WebKitAllowsAirPlayForMediaPlaybackPreferenceKey,
#endif
        [NSNumber numberWithBool:YES],  WebKitAllowsPictureInPictureMediaPlaybackPreferenceKey,
        [NSNumber numberWithBool:YES],  WebKitRequestAnimationFrameEnabledPreferenceKey,
        [NSNumber numberWithBool:NO],   WebKitWantsBalancedSetDefersLoadingBehaviorKey,
        [NSNumber numberWithBool:NO],   WebKitDiagnosticLoggingEnabledKey,
        [NSNumber numberWithInt:WebAllowAllStorage], WebKitStorageBlockingPolicyKey,
        [NSNumber numberWithBool:NO],   WebKitPlugInSnapshottingEnabledPreferenceKey,

#if PLATFORM(IOS_FAMILY)
        [NSNumber numberWithBool:NO],   WebKitTelephoneParsingEnabledPreferenceKey,
        [NSNumber numberWithInt:-1],      WebKitLayoutIntervalPreferenceKey,
        [NSNumber numberWithFloat:-1.0f], WebKitMaxParseDurationPreferenceKey,
        [NSNumber numberWithBool:NO],     WebKitAllowMultiElementImplicitFormSubmissionPreferenceKey,
        [NSNumber numberWithBool:NO],     WebKitAlwaysRequestGeolocationPermissionPreferenceKey,
        [NSNumber numberWithInt:InterpolationLow], WebKitInterpolationQualityPreferenceKey,
        [NSNumber numberWithBool:YES],    WebKitPasswordEchoEnabledPreferenceKey,
        [NSNumber numberWithFloat:2.0f],  WebKitPasswordEchoDurationPreferenceKey,
        [NSNumber numberWithBool:NO],     WebKitNetworkDataUsageTrackingEnabledPreferenceKey,
        @"",                              WebKitNetworkInterfaceNamePreferenceKey,
#endif
#if ENABLE(TEXT_AUTOSIZING)
        [NSNumber numberWithFloat:Settings::defaultMinimumZoomFontSize()], WebKitMinimumZoomFontSizePreferenceKey,
        [NSNumber numberWithBool:Settings::defaultTextAutosizingEnabled()], WebKitTextAutosizingEnabledPreferenceKey,
#endif
        [NSNumber numberWithLongLong:ApplicationCacheStorage::noQuota()], WebKitApplicationCacheTotalQuota,
        [NSNumber numberWithLongLong:ApplicationCacheStorage::noQuota()], WebKitApplicationCacheDefaultOriginQuota,
        [NSNumber numberWithBool:NO], WebKitHiddenPageDOMTimerThrottlingEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitHiddenPageCSSAnimationSuspensionEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitLowPowerVideoAudioBufferSizeEnabledPreferenceKey,
        
        [NSNumber numberWithBool:NO], WebKitUseLegacyTextAlignPositionedElementBehaviorPreferenceKey,
#if ENABLE(MEDIA_SOURCE)
        [NSNumber numberWithBool:YES], WebKitMediaSourceEnabledPreferenceKey,
        @YES, WebKitSourceBufferChangeTypeEnabledPreferenceKey,
#endif
#if ENABLE(SERVICE_CONTROLS)
        [NSNumber numberWithBool:NO], WebKitImageControlsEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitServiceControlsEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:NO], WebKitEnableInheritURIQueryComponentPreferenceKey,
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        @"~/Library/WebKit/MediaKeys", WebKitMediaKeysStorageDirectoryKey,
#endif
#if ENABLE(MEDIA_STREAM)
        [NSNumber numberWithBool:NO], WebKitMockCaptureDevicesEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitMockCaptureDevicesPromptEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitMediaCaptureRequiresSecureConnectionPreferenceKey,
#endif
        [NSNumber numberWithBool:YES], WebKitShadowDOMEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitCustomElementsEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitDataTransferItemsEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitCustomPasteboardDataEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitModernMediaControlsEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitWebAnimationsCSSIntegrationEnabledPreferenceKey,

#if ENABLE(WEBGL2)
        [NSNumber numberWithBool:NO], WebKitWebGL2EnabledPreferenceKey,
#endif
#if ENABLE(WEBGPU)
        [NSNumber numberWithBool:NO], WebKitWebGPUEnabledPreferenceKey,
#endif
#if ENABLE(WEBMETAL)
        [NSNumber numberWithBool:NO], WebKitWebMetalEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:NO], WebKitCacheAPIEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitFetchAPIEnabledPreferenceKey,

#if ENABLE(STREAMS_API)
        [NSNumber numberWithBool:NO], WebKitReadableByteStreamAPIEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitWritableStreamAPIEnabledPreferenceKey,
#endif
#if ENABLE(DOWNLOAD_ATTRIBUTE)
        [NSNumber numberWithBool:NO], WebKitDownloadAttributeEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:NO], WebKitDirectoryUploadEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitWebAnimationsEnabledPreferenceKey,

#if PLATFORM(IOS_FAMILY)
        @NO, WebKitVisualViewportAPIEnabledPreferenceKey,
#else
        @YES, WebKitVisualViewportAPIEnabledPreferenceKey,
#endif

        [NSNumber numberWithBool:NO], WebKitCSSOMViewScrollingAPIEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitNeedsStorageAccessFromFileURLsQuirkKey,
        [NSNumber numberWithBool:NO], WebKitAllowCrossOriginSubresourcesToAskForCredentialsKey,
#if ENABLE(MEDIA_STREAM)
        [NSNumber numberWithBool:NO], WebKitMediaDevicesEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitMediaStreamEnabledPreferenceKey,
        [NSNumber numberWithBool:YES], WebKitMediaRecorderEnabledPreferenceKey,
#endif
#if ENABLE(WEB_RTC)
        [NSNumber numberWithBool:YES], WebKitPeerConnectionEnabledPreferenceKey,
#endif
        [NSNumber numberWithBool:YES], WebKitSelectionAcrossShadowBoundariesEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitCSSLogicalEnabledPreferenceKey,
        [NSNumber numberWithBool:NO], WebKitAdClickAttributionEnabledPreferenceKey,
#if ENABLE(INTERSECTION_OBSERVER)
        @NO, WebKitIntersectionObserverEnabledPreferenceKey,
#endif
        @YES, WebKitDisplayContentsEnabledPreferenceKey,
        @NO, WebKitUserTimingEnabledPreferenceKey,
        @NO, WebKitResourceTimingEnabledPreferenceKey,
        @NO, WebKitMediaUserGestureInheritsFromDocument,
        @NO, WebKitIsSecureContextAttributeEnabledPreferenceKey,
        @YES, WebKitLegacyEncryptedMediaAPIEnabledKey,
        @NO, WebKitEncryptedMediaAPIEnabledKey,
        @YES, WebKitViewportFitEnabledPreferenceKey,
        @YES, WebKitConstantPropertiesEnabledPreferenceKey,
        @NO, WebKitColorFilterEnabledPreferenceKey,
        @NO, WebKitPunchOutWhiteBackgroundsInDarkModePreferenceKey,
        @YES, WebKitAllowMediaContentTypesRequiringHardwareSupportAsFallbackKey,
        @NO, WebKitInspectorAdditionsEnabledPreferenceKey,
        (NSString *)Settings::defaultMediaContentTypesRequiringHardwareSupport(), WebKitMediaContentTypesRequiringHardwareSupportPreferenceKey,
        @NO, WebKitAccessibilityObjectModelEnabledPreferenceKey,
        @YES, WebKitAriaReflectionEnabledPreferenceKey,
        @NO, WebKitMediaCapabilitiesEnabledPreferenceKey,
        @NO, WebKitFetchAPIKeepAliveEnabledPreferenceKey,
        @NO, WebKitServerTimingEnabledPreferenceKey,
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
#if PLATFORM(IOS_FAMILY)
    dispatch_barrier_sync(_private->readWriteQueue, ^{
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
    dispatch_barrier_sync(_private->readWriteQueue, ^{
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
    dispatch_barrier_sync(_private->readWriteQueue, ^{
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
    dispatch_barrier_sync(_private->readWriteQueue, ^{
#endif
    [_private->values.get() setObject:@(value) forKey:_key];
#if PLATFORM(IOS_FAMILY)
    });
#endif
    if (_private->autosaves)
        [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithUnsignedInt:value] forKey:_key];
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
    dispatch_barrier_sync(_private->readWriteQueue, ^{
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
    dispatch_barrier_sync(_private->readWriteQueue, ^{
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
    dispatch_barrier_sync(_private->readWriteQueue, ^{
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
    dispatch_barrier_sync(_private->readWriteQueue, ^{
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

#if !PLATFORM(IOS_FAMILY)
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
#if ENABLE(WIRELESS_TARGET_PLAYBACK)
    return [self _boolValueForKey:WebKitAllowsAirPlayForMediaPlaybackPreferenceKey];
#else
    return false;
#endif
}

- (void)setAllowsAirPlayForMediaPlayback:(BOOL)flag
{
#if ENABLE(WIRELESS_TARGET_PLAYBACK)
    [self _setBoolValue:flag forKey:WebKitAllowsAirPlayForMediaPlaybackPreferenceKey];
#endif
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

- (BOOL)isXSSAuditorEnabled
{
    return [self _boolValueForKey:WebKitXSSAuditorEnabledPreferenceKey];
}

- (void)setXSSAuditorEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitXSSAuditorEnabledPreferenceKey];
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
#if !PLATFORM(IOS_FAMILY)
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
    return webDefaultCFStringEncoding();
}

+ (void)_setInitialDefaultTextEncodingToSystemEncoding
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:
        [NSDictionary dictionaryWithObject:defaultTextEncodingNameForSystemLanguage() forKey:WebKitDefaultTextEncodingNamePreferenceKey]];
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
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif
    NetworkStorageSessionMap::switchToNewTestingSession();
}

+ (void)_clearNetworkLoaderSession
{
    NetworkStorageSessionMap::defaultStorageSession().deleteAllCookies();
}

+ (void)_setCurrentNetworkLoaderSessionCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy
{
    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = NetworkStorageSessionMap::defaultStorageSession().cookieStorage();
    ASSERT(cookieStorage); // Will fail when NetworkStorageSessionMap::switchToNewTestingSession() was not called beforehand.
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), policy);
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

- (BOOL)subpixelAntialiasedLayerTextEnabled
{
    return [self _boolValueForKey:WebKitSubpixelAntialiasedLayerTextEnabledPreferenceKey];
}

- (void)setSubpixelAntialiasedLayerTextEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitSubpixelAntialiasedLayerTextEnabledPreferenceKey];
}

- (BOOL)simpleLineLayoutEnabled
{
    return [self _boolValueForKey:WebKitSimpleLineLayoutEnabledPreferenceKey];
}

- (void)setSimpleLineLayoutEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitSimpleLineLayoutEnabledPreferenceKey];
}

- (BOOL)simpleLineLayoutDebugBordersEnabled
{
    return [self _boolValueForKey:WebKitSimpleLineLayoutDebugBordersEnabledPreferenceKey];
}

- (void)setSimpleLineLayoutDebugBordersEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitSimpleLineLayoutDebugBordersEnabledPreferenceKey];
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

- (BOOL)subpixelCSSOMElementMetricsEnabled
{
    return [self _boolValueForKey:WebKitSubpixelCSSOMElementMetricsEnabledPreferenceKey];
}

- (void)setSubpixelCSSOMElementMetricsEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitSubpixelCSSOMElementMetricsEnabledPreferenceKey];
}

- (BOOL)webGLEnabled
{
    return [self _boolValueForKey:WebKitWebGLEnabledPreferenceKey];
}

- (void)setWebGLEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitWebGLEnabledPreferenceKey];
}

- (BOOL)webGL2Enabled
{
    return [self _boolValueForKey:WebKitWebGL2EnabledPreferenceKey];
}

- (void)setWebGL2Enabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitWebGL2EnabledPreferenceKey];
}

- (BOOL)forceSoftwareWebGLRendering
{
    return [self _boolValueForKey:WebKitForceSoftwareWebGLRenderingPreferenceKey];
}

- (void)setForceSoftwareWebGLRendering:(BOOL)forced
{
    [self _setBoolValue:forced forKey:WebKitForceSoftwareWebGLRenderingPreferenceKey];
}

- (BOOL)forceLowPowerGPUForWebGL
{
    return [self _boolValueForKey:WebKitForceWebGLUsesLowPowerPreferenceKey];
}

- (void)setForceWebGLUsesLowPower:(BOOL)forceLowPower
{
    [self _setBoolValue:forceLowPower forKey:WebKitForceWebGLUsesLowPowerPreferenceKey];
}

- (BOOL)webGPUEnabled
{
    return [self _boolValueForKey:WebKitWebGPUEnabledPreferenceKey];
}

- (void)setWebGPUEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitWebGPUEnabledPreferenceKey];
}

- (BOOL)webMetalEnabled
{
    return [self _boolValueForKey:WebKitWebMetalEnabledPreferenceKey];
}

- (void)setWebMetalEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitWebMetalEnabledPreferenceKey];
}

- (BOOL)accelerated2dCanvasEnabled
{
    return [self _boolValueForKey:WebKitAccelerated2dCanvasEnabledPreferenceKey];
}

- (void)setAccelerated2dCanvasEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitAccelerated2dCanvasEnabledPreferenceKey];
}

- (void)setDiskImageCacheEnabled:(BOOL)enabled
{
    // Staging. Can be removed once there are no more callers.
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

- (void)setAVFoundationNSURLSessionEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAVFoundationNSURLSessionEnabledKey];
}

- (BOOL)isAVFoundationNSURLSessionEnabled
{
    return [self _boolValueForKey:WebKitAVFoundationNSURLSessionEnabledKey];
}

- (void)setVideoPluginProxyEnabled:(BOOL)flag
{
    // No-op, left for SPI compatibility.
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
    return false;
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
#if HAVE(AUDIO_TOOLBOX_AUDIO_SESSION)
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
#endif

    [self _setUnsignedIntValue:override forKey:WebKitAudioSessionCategoryOverride];
}

- (BOOL)avKitEnabled
{
    return [self _boolValueForKey:WebKitAVKitEnabled];
}

- (void)setAVKitEnabled:(bool)flag
{
#if HAVE(AVKIT)
    [self _setBoolValue:flag forKey:WebKitAVKitEnabled];
#endif
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
    dispatch_barrier_sync(_private->readWriteQueue, ^{
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

- (void)setStorageBlockingPolicy:(WebStorageBlockingPolicy)storageBlockingPolicy
{
#if PLATFORM(IOS_FAMILY)
    // We don't want to write the setting out, so we just reset the default instead of storing the new setting.
    // FIXME: This code removes any defaults previously registered by client process, which is not appropriate for this method to do.
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
    return [self _boolValueForKey:WebKitImageControlsEnabledPreferenceKey];
}

- (void)setImageControlsEnabled:(BOOL)enabled
{
    [self _setBoolValue:enabled forKey:WebKitImageControlsEnabledPreferenceKey];
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

- (BOOL)shadowDOMEnabled
{
    return [self _boolValueForKey:WebKitShadowDOMEnabledPreferenceKey];
}

- (void)setShadowDOMEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitShadowDOMEnabledPreferenceKey];
}

- (BOOL)customElementsEnabled
{
    return [self _boolValueForKey:WebKitCustomElementsEnabledPreferenceKey];
}

- (void)setCustomElementsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCustomElementsEnabledPreferenceKey];
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

- (BOOL)fetchAPIEnabled
{
    return [self _boolValueForKey:WebKitFetchAPIEnabledPreferenceKey];
}

- (void)setFetchAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitFetchAPIEnabledPreferenceKey];
}

- (BOOL)readableByteStreamAPIEnabled
{
    return [self _boolValueForKey:WebKitReadableByteStreamAPIEnabledPreferenceKey];
}

- (void)setReadableByteStreamAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitReadableByteStreamAPIEnabledPreferenceKey];
}

- (BOOL)writableStreamAPIEnabled
{
    return [self _boolValueForKey:WebKitWritableStreamAPIEnabledPreferenceKey];
}

- (void)setWritableStreamAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitWritableStreamAPIEnabledPreferenceKey];
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

- (BOOL)visualViewportAPIEnabled
{
    return [self _boolValueForKey:WebKitVisualViewportAPIEnabledPreferenceKey];
}

- (void)setVisualViewportAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitVisualViewportAPIEnabledPreferenceKey];
}

- (BOOL)CSSOMViewScrollingAPIEnabled
{
    return [self _boolValueForKey:WebKitCSSOMViewScrollingAPIEnabledPreferenceKey];
}

- (void)setCSSOMViewScrollingAPIEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCSSOMViewScrollingAPIEnabledPreferenceKey];
}

- (BOOL)webAnimationsEnabled
{
    return [self _boolValueForKey:WebKitWebAnimationsEnabledPreferenceKey];
}

- (void)setWebAnimationsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitWebAnimationsEnabledPreferenceKey];
}

- (BOOL)fetchAPIKeepAliveEnabled
{
    return [self _boolValueForKey:WebKitFetchAPIEnabledPreferenceKey];
}

- (void)setFetchAPIKeepAliveEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitFetchAPIEnabledPreferenceKey];
}

- (BOOL)modernMediaControlsEnabled
{
    return [self _boolValueForKey:WebKitModernMediaControlsEnabledPreferenceKey];
}

- (void)setModernMediaControlsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitModernMediaControlsEnabledPreferenceKey];
}

- (BOOL)webAnimationsCSSIntegrationEnabled
{
    return [self _boolValueForKey:WebKitWebAnimationsCSSIntegrationEnabledPreferenceKey];
}

- (void)setWebAnimationsCSSIntegrationEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitWebAnimationsCSSIntegrationEnabledPreferenceKey];
}

- (BOOL)intersectionObserverEnabled
{
    return [self _boolValueForKey:WebKitIntersectionObserverEnabledPreferenceKey];
}

- (void)setIntersectionObserverEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitIntersectionObserverEnabledPreferenceKey];
}

- (BOOL)menuItemElementEnabled
{
    return [self _boolValueForKey:WebKitMenuItemElementEnabledPreferenceKey];
}

- (void)setMenuItemElementEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMenuItemElementEnabledPreferenceKey];
}

- (BOOL)displayContentsEnabled
{
    return [self _boolValueForKey:WebKitDisplayContentsEnabledPreferenceKey];
}

- (void)setDisplayContentsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitDisplayContentsEnabledPreferenceKey];
}

- (BOOL)userTimingEnabled
{
    return [self _boolValueForKey:WebKitUserTimingEnabledPreferenceKey];
}

- (void)setUserTimingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitUserTimingEnabledPreferenceKey];
}

- (BOOL)resourceTimingEnabled
{
    return [self _boolValueForKey:WebKitResourceTimingEnabledPreferenceKey];
}

- (void)setResourceTimingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitResourceTimingEnabledPreferenceKey];
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

- (BOOL)isSecureContextAttributeEnabled
{
    return [self _boolValueForKey:WebKitIsSecureContextAttributeEnabledPreferenceKey];
}

- (void)setIsSecureContextAttributeEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitIsSecureContextAttributeEnabledPreferenceKey];
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

- (BOOL)viewportFitEnabled
{
    return [self _boolValueForKey:WebKitViewportFitEnabledPreferenceKey];
}

- (void)setViewportFitEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitViewportFitEnabledPreferenceKey];
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

- (BOOL)inspectorAdditionsEnabled
{
    return [self _boolValueForKey:WebKitInspectorAdditionsEnabledPreferenceKey];
}

- (void)setInspectorAdditionsEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitInspectorAdditionsEnabledPreferenceKey];
}

- (BOOL)accessibilityObjectModelEnabled
{
    return [self _boolValueForKey:WebKitAccessibilityObjectModelEnabledPreferenceKey];
}

- (void)setAccessibilityObjectModelEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAccessibilityObjectModelEnabledPreferenceKey];
}

- (BOOL)ariaReflectionEnabled
{
    return [self _boolValueForKey:WebKitAriaReflectionEnabledPreferenceKey];
}

- (void)setAriaReflectionEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAriaReflectionEnabledPreferenceKey];
}

- (BOOL)mediaCapabilitiesEnabled
{
    return [self _boolValueForKey:WebKitMediaCapabilitiesEnabledPreferenceKey];
}

- (void)setMediaCapabilitiesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaCapabilitiesEnabledPreferenceKey];
}

- (BOOL)mediaRecorderEnabled
{
    return [self _boolValueForKey:WebKitMediaRecorderEnabledPreferenceKey];
}

- (void)setMediaRecorderEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitMediaRecorderEnabledPreferenceKey];
}

- (BOOL)serverTimingEnabled
{
    return [self _boolValueForKey:WebKitServerTimingEnabledPreferenceKey];
}

- (void)setServerTimingEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitServerTimingEnabledPreferenceKey];
}

- (BOOL)selectionAcrossShadowBoundariesEnabled
{
    return [self _boolValueForKey:WebKitSelectionAcrossShadowBoundariesEnabledPreferenceKey];
}

- (void)setSelectionAcrossShadowBoundariesEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitSelectionAcrossShadowBoundariesEnabledPreferenceKey];
}

- (BOOL)cssLogicalEnabled
{
    return [self _boolValueForKey:WebKitCSSLogicalEnabledPreferenceKey];
}

- (void)setCSSLogicalEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitCSSLogicalEnabledPreferenceKey];
}

- (BOOL)adClickAttributionEnabled
{
    return [self _boolValueForKey:WebKitAdClickAttributionEnabledPreferenceKey];
}

- (void)setAdClickAttributionEnabled:(BOOL)flag
{
    [self _setBoolValue:flag forKey:WebKitAdClickAttributionEnabledPreferenceKey];
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
