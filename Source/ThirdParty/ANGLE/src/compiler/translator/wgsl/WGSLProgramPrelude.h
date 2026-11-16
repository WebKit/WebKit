//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_WGSL_WGSL_PROGRAM_PRELUDE_H_
#define COMPILER_TRANSLATOR_WGSL_WGSL_PROGRAM_PRELUDE_H_

#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Operator_autogen.h"
#include "compiler/translator/wgsl/Utils.h"

namespace sh
{
struct WGSLWrapperFunction
{
    ImmutableString prefix;
    ImmutableString suffix;
};

class WGSLProgramPrelude
{
  public:
    // The wrapper functions take a pointer to the modified value, in the address space of the
    // variable that contains the value.
    WGSLWrapperFunction preIncrement(const TType &preIncrementedType,
                                     WgslPointerAddressSpace addressSpace);
    WGSLWrapperFunction preDecrement(const TType &preDecrementedType,
                                     WgslPointerAddressSpace addressSpace);
    WGSLWrapperFunction postIncrement(const TType &postIncrementedType,
                                      WgslPointerAddressSpace addressSpace);
    WGSLWrapperFunction postDecrement(const TType &postDecrementedType,
                                      WgslPointerAddressSpace addressSpace);

    WGSLWrapperFunction assign(const TType &dest,
                               const TType &src,
                               TOperator op,
                               WgslPointerAddressSpace addressSpace);

    void outputPrelude(TInfoSinkBase &sink);

  private:
    using FuncId = uint64_t;

    FuncId mUniqueFuncId = 0;

    TMap<TType, FuncId> mPreIncrementedTypes;
    TMap<TType, FuncId> mPreDecrementedTypes;
    TMap<TType, FuncId> mPostIncrementedTypes;
    TMap<TType, FuncId> mPostDecrementedTypes;

    struct Assignment
    {
        TType dest;
        TType src;
        TOperator op;

        bool operator<(const Assignment &other) const
        {
            return std::tie(dest, src, op) < std::tie(other.dest, other.src, other.op);
        }
    };
    TMap<Assignment, FuncId> mAssigned;
};
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_WGSL_WGSL_PROGRAM_PRELUDE_H_
