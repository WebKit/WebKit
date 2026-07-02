//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/wgsl/Utils.h"

#include "common/log_utils.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Common.h"
#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/Types.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{
const char kWrappedPrefix[] = "ANGLE_wrapped_";
}

template <typename StringStreamType>
void WriteWgslBareTypeName(StringStreamType &output,
                           const TType &type,
                           const EmitTypeConfig &config)
{
    const TBasicType basicType = type.getBasicType();

    switch (basicType)
    {
        case TBasicType::EbtVoid:
            output << type.getBasicString();
            break;
        case TBasicType::EbtBool:
            if (config.addressSpace == WgslAddressSpace::Uniform)
            {
                // The uniform address space does not support bools in WGSL, so they are emulated
                // with u32 (which matches gles because bools are 4-bytes long in std140).
                output << "u32";
            }
            else
            {
                output << "bool";
            }
            break;
        // TODO(anglebug.com/42267100): is there double precision (f64) in GLSL? It doesn't really
        // exist in WGSL (i.e. f64 does not exist but AbstractFloat can handle 64 bits???) Metal
        // does not have 64 bit double precision types. It's being implemented in WGPU:
        // https://github.com/gpuweb/gpuweb/issues/2805
        case TBasicType::EbtFloat:
            output << "f32";
            break;
        case TBasicType::EbtInt:
            output << "i32";
            break;
        case TBasicType::EbtUInt:
            output << "u32";
            break;

        case TBasicType::EbtStruct:
            WriteNameOf(output, *type.getStruct());
            break;

        case TBasicType::EbtInterfaceBlock:
            WriteNameOf(output, *type.getInterfaceBlock());
            break;

        default:
            if (IsSampler(basicType))
            {
                // Variables of sampler type should be written elsewhere since they require special
                // handling; they are split into two different variables in WGSL.

                // TODO(anglebug.com/389145696): this is reachable if a sampler is passed as a
                // function parameter. They should be monomorphized.
                UNIMPLEMENTED();
            }
            else if (IsImage(basicType))
            {
                // GLSL's image types are not implemented in this backend.
                UNIMPLEMENTED();

                output << "texture_storage_2d<";
                switch (type.getBasicType())
                {
                    case EbtImage2D:
                        output << "f32";
                        break;
                    case EbtIImage2D:
                        output << "i32";
                        break;
                    case EbtUImage2D:
                        output << "u32";
                        break;
                    default:
                        UNIMPLEMENTED();
                        break;
                }
                if (type.getMemoryQualifier().readonly || type.getMemoryQualifier().writeonly)
                {
                    UNIMPLEMENTED();
                }
                output << ">";
            }
            else
            {
                UNREACHABLE();
            }
            break;
    }
}

template <typename StringStreamType>
void WriteNameOf(StringStreamType &output, SymbolType symbolType, const ImmutableString &name)
{
    switch (symbolType)
    {
        case SymbolType::BuiltIn:
            output << name;
            break;
        case SymbolType::UserDefined:
            output << '_' << kUserDefinedNamePrefix << name;
            break;
        case SymbolType::AngleInternal:
            output << name;
            break;
        case SymbolType::Empty:
            // TODO(anglebug.com/42267100): support this if necessary
            UNREACHABLE();
    }
}

template <typename StringStreamType>
void WriteWgslType(StringStreamType &output, const TType &type, const EmitTypeConfig &config)
{
    if (type.isArray())
    {
        // WGSL does not support samplers anywhere inside structs or arrays.
        ASSERT(!type.isSampler() && !type.isStructureContainingSamplers());

        // Examples:
        // array<f32, 5>
        // array<array<u32, 5>, 10>
        output << "array<";
        TType innerType = type;
        innerType.toArrayElementType();
        if (ElementTypeNeedsUniformWrapperStruct(config.addressSpace == WgslAddressSpace::Uniform,
                                                 &type))
        {
            // Multidimensional arrays not currently supported in uniforms in the WebGPU backend
            ASSERT(!innerType.isArray());

            // Due to uniform address space layout constraints, certain array element types must
            // be wrapped in a wrapper struct.
            // Example: array<ANGLE_wrapped_f32, 5>
            output << MakeUniformWrapperStructName(&innerType);
        }
        else
        {
            WriteWgslType(output, innerType, config);
        }
        output << ", " << type.getOutermostArraySize() << ">";
    }
    else if (type.isVector())
    {
        output << "vec" << static_cast<uint32_t>(type.getNominalSize()) << "<";
        WriteWgslBareTypeName(output, type, config);
        output << ">";
    }
    else if (type.isMatrix())
    {
        if (config.addressSpace == WgslAddressSpace::Uniform && type.getRows() == 2)
        {
            // matCx2 in the uniform address space is too packed for std140, and so they will be
            // represented by an array<ANGLE_wrapped_vec2, C>.
            output << "array<" << kWrappedPrefix << "vec2, "
                   << static_cast<uint32_t>(type.getCols()) << ">";
        }
        else
        {
            output << "mat" << static_cast<uint32_t>(type.getCols()) << "x"
                   << static_cast<uint32_t>(type.getRows()) << "<";
            WriteWgslBareTypeName(output, type, config);
            output << ">";
        }
    }
    else
    {
        // This type has no dimensions and is equivalent to its bare type.
        WriteWgslBareTypeName(output, type, config);
    }
}

template void WriteWgslBareTypeName<TInfoSinkBase>(TInfoSinkBase &output,
                                                   const TType &type,
                                                   const EmitTypeConfig &config);
template void WriteNameOf<TInfoSinkBase>(TInfoSinkBase &output,
                                         SymbolType symbolType,
                                         const ImmutableString &name);
template void WriteWgslType<TInfoSinkBase>(TInfoSinkBase &output,
                                           const TType &type,
                                           const EmitTypeConfig &config);

template void WriteWgslBareTypeName<TStringStream>(TStringStream &output,
                                                   const TType &type,
                                                   const EmitTypeConfig &config);
template void WriteNameOf<TStringStream>(TStringStream &output,
                                         SymbolType symbolType,
                                         const ImmutableString &name);
template void WriteWgslType<TStringStream>(TStringStream &output,
                                           const TType &type,
                                           const EmitTypeConfig &config);

template <typename StringStreamType>
void WriteWgslSamplerType(StringStreamType &output,
                          const TType &type,
                          WgslSamplerTypeConfig samplerType)
{
    ASSERT(type.isSampler());
    if (samplerType == WgslSamplerTypeConfig::Texture)
    {
        output << "texture";
        if (IsShadowSampler(type.getBasicType()))
        {
            output << "_depth";
        }

        if (IsSamplerMS(type.getBasicType()))
        {
            output << "_multisampled";
            ASSERT(IsSampler2D(type.getBasicType()));
            // Unsupported in wGSL, it seems.
            ASSERT(!IsSampler2DMSArray(type.getBasicType()));
        }

        if (IsSampler2D(type.getBasicType()) || IsSampler2DArray(type.getBasicType()))
        {
            output << "_2d";
        }
        else if (IsSampler3D(type.getBasicType()))
        {
            output << "_3d";
        }
        else if (IsSamplerCube(type.getBasicType()))
        {
            output << "_cube";
        }

        if (IsSamplerArray(type.getBasicType()))
        {
            ASSERT(!IsSampler3D(type.getBasicType()));
            output << "_array";
        }

        // Shadow samplers are always floating point in both GLSL and WGSL and don't need to be
        // parameterized.
        if (!IsShadowSampler(type.getBasicType()))
        {
            output << "<";
            if (!IsIntegerSampler(type.getBasicType()))
            {
                output << "f32";
            }
            else if (!IsIntegerSamplerUnsigned(type.getBasicType()))
            {
                output << "i32";
            }
            else
            {
                output << "u32";
            }
            output << ">";
        }
        if (type.getMemoryQualifier().readonly || type.getMemoryQualifier().writeonly)
        {
            // TODO(anglebug.com/42267100): implement memory qualifiers.
            UNIMPLEMENTED();
        }
    }
    else
    {
        ASSERT(samplerType == WgslSamplerTypeConfig::Sampler);
        // sampler or sampler_comparison.
        if (IsShadowSampler(type.getBasicType()))
        {
            output << "sampler_comparison";
        }
        else
        {
            output << "sampler";
        }
    }
}

template void WriteWgslSamplerType<TInfoSinkBase>(TInfoSinkBase &output,
                                                  const TType &type,
                                                  WgslSamplerTypeConfig samplerType);

template void WriteWgslSamplerType<TStringStream>(TStringStream &output,
                                                  const TType &type,
                                                  WgslSamplerTypeConfig samplerType);

ImmutableString MakeUniformWrapperStructName(const TType *type)
{
    TType typeToOutput(*type);

    const char *typeStr;
    // Bools are represented as u32s in the uniform address space (and bvecs as uvecs).
    // TODO(anglebug.com/376553328): simplify by using WriteWgslType({WgslAddressSpace::Uniform})
    // here.
    if (typeToOutput.getBasicType() == EbtBool)
    {
        typeToOutput.setBasicType(TBasicType::EbtUInt);
    }

    typeStr = typeToOutput.getBuiltInTypeNameString();

    return BuildConcatenatedImmutableString(kWrappedPrefix, typeStr);
}

bool ElementTypeNeedsUniformWrapperStruct(bool inUniformAddressSpace, const TType *type)
{
    // Only types that are used as array element types in the uniform address space need wrapper
    // structs. If the array element type is a struct it does not need to be wrapped in another
    // layer of struct.
    if (!inUniformAddressSpace || !type->isArray() || type->getStruct())
    {
        return false;
    }

    TType elementType = *type;
    elementType.toArrayElementType();
    // If the array element type's stride is already a multiple of 16, it does not need a wrapper
    // struct.
    //
    // The remaining possible element types are scalars, vectors, matrices, and other arrays.
    // - Scalars need to be aligned to 16.
    // - vec3 and vec4 are already aligned to 16, but vec2 needs to be aligned.
    // - Matrices are aligned to 16 automatically, except matCx2 which already needs to be handled
    // by specialized code anyway.
    // - WebGL2 doesn't support nested arrays so this won't either.
    ASSERT(!elementType.isArray());

    return elementType.isScalar() || (elementType.isVector() && elementType.getNominalSize() == 2);
}

GlobalVars FindGlobalVars(TIntermBlock *root)
{
    GlobalVars globals;
    for (TIntermNode *node : *root->getSequence())
    {
        if (TIntermDeclaration *declNode = node->getAsDeclarationNode())
        {
            Declaration decl = ViewDeclaration(*declNode);
            globals.insert({decl.symbol.variable().name(), declNode});
        }
    }
    return globals;
}

WgslPointerAddressSpace GetWgslAddressSpaceForPointer(const TType &type)
{
    switch (type.getQualifier())
    {
        case EvqTemporary:
        // NOTE: As of Sept 2025, parameters are immutable in WGSL (and are handled by an AST pass
        // that copies parameters to temporaries). Include these here in case parameters become
        // mutable in the future.
        case EvqParamIn:
        case EvqParamOut:
        case EvqParamInOut:
            return WgslPointerAddressSpace::Function;
        default:
            // EvqGlobal and various other shader outputs/builtins are all globals.
            return WgslPointerAddressSpace::Private;
    }
}

ImmutableString StringForWgslPointerAddressSpace(WgslPointerAddressSpace as)
{
    switch (as)
    {
        case WgslPointerAddressSpace::Function:
            return ImmutableString("function");
        case WgslPointerAddressSpace::Private:
            return ImmutableString("private");
    }
}

}  // namespace sh
