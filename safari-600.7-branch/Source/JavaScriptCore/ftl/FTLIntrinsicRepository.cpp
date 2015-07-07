/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FTLIntrinsicRepository.h"

#if ENABLE(FTL_JIT)

namespace JSC { namespace FTL {

IntrinsicRepository::IntrinsicRepository(LContext context)
    : CommonValues(context)
#define INTRINSIC_INITIALIZATION(ourName, llvmName, type) , m_##ourName(0)
    FOR_EACH_FTL_INTRINSIC(INTRINSIC_INITIALIZATION)
#undef INTRINSIC_INITIALIZATION
#define FUNCTION_TYPE_INITIALIZATION(typeName, type) , m_##typeName(0)
    FOR_EACH_FUNCTION_TYPE(FUNCTION_TYPE_INITIALIZATION)
#undef FUNCTION_TYPE_INITIALIZATION
{
}

#define INTRINSIC_GETTER_SLOW_DEFINITION(ourName, llvmName, type)       \
    LValue IntrinsicRepository::ourName##IntrinsicSlow()                \
    {                                                                   \
        m_##ourName = addExternFunction(m_module, llvmName, type);      \
        return m_##ourName;                                             \
    }
FOR_EACH_FTL_INTRINSIC(INTRINSIC_GETTER_SLOW_DEFINITION)
#undef INTRINSIC_GETTER

#define FUNCTION_TYPE_GETTER_SLOW_DEFINITION(typeName, type) \
    LType IntrinsicRepository::typeName##Slow()              \
    {                                                        \
        m_##typeName = type;                                 \
        return m_##typeName;                                 \
    }
FOR_EACH_FUNCTION_TYPE(FUNCTION_TYPE_GETTER_SLOW_DEFINITION)
#undef FUNCTION_TYPE_GETTER_SLOW_DEFINITION

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

