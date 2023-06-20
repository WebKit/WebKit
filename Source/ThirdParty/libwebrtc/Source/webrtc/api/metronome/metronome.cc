/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/metronome/metronome.h"

namespace webrtc {

// TODO(crbug.com/1381982): Remove outdated methods.
void Metronome::AddListener(TickListener* listener) {}
void Metronome::RemoveListener(TickListener* listener) {}

}  // namespace webrtc
