/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/beamformer/array_util.h"

#include <algorithm>
#include <limits>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

const float kMaxDotProduct = 1e-6f;

}  // namespace

float GetMinimumSpacing(const std::vector<Point>& array_geometry) {
  RTC_CHECK_GT(array_geometry.size(), 1);
  float mic_spacing = std::numeric_limits<float>::max();
  for (size_t i = 0; i < (array_geometry.size() - 1); ++i) {
    for (size_t j = i + 1; j < array_geometry.size(); ++j) {
      mic_spacing =
          std::min(mic_spacing, Distance(array_geometry[i], array_geometry[j]));
    }
  }
  return mic_spacing;
}

Point PairDirection(const Point& a, const Point& b) {
  return {b.x() - a.x(), b.y() - a.y(), b.z() - a.z()};
}

float DotProduct(const Point& a, const Point& b) {
  return a.x() * b.x() + a.y() * b.y() + a.z() * b.z();
}

Point CrossProduct(const Point& a, const Point& b) {
  return {a.y() * b.z() - a.z() * b.y(), a.z() * b.x() - a.x() * b.z(),
          a.x() * b.y() - a.y() * b.x()};
}

bool AreParallel(const Point& a, const Point& b) {
  Point cross_product = CrossProduct(a, b);
  return DotProduct(cross_product, cross_product) < kMaxDotProduct;
}

bool ArePerpendicular(const Point& a, const Point& b) {
  return std::abs(DotProduct(a, b)) < kMaxDotProduct;
}

rtc::Optional<Point> GetDirectionIfLinear(
    const std::vector<Point>& array_geometry) {
  RTC_DCHECK_GT(array_geometry.size(), 1);
  const Point first_pair_direction =
      PairDirection(array_geometry[0], array_geometry[1]);
  for (size_t i = 2u; i < array_geometry.size(); ++i) {
    const Point pair_direction =
        PairDirection(array_geometry[i - 1], array_geometry[i]);
    if (!AreParallel(first_pair_direction, pair_direction)) {
      return rtc::nullopt;
    }
  }
  return first_pair_direction;
}

rtc::Optional<Point> GetNormalIfPlanar(
    const std::vector<Point>& array_geometry) {
  RTC_DCHECK_GT(array_geometry.size(), 1);
  const Point first_pair_direction =
      PairDirection(array_geometry[0], array_geometry[1]);
  Point pair_direction(0.f, 0.f, 0.f);
  size_t i = 2u;
  bool is_linear = true;
  for (; i < array_geometry.size() && is_linear; ++i) {
    pair_direction = PairDirection(array_geometry[i - 1], array_geometry[i]);
    if (!AreParallel(first_pair_direction, pair_direction)) {
      is_linear = false;
    }
  }
  if (is_linear) {
    return rtc::nullopt;
  }
  const Point normal_direction =
      CrossProduct(first_pair_direction, pair_direction);
  for (; i < array_geometry.size(); ++i) {
    pair_direction = PairDirection(array_geometry[i - 1], array_geometry[i]);
    if (!ArePerpendicular(normal_direction, pair_direction)) {
      return rtc::nullopt;
    }
  }
  return normal_direction;
}

rtc::Optional<Point> GetArrayNormalIfExists(
    const std::vector<Point>& array_geometry) {
  const rtc::Optional<Point> direction = GetDirectionIfLinear(array_geometry);
  if (direction) {
    return Point(direction->y(), -direction->x(), 0.f);
  }
  const rtc::Optional<Point> normal = GetNormalIfPlanar(array_geometry);
  if (normal && normal->z() < kMaxDotProduct) {
    return normal;
  }
  return rtc::nullopt;
}

Point AzimuthToPoint(float azimuth) {
  return Point(std::cos(azimuth), std::sin(azimuth), 0.f);
}

}  // namespace webrtc
