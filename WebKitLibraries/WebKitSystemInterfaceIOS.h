/*
 *  WebKitSystemInterfaceIOS.h
 *  Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
 */

#import <CoreGraphics/CoreGraphics.h>

#ifdef __OBJC__
@class UIScreen;
#else
class UIScreen;
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool WKExecutableWasLinkedOnOrAfterIOSVersion(int);

bool WKIsGB18030ComplianceRequired(void);

typedef enum {
    WKDeviceClassInvalid = -1,
    WKDeviceClassiPad,
    WKDeviceClassiPhone,
    WKDeviceClassiPod,
} WKDeviceClass;
int WKGetDeviceClass(void);

CFStringRef WKGetUserAgent(void);
CFStringRef WKGetOSNameForUserAgent(void);
CFStringRef WKGetDeviceName(void);
CFStringRef WKGetPlatformNameForNavigator(void);
CFStringRef WKGetVendorNameForNavigator(void);

CGSize WKGetScreenSize(void);
CGSize WKGetAvailableScreenSize(void);

float WKGetMinimumZoomFontSize(void);

float WKGetScreenScaleFactor(void);
float WKGetScaleFactorForScreen(UIScreen *);

#ifdef __OBJC__
NSData *WKAXRemoteToken(CFUUIDRef);
void WKAXStoreRemoteConnectionInformation(id, pid_t, mach_port_t, CFUUIDRef);
void WKAXRegisterRemoteApp(void);
#endif

bool WKIsOptimizedFullscreenSupported(void);
typedef enum {
    WKMediaUIPartOptimizedFullscreenButton = 0,
    WKMediaUIPartOptimizedFullscreenPlaceholder
} WKMediaUIPart;
CFStringRef WKGetMediaUIImageData(int);

#ifdef __cplusplus
}
#endif
