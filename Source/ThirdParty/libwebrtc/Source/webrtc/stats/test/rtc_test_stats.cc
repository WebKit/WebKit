/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "stats/test/rtc_test_stats.h"

#include "api/stats/attribute.h"
#include "rtc_base/checks.h"

namespace webrtc {

WEBRTC_RTCSTATS_IMPL(RTCTestStats,
                     RTCStats,
                     "test-stats",
                     AttributeInit("mBool", &m_bool),
                     AttributeInit("mInt32", &m_int32),
                     AttributeInit("mUint32", &m_uint32),
                     AttributeInit("mInt64", &m_int64),
                     AttributeInit("mUint64", &m_uint64),
                     AttributeInit("mDouble", &m_double),
                     AttributeInit("mString", &m_string),
                     AttributeInit("mSequenceBool", &m_sequence_bool),
                     AttributeInit("mSequenceInt32", &m_sequence_int32),
                     AttributeInit("mSequenceUint32", &m_sequence_uint32),
                     AttributeInit("mSequenceInt64", &m_sequence_int64),
                     AttributeInit("mSequenceUint64", &m_sequence_uint64),
                     AttributeInit("mSequenceDouble", &m_sequence_double),
                     AttributeInit("mSequenceString", &m_sequence_string),
                     AttributeInit("mMapStringUint64", &m_map_string_uint64),
                     AttributeInit("mMapStringDouble", &m_map_string_double))

RTCTestStats::RTCTestStats(const std::string& id, Timestamp timestamp)
    : RTCStats(id, timestamp) {}

RTCTestStats::~RTCTestStats() {}

}  // namespace webrtc
