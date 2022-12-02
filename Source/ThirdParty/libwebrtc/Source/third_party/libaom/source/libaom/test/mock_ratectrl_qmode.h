/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_TEST_MOCK_RATECTRL_QMODE_H_
#define AOM_TEST_MOCK_RATECTRL_QMODE_H_

#include "av1/ratectrl_qmode_interface.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"

namespace aom {

class MockRateControlQMode : public AV1RateControlQModeInterface {
 public:
  MOCK_METHOD(Status, SetRcParam, (const RateControlParam &rc_param),
              (override));
  MOCK_METHOD(StatusOr<GopStructList>, DetermineGopInfo,
              (const FirstpassInfo &firstpass_info), (override));
  MOCK_METHOD(StatusOr<GopEncodeInfo>, GetGopEncodeInfo,
              (const GopStruct &gop_struct, const TplGopStats &tpl_gop_stats,
               const RefFrameTable &ref_frame_table_snapshot_init),
              (override));
};

}  // namespace aom

#endif  // AOM_TEST_MOCK_RATECTRL_QMODE_H_
