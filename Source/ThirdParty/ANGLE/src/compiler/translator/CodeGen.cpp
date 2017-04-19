//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef ANGLE_ENABLE_ESSL
#include "compiler/translator/TranslatorESSL.h"
#endif  // ANGLE_ENABLE_ESSL

#ifdef ANGLE_ENABLE_GLSL
#include "compiler/translator/TranslatorGLSL.h"
#endif  // ANGLE_ENABLE_GLSL

#ifdef ANGLE_ENABLE_HLSL
#include "compiler/translator/TranslatorHLSL.h"
#endif  // ANGLE_ENABLE_HLSL

#ifdef ANGLE_ENABLE_VULKAN
#include "compiler/translator/TranslatorVulkan.h"
#endif  // ANGLE_ENABLE_VULKAN

namespace sh
{

//
// This function must be provided to create the actual
// compile object used by higher level code.  It returns
// a subclass of TCompiler.
//
TCompiler *ConstructCompiler(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
{
    switch (output)
    {
        case SH_ESSL_OUTPUT:
#ifdef ANGLE_ENABLE_ESSL
            return new TranslatorESSL(type, spec);
#else
            // This compiler is not supported in this configuration. Return NULL per the
            // sh::ConstructCompiler API.
            return nullptr;
#endif  // ANGLE_ENABLE_ESSL

        case SH_GLSL_130_OUTPUT:
        case SH_GLSL_140_OUTPUT:
        case SH_GLSL_150_CORE_OUTPUT:
        case SH_GLSL_330_CORE_OUTPUT:
        case SH_GLSL_400_CORE_OUTPUT:
        case SH_GLSL_410_CORE_OUTPUT:
        case SH_GLSL_420_CORE_OUTPUT:
        case SH_GLSL_430_CORE_OUTPUT:
        case SH_GLSL_440_CORE_OUTPUT:
        case SH_GLSL_450_CORE_OUTPUT:
        case SH_GLSL_COMPATIBILITY_OUTPUT:
#ifdef ANGLE_ENABLE_GLSL
            return new TranslatorGLSL(type, spec, output);
#else
            // This compiler is not supported in this configuration. Return NULL per the
            // sh::ConstructCompiler API.
            return nullptr;
#endif  // ANGLE_ENABLE_GLSL

        case SH_HLSL_3_0_OUTPUT:
        case SH_HLSL_4_1_OUTPUT:
        case SH_HLSL_4_0_FL9_3_OUTPUT:
#ifdef ANGLE_ENABLE_HLSL
            return new TranslatorHLSL(type, spec, output);
#else
            // This compiler is not supported in this configuration. Return NULL per the
            // sh::ConstructCompiler API.
            return nullptr;
#endif  // ANGLE_ENABLE_HLSL

        case SH_GLSL_VULKAN_OUTPUT:
#ifdef ANGLE_ENABLE_VULKAN
            return new TranslatorVulkan(type, spec);
#else
            // This compiler is not supported in this configuration. Return NULL per the
            // ShConstructCompiler API.
            return nullptr;
#endif  // ANGLE_ENABLE_VULKAN

        default:
            // Unknown format. Return NULL per the sh::ConstructCompiler API.
            return nullptr;
    }
}

//
// Delete the compiler made by ConstructCompiler
//
void DeleteCompiler(TCompiler *compiler)
{
    SafeDelete(compiler);
}

}  // namespace sh
