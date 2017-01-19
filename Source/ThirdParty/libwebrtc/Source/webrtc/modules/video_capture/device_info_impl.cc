/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdlib.h>

#include "webrtc/modules/video_capture/device_info_impl.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/system_wrappers/include/logging.h"

#ifndef abs
#define abs(a) (a>=0?a:-a)
#endif

namespace webrtc
{
namespace videocapturemodule
{
DeviceInfoImpl::DeviceInfoImpl(const int32_t id)
    : _id(id), _apiLock(*RWLockWrapper::CreateRWLock()), _lastUsedDeviceName(NULL),
      _lastUsedDeviceNameLength(0)
{
}

DeviceInfoImpl::~DeviceInfoImpl(void)
{
    _apiLock.AcquireLockExclusive();
    free(_lastUsedDeviceName);
    _apiLock.ReleaseLockExclusive();

    delete &_apiLock;
}
int32_t DeviceInfoImpl::NumberOfCapabilities(
                                        const char* deviceUniqueIdUTF8)
{

    if (!deviceUniqueIdUTF8)
        return -1;

    _apiLock.AcquireLockShared();

    if (_lastUsedDeviceNameLength == strlen((char*) deviceUniqueIdUTF8))
    {
        // Is it the same device that is asked for again.
#if defined(WEBRTC_MAC) || defined(WEBRTC_LINUX)
        if(strncasecmp((char*)_lastUsedDeviceName,
                       (char*) deviceUniqueIdUTF8,
                       _lastUsedDeviceNameLength)==0)
#else
        if (_strnicmp((char*) _lastUsedDeviceName,
                      (char*) deviceUniqueIdUTF8,
                      _lastUsedDeviceNameLength) == 0)
#endif
        {
            //yes
            _apiLock.ReleaseLockShared();
            return static_cast<int32_t>(_captureCapabilities.size());
        }
    }
    // Need to get exclusive rights to create the new capability map.
    _apiLock.ReleaseLockShared();
    WriteLockScoped cs2(_apiLock);

    int32_t ret = CreateCapabilityMap(deviceUniqueIdUTF8);
    return ret;
}

int32_t DeviceInfoImpl::GetCapability(const char* deviceUniqueIdUTF8,
                                      const uint32_t deviceCapabilityNumber,
                                      VideoCaptureCapability& capability)
{
    assert(deviceUniqueIdUTF8 != NULL);

    ReadLockScoped cs(_apiLock);

    if ((_lastUsedDeviceNameLength != strlen((char*) deviceUniqueIdUTF8))
#if defined(WEBRTC_MAC) || defined(WEBRTC_LINUX)
        || (strncasecmp((char*)_lastUsedDeviceName,
                        (char*) deviceUniqueIdUTF8,
                        _lastUsedDeviceNameLength)!=0))
#else
        || (_strnicmp((char*) _lastUsedDeviceName,
                      (char*) deviceUniqueIdUTF8,
                      _lastUsedDeviceNameLength) != 0))
#endif

    {
        _apiLock.ReleaseLockShared();
        _apiLock.AcquireLockExclusive();
        if (-1 == CreateCapabilityMap(deviceUniqueIdUTF8))
        {
            _apiLock.ReleaseLockExclusive();
            _apiLock.AcquireLockShared();
            return -1;
        }
        _apiLock.ReleaseLockExclusive();
        _apiLock.AcquireLockShared();
    }

    // Make sure the number is valid
    if (deviceCapabilityNumber >= (unsigned int) _captureCapabilities.size())
    {
        LOG(LS_ERROR) << "Invalid deviceCapabilityNumber "
                      << deviceCapabilityNumber << ">= number of capabilities ("
                      << _captureCapabilities.size() << ").";
        return -1;
    }

    capability = _captureCapabilities[deviceCapabilityNumber];
    return 0;
}

int32_t DeviceInfoImpl::GetBestMatchedCapability(
                                        const char*deviceUniqueIdUTF8,
                                        const VideoCaptureCapability& requested,
                                        VideoCaptureCapability& resulting)
{


    if (!deviceUniqueIdUTF8)
        return -1;

    ReadLockScoped cs(_apiLock);
    if ((_lastUsedDeviceNameLength != strlen((char*) deviceUniqueIdUTF8))
#if defined(WEBRTC_MAC) || defined(WEBRTC_LINUX)
        || (strncasecmp((char*)_lastUsedDeviceName,
                        (char*) deviceUniqueIdUTF8,
                        _lastUsedDeviceNameLength)!=0))
#else
        || (_strnicmp((char*) _lastUsedDeviceName,
                      (char*) deviceUniqueIdUTF8,
                      _lastUsedDeviceNameLength) != 0))
#endif
    {
        _apiLock.ReleaseLockShared();
        _apiLock.AcquireLockExclusive();
        if (-1 == CreateCapabilityMap(deviceUniqueIdUTF8))
        {
            return -1;
        }
        _apiLock.ReleaseLockExclusive();
        _apiLock.AcquireLockShared();
    }

    int32_t bestformatIndex = -1;
    int32_t bestWidth = 0;
    int32_t bestHeight = 0;
    int32_t bestFrameRate = 0;
    RawVideoType bestRawType = kVideoUnknown;
    webrtc::VideoCodecType bestCodecType = webrtc::kVideoCodecUnknown;

    const int32_t numberOfCapabilies =
        static_cast<int32_t>(_captureCapabilities.size());

    for (int32_t tmp = 0; tmp < numberOfCapabilies; ++tmp) // Loop through all capabilities
    {
        VideoCaptureCapability& capability = _captureCapabilities[tmp];

        const int32_t diffWidth = capability.width - requested.width;
        const int32_t diffHeight = capability.height - requested.height;
        const int32_t diffFrameRate = capability.maxFPS - requested.maxFPS;

        const int32_t currentbestDiffWith = bestWidth - requested.width;
        const int32_t currentbestDiffHeight = bestHeight - requested.height;
        const int32_t currentbestDiffFrameRate = bestFrameRate - requested.maxFPS;

        if ((diffHeight >= 0 && diffHeight <= abs(currentbestDiffHeight)) // Height better or equalt that previouse.
            || (currentbestDiffHeight < 0 && diffHeight >= currentbestDiffHeight))
        {

            if (diffHeight == currentbestDiffHeight) // Found best height. Care about the width)
            {
                if ((diffWidth >= 0 && diffWidth <= abs(currentbestDiffWith)) // Width better or equal
                    || (currentbestDiffWith < 0 && diffWidth >= currentbestDiffWith))
                {
                    if (diffWidth == currentbestDiffWith && diffHeight
                        == currentbestDiffHeight) // Same size as previously
                    {
                        //Also check the best frame rate if the diff is the same as previouse
                        if (((diffFrameRate >= 0 &&
                              diffFrameRate <= currentbestDiffFrameRate) // Frame rate to high but better match than previouse and we have not selected IUV
                            ||
                            (currentbestDiffFrameRate < 0 &&
                             diffFrameRate >= currentbestDiffFrameRate)) // Current frame rate is lower than requested. This is better.
                        )
                        {
                            if ((currentbestDiffFrameRate == diffFrameRate) // Same frame rate as previous  or frame rate allready good enough
                                || (currentbestDiffFrameRate >= 0))
                            {
                                if (bestRawType != requested.rawType
                                    && requested.rawType != kVideoUnknown
                                    && (capability.rawType == requested.rawType
                                        || capability.rawType == kVideoI420
                                        || capability.rawType == kVideoYUY2
                                        || capability.rawType == kVideoYV12))
                                {
                                    bestCodecType = capability.codecType;
                                    bestRawType = capability.rawType;
                                    bestformatIndex = tmp;
                                }
                                // If width height and frame rate is full filled we can use the camera for encoding if it is supported.
                                if (capability.height == requested.height
                                    && capability.width == requested.width
                                    && capability.maxFPS >= requested.maxFPS)
                                {
                                    if (capability.codecType == requested.codecType
                                        && bestCodecType != requested.codecType)
                                    {
                                        bestCodecType = capability.codecType;
                                        bestformatIndex = tmp;
                                    }
                                }
                            }
                            else // Better frame rate
                            {
                                if (requested.codecType == capability.codecType)
                                {

                                    bestWidth = capability.width;
                                    bestHeight = capability.height;
                                    bestFrameRate = capability.maxFPS;
                                    bestCodecType = capability.codecType;
                                    bestRawType = capability.rawType;
                                    bestformatIndex = tmp;
                                }
                            }
                        }
                    }
                    else // Better width than previously
                    {
                        if (requested.codecType == capability.codecType)
                        {
                            bestWidth = capability.width;
                            bestHeight = capability.height;
                            bestFrameRate = capability.maxFPS;
                            bestCodecType = capability.codecType;
                            bestRawType = capability.rawType;
                            bestformatIndex = tmp;
                        }
                    }
                }// else width no good
            }
            else // Better height
            {
                if (requested.codecType == capability.codecType)
                {
                    bestWidth = capability.width;
                    bestHeight = capability.height;
                    bestFrameRate = capability.maxFPS;
                    bestCodecType = capability.codecType;
                    bestRawType = capability.rawType;
                    bestformatIndex = tmp;
                }
            }
        }// else height not good
    }//end for

    LOG(LS_VERBOSE) << "Best camera format: " << bestWidth << "x" << bestHeight
                    << "@" << bestFrameRate
                    << "fps, color format: " << bestRawType;

    // Copy the capability
    if (bestformatIndex < 0)
        return -1;
    resulting = _captureCapabilities[bestformatIndex];
    return bestformatIndex;
}

/* Returns the expected Capture delay*/
int32_t DeviceInfoImpl::GetExpectedCaptureDelay(
                                          const DelayValues delayValues[],
                                          const uint32_t sizeOfDelayValues,
                                          const char* productId,
                                          const uint32_t width,
                                          const uint32_t height)
{
    int32_t bestDelay = kDefaultCaptureDelay;

    for (uint32_t device = 0; device < sizeOfDelayValues; ++device)
    {
        if (delayValues[device].productId && strncmp((char*) productId,
                                                     (char*) delayValues[device].productId,
                                                     kVideoCaptureProductIdLength) == 0)
        {
            // We have found the camera

            int32_t bestWidth = 0;
            int32_t bestHeight = 0;

            //Loop through all tested sizes and find one that seems fitting
            for (uint32_t delayIndex = 0; delayIndex < NoOfDelayValues; ++delayIndex)
            {
                const DelayValue& currentValue = delayValues[device].delayValues[delayIndex];

                const int32_t diffWidth = currentValue.width - width;
                const int32_t diffHeight = currentValue.height - height;

                const int32_t currentbestDiffWith = bestWidth - width;
                const int32_t currentbestDiffHeight = bestHeight - height;

                if ((diffHeight >= 0 && diffHeight <= abs(currentbestDiffHeight)) // Height better or equal than previous.
                    || (currentbestDiffHeight < 0 && diffHeight >= currentbestDiffHeight))
                {

                    if (diffHeight == currentbestDiffHeight) // Found best height. Care about the width)
                    {
                        if ((diffWidth >= 0 && diffWidth <= abs(currentbestDiffWith)) // Width better or equal
                            || (currentbestDiffWith < 0 && diffWidth >= currentbestDiffWith))
                        {
                            if (diffWidth == currentbestDiffWith && diffHeight
                                == currentbestDiffHeight) // Same size as previous
                            {
                            }
                            else // Better width than previously
                            {
                                bestWidth = currentValue.width;
                                bestHeight = currentValue.height;
                                bestDelay = currentValue.delay;
                            }
                        }// else width no good
                    }
                    else // Better height
                    {
                        bestWidth = currentValue.width;
                        bestHeight = currentValue.height;
                        bestDelay = currentValue.delay;
                    }
                }// else height not good
            }//end for
            break;
        }
    }
    if (bestDelay > kMaxCaptureDelay)
    {
        LOG(LS_WARNING) << "Expected capture delay (" << bestDelay
                        << " ms) too high, using " << kMaxCaptureDelay
                        << " ms.";
        bestDelay = kMaxCaptureDelay;
    }

    return bestDelay;

}

//Default implementation. This should be overridden by Mobile implementations.
int32_t DeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                       VideoRotation& orientation) {
  orientation = kVideoRotation_0;
    return -1;
}
}  // namespace videocapturemodule
}  // namespace webrtc
