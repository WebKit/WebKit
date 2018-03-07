/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_HELP_FUNCTIONS_DS_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_HELP_FUNCTIONS_DS_H_

#include <dshow.h>

DEFINE_GUID(MEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00,
            0xAA, 0x00, 0x38, 0x9B, 0x71);
DEFINE_GUID(MEDIASUBTYPE_HDYC, 0x43594448, 0x0000, 0x0010, 0x80, 0x00, 0x00,
            0xAA, 0x00, 0x38, 0x9B, 0x71);

#define RELEASE_AND_CLEAR(p) if (p) { (p) -> Release () ; (p) = NULL ; }

namespace webrtc
{
namespace videocapturemodule
{
LONGLONG GetMaxOfFrameArray(LONGLONG *maxFps, long size);

IPin* GetInputPin(IBaseFilter* filter);
IPin* GetOutputPin(IBaseFilter* filter, REFGUID Category);
BOOL PinMatchesCategory(IPin *pPin, REFGUID Category);

}  // namespace videocapturemodule
}  // namespace webrtc
#endif // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_HELP_FUNCTIONS_DS_H_
