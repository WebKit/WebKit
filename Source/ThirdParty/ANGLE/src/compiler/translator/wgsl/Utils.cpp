//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/wgsl/Utils.h"

#include "compiler/translator/Common.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/Types.h"
#include "compiler/translator/util.h"

namespace sh
{

template <typename StringStreamType>
void WriteWgslBareTypeName(StringStreamType &output, const TType &type)
{
    const TBasicType basicType = type.getBasicType();

    switch (basicType)
    {
        case TBasicType::EbtVoid:
        case TBasicType::EbtBool:
            output << type.getBasicString();
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
                //  TODO(anglebug.com/42267100): possibly emit both a sampler and a texture2d. WGSL
                //  has sampler variables for the sampler configuration, whereas GLSL has sampler2d
                //  and other sampler* variables for an actual texture.
                output << "texture2d<";
                switch (type.getBasicType())
                {
                    case EbtSampler2D:
                        output << "f32";
                        break;
                    case EbtISampler2D:
                        output << "i32";
                        break;
                    case EbtUSampler2D:
                        output << "u32";
                        break;
                    default:
                        // TODO(anglebug.com/42267100): are any of the other sampler types necessary
                        // to translate?
                        UNIMPLEMENTED();
                        break;
                }
                if (type.getMemoryQualifier().readonly || type.getMemoryQualifier().writeonly)
                {
                    // TODO(anglebug.com/42267100): implement memory qualifiers.
                    UNIMPLEMENTED();
                }
                output << ">";
            }
            else if (IsImage(basicType))
            {
                // TODO(anglebug.com/42267100): does texture2d also correspond to GLSL's image type?
                output << "texture2d<";
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
                        // TODO(anglebug.com/42267100): are any of the other image types necessary
                        // to translate?
                        UNIMPLEMENTED();
                        break;
                }
                if (type.getMemoryQualifier().readonly || type.getMemoryQualifier().writeonly)
                {
                    // TODO(anglebug.com/42267100): implement memory qualifiers.
                    UNREACHABLE();
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
            output << kUserDefinedNamePrefix << name;
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
void WriteWgslType(StringStreamType &output, const TType &type)
{
    if (type.isArray())
    {
        // Examples:
        // array<f32, 5>
        // array<array<u32, 5>, 10>
        output << "array<";
        TType innerType = type;
        innerType.toArrayElementType();
        WriteWgslType(output, innerType);
        output << ", " << type.getOutermostArraySize() << ">";
    }
    else if (type.isVector())
    {
        output << "vec" << static_cast<uint32_t>(type.getNominalSize()) << "<";
        WriteWgslBareTypeName(output, type);
        output << ">";
    }
    else if (type.isMatrix())
    {
        output << "mat" << static_cast<uint32_t>(type.getCols()) << "x"
               << static_cast<uint32_t>(type.getRows()) << "<";
        WriteWgslBareTypeName(output, type);
        output << ">";
    }
    else
    {
        // This type has no dimensions and is equivalent to its bare type.
        WriteWgslBareTypeName(output, type);
    }
}

template void WriteWgslBareTypeName<TInfoSinkBase>(TInfoSinkBase &output, const TType &type);
template void WriteNameOf<TInfoSinkBase>(TInfoSinkBase &output,
                                         SymbolType symbolType,
                                         const ImmutableString &name);
template void WriteWgslType<TInfoSinkBase>(TInfoSinkBase &output, const TType &type);

template void WriteWgslBareTypeName<TStringStream>(TStringStream &output, const TType &type);
template void WriteNameOf<TStringStream>(TStringStream &output,
                                         SymbolType symbolType,
                                         const ImmutableString &name);
template void WriteWgslType<TStringStream>(TStringStream &output, const TType &type);

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

}  // namespace sh
