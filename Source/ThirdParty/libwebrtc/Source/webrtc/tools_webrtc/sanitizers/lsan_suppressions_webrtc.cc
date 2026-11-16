/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the WebRTC suppressions for LeakSanitizer.
// You can also pass additional suppressions via LSAN_OPTIONS:
// LSAN_OPTIONS=suppressions=/path/to/suppressions. Please refer to
// http://dev.chromium.org/developers/testing/leaksanitizer for more info.

#if defined(LEAK_SANITIZER)

// Please make sure the code below declares a single string variable
// kLSanDefaultSuppressions which contains LSan suppressions delimited by
// newlines. See http://dev.chromium.org/developers/testing/leaksanitizer
// for the instructions on writing suppressions.
char kLSanDefaultSuppressions[] =

    // ============ Leaks in third-party code shared with Chromium =============
    // These entries are copied from build/sanitizers/lsan_suppressions.cc in
    // Chromium. Please don't add new entries here unless they're present in
    // there.

    // False positives in libfontconfig. http://crbug.com/39050
    "leak:libfontconfig\n"

    // Leaks in Nvidia's libGL.
    "leak:libGL.so\n"

    // XRandR has several one time leaks.
    "leak:libxrandr\n"

    // xrandr leak. http://crbug.com/119677
    "leak:XRRFindDisplay\n"

    // ========== Leaks in third-party code not shared with Chromium ===========

    // PipeWire module loading leaks
    "leak:pw_context_load_module\n"

    // ================ Leaks in WebRTC code ================
    // PLEASE DO NOT ADD SUPPRESSIONS FOR NEW LEAKS.
    // Instead, commits that introduce memory leaks should be reverted.
    // Suppressing the leak is acceptable in some cases when reverting is
    // impossible, i.e. when enabling leak detection for the first time for a
    // test target with pre-existing leaks.

    // ALSA Linux system library leaks.
    "leak:webrtc::AudioDeviceLinuxALSA::InitMicrophone\n"
    "leak:webrtc::AudioDeviceLinuxALSA::InitSpeaker\n"

    // PLEASE READ ABOVE BEFORE ADDING NEW SUPPRESSIONS.

    // End of suppressions.
    ;  // Please keep this semicolon.

#endif  // LEAK_SANITIZER
