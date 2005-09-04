/*      
    WebKitSystemInterface.h
    Copyright (C) 2005 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
*/

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

typedef enum {
    WKCertificateParseResultSucceeded  = 0,
    WKCertificateParseResultFailed     = 1,
    WKCertificateParseResultPKCS7      = 2,
} WKCertificateParseResult;

NSString *WKCreateURLPasteboardFlavorTypeName(void);
NSString *WKCreateURLNPasteboardFlavorTypeName(void);

CFStringRef WKCopyCFLocalizationPreferredName(CFStringRef localization);
CFStringRef WKSignedPublicKeyAndChallengeString(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
WKCertificateParseResult WKAddCertificatesToKeychainFromData(const void *bytes, unsigned length);

NSString *WKGetPreferredExtensionForMIMEType(NSString *type);
NSArray *WKGetExtensionsForMIMEType(NSString *type);
NSString *WKGetMIMETypeForExtension(NSString *extension);

NSDate *WKGetNSURLResponseLastModifiedDate(NSURLResponse *response);
NSTimeInterval WKGetNSURLResponseFreshnessLifetime(NSURLResponse *response);
NSTimeInterval WKGetNSURLResponseCalculatedExpiration(NSURLResponse *response);
BOOL WKGetNSURLResponseMustRevalidate(NSURLResponse *response);

CFStringEncoding WKGetWebDefaultCFStringEncoding(void);

float WKSecondsSinceLastInputEvent(void);
CFStringRef WKPreferRGB32Key(void);

void WKSetNSURLConnectionDefersCallbacks(NSURLConnection *connection, BOOL defers);
float WKSecondsSinceLastInputEvent(void);

void WKShowKeyAndMain(void);
OSStatus WKSyncWindowWithCGAfterMove(WindowRef);
unsigned WKCarbonWindowMask(void);
void *WKGetNativeWindowFromWindowRef(WindowRef);
OSType WKCarbonWindowPropertyCreator(void);
OSType WKCarbonWindowPropertyTag(void);

typedef id WKNSURLConnectionDelegateProxyPtr;

WKNSURLConnectionDelegateProxyPtr WKCreateNSURLConnectionDelegateProxy(void);

void WKDisableCGDeferredUpdates(void);

Class WKNSURLProtocolClassForReqest(NSURLRequest *request);

unsigned WKGetNSAutoreleasePoolCount(void);

NSString *WKMouseMovedNotification(void);
BOOL WKMouseIsDown(void);
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

BOOL WKFontSmoothingModeIsLCD(int mode);
void WKSetUpFontCache(size_t s);

void WKSignalCFReadStreamEnd(CFReadStreamRef stream);
void WKSignalCFReadStreamHasBytes(CFReadStreamRef stream);
void WKSignalCFReadStreamError(CFReadStreamRef stream, CFStreamError *error);

CFReadStreamRef WKCreateCustomCFReadStream(void *(*formCreate)(CFReadStreamRef, void *), 
    void (*formFinalize)(CFReadStreamRef, void *), 
    Boolean (*formOpen)(CFReadStreamRef, CFStreamError *, Boolean *, void *), 
    CFIndex (*formRead)(CFReadStreamRef, UInt8 *, CFIndex, CFStreamError *, Boolean *, void *), 
    Boolean (*formCanRead)(CFReadStreamRef, void *), 
    void (*formClose)(CFReadStreamRef, void *), 
    void (*formSchedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *), 
    void (*formUnschedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *),
    void *context);

void WKSetFocusRingStyle(NSFocusRingPlacement placement, int radius, NSColor *color);
void WKSetDragImage(NSImage *image, NSPoint offset);

void WKSendUserChangeNotifications(void);
BOOL WKConvertNSEventToCarbonEvent(EventRecord *carbonEvent, NSEvent *cocoaEvent);
void WKSendKeyEventToTSM(NSEvent *theEvent);
void WKCallDrawingNotification(CGrafPtr port, Rect *bounds);

BOOL WKGetGlyphTransformedAdvances(NSFont *font, CGAffineTransform *m, ATSGlyphRef *glyph, CGSize *advance);
CGFontRef WKGetCGFontFromNSFont(NSFont *font);
void WKGetFontMetrics(NSFont *font, int *ascent, int *descent, int *lineGap, unsigned *unitsPerEm);
NSFont *WKGetFontInLanguageForRange(NSFont *font, NSString *string, NSRange range);
NSFont *WKGetFontInLanguageForCharacter(NSFont *font, UniChar ch);
void WKSetCGFontRenderingMode(CGContextRef cgContext, NSFont *font);
ATSUFontID WKGetNSFontATSUFontId(NSFont *font);
void WKReleaseStyleGroup(void *group);
BOOL WKCGContextGetShouldSmoothFonts(CGContextRef cgContext);

#define WKGlyphVectorSize (50 * 32)

typedef void *WKGlyphVectorRef;
OSStatus WKConvertCharToGlyphs(void *styleGroup, const UniChar *characters, unsigned numCharacters, WKGlyphVectorRef glyphs);
OSStatus WKGetATSStyleGroup(ATSUStyle fontStyle, void **styleGroup);
OSStatus WKInitializeGlyphVector(int count, WKGlyphVectorRef glyphs);
void WKClearGlyphVector(WKGlyphVectorRef glyphs);

int WKGetGlyphVectorNumGlyphs(WKGlyphVectorRef glyphVector);
ATSLayoutRecord *WKGetGlyphVectorFirstRecord(WKGlyphVectorRef glyphVector);
size_t WKGetGlyphVectorRecordSize(WKGlyphVectorRef glyphVector);
ATSGlyphRef WKGetDefaultGlyphForChar(NSFont *font, UniChar c);

NSEvent *WKCreateNSEventWithCarbonEvent(EventRef eventRef);
NSEvent *WKCreateNSEventWithCarbonMouseMoveEvent(EventRef inEvent, NSWindow *window);
NSEvent *WKCreateNSEventWithCarbonClickEvent(EventRef inEvent, WindowRef windowRef);

BOOL WKExecutableLinkedInTigerOrEarlier(void);

CGContextRef WKNSWindowOverrideCGContext(NSWindow *, CGContextRef);
void WKNSWindowRestoreCGContext(NSWindow *, CGContextRef);
