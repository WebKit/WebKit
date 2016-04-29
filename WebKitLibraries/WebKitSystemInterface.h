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

CFStringEncoding WKGetWebDefaultCFStringEncoding(void);

void WKSetMetadataURL(NSString *URLString, NSString *referrer, NSString *path);
    
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

enum {
    WKCTFontTransformApplyShaping = (1 << 0),
    WKCTFontTransformApplyPositioning = (1 << 1)
};
typedef int WKCTFontTransformOptions;

typedef enum {
    WKPatternTilingNoDistortion,
    WKPatternTilingConstantSpacingMinimalDistortion,
    WKPatternTilingConstantSpacing
} WKPatternTiling;

CGPatternRef WKCGPatternCreateWithImageAndTransform(CGImageRef image, CGAffineTransform transform, int tiling);

BOOL WKCGContextIsBitmapContext(CGContextRef context);
bool WKCGContextIsPDFContext(CGContextRef context);

CFStringRef WKCopyFoundationCacheDirectory(void);

typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
CFURLStorageSessionRef WKCreatePrivateStorageSession(CFStringRef);
NSURLRequest *WKCopyRequestWithStorageSession(CFURLStorageSessionRef, NSURLRequest *);
NSCachedURLResponse *WKCachedResponseForRequest(CFURLStorageSessionRef, NSURLRequest *);

typedef struct OpaqueCFHTTPCookieStorage* CFHTTPCookieStorageRef;
unsigned WKGetHTTPCookieAcceptPolicy(CFHTTPCookieStorageRef);
NSArray *WKHTTPCookies(CFHTTPCookieStorageRef);
NSArray *WKHTTPCookiesForURL(CFHTTPCookieStorageRef, NSURL *, NSURL *);
void WKSetHTTPCookiesForURL(CFHTTPCookieStorageRef, NSArray *, NSURL *, NSURL *);
void WKDeleteAllHTTPCookies(CFHTTPCookieStorageRef);
void WKDeleteHTTPCookie(CFHTTPCookieStorageRef, NSHTTPCookie *);

CFURLRef WKCopyBundleURLForExecutableURL(CFURLRef);


CALayer *WKMakeRenderLayer(uint32_t contextID);

typedef struct __WKCAContextRef *WKCAContextRef;
WKCAContextRef WKCAContextMakeRemoteWithServerPort(mach_port_t port);
void WKDestroyRenderingResources(void);

void WKCALayerEnumerateRectsBeingDrawnWithBlock(CALayer *layer, CGContextRef context, void (^block)(CGRect rect));

void WKSetCONNECTProxyForStream(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
void WKSetCONNECTProxyAuthorizationForStream(CFReadStreamRef, CFStringRef proxyAuthorizationString);
CFHTTPMessageRef WKCopyCONNECTProxyResponse(CFReadStreamRef, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort);

CFDictionaryRef WKNSURLRequestCreateSerializableRepresentation(NSURLRequest *request, CFTypeRef tokenNull);
NSURLRequest *WKNSURLRequestFromSerializableRepresentation(CFDictionaryRef representation, CFTypeRef tokenNull);
CFDictionaryRef WKCFURLRequestCreateSerializableRepresentation(CFURLRequestRef cfRequest, CFTypeRef tokenNull);
CFURLRequestRef WKCreateCFURLRequestFromSerializableRepresentation(CFDictionaryRef representation, CFTypeRef tokenNull);

CFArrayRef WKCFURLCacheCopyAllHostNamesInPersistentStore(void);
void WKCFURLCacheDeleteHostNamesInPersistentStore(CFArrayRef hostArray);    

typedef enum {
    WKSandboxExtensionTypeReadOnly,
    WKSandboxExtensionTypeReadWrite,
    WKSandboxExtensionTypeGeneric,
} WKSandboxExtensionType;
typedef struct __WKSandboxExtension *WKSandboxExtensionRef;

WKSandboxExtensionRef WKSandboxExtensionCreate(const char* path, WKSandboxExtensionType type);
void WKSandboxExtensionDestroy(WKSandboxExtensionRef sandboxExtension);

bool WKSandboxExtensionConsume(WKSandboxExtensionRef sandboxExtension);
bool WKSandboxExtensionInvalidate(WKSandboxExtensionRef sandboxExtension);

const char* WKSandboxExtensionGetSerializedFormat(WKSandboxExtensionRef sandboxExtension, size_t* length);
WKSandboxExtensionRef WKSandboxExtensionCreateFromSerializedFormat(const char* serializationFormat, size_t length);

void WKSetCrashReportApplicationSpecificInformation(CFStringRef);

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
void WKAXInitializeElementWithPresenterPid(id, pid_t);
NSData *WKAXRemoteTokenForElement(id);
id WKAXRemoteElementForToken(NSData *);
void WKAXSetWindowForRemoteElement(id remoteWindow, id remoteElement);
void WKAXRegisterRemoteProcess(bool registerProcess, pid_t);
pid_t WKAXRemoteProcessIdentifier(id remoteElement);

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
NSString *WKQTMovieMaxTimeLoadedChangeNotification(void);
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
bool WKEnableWindowOcclusionNotifications(NSInteger windowID, bool *outCurrentOcclusionState);

#if defined(__x86_64__)
#import <mach/mig.h>
CFRunLoopSourceRef WKCreateMIGServerSource(mig_subsystem_t subsystem, mach_port_t serverPort);
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

NSEvent *WKCreateNSEventWithCarbonEvent(EventRef eventRef);
NSEvent *WKCreateNSEventWithCarbonMouseMoveEvent(EventRef inEvent, NSWindow *window);
NSEvent *WKCreateNSEventWithCarbonClickEvent(EventRef inEvent, WindowRef windowRef);

ScriptCode WKGetScriptCodeFromCurrentKeyboardInputSource(void);

#endif /* __LP64__ */

#endif /* !TARGET_OS_IPHONE */

#ifdef __cplusplus
}
#endif
