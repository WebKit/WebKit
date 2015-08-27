/*
 * Copyright (C) 2014 Igalia S.L
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


void WKUserMediaPermissionRequestAllowBest(WKUserMediaPermissionRequestRef userMediaPermissionRequestRef)
{
    const String& videoDevice = toImpl(userMediaPermissionRequestRef)->firstVideoDeviceUID();
    const String& audioDevice = toImpl(userMediaPermissionRequestRef)->firstAudioDeviceUID();
    toImpl(userMediaPermissionRequestRef)->allow(videoDevice, audioDevice);
}

void WKUserMediaPermissionRequestAllow(WKUserMediaPermissionRequestRef userMediaPermissionRequestRef, WKStringRef videoDeviceUID, WKStringRef audioDeviceUID)
{
    toImpl(userMediaPermissionRequestRef)->allow(toWTFString(videoDeviceUID), toWTFString(audioDeviceUID));
}

void WKUserMediaPermissionRequestDeny(WKUserMediaPermissionRequestRef userMediaPermissionRequestRef)
{
    toImpl(userMediaPermissionRequestRef)->deny();
}

WKArrayRef WKUserMediaPermissionRequestDeviceNamesVideo(WKUserMediaPermissionRequestRef userMediaPermissionRef)
{
    WKMutableArrayRef array = WKMutableArrayCreate();
#if ENABLE(MEDIA_STREAM)
    for (auto& name : toImpl(userMediaPermissionRef)->videoDeviceUIDs()) {
        String deviceName = toImpl(userMediaPermissionRef)->getDeviceNameForUID(name, WebCore::RealtimeMediaSource::Type::Video);
        WKArrayAppendItem(array, toAPI(API::String::create(deviceName).ptr()));
    }
#endif
    return array;
}

WKArrayRef WKUserMediaPermissionRequestDeviceNamesAudio(WKUserMediaPermissionRequestRef userMediaPermissionRef)
{
    WKMutableArrayRef array = WKMutableArrayCreate();
#if ENABLE(MEDIA_STREAM)
    for (auto& name : toImpl(userMediaPermissionRef)->audioDeviceUIDs()) {
        String deviceName = toImpl(userMediaPermissionRef)->getDeviceNameForUID(name, WebCore::RealtimeMediaSource::Type::Audio);
        WKArrayAppendItem(array, toAPI(API::String::create(deviceName).ptr()));
    }
#endif
    return array;
}

WKStringRef WKUserMediaPermissionRequestFirstVideoDeviceUID(WKUserMediaPermissionRequestRef userMediaPermissionRef)
{
    return !toImpl(userMediaPermissionRef)->videoDeviceUIDs().isEmpty() ? reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(WKUserMediaPermissionRequestDeviceNamesVideo(userMediaPermissionRef), 0)) : reinterpret_cast<WKStringRef>(WKStringCreateWithUTF8CString(""));
}

WKStringRef WKUserMediaPermissionRequestFirstAudioDeviceUID(WKUserMediaPermissionRequestRef userMediaPermissionRef)
{
    return !toImpl(userMediaPermissionRef)->audioDeviceUIDs().isEmpty() ? reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(WKUserMediaPermissionRequestDeviceNamesAudio(userMediaPermissionRef), 0)) : reinterpret_cast<WKStringRef>(WKStringCreateWithUTF8CString(""));
}
