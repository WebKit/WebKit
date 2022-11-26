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
    constexpr Serial(const Serial &other)  = default;
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
    friend class AtomicQueueSerial;
    constexpr explicit Serial(uint64_t value) : mValue(value) {}
    uint64_t mValue;
    static constexpr uint64_t kInvalid = 0;
};

// Defines class to track the queue serial that can be load/store from multiple threads atomically.
class AtomicQueueSerial final
{
  public:
    AtomicQueueSerial() : mValue(kInvalid) { ASSERT(mValue.is_lock_free()); }
    AtomicQueueSerial &operator=(const Serial &other)
    {
        mValue.store(other.mValue, std::memory_order_release);
        return *this;
    }
    Serial getSerial() const { return Serial(mValue.load(std::memory_order_consume)); }

  private:
    std::atomic<uint64_t> mValue;
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
        uint64_t current = mSerial++;
        ASSERT(mSerial > current);  // Integer overflow
        return Serial(current);
    }

  private:
    SerialBaseType mSerial;
};

using SerialFactory           = SerialFactoryBase<uint64_t>;
using AtomicSerialFactory     = SerialFactoryBase<std::atomic<uint64_t>>;
using RenderPassSerialFactory = SerialFactoryBase<uint64_t>;

}  // namespace rx

#endif  // LIBANGLE_RENDERER_SERIAL_UTILS_H_
