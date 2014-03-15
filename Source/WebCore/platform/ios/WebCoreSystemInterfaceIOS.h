/*
 * Copyright 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebCoreSystemInterfaceIOS_h
#define WebCoreSystemInterfaceIOS_h

#include <Availability.h>

typedef enum {
    wkIOSSystemVersion_3_0 = __IPHONE_3_0,
    wkIOSSystemVersion_4_2 = __IPHONE_4_2,
    wkIOSSystemVersion_5_0 = __IPHONE_5_0,
    wkIOSSystemVersion_6_0 = __IPHONE_6_0,
} wkIOSSystemVersion;

bool iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion);

extern bool (*wkExecutableWasLinkedOnOrAfterIOSVersion)(int);

extern bool (*wkIsGB18030ComplianceRequired)(void);

inline bool iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion version)
{
    return wkExecutableWasLinkedOnOrAfterIOSVersion(version);
}

typedef enum {
    wkDeviceClassInvalid = -1,
    wkDeviceClassiPad,
    wkDeviceClassiPhone,
    wkDeviceClassiPod,
} wkDeviceClass;
extern int (*wkGetDeviceClass)(void);
inline wkDeviceClass iosDeviceClass(void)
{
    int deviceClass = wkGetDeviceClass();
    switch (deviceClass) {
    case wkDeviceClassInvalid:
    case wkDeviceClassiPad:
    case wkDeviceClassiPhone:
    case wkDeviceClassiPod:
        return (wkDeviceClass)deviceClass;
    }
    assert(false);
    return wkDeviceClassInvalid;
}

extern CFStringRef (*wkGetUserAgent)(void);
extern CFStringRef (*wkGetDeviceName)(void);
extern CFStringRef (*wkGetOSNameForUserAgent)(void);
extern CFStringRef (*wkGetPlatformNameForNavigator)(void);
extern CFStringRef (*wkGetVendorNameForNavigator)(void);

#endif // WebCoreSystemInterfaceIOS_h
