/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __cplusplus

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

#include "api/array_view.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#include "absl/base/call_once.h"
#include "absl/base/fast_type_id.h"
#include "absl/strings/ascii.h"
#include "api/neteq/tick_timer.h"
#include "call/payload_type.h"
#include "libyuv/basic_types.h"
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "libyuv/convert_from.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/row.h"
#include "libyuv/scale.h"
#include "libyuv/scale_row.h"
#include "media/base/codec_list.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/include/module_common_types.h"
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "modules/video_coding/svc/scalable_video_controller.h"
#include "modules/video_coding/timing/decode_time_percentile_filter.h"
#include "modules/video_coding/timing/timestamp_extrapolator.h"
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/packet/tlv_trait.h"
#include "net/dcsctp/public/dcsctp_handover_state.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/timeout.h"
#include "openssl/asm_base.h"
#include "openssl/rand.h"
#include "push_resampler.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/numerics/moving_percentile_filter.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/openssl.h"
#include "rtc_base/rtp_to_ntp_estimator.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/time_utils.h"
#include "signal_processing_library.h"
#include "system_wrappers/include/metrics.h"
#include "third_party/boringssl/src/crypto/bcm_support.h"
#include "third_party/boringssl/src/crypto/bytestring/internal.h"
#include "third_party/boringssl/src/crypto/evp/internal.h"
#include "third_party/boringssl/src/crypto/fipsmodule/cipher/internal.h"
#include "third_party/boringssl/src/crypto/fipsmodule/service_indicator/internal.h"
#include "third_party/boringssl/src/crypto/x509/internal.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parser.h"
#include "third_party/boringssl/src/ssl/internal.h"
#include "vpx/vpx_codec.h"

#endif
