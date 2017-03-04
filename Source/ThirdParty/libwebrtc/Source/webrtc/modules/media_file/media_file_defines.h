/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_DEFINES_H_
#define WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_DEFINES_H_

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {
// Callback class for the MediaFile class.
class FileCallback
{
public:
    virtual ~FileCallback(){}

    // This function is called by MediaFile when a file has been playing for
    // durationMs ms. id is the identifier for the MediaFile instance calling
    // the callback.
    virtual void PlayNotification(const int32_t id,
                                  const uint32_t durationMs) = 0;

    // This function is called by MediaFile when a file has been recording for
    // durationMs ms. id is the identifier for the MediaFile instance calling
    // the callback.
    virtual void RecordNotification(const int32_t id,
                                    const uint32_t durationMs) = 0;

    // This function is called by MediaFile when a file has been stopped
    // playing. id is the identifier for the MediaFile instance calling the
    // callback.
    virtual void PlayFileEnded(const int32_t id) = 0;

    // This function is called by MediaFile when a file has been stopped
    // recording. id is the identifier for the MediaFile instance calling the
    // callback.
    virtual void RecordFileEnded(const int32_t id) = 0;

protected:
    FileCallback() {}
};
}  // namespace webrtc
#endif // WEBRTC_MODULES_MEDIA_FILE_MEDIA_FILE_DEFINES_H_
