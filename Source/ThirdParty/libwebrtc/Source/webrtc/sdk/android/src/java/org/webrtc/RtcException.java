/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * RtcException represents exceptions that are specific to the WebRTC library. Refer to the file
 * api/rtc_error.h for more information.
 */
public class RtcException extends RuntimeException {

    public RtcException(String message) {
        super(message);
    }
}
