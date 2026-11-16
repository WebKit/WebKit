/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_SFRAME_SFRAME_CONTEXT_FACTORY_H_
#define MODULES_SFRAME_SFRAME_CONTEXT_FACTORY_H_

#include <memory>

#include "absl/base/nullability.h"
#include "api/sframe/sframe_types.h"
#include "third_party/sframe/src/include/sframe/sframe.h"

namespace webrtc {

// Creates a third_party/sframe context for the given cipher suite. Shared by
// sender and receiver crypto implementations.
absl_nonnull std::unique_ptr<sframe::Context> CreateSframeContext(
    SframeCipherSuite suite);

}  // namespace webrtc

#endif  // MODULES_SFRAME_SFRAME_CONTEXT_FACTORY_H_
