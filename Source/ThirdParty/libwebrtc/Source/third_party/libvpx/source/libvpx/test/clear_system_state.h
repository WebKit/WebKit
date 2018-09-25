/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_CLEAR_SYSTEM_STATE_H_
#define TEST_CLEAR_SYSTEM_STATE_H_

#include "./vpx_config.h"
#if ARCH_X86 || ARCH_X86_64
#include "vpx_ports/x86.h"
#endif

namespace libvpx_test {

// Reset system to a known state. This function should be used for all non-API
// test cases.
inline void ClearSystemState() {
#if ARCH_X86 || ARCH_X86_64
  vpx_reset_mmx_state();
#endif
}

}  // namespace libvpx_test
#endif  // TEST_CLEAR_SYSTEM_STATE_H_
