//
// Copyright (c) 2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/util.h"

#include <limits>

#include "compiler/preprocessor/numeric_lex.h"
#include "compiler/translator/SymbolTable.h"
#include "common/utilities.h"

bool strtof_clamp(const std::string &str, float *value)
{
    bool success = pp::numeric_lex_float(str, value);
    if (!success)
        *value = std::numeric_limits<float>::max();
    return success;
}

bool atoi_clamp(const char *str, unsigned int *value)
{
    bool success = pp::numeric_lex_int(str, value);
    if (!success)
        *value = std::numeric_limits<unsigned int>::max();
    return success;
}

namespace sh
{

GLenum GLVariableType(const TType &type)
{
    if (type.getBasicType() == EbtFloat)
    {
        if (type.isScalar())
        {
            return GL_FLOAT;
        }
        else if (type.isVector())
        {
            switch (type.getNominalSize())
            {
              case 2: return GL_FLOAT_VEC2;
              case 3: return GL_FLOAT_VEC3;
              case 4: return GL_FLOAT_VEC4;
              default: UNREACHABLE();
            }
        }
        else if (type.isMatrix())
        {
            switch (type.getCols())
            {
              case 2:
                switch (type.getRows())
                {
                  case 2: return GL_FLOAT_MAT2;
                  case 3: return GL_FLOAT_MAT2x3;
                  case 4: return GL_FLOAT_MAT2x4;
                  default: UNREACHABLE();
                }

              case 3:
                switch (type.getRows())
                {
                  case 2: return GL_FLOAT_MAT3x2;
                  case 3: return GL_FLOAT_MAT3;
                  case 4: return GL_FLOAT_MAT3x4;
                  default: UNREACHABLE();
                }

              case 4:
                switch (type.getRows())
                {
                  case 2: return GL_FLOAT_MAT4x2;
                  case 3: return GL_FLOAT_MAT4x3;
                  case 4: return GL_FLOAT_MAT4;
                  default: UNREACHABLE();
                }

              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();
    }
    else if (type.getBasicType() == EbtInt)
    {
        if (type.isScalar())
        {
            return GL_INT;
        }
        else if (type.isVector())
        {
            switch (type.getNominalSize())
            {
              case 2: return GL_INT_VEC2;
              case 3: return GL_INT_VEC3;
              case 4: return GL_INT_VEC4;
              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();
    }
    else if (type.getBasicType() == EbtUInt)
    {
        if (type.isScalar())
        {
            return GL_UNSIGNED_INT;
        }
        else if (type.isVector())
        {
            switch (type.getNominalSize())
            {
              case 2: return GL_UNSIGNED_INT_VEC2;
              case 3: return GL_UNSIGNED_INT_VEC3;
              case 4: return GL_UNSIGNED_INT_VEC4;
              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();
    }
    else if (type.getBasicType() == EbtBool)
    {
        if (type.isScalar())
        {
            return GL_BOOL;
        }
        else if (type.isVector())
        {
            switch (type.getNominalSize())
            {
              case 2: return GL_BOOL_VEC2;
              case 3: return GL_BOOL_VEC3;
              case 4: return GL_BOOL_VEC4;
              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();
    }

    switch (type.getBasicType())
    {
      case EbtSampler2D:            return GL_SAMPLER_2D;
      case EbtSampler3D:            return GL_SAMPLER_3D;
      case EbtSamplerCube:          return GL_SAMPLER_CUBE;
      case EbtSamplerExternalOES:   return GL_SAMPLER_EXTERNAL_OES;
      case EbtSampler2DRect:        return GL_SAMPLER_2D_RECT_ARB;
      case EbtSampler2DArray:       return GL_SAMPLER_2D_ARRAY;
      case EbtISampler2D:           return GL_INT_SAMPLER_2D;
      case EbtISampler3D:           return GL_INT_SAMPLER_3D;
      case EbtISamplerCube:         return GL_INT_SAMPLER_CUBE;
      case EbtISampler2DArray:      return GL_INT_SAMPLER_2D_ARRAY;
      case EbtUSampler2D:           return GL_UNSIGNED_INT_SAMPLER_2D;
      case EbtUSampler3D:           return GL_UNSIGNED_INT_SAMPLER_3D;
      case EbtUSamplerCube:         return GL_UNSIGNED_INT_SAMPLER_CUBE;
      case EbtUSampler2DArray:      return GL_UNSIGNED_INT_SAMPLER_2D_ARRAY;
      case EbtSampler2DShadow:      return GL_SAMPLER_2D_SHADOW;
      case EbtSamplerCubeShadow:    return GL_SAMPLER_CUBE_SHADOW;
      case EbtSampler2DArrayShadow: return GL_SAMPLER_2D_ARRAY_SHADOW;
      default: UNREACHABLE();
    }

    return GL_NONE;
}

GLenum GLVariablePrecision(const TType &type)
{
    if (type.getBasicType() == EbtFloat)
    {
        switch (type.getPrecision())
        {
          case EbpHigh:
            return GL_HIGH_FLOAT;
          case EbpMedium:
            return GL_MEDIUM_FLOAT;
          case EbpLow:
            return GL_LOW_FLOAT;
          case EbpUndefined:
          // Should be defined as the default precision by the parser
          default:
            UNREACHABLE();
        }
    }
    else if (type.getBasicType() == EbtInt || type.getBasicType() == EbtUInt)
    {
        switch (type.getPrecision())
        {
          case EbpHigh:
            return GL_HIGH_INT;
          case EbpMedium:
            return GL_MEDIUM_INT;
          case EbpLow:
            return GL_LOW_INT;
          case EbpUndefined:
          // Should be defined as the default precision by the parser
          default:
            UNREACHABLE();
        }
    }

    // Other types (boolean, sampler) don't have a precision
    return GL_NONE;
}

TString ArrayString(const TType &type)
{
    if (!type.isArray())
    {
        return "";
    }

    return "[" + str(type.getArraySize()) + "]";
}

bool IsVaryingOut(TQualifier qualifier)
{
    switch (qualifier)
    {
      case EvqVaryingOut:
      case EvqSmoothOut:
      case EvqFlatOut:
      case EvqCentroidOut:
      case EvqVertexOut:
        return true;

      default: break;
    }

    return false;
}

bool IsVaryingIn(TQualifier qualifier)
{
    switch (qualifier)
    {
      case EvqVaryingIn:
      case EvqSmoothIn:
      case EvqFlatIn:
      case EvqCentroidIn:
      case EvqFragmentIn:
        return true;

      default: break;
    }

    return false;
}

bool IsVarying(TQualifier qualifier)
{
    return IsVaryingIn(qualifier) || IsVaryingOut(qualifier);
}

InterpolationType GetInterpolationType(TQualifier qualifier)
{
    switch (qualifier)
    {
      case EvqFlatIn:
      case EvqFlatOut:
        return INTERPOLATION_FLAT;

      case EvqSmoothIn:
      case EvqSmoothOut:
      case EvqVertexOut:
      case EvqFragmentIn:
      case EvqVaryingIn:
      case EvqVaryingOut:
        return INTERPOLATION_SMOOTH;

      case EvqCentroidIn:
      case EvqCentroidOut:
        return INTERPOLATION_CENTROID;

      default: UNREACHABLE();
        return INTERPOLATION_SMOOTH;
    }
}

TType GetInterfaceBlockType(const sh::InterfaceBlock &block)
{
    TType type;
    TFieldList *fields = new TFieldList;
    TSourceLoc loc;
    for (const auto &field : block.fields)
    {
        TType *fieldType = new TType(GetShaderVariableType(field));
        fields->push_back(new TField(fieldType, new TString(field.name.c_str()), loc));
    }

    TInterfaceBlock *interfaceBlock = new TInterfaceBlock(
        new TString(block.name.c_str()), fields, new TString(block.instanceName.c_str()),
        block.arraySize, TLayoutQualifier::create());

    type.setBasicType(EbtInterfaceBlock);
    type.setInterfaceBlock(interfaceBlock);
    type.setArraySize(block.arraySize);
    return type;
}

TType GetShaderVariableBasicType(const sh::ShaderVariable &var)
{
    switch (var.type)
    {
        case GL_BOOL:
            return TType(EbtBool);
        case GL_BOOL_VEC2:
            return TType(EbtBool, 2);
        case GL_BOOL_VEC3:
            return TType(EbtBool, 3);
        case GL_BOOL_VEC4:
            return TType(EbtBool, 4);
        case GL_FLOAT:
            return TType(EbtFloat);
        case GL_FLOAT_VEC2:
            return TType(EbtFloat, 2);
        case GL_FLOAT_VEC3:
            return TType(EbtFloat, 3);
        case GL_FLOAT_VEC4:
            return TType(EbtFloat, 4);
        case GL_FLOAT_MAT2:
            return TType(EbtFloat, 2, 2);
        case GL_FLOAT_MAT3:
            return TType(EbtFloat, 3, 3);
        case GL_FLOAT_MAT4:
            return TType(EbtFloat, 4, 4);
        case GL_FLOAT_MAT2x3:
            return TType(EbtFloat, 2, 3);
        case GL_FLOAT_MAT2x4:
            return TType(EbtFloat, 2, 4);
        case GL_FLOAT_MAT3x2:
            return TType(EbtFloat, 3, 2);
        case GL_FLOAT_MAT3x4:
            return TType(EbtFloat, 3, 4);
        case GL_FLOAT_MAT4x2:
            return TType(EbtFloat, 4, 2);
        case GL_FLOAT_MAT4x3:
            return TType(EbtFloat, 4, 3);
        case GL_INT:
            return TType(EbtInt);
        case GL_INT_VEC2:
            return TType(EbtInt, 2);
        case GL_INT_VEC3:
            return TType(EbtInt, 3);
        case GL_INT_VEC4:
            return TType(EbtInt, 4);
        case GL_UNSIGNED_INT:
            return TType(EbtUInt);
        case GL_UNSIGNED_INT_VEC2:
            return TType(EbtUInt, 2);
        case GL_UNSIGNED_INT_VEC3:
            return TType(EbtUInt, 3);
        case GL_UNSIGNED_INT_VEC4:
            return TType(EbtUInt, 4);
        default:
            UNREACHABLE();
            return TType();
    }
}

TType GetShaderVariableType(const sh::ShaderVariable &var)
{
    TType type;
    if (var.isStruct())
    {
        TFieldList *fields = new TFieldList;
        TSourceLoc loc;
        for (const auto &field : var.fields)
        {
            TType *fieldType = new TType(GetShaderVariableType(field));
            fields->push_back(new TField(fieldType, new TString(field.name.c_str()), loc));
        }
        TStructure *structure = new TStructure(new TString(var.structName.c_str()), fields);

        type.setBasicType(EbtStruct);
        type.setStruct(structure);
    }
    else
    {
        type = GetShaderVariableBasicType(var);
    }

    if (var.isArray())
    {
        type.setArraySize(var.elementCount());
    }
    return type;
}

TOperator TypeToConstructorOperator(const TType &type)
{
    switch (type.getBasicType())
    {
        case EbtFloat:
            if (type.isMatrix())
            {
                switch (type.getCols())
                {
                    case 2:
                        switch (type.getRows())
                        {
                            case 2:
                                return EOpConstructMat2;
                            case 3:
                                return EOpConstructMat2x3;
                            case 4:
                                return EOpConstructMat2x4;
                            default:
                                break;
                        }
                        break;

                    case 3:
                        switch (type.getRows())
                        {
                            case 2:
                                return EOpConstructMat3x2;
                            case 3:
                                return EOpConstructMat3;
                            case 4:
                                return EOpConstructMat3x4;
                            default:
                                break;
                        }
                        break;

                    case 4:
                        switch (type.getRows())
                        {
                            case 2:
                                return EOpConstructMat4x2;
                            case 3:
                                return EOpConstructMat4x3;
                            case 4:
                                return EOpConstructMat4;
                            default:
                                break;
                        }
                        break;
                }
            }
            else
            {
                switch (type.getNominalSize())
                {
                    case 1:
                        return EOpConstructFloat;
                    case 2:
                        return EOpConstructVec2;
                    case 3:
                        return EOpConstructVec3;
                    case 4:
                        return EOpConstructVec4;
                    default:
                        break;
                }
            }
            break;

        case EbtInt:
            switch (type.getNominalSize())
            {
                case 1:
                    return EOpConstructInt;
                case 2:
                    return EOpConstructIVec2;
                case 3:
                    return EOpConstructIVec3;
                case 4:
                    return EOpConstructIVec4;
                default:
                    break;
            }
            break;

        case EbtUInt:
            switch (type.getNominalSize())
            {
                case 1:
                    return EOpConstructUInt;
                case 2:
                    return EOpConstructUVec2;
                case 3:
                    return EOpConstructUVec3;
                case 4:
                    return EOpConstructUVec4;
                default:
                    break;
            }
            break;

        case EbtBool:
            switch (type.getNominalSize())
            {
                case 1:
                    return EOpConstructBool;
                case 2:
                    return EOpConstructBVec2;
                case 3:
                    return EOpConstructBVec3;
                case 4:
                    return EOpConstructBVec4;
                default:
                    break;
            }
            break;

        case EbtStruct:
            return EOpConstructStruct;

        default:
            break;
    }

    return EOpNull;
}

GetVariableTraverser::GetVariableTraverser(const TSymbolTable &symbolTable)
    : mSymbolTable(symbolTable)
{
}

template void GetVariableTraverser::setTypeSpecificInfo(
    const TType &type, const TString& name, InterfaceBlockField *variable);
template void GetVariableTraverser::setTypeSpecificInfo(
    const TType &type, const TString& name, ShaderVariable *variable);
template void GetVariableTraverser::setTypeSpecificInfo(
    const TType &type, const TString& name, Uniform *variable);

template<>
void GetVariableTraverser::setTypeSpecificInfo(
    const TType &type, const TString& name, Varying *variable)
{
    ASSERT(variable);
    switch (type.getQualifier())
    {
      case EvqVaryingIn:
      case EvqVaryingOut:
      case EvqVertexOut:
      case EvqSmoothOut:
      case EvqFlatOut:
      case EvqCentroidOut:
        if (mSymbolTable.isVaryingInvariant(std::string(name.c_str())) || type.isInvariant())
        {
            variable->isInvariant = true;
        }
        break;
      default:
        break;
    }

    variable->interpolation = GetInterpolationType(type.getQualifier());
}

template <typename VarT>
void GetVariableTraverser::traverse(const TType &type,
                                    const TString &name,
                                    std::vector<VarT> *output)
{
    const TStructure *structure = type.getStruct();

    VarT variable;
    variable.name = name.c_str();
    variable.arraySize = type.getArraySize();

    if (!structure)
    {
        variable.type = GLVariableType(type);
        variable.precision = GLVariablePrecision(type);
    }
    else
    {
        // Note: this enum value is not exposed outside ANGLE
        variable.type = GL_STRUCT_ANGLEX;
        variable.structName = structure->name().c_str();

        const TFieldList &fields = structure->fields();

        for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
        {
            TField *field = fields[fieldIndex];
            traverse(*field->type(), field->name(), &variable.fields);
        }
    }
    setTypeSpecificInfo(type, name, &variable);
    visitVariable(&variable);

    ASSERT(output);
    output->push_back(variable);
}

template void GetVariableTraverser::traverse(const TType &, const TString &, std::vector<InterfaceBlockField> *);
template void GetVariableTraverser::traverse(const TType &, const TString &, std::vector<ShaderVariable> *);
template void GetVariableTraverser::traverse(const TType &, const TString &, std::vector<Uniform> *);
template void GetVariableTraverser::traverse(const TType &, const TString &, std::vector<Varying> *);

// GLSL ES 1.0.17 4.6.1 The Invariant Qualifier
bool CanBeInvariantESSL1(TQualifier qualifier)
{
    return IsVaryingIn(qualifier) || IsVaryingOut(qualifier) ||
           IsBuiltinOutputVariable(qualifier) ||
           (IsBuiltinFragmentInputVariable(qualifier) && qualifier != EvqFrontFacing);
}

// GLSL ES 3.00 Revision 6, 4.6.1 The Invariant Qualifier
// GLSL ES 3.10 Revision 4, 4.8.1 The Invariant Qualifier
bool CanBeInvariantESSL3OrGreater(TQualifier qualifier)
{
    return IsVaryingOut(qualifier) || qualifier == EvqFragmentOut ||
           IsBuiltinOutputVariable(qualifier);
}

bool IsBuiltinOutputVariable(TQualifier qualifier)
{
    switch (qualifier)
    {
        case EvqPosition:
        case EvqPointSize:
        case EvqFragDepth:
        case EvqFragDepthEXT:
        case EvqFragColor:
        case EvqSecondaryFragColorEXT:
        case EvqFragData:
        case EvqSecondaryFragDataEXT:
            return true;
        default:
            break;
    }
    return false;
}

bool IsBuiltinFragmentInputVariable(TQualifier qualifier)
{
    switch (qualifier)
    {
        case EvqFragCoord:
        case EvqPointCoord:
        case EvqFrontFacing:
            return true;
        default:
            break;
    }
    return false;
}
}  // namespace sh
