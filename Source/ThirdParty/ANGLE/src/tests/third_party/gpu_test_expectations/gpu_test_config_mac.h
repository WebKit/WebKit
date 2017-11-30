//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gpu_test_config_mac.h:
//   Helper functions for gpu_test_config that have to be compiled in ObjectiveC++
//

#ifndef ANGLE_GPU_TEST_EXPECTATIONS_GPU_TEST_CONFIG_MAC_H_
#define ANGLE_GPU_TEST_EXPECTATIONS_GPU_TEST_CONFIG_MAC_H_

#include "gpu_info.h"

namespace angle
{

void GetOperatingSystemVersionNumbers(int32_t *major_version,
                                      int32_t *minor_version,
                                      int32_t *bugfix_version);

} // namespace angle

#endif // ANGLE_GPU_TEST_EXPECTATIONS_GPU_TEST_CONFIG_MAC_H_
