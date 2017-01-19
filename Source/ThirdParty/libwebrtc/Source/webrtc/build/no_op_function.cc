/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

namespace webrtc {

// TODO(kjellander): Remove this whenever possible. GN's static_library
// target type requires at least one object to avoid errors linking.

// No-op function that can be used to compile an object necessary
// for linking into a static library.
int foobarbaz() {
  return 0;
}

}  // namespace webrtc

