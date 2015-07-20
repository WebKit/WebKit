/*      
    WebKitSystemInterface.h
    Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.

    Public header file.
*/

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#if !TARGET_OS_IPHONE
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#else
#import <CoreImage/CoreImage.h>
#import <CoreText/CoreText.h>
#import <WebKitSystemInterfaceIOS.h>
#endif

@class AVAsset;
@class AVPlayer;
@class CALayer;
@class QTMovie;
@class QTMovieView;

#ifdef __cplusplus
extern "C" {
#endif

#pragma mark Shared

typedef struct _CFURLResponse* CFURLResponseRef;
typedef const struct _CFURLRequest* CFURLRequestRef;
typedef struct _CFURLRequest* CFMutableURLRequestRef;

typedef struct _CFURLCredential* WKCFURLCredentialRef;
typedef struct _CFURLProtectionSpace* CFURLProtectionSpaceRef;

typedef enum {
    WKCertificateParseResultSucceeded  = 0,
    WKCertificateParseResultFailed     = 1,
    WKCertificateParseResultPKCS7      = 2,
} WKCertificateParseResult;

CFStringRef WKSignedPublicKeyAndChallengeString(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
WKCertificateParseResult WKAddCertificatesToKeychainFromData(const void *bytes, unsigned length);

NSDate *WKGetNSURLResponseLastModifiedDate(NSURLResponse *response);
NSString *WKCopyNSURLResponseStatusLine(NSURLResponse *response);

CFArrayRef WKCopyNSURLResponseCertificateChain(NSURLResponse *response);

CFStringEncoding WKGetWebDefaultCFStringEncoding(void);

void WKSetMetadataURL(NSString *URLString, NSString *referrer, NSString *path);
void WKSetNSURLConnectionDefersCallbacks(NSURLConnection *connection, BOOL defers);
void WKSetNSURLRequestShouldContentSniff(NSMutableURLRequest *, BOOL shouldContentSniff);
    
typedef enum {
    WKPlugInModuleLoadPolicyLoadNormally = 0,
    WKPlugInModuleLoadPolicyLoadUnsandboxed,
    WKPlugInModuleLoadPolicyBlockedForSecurity,
    WKPlugInModuleLoadPolicyBlockedForCompatibility,
} WKPlugInModuleLoadPolicy;

WKPlugInModuleLoadPolicy WKLoadPolicyForPluginVersion(NSString *bundleIdentifier, NSString *bundleVersionString);
BOOL WKShouldBlockPlugin(NSString *bundleIdentifier, NSString *bundleVersionString);
BOOL WKIsPluginUpdateAvailable(NSString *bundleIdentifier);

BOOL WKShouldBlockWebGL();
BOOL WKShouldSuggestBlockingWebGL();

BOOL WKCGContextGetShouldSmoothFonts(CGContextRef);

void WKSetUpFontCache(void);

void WKSetBaseCTM(CGContextRef, CGAffineTransform);
void WKSetPatternPhaseInUserSpace(CGContextRef, CGPoint);
CGAffineTransform WKGetUserToBaseCTM(CGContextRef);

void WKGetGlyphsForCharacters(CGFontRef, const UniChar[], CGGlyph[], size_t);
bool WKGetVerticalGlyphsForCharacters(CTFontRef, const UniChar[], CGGlyph[], size_t);

CTLineRef WKCreateCTLineWithUniCharProvider(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*);

CTTypesetterRef WKCreateCTTypesetterWithUniCharProviderAndOptions(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*, CFDictionaryRef options);

CGSize WKCTRunGetInitialAdvance(CTRunRef);

enum {
    WKCTFontTransformApplyShaping = (1 << 0),
    WKCTFontTransformApplyPositioning = (1 << 1)
};
typedef int WKCTFontTransformOptions;

bool WKCTFontTransformGlyphs(CTFontRef, CGGlyph glyphs[], CGSize advances[], CFIndex count, WKCTFontTransformOptions);

typedef enum {
    WKPatternTilingNoDistortion,
    WKPatternTilingConstantSpacingMinimalDistortion,
    WKPatternTilingConstantSpacing
} WKPatternTiling;

CGPatternRef WKCGPatternCreateWithImageAndTransform(CGImageRef, CGAffineTransform, int tiling);
void WKCGContextResetClip(CGContextRef);

BOOL WKCGContextIsBitmapContext(CGContextRef);
bool WKCGContextIsPDFContext(CGContextRef);

CFStringRef WKCopyFoundationCacheDirectory(void);

typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
CFURLStorageSessionRef WKCreatePrivateStorageSession(CFStringRef);
NSURLRequest *WKCopyRequestWithStorageSession(CFURLStorageSessionRef, NSURLRequest *);
NSCachedURLResponse *WKCachedResponseForRequest(CFURLStorageSessionRef, NSURLRequest *);
void WKSetRequestStorageSession(CFURLStorageSessionRef, CFMutableURLRequestRef);

typedef struct OpaqueCFHTTPCookieStorage* CFHTTPCookieStorageRef;
CFHTTPCookieStorageRef WKCopyHTTPCookieStorage(CFURLStorageSessionRef);
unsigned WKGetHTTPCookieAcceptPolicy(CFHTTPCookieStorageRef);
void WKSetHTTPCookieAcceptPolicy(CFHTTPCookieStorageRef, unsigned policy);
NSArray *WKHTTPCookies(CFHTTPCookieStorageRef);
NSArray *WKHTTPCookiesForURL(CFHTTPCookieStorageRef, NSURL *, NSURL *);
void WKSetHTTPCookiesForURL(CFHTTPCookieStorageRef, NSArray *, NSURL *, NSURL *);
void WKDeleteAllHTTPCookies(CFHTTPCookieStorageRef);
void WKDeleteHTTPCookie(CFHTTPCookieStorageRef, NSHTTPCookie *);

CFHTTPCookieStorageRef WKGetDefaultHTTPCookieStorage(void);
WKCFURLCredentialRef WKCopyCredentialFromCFPersistentStorage(CFURLProtectionSpaceRef);
void WKSetCFURLRequestShouldContentSniff(CFMutableURLRequestRef, bool flag);

CFURLRef WKCopyBundleURLForExecutableURL(CFURLRef);


CALayer *WKMakeRenderLayer(uint32_t contextID);

typedef struct __WKCAContextRef *WKCAContextRef;
WKCAContextRef WKCAContextMakeRemoteWithServerPort(mach_port_t);
void WKCAContextInvalidate(WKCAContextRef);
uint32_t WKCAContextGetContextId(WKCAContextRef);
void WKCAContextSetLayer(WKCAContextRef, CALayer *);
CALayer *WKCAContextGetLayer(WKCAContextRef);
void WKCAContextSetColorSpace(WKCAContextRef, CGColorSpaceRef);
CGColorSpaceRef WKCAContextGetColorSpace(WKCAContextRef);
void WKDestroyRenderingResources(void);

void WKCALayerEnumerateRectsBeingDrawnWithBlock(CALayer *, CGContextRef, void (^block)(CGRect rect));

unsigned WKInitializeMaximumHTTPConnectionCountPerHost(unsigned preferredConnectionCount);
int WKGetHTTPRequestPriority(CFURLRequestRef);
void WKSetHTTPRequestMaximumPriority(int maximumPriority);
void WKSetHTTPRequestPriority(CFURLRequestRef, int priority);
void WKSetHTTPRequestMinimumFastLanePriority(int priority);
void WKHTTPRequestEnablePipelining(CFURLRequestRef);

void WKSetCONNECTProxyForStream(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
void WKSetCONNECTProxyAuthorizationForStream(CFReadStreamRef, CFStringRef proxyAuthorizationString);
CFHTTPMessageRef WKCopyCONNECTProxyResponse(CFReadStreamRef, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort);

CFDictionaryRef WKNSURLRequestCreateSerializableRepresentation(NSURLRequest *, CFTypeRef tokenNull);
NSURLRequest *WKNSURLRequestFromSerializableRepresentation(CFDictionaryRef representation, CFTypeRef tokenNull);
CFDictionaryRef WKCFURLRequestCreateSerializableRepresentation(CFURLRequestRef cfRequest, CFTypeRef tokenNull);
CFURLRequestRef WKCreateCFURLRequestFromSerializableRepresentation(CFDictionaryRef representation, CFTypeRef tokenNull);

CFDictionaryRef WKNSURLResponseCreateSerializableRepresentation(NSURLResponse *, CFTypeRef tokenNull);
NSURLResponse *WKNSURLResponseFromSerializableRepresentation(CFDictionaryRef representation, CFTypeRef tokenNull);

CFArrayRef WKCFURLCacheCopyAllHostNamesInPersistentStore(void);
void WKCFURLCacheDeleteHostNamesInPersistentStore(CFArrayRef hostArray);    

CFStringRef WKGetCFURLResponseMIMEType(CFURLResponseRef);
CFURLRef WKGetCFURLResponseURL(CFURLResponseRef);
CFHTTPMessageRef WKGetCFURLResponseHTTPResponse(CFURLResponseRef);
CFStringRef WKCopyCFURLResponseSuggestedFilename(CFURLResponseRef);
void WKSetCFURLResponseMIMEType(CFURLResponseRef, CFStringRef mimeType);

typedef enum {
    WKSandboxExtensionTypeReadOnly,
    WKSandboxExtensionTypeReadWrite,
} WKSandboxExtensionType;
typedef struct __WKSandboxExtension *WKSandboxExtensionRef;

WKSandboxExtensionRef WKSandboxExtensionCreate(const char* path, WKSandboxExtensionType);
void WKSandboxExtensionDestroy(WKSandboxExtensionRef);

bool WKSandboxExtensionConsume(WKSandboxExtensionRef);
bool WKSandboxExtensionInvalidate(WKSandboxExtensionRef);

const char* WKSandboxExtensionGetSerializedFormat(WKSandboxExtensionRef, size_t* length);
WKSandboxExtensionRef WKSandboxExtensionCreateFromSerializedFormat(const char* serializationFormat, size_t length);

void WKSetCrashReportApplicationSpecificInformation(CFStringRef);

void WKCGPathAddRoundedRect(CGMutablePathRef, const CGAffineTransform* matrix, CGRect, CGFloat cornerWidth, CGFloat cornerHeight);

void WKCFURLRequestAllowAllPostCaching(CFURLRequestRef);
void WKCFNetworkSetOverrideSystemProxySettings(CFDictionaryRef);

bool WKIsPublicSuffix(NSString *domain);

CFArrayRef WKCFURLCacheCopyAllHostNamesInPersistentStoreForPartition(CFStringRef partition);
typedef void (^CFURLCacheCopyAllPartitionNamesResultsNotification)(CFArrayRef partitionNames);

void WKCFURLCacheDeleteHostNamesInPersistentStoreForPartition(CFArrayRef hostArray, CFStringRef partition);
CFStringRef WKCachePartitionKey(void);
void WKCFURLCacheCopyAllPartitionNames(CFURLCacheCopyAllPartitionNamesResultsNotification resultsBlock);

typedef enum {
    WKExternalPlaybackTypeNone,
    WKExternalPlaybackTypeAirPlay,
    WKExternalPlaybackTypeTVOut,
} WKExternalPlaybackType;

int WKExernalDeviceTypeForPlayer(AVPlayer *);
NSString *WKExernalDeviceDisplayNameForPlayer(AVPlayer *);

bool WKQueryDecoderAvailability(void);

#pragma mark Mac Only

#if !TARGET_OS_IPHONE

void WKShowKeyAndMain(void);

void WKAdvanceDefaultButtonPulseAnimation(NSButtonCell *button);

NSString *WKWindowWillOrderOnScreenNotification(void);
NSString *WKWindowWillOrderOffScreenNotification(void);
void WKSetNSWindowShouldPostEventNotifications(NSWindow *window, BOOL post);

CFTypeID WKGetAXTextMarkerTypeID(void);
CFTypeID WKGetAXTextMarkerRangeTypeID(void);
CFTypeRef WKCreateAXTextMarker(const void *bytes, size_t len);
BOOL WKGetBytesFromAXTextMarker(CFTypeRef textMarker, void *bytes, size_t length);
CFTypeRef WKCreateAXTextMarkerRange(CFTypeRef start, CFTypeRef end);
CFTypeRef WKCopyAXTextMarkerRangeStart(CFTypeRef range);
CFTypeRef WKCopyAXTextMarkerRangeEnd(CFTypeRef range);
void WKAccessibilityHandleFocusChanged(void);
AXUIElementRef WKCreateAXUIElementRef(id element);
void WKUnregisterUniqueIdForElement(id element);
    
NSArray *WKSpeechSynthesisGetVoiceIdentifiers(void);
NSString *WKSpeechSynthesisGetDefaultVoiceIdentifierForLocale(NSLocale*);

// Remote Accessibility API.
void WKAXRegisterRemoteApp(void);
void WKAXInitializeElementWithPresenterPid(id, pid_t);
NSData *WKAXRemoteTokenForElement(id);
id WKAXRemoteElementForToken(NSData *);
void WKAXSetWindowForRemoteElement(id remoteWindow, id remoteElement);
void WKAXRegisterRemoteProcess(bool registerProcess, pid_t);
pid_t WKAXRemoteProcessIdentifier(id remoteElement);

void WKDrawCapsLockIndicator(CGContextRef, CGRect);

// The CG context's current path is the focus ring's path.
// Color and radius are ignored. Older versions of WebKit expected to
// be able to change the rendering of the system focus ring.
void WKDrawFocusRing(CGContextRef context, CGColorRef color, int radius);
bool WKDrawFocusRingAtTime(CGContextRef context, NSTimeInterval time);
bool WKDrawCellFocusRingWithFrameAtTime(NSCell *cell, NSRect cellFrame, NSView *controlView, NSTimeInterval time);

void WKSetDragImage(NSImage *image, NSPoint offset);

void WKDrawBezeledTextArea(NSRect, BOOL enabled);

void WKPopupMenu(NSMenu*, NSPoint location, float width, NSView*, int selectedItem, NSFont*, NSControlSize controlSize, bool usesCustomAppearance);
void WKPopupContextMenu(NSMenu *menu, NSPoint screenLocation);
void WKSendUserChangeNotifications(void);

NSFont *WKGetFontInLanguageForRange(NSFont *font, NSString *string, NSRange range);
NSFont *WKGetFontInLanguageForCharacter(NSFont *font, UniChar ch);
void WKSetCGFontRenderingMode(CGContextRef cgContext, NSFont *font, BOOL shouldSubpixelQuantize);

bool WKCGContextDrawsWithCorrectShadowOffsets(CGContextRef);

CGContextRef WKNSWindowOverrideCGContext(NSWindow *, CGContextRef);
void WKNSWindowRestoreCGContext(NSWindow *, CGContextRef);

void WKGetWheelEventDeltas(NSEvent *, float *deltaX, float *deltaY, BOOL *continuous);

BOOL WKAppVersionCheckLessThan(NSString *, int, double);

typedef enum {
    WKMovieTypeUnknown,
    WKMovieTypeDownload,
    WKMovieTypeStoredStream,
    WKMovieTypeLiveStream
} WKMovieType;

int WKQTMovieGetType(QTMovie* movie);

BOOL WKQTMovieHasClosedCaptions(QTMovie* movie);
void WKQTMovieSetShowClosedCaptions(QTMovie* movie, BOOL showClosedCaptions);
void WKQTMovieSelectPreferredAlternates(QTMovie* movie);

unsigned WKQTIncludeOnlyModernMediaFileTypes(void);
float WKQTMovieMaxTimeLoaded(QTMovie* movie);
float WKQTMovieMaxTimeSeekable(QTMovie* movie);
NSString *WKQTMovieMaxTimeLoadedChangeNotification(void);
void WKQTMovieViewSetDrawSynchronously(QTMovieView* view, BOOL sync);
void WKQTMovieDisableComponent(uint32_t[5]);
NSURL *WKQTMovieResolvedURL(QTMovie* movie);

void WKSetVisibleApplicationName(CFStringRef);
void WKSetApplicationInformationItem(CFStringRef key, CFTypeRef value);

typedef enum {
    WKMediaUIPartFullscreenButton   = 0,
    WKMediaUIPartMuteButton,
    WKMediaUIPartPlayButton,
    WKMediaUIPartSeekBackButton,
    WKMediaUIPartSeekForwardButton,
    WKMediaUIPartTimelineSlider,
    WKMediaUIPartTimelineSliderThumb,
    WKMediaUIPartRewindButton,
    WKMediaUIPartSeekToRealtimeButton,
    WKMediaUIPartShowClosedCaptionsButton,
    WKMediaUIPartHideClosedCaptionsButton,
    WKMediaUIPartUnMuteButton,
    WKMediaUIPartPauseButton,
    WKMediaUIPartBackground,
    WKMediaUIPartCurrentTimeDisplay,
    WKMediaUIPartTimeRemainingDisplay,
    WKMediaUIPartStatusDisplay,
    WKMediaUIPartControlsPanel,
    WKMediaUIPartVolumeSliderContainer,
    WKMediaUIPartVolumeSlider,
    WKMediaUIPartVolumeSliderThumb,
    WKMediaUIPartFullScreenVolumeSlider,
    WKMediaUIPartFullScreenVolumeSliderThumb,
    WKMediaUIPartVolumeSliderMuteButton,
    WKMediaUIPartTextTrackDisplayContainer,
    WKMediaUIPartTextTrackDisplay,
    WKMediaUIPartExitFullscreenButton,
} WKMediaUIPart;

typedef enum {
    WKMediaControllerFlagDisabled = 1 << 0,
    WKMediaControllerFlagPressed = 1 << 1,
    WKMediaControllerFlagDrawEndCaps = 1 << 3,
    WKMediaControllerFlagFocused = 1 << 4
} WKMediaControllerThemeState;

BOOL WKHitTestMediaUIPart(int part, CGRect bounds, CGPoint point);
void WKMeasureMediaUIPart(int part, CGRect *bounds, CGSize *naturalSize);
void WKDrawMediaUIPart(int part, CGContextRef context, CGRect rect, unsigned state);
void WKDrawMediaSliderTrack(CGContextRef context, CGRect rect, float timeLoaded, float currentTime, float duration, unsigned state);
NSView *WKCreateMediaUIBackgroundView(void);

typedef enum {
    WKMediaUIControlTimeline,
    WKMediaUIControlSlider,
    WKMediaUIControlPlayPauseButton,
    WKMediaUIControlExitFullscreenButton,
    WKMediaUIControlRewindButton,
    WKMediaUIControlFastForwardButton,
    WKMediaUIControlVolumeUpButton,
    WKMediaUIControlVolumeDownButton
} WKMediaUIControlType;
    
NSControl *WKCreateMediaUIControl(int controlType);

NSArray *WKQTGetSitesInMediaDownloadCache();
void WKQTClearMediaDownloadCacheForSite(NSString *site);
void WKQTClearMediaDownloadCache();
    
mach_port_t WKInitializeRenderServer(void);

typedef struct __WKSoftwareCARendererRef *WKSoftwareCARendererRef;

WKSoftwareCARendererRef WKSoftwareCARendererCreate(uint32_t contextID);
void WKSoftwareCARendererDestroy(WKSoftwareCARendererRef);
void WKSoftwareCARendererRender(WKSoftwareCARendererRef, CGContextRef, CGRect);
WKCAContextRef WKCAContextMakeRemoteForWindowServer(void);

void WKWindowSetClipRect(NSWindow*, NSRect);

NSUInteger WKGetInputPanelWindowStyle(void);
UInt8 WKGetNSEventKeyChar(NSEvent *);

void WKWindowSetAlpha(NSWindow *window, float alphaValue);
void WKWindowSetScaledFrame(NSWindow *window, NSRect scaleFrame, NSRect nonScaledFrame);

void WKEnableSettingCursorWhenInBackground(void);

OSStatus WKEnableSandboxStyleFileQuarantine(void);

int WKRecommendedScrollerStyle(void);

bool WKExecutableWasLinkedOnOrBeforeSnowLeopard(void);

CFStringRef WKCopyDefaultSearchProviderDisplayName(void);

NSCursor *WKCursor(const char *name);

bool WKExecutableWasLinkedOnOrBeforeLion(void);
bool WKExecutableWasLinkedOnOrBeforeMountainLion(void);

CGFloat WKNSElasticDeltaForTimeDelta(CGFloat initialPosition, CGFloat initialVelocity, CGFloat elapsedTime);
CGFloat WKNSElasticDeltaForReboundDelta(CGFloat delta);
CGFloat WKNSReboundDeltaForElasticDelta(CGFloat delta);

typedef enum {
    WKOcclusionNotificationTypeApplicationBecameVisible,
    WKOcclusionNotificationTypeApplicationBecameOccluded,
    WKOcclusionNotificationTypeApplicationWindowModificationsStarted,
    WKOcclusionNotificationTypeApplicationWindowModificationsStopped,
    WKOcclusionNotificationTypeWindowBecameVisible,
    WKOcclusionNotificationTypeWindowBecameOccluded,
} WKOcclusionNotificationType;

typedef uint32_t WKWindowID;

typedef void (*WKOcclusionNotificationHandler)(uint32_t, void* data, uint32_t dataLength, void*, uint32_t);

bool WKRegisterOcclusionNotificationHandler(WKOcclusionNotificationType, WKOcclusionNotificationHandler);
bool WKUnregisterOcclusionNotificationHandler(WKOcclusionNotificationType, WKOcclusionNotificationHandler);
bool WKEnableWindowOcclusionNotifications(NSInteger windowID, bool *outCurrentOcclusionState);

#if defined(__x86_64__)
#import <mach/mig.h>
CFRunLoopSourceRef WKCreateMIGServerSource(mig_subsystem_t, mach_port_t serverPort);
#endif


#ifndef __LP64__

OSStatus WKSyncWindowWithCGAfterMove(WindowRef);
unsigned WKCarbonWindowMask(void);
void *WKGetNativeWindowFromWindowRef(WindowRef);
OSType WKCarbonWindowPropertyCreator(void);
OSType WKCarbonWindowPropertyTag(void);

unsigned WKGetNSAutoreleasePoolCount(void);

BOOL WKConvertNSEventToCarbonEvent(EventRecord *carbonEvent, NSEvent *cocoaEvent);
void WKSendKeyEventToTSM(NSEvent *theEvent);
void WKCallDrawingNotification(CGrafPtr port, Rect *bounds);

NSEvent *WKCreateNSEventWithCarbonEvent(EventRef);
NSEvent *WKCreateNSEventWithCarbonMouseMoveEvent(EventRef inEvent, NSWindow *);
NSEvent *WKCreateNSEventWithCarbonClickEvent(EventRef inEvent, WindowRef);

ScriptCode WKGetScriptCodeFromCurrentKeyboardInputSource(void);

#endif /* __LP64__ */

#endif /* !TARGET_OS_IPHONE */

#ifdef __cplusplus
}
#endif
