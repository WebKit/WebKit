/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/resources_dir_flag.h"

#include "absl/flags/flag.h"

ABSL_FLAG(std::string,
          resources_dir,
          "",
          "Where to look for the runtime dependencies. If not specified, we "
          "will use a reasonable default depending on where we are running. "
          "This flag is useful if we copy over test resources to a phone and "
          "need to tell the tests where their resources are.");
