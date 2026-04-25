//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Version.h: Encapsulation of a GL version.

#ifndef LIBANGLE_VERSION_H_
#define LIBANGLE_VERSION_H_

#include <cstdint>

namespace gl
{

struct alignas(uint16_t) Version
{
    // Avoid conflicts with linux system defines
#undef major
#undef minor

    constexpr Version() = default;
    constexpr Version(uint8_t major, uint8_t minor) : value((major << 8) | minor) {}

    constexpr bool operator==(const Version &other) const { return value == other.value; }
    constexpr bool operator!=(const Version &other) const { return value != other.value; }
    constexpr bool operator>=(const Version &other) const { return value >= other.value; }
    constexpr bool operator<=(const Version &other) const { return value <= other.value; }
    constexpr bool operator<(const Version &other) const { return value < other.value; }
    constexpr bool operator>(const Version &other) const { return value > other.value; }

    // These functions should not be used for compare operations.
    constexpr uint32_t getMajor() const { return value >> 8; }
    constexpr uint32_t getMinor() const { return value & 0xFF; }

  private:
    uint16_t value = 0;
};
static_assert(sizeof(Version) == 2);

static_assert(Version().getMajor() == 0);
static_assert(Version().getMinor() == 0);
static_assert(Version(0, 255).getMajor() == 0);
static_assert(Version(0, 255).getMinor() == 255);
static_assert(Version(255, 0).getMajor() == 255);
static_assert(Version(255, 0).getMinor() == 0);
static_assert(Version(4, 5).getMajor() == 4);
static_assert(Version(4, 5).getMinor() == 5);
static_assert(Version(4, 6) == Version(4, 6));
static_assert(Version(1, 0) != Version(1, 1));
static_assert(Version(1, 0) != Version(2, 0));
static_assert(Version(2, 0) > Version(1, 0));
static_assert(Version(3, 1) > Version(3, 0));
static_assert(Version(3, 0) > Version(1, 1));
static_assert(Version(2, 0) < Version(3, 0));
static_assert(Version(3, 1) < Version(3, 2));
static_assert(Version(1, 1) < Version(2, 0));

static constexpr Version ES_1_0 = Version(1, 0);
static constexpr Version ES_1_1 = Version(1, 1);
static constexpr Version ES_2_0 = Version(2, 0);
static constexpr Version ES_3_0 = Version(3, 0);
static constexpr Version ES_3_1 = Version(3, 1);
static constexpr Version ES_3_2 = Version(3, 2);

}  // namespace gl

#endif  // LIBANGLE_VERSION_H_
