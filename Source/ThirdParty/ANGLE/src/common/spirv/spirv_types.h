//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// spirv_types.h:
//   Strong types for SPIR-V Ids to prevent mistakes when using the builder and parser APIs.
//

#ifndef COMMON_SPIRV_TYPES_H_
#define COMMON_SPIRV_TYPES_H_

#include "common/FastVector.h"

#include <vector>

namespace angle
{
namespace spirv
{
template <typename Helper>
class BoxedUint32
{
  public:
    BoxedUint32() : mValue{0} {}
    explicit BoxedUint32(uint32_t value) : mValue{value} {}
    template <typename T>
    T as() const
    {
        return T{mValue};
    }
    BoxedUint32(const BoxedUint32 &other)            = default;
    BoxedUint32 &operator=(const BoxedUint32 &other) = default;
    operator uint32_t() const { return mValue.value; }
    bool operator==(const BoxedUint32 &other) const { return mValue.value == other.mValue.value; }
    // Applicable to ids, which cannot be 0.
    bool valid() const { return static_cast<bool>(mValue.value); }

  private:
    Helper mValue;
};

struct IdRefHelper
{
    uint32_t value;
};
struct LiteralIntegerHelper
{
    uint32_t value;
};

using IdRef = BoxedUint32<IdRefHelper>;

template <>
inline BoxedUint32<IdRefHelper>::operator uint32_t() const
{
    ASSERT(valid());
    return mValue.value;
}

// IdResult, IdResultType, IdMemorySemantics and IdScope are all translated as IdRef.  This makes
// the type verification weaker, but stops the API from becoming tediously verbose.
using IdResult          = IdRef;
using IdResultType      = IdRef;
using IdMemorySemantics = IdRef;
using IdScope           = IdRef;
using LiteralInteger    = BoxedUint32<LiteralIntegerHelper>;
using LiteralString     = const char *;
// Note: In ANGLE's use cases, all literals fit in 32 bits.
using LiteralContextDependentNumber = LiteralInteger;
// TODO(syoussefi): To be made stronger when generating SPIR-V from the translator.
// http://anglebug.com/4889
using LiteralExtInstInteger = LiteralInteger;

struct PairLiteralIntegerIdRef
{
    LiteralInteger literal;
    IdRef id;
};

struct PairIdRefLiteralInteger
{
    IdRef id;
    LiteralInteger literal;
};

struct PairIdRefIdRef
{
    IdRef id1;
    IdRef id2;
};

// Some instructions need 4 components.  The drivers uniform struct in ANGLE has 8 fields.  A value
// of 8 means almost no instruction would end up making dynamic allocations.  Notable exceptions are
// user-defined structs/blocks and OpEntryPoint.
constexpr size_t kFastVectorSize = 8;

template <typename T>
using FastVectorHelper = angle::FastVector<T, kFastVectorSize>;

using IdRefList                   = FastVectorHelper<IdRef>;
using LiteralIntegerList          = FastVectorHelper<LiteralInteger>;
using PairLiteralIntegerIdRefList = FastVectorHelper<PairLiteralIntegerIdRef>;
using PairIdRefLiteralIntegerList = FastVectorHelper<PairIdRefLiteralInteger>;
using PairIdRefIdRefList          = FastVectorHelper<PairIdRefIdRef>;

// Id 0 is invalid in SPIR-V.
constexpr uint32_t kMinValidId = 1;

// The SPIR-V blob is a sequence of uint32_t's
using Blob = std::vector<uint32_t>;

// Format of the SPIR-V header.
// SPIR-V 1.0 Table 1: First Words of Physical Layout
enum HeaderIndex
{
    kHeaderIndexMagic        = 0,
    kHeaderIndexVersion      = 1,
    kHeaderIndexGenerator    = 2,
    kHeaderIndexIndexBound   = 3,
    kHeaderIndexSchema       = 4,
    kHeaderIndexInstructions = 5,
};

// Returns whether SPIR-V is valid.  Useful for ASSERTs.  Automatically generates a warning if
// SPIR-V is not valid.
bool Validate(const Blob &blob);
void Print(const Blob &blob);

}  // namespace spirv
}  // namespace angle

#endif  // COMMON_SPIRV_TYPES_H_
