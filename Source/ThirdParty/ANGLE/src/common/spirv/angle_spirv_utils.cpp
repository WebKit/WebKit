// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// spirv_types.cpp:
//   Helper SPIR-V functions.

#include "spirv_types.h"

// SPIR-V tools include for AST validation.
#include <spirv-tools/libspirv.hpp>

namespace angle
{
namespace spirv
{

#if defined(ANGLE_ENABLE_ASSERTS)
namespace
{
void ValidateSpirvMessage(spv_message_level_t level,
                          const char *source,
                          const spv_position_t &position,
                          const char *message)
{
    WARN() << "Level" << level << ": " << message;
}
}  // anonymous namespace

bool Validate(const Blob &blob)
{
    spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);

    spirvTools.SetMessageConsumer(ValidateSpirvMessage);
    bool result = spirvTools.Validate(blob);

    if (!result)
    {
        std::string readableSpirv;
        spirvTools.Disassemble(blob, &readableSpirv, 0);
        WARN() << "Invalid SPIR-V:\n" << readableSpirv;
    }

    return result;
}
#else   // ANGLE_ENABLE_ASSERTS
bool Validate(const Blob &blob)
{
    // Placeholder implementation since this is only used inside an ASSERT().
    // Return false to indicate an error in case this is ever accidentally used somewhere else.
    return false;
}
#endif  // ANGLE_ENABLE_ASSERTS

}  // namespace spirv
}  // namespace angle
