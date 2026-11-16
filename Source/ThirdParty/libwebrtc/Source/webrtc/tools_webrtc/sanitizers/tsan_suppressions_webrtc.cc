/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the WebRTC suppressions for ThreadSanitizer.
// Please refer to
// http://dev.chromium.org/developers/testing/threadsanitizer-tsan-v2
// for more info.

#if defined(THREAD_SANITIZER)

// Please make sure the code below declares a single string variable
// kTSanDefaultSuppressions contains TSan suppressions delimited by newlines.
// See http://dev.chromium.org/developers/testing/threadsanitizer-tsan-v2
// for the instructions on writing suppressions.
char kTSanDefaultSuppressions[] =

    // WebRTC specific suppressions.

    // TODO(pbos): Trace events are racy due to lack of proper POD atomics.
    // https://code.google.com/p/webrtc/issues/detail?id=2497
    "race:*trace_event_unique_catstatic*\n"

    // http://crbug.com/244856
    "race:libpulsecommon*.so\n"

    // End of suppressions.
    ;  // Please keep this semicolon.

#endif  // THREAD_SANITIZER
