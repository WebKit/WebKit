//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// serial_utils:
//   Utilities for generating unique IDs for resources in ANGLE.
//

#ifndef LIBANGLE_RENDERER_SERIAL_UTILS_H_
#define LIBANGLE_RENDERER_SERIAL_UTILS_H_

#include <atomic>
#include <limits>

#include "common/angleutils.h"
#include "common/debug.h"

namespace rx
{
class ResourceSerial
{
  public:
    constexpr ResourceSerial() : mValue(kDirty) {}
    explicit constexpr ResourceSerial(uintptr_t value) : mValue(value) {}
    constexpr bool operator==(ResourceSerial other) const { return mValue == other.mValue; }
    constexpr bool operator!=(ResourceSerial other) const { return mValue != other.mValue; }

    void dirty() { mValue = kDirty; }
    void clear() { mValue = kEmpty; }

    constexpr bool valid() const { return mValue != kEmpty && mValue != kDirty; }
    constexpr bool empty() const { return mValue == kEmpty; }

  private:
    constexpr static uintptr_t kDirty = std::numeric_limits<uintptr_t>::max();
    constexpr static uintptr_t kEmpty = 0;

    uintptr_t mValue;
};

class Serial final
{
  public:
    constexpr Serial() : mValue(kInvalid) {}
    constexpr Serial(const Serial &other) = default;
    Serial &operator=(const Serial &other) = default;

    static constexpr Serial Infinite() { return Serial(std::numeric_limits<uint64_t>::max()); }

    constexpr bool operator==(const Serial &other) const
    {
        return mValue != kInvalid && mValue == other.mValue;
    }
    constexpr bool operator==(uint32_t value) const
    {
        return mValue != kInvalid && mValue == static_cast<uint64_t>(value);
    }
    constexpr bool operator!=(const Serial &other) const
    {
        return mValue == kInvalid || mValue != other.mValue;
    }
    constexpr bool operator>(const Serial &other) const { return mValue > other.mValue; }
    constexpr bool operator>=(const Serial &other) const { return mValue >= other.mValue; }
    constexpr bool operator<(const Serial &other) const { return mValue < other.mValue; }
    constexpr bool operator<=(const Serial &other) const { return mValue <= other.mValue; }

    constexpr bool operator<(uint32_t value) const { return mValue < static_cast<uint64_t>(value); }

    // Useful for serialization.
    constexpr uint64_t getValue() const { return mValue; }
    constexpr bool valid() const { return mValue != kInvalid; }

  private:
    template <typename T>
    friend class SerialFactoryBase;
    constexpr explicit Serial(uint64_t value) : mValue(value) {}
    uint64_t mValue;
    static constexpr uint64_t kInvalid = 0;
};

// Used as default/initial serial
static constexpr Serial kZeroSerial = Serial();

template <typename SerialBaseType>
class SerialFactoryBase final : angle::NonCopyable
{
  public:
    SerialFactoryBase() : mSerial(1) {}

    Serial generate()
    {
        ASSERT(mSerial + 1 > mSerial);
        return Serial(mSerial++);
    }

  private:
    SerialBaseType mSerial;
};

using SerialFactory       = SerialFactoryBase<uint64_t>;
using AtomicSerialFactory = SerialFactoryBase<std::atomic<uint64_t>>;

// Clang Format doesn't like the following X macro.
// clang-format off
#define OBJECT_SERIAL_TYPES_OP(X)    \
    X(Buffer)                           \
    X(Texture)                          \
    X(Sampler)                          \
    X(ImageView)
// clang-format on

#define DEFINE_UNIQUE_OBJECT_SERIAL_TYPE(Type)                       \
    class Type##Serial                                               \
    {                                                                \
      public:                                                        \
        constexpr Type##Serial() : mSerial(kInvalid) {}              \
        Type##Serial(uint32_t serial) : mSerial(serial) {}           \
                                                                     \
        constexpr bool operator==(const Type##Serial &other) const   \
        {                                                            \
            ASSERT(mSerial != kInvalid);                             \
            ASSERT(other.mSerial != kInvalid);                       \
            return mSerial == other.mSerial;                         \
        }                                                            \
        constexpr bool operator!=(const Type##Serial &other) const   \
        {                                                            \
            ASSERT(mSerial != kInvalid);                             \
            ASSERT(other.mSerial != kInvalid);                       \
            return mSerial != other.mSerial;                         \
        }                                                            \
        constexpr uint32_t getValue() const { return mSerial; }      \
        constexpr bool valid() const { return mSerial != kInvalid; } \
                                                                     \
      private:                                                       \
        uint32_t mSerial;                                            \
        static constexpr uint32_t kInvalid = 0;                      \
    };                                                               \
    static constexpr Type##Serial kInvalid##Type##Serial = Type##Serial();

OBJECT_SERIAL_TYPES_OP(DEFINE_UNIQUE_OBJECT_SERIAL_TYPE)

}  // namespace rx

#endif  // LIBANGLE_RENDERER_SERIAL_UTILS_H_
