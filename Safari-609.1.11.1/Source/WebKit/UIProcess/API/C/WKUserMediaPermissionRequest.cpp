/*
 * Copyright (C) 2014 Igalia S.L
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WKUserMediaPermissionRequest.h"

#include "UserMediaPermissionRequestProxy.h"
#include "WKAPICast.h"
#include "WKArray.h"
#include "WKMutableArray.h"
#include "WKString.h"

using namespace WebKit;

WKTypeID WKUserMediaPermissionRequestGetTypeID()
{
    return toAPI(UserMediaPermissionRequestProxy::APIType);
}


void WKUserMediaPermissionRequestAllow(WKUserMediaPermissionRequestRef userMediaPermissionRequestRef, WKStringRef audioDeviceUID, WKStringRef videoDeviceUID)
{
    toImpl(userMediaPermissionRequestRef)->allow(toWTFString(audioDeviceUID), toWTFString(videoDeviceUID));
}

static UserMediaPermissionRequestProxy::UserMediaAccessDenialReason toWK(UserMediaPermissionRequestDenialReason reason)
{
    switch (reason) {
    case kWKNoConstraints:
        return UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints;
        break;
    case kWKUserMediaDisabled:
        return UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled;
        break;
    case kWKNoCaptureDevices:
        return UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoCaptureDevices;
        break;
    case kWKInvalidConstraint:
        return UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint;
        break;
    case kWKHardwareError:
        return UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::HardwareError;
        break;
    case kWKPermissionDenied:
        return UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied;
        break;
    case kWKOtherFailure:
        return UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure;
        break;
    }

    ASSERT_NOT_REACHED();
    return UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure;
    
}

void WKUserMediaPermissionRequestDeny(WKUserMediaPermissionRequestRef userMediaPermissionRequestRef, UserMediaPermissionRequestDenialReason reason)
{
    toImpl(userMediaPermissionRequestRef)->deny(toWK(reason));
}

WKArrayRef WKUserMediaPermissionRequestVideoDeviceUIDs(WKUserMediaPermissionRequestRef userMediaPermissionRef)
{
    WKMutableArrayRef array = WKMutableArrayCreate();
#if ENABLE(MEDIA_STREAM)
    for (auto& deviceUID : toImpl(userMediaPermissionRef)->videoDeviceUIDs())
        WKArrayAppendItem(array, toAPI(API::String::create(deviceUID).ptr()));
#endif
    return array;
}

WKArrayRef WKUserMediaPermissionRequestAudioDeviceUIDs(WKUserMediaPermissionRequestRef userMediaPermissionRef)
{
    WKMutableArrayRef array = WKMutableArrayCreate();
#if ENABLE(MEDIA_STREAM)
    for (auto& deviceUID : toImpl(userMediaPermissionRef)->audioDeviceUIDs())
        WKArrayAppendItem(array, toAPI(API::String::create(deviceUID).ptr()));
#endif
    return array;
}
