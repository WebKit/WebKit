//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BuildSPIRV: Helper for OutputSPIRV to build SPIR-V.
//

#include "compiler/translator/BuildSPIRV.h"

#include "common/spirv/spirv_instruction_builder_autogen.h"
#include "compiler/translator/ValidateVaryingLocations.h"
#include "compiler/translator/blocklayout.h"
#include "compiler/translator/util.h"

namespace sh
{
bool operator==(const SpirvType &a, const SpirvType &b)
{
    if (a.block != b.block)
    {
        return false;
    }

    if (a.arraySizes != b.arraySizes)
    {
        return false;
    }

    // If structure or interface block, they should match by pointer (i.e. be the same block).  The
    // AST transformations are expected to keep the AST consistent by using the same structure and
    // interface block pointer between declarations and usages.  This is validated by
    // ValidateASTOptions::validateVariableReferences.
    if (a.block != nullptr)
    {
        return a.typeSpec.blockStorage == b.typeSpec.blockStorage &&
               a.typeSpec.isInvariantBlock == b.typeSpec.isInvariantBlock &&
               a.typeSpec.isRowMajorQualifiedBlock == b.typeSpec.isRowMajorQualifiedBlock &&
               a.typeSpec.isPatchIOBlock == b.typeSpec.isPatchIOBlock &&
               a.typeSpec.isOrHasBoolInInterfaceBlock == b.typeSpec.isOrHasBoolInInterfaceBlock;
    }

    // Otherwise, match by the type contents.  The AST transformations sometimes recreate types that
    // are already defined, so we can't rely on pointers being unique.
    return a.type == b.type && a.primarySize == b.primarySize &&
           a.secondarySize == b.secondarySize && a.imageInternalFormat == b.imageInternalFormat &&
           a.isSamplerBaseImage == b.isSamplerBaseImage &&
           a.typeSpec.blockStorage == b.typeSpec.blockStorage &&
           a.typeSpec.isRowMajorQualifiedArray == b.typeSpec.isRowMajorQualifiedArray &&
           a.typeSpec.isOrHasBoolInInterfaceBlock == b.typeSpec.isOrHasBoolInInterfaceBlock;
}

namespace
{
bool IsBlockFieldRowMajorQualified(const TType &fieldType, bool isParentBlockRowMajorQualified)
{
    // If the field is specifically qualified as row-major, it will be row-major.  Otherwise unless
    // specifically qualified as column-major, its matrix packing is inherited from the parent
    // block.
    const TLayoutMatrixPacking fieldMatrixPacking = fieldType.getLayoutQualifier().matrixPacking;
    return fieldMatrixPacking == EmpRowMajor ||
           (fieldMatrixPacking == EmpUnspecified && isParentBlockRowMajorQualified);
}

bool IsNonSquareRowMajorArrayInBlock(const TType &type, const SpirvTypeSpec &parentTypeSpec)
{
    return parentTypeSpec.blockStorage != EbsUnspecified && type.isArray() && type.isMatrix() &&
           type.getCols() != type.getRows() &&
           IsBlockFieldRowMajorQualified(type, parentTypeSpec.isRowMajorQualifiedBlock);
}

bool IsInvariant(const TType &type, TCompiler *compiler)
{
    const bool invariantAll = compiler->getPragma().stdgl.invariantAll;

    // The Invariant decoration is applied to output variables if specified or if globally enabled.
    return type.isInvariant() || (IsShaderOut(type.getQualifier()) && invariantAll);
}

TLayoutBlockStorage GetBlockStorage(const TType &type)
{
    // For interface blocks, the block storage is specified on the symbol itself.
    if (type.getInterfaceBlock() != nullptr)
    {
        return type.getInterfaceBlock()->blockStorage();
    }

    // I/O blocks must have been handled above.
    ASSERT(!IsShaderIoBlock(type.getQualifier()));

    // Additionally, interface blocks are already handled, so it's not expected for the type to have
    // a block storage specified.
    ASSERT(type.getLayoutQualifier().blockStorage == EbsUnspecified);

    // Default to std140 for uniform and std430 for buffer blocks.
    return type.getQualifier() == EvqBuffer ? EbsStd430 : EbsStd140;
}

ShaderVariable ToShaderVariable(const TFieldListCollection *block,
                                GLenum type,
                                const TSpan<const unsigned int> arraySizes,
                                bool isRowMajor)
{
    ShaderVariable var;

    var.type             = type;
    var.arraySizes       = {arraySizes.begin(), arraySizes.end()};
    var.isRowMajorLayout = isRowMajor;

    if (block != nullptr)
    {
        for (const TField *field : block->fields())
        {
            const TType &fieldType = *field->type();

            const TLayoutMatrixPacking fieldMatrixPacking =
                fieldType.getLayoutQualifier().matrixPacking;
            const bool isFieldRowMajor = fieldMatrixPacking == EmpRowMajor ||
                                         (fieldMatrixPacking == EmpUnspecified && isRowMajor);
            const GLenum glType =
                fieldType.getStruct() != nullptr ? GL_NONE : GLVariableType(fieldType);

            var.fields.push_back(ToShaderVariable(fieldType.getStruct(), glType,
                                                  fieldType.getArraySizes(), isFieldRowMajor));
        }
    }

    return var;
}

ShaderVariable SpirvTypeToShaderVariable(const SpirvType &type)
{
    const bool isRowMajor =
        type.typeSpec.isRowMajorQualifiedBlock || type.typeSpec.isRowMajorQualifiedArray;
    const GLenum glType =
        type.block != nullptr
            ? EbtStruct
            : GLVariableType(TType(type.type, type.primarySize, type.secondarySize));

    return ToShaderVariable(type.block, glType, type.arraySizes, isRowMajor);
}

// The following function encodes a variable in a std140 or std430 block.  The variable could be:
//
// - An interface block: In this case, |decorationsBlob| is provided and SPIR-V decorations are
//   output to this blob.
// - A struct: In this case, the return value is of interest as the size of the struct in the
//   encoding.
//
// This function ignores arrayness in calculating the struct size.
//
uint32_t Encode(const ShaderVariable &var,
                bool isStd140,
                spirv::IdRef blockTypeId,
                spirv::Blob *decorationsBlob)
{
    Std140BlockEncoder std140;
    Std430BlockEncoder std430;
    BlockLayoutEncoder *encoder = isStd140 ? &std140 : &std430;

    ASSERT(var.isStruct());
    encoder->enterAggregateType(var);

    uint32_t fieldIndex = 0;

    for (const ShaderVariable &fieldVar : var.fields)
    {
        BlockMemberInfo fieldInfo;

        // Encode the variable.
        if (fieldVar.isStruct())
        {
            // For structs, recursively encode it.
            const uint32_t structSize = Encode(fieldVar, isStd140, {}, nullptr);

            encoder->enterAggregateType(fieldVar);
            fieldInfo = encoder->encodeArrayOfPreEncodedStructs(structSize, fieldVar.arraySizes);
            encoder->exitAggregateType(fieldVar);
        }
        else
        {
            fieldInfo =
                encoder->encodeType(fieldVar.type, fieldVar.arraySizes, fieldVar.isRowMajorLayout);
        }

        if (decorationsBlob)
        {
            ASSERT(blockTypeId.valid());

            // Write the Offset decoration.
            spirv::WriteMemberDecorate(decorationsBlob, blockTypeId,
                                       spirv::LiteralInteger(fieldIndex), spv::DecorationOffset,
                                       {spirv::LiteralInteger(fieldInfo.offset)});

            // For matrix types, write the MatrixStride decoration as well.
            if (IsMatrixGLType(fieldVar.type))
            {
                ASSERT(fieldInfo.matrixStride > 0);

                // MatrixStride
                spirv::WriteMemberDecorate(
                    decorationsBlob, blockTypeId, spirv::LiteralInteger(fieldIndex),
                    spv::DecorationMatrixStride, {spirv::LiteralInteger(fieldInfo.matrixStride)});
            }
        }

        ++fieldIndex;
    }

    encoder->exitAggregateType(var);
    return static_cast<uint32_t>(encoder->getCurrentOffset());
}

uint32_t GetArrayStrideInBlock(const ShaderVariable &var, bool isStd140)
{
    Std140BlockEncoder std140;
    Std430BlockEncoder std430;
    BlockLayoutEncoder *encoder = isStd140 ? &std140 : &std430;

    ASSERT(var.isArray());

    // For structs, encode the struct to get the size, and calculate the stride based on that.
    if (var.isStruct())
    {
        // Remove arrayness.
        ShaderVariable element = var;
        element.arraySizes.clear();

        const uint32_t structSize = Encode(element, isStd140, {}, nullptr);

        // Stride is struct size by inner array size
        return structSize * var.getInnerArraySizeProduct();
    }

    // Otherwise encode the basic type.
    BlockMemberInfo memberInfo =
        encoder->encodeType(var.type, var.arraySizes, var.isRowMajorLayout);

    // The encoder returns the array stride for the base element type (which is not an array!), so
    // need to multiply by the inner array sizes to get the outermost array's stride.
    return memberInfo.arrayStride * var.getInnerArraySizeProduct();
}

spv::ExecutionMode GetGeometryInputExecutionMode(TLayoutPrimitiveType primitiveType)
{
    // Default input primitive type for geometry shaders is points
    if (primitiveType == EptUndefined)
    {
        primitiveType = EptPoints;
    }

    switch (primitiveType)
    {
        case EptPoints:
            return spv::ExecutionModeInputPoints;
        case EptLines:
            return spv::ExecutionModeInputLines;
        case EptLinesAdjacency:
            return spv::ExecutionModeInputLinesAdjacency;
        case EptTriangles:
            return spv::ExecutionModeTriangles;
        case EptTrianglesAdjacency:
            return spv::ExecutionModeInputTrianglesAdjacency;
        case EptLineStrip:
        case EptTriangleStrip:
        default:
            UNREACHABLE();
            return {};
    }
}

spv::ExecutionMode GetGeometryOutputExecutionMode(TLayoutPrimitiveType primitiveType)
{
    // Default output primitive type for geometry shaders is points
    if (primitiveType == EptUndefined)
    {
        primitiveType = EptPoints;
    }

    switch (primitiveType)
    {
        case EptPoints:
            return spv::ExecutionModeOutputPoints;
        case EptLineStrip:
            return spv::ExecutionModeOutputLineStrip;
        case EptTriangleStrip:
            return spv::ExecutionModeOutputTriangleStrip;
        case EptLines:
        case EptLinesAdjacency:
        case EptTriangles:
        case EptTrianglesAdjacency:
        default:
            UNREACHABLE();
            return {};
    }
}

spv::ExecutionMode GetTessEvalInputExecutionMode(TLayoutTessEvaluationType inputType)
{
    // It's invalid for input type to not be specified, but that's a link-time error.  Default to
    // anything.
    if (inputType == EtetUndefined)
    {
        inputType = EtetTriangles;
    }

    switch (inputType)
    {
        case EtetTriangles:
            return spv::ExecutionModeTriangles;
        case EtetQuads:
            return spv::ExecutionModeQuads;
        case EtetIsolines:
            return spv::ExecutionModeIsolines;
        default:
            UNREACHABLE();
            return {};
    }
}

spv::ExecutionMode GetTessEvalSpacingExecutionMode(TLayoutTessEvaluationType spacing)
{
    switch (spacing)
    {
        case EtetEqualSpacing:
        case EtetUndefined:
            return spv::ExecutionModeSpacingEqual;
        case EtetFractionalEvenSpacing:
            return spv::ExecutionModeSpacingFractionalEven;
        case EtetFractionalOddSpacing:
            return spv::ExecutionModeSpacingFractionalOdd;
        default:
            UNREACHABLE();
            return {};
    }
}

spv::ExecutionMode GetTessEvalOrderingExecutionMode(TLayoutTessEvaluationType ordering)
{
    switch (ordering)
    {
        case EtetCw:
            return spv::ExecutionModeVertexOrderCw;
        case EtetCcw:
        case EtetUndefined:
            return spv::ExecutionModeVertexOrderCcw;
        default:
            UNREACHABLE();
            return {};
    }
}
}  // anonymous namespace

void SpirvTypeSpec::inferDefaults(const TType &type, TCompiler *compiler)
{
    // Infer some defaults based on type.  If necessary, this overrides some fields (if not already
    // specified).  Otherwise, it leaves the pre-initialized values as-is.

    // Handle interface blocks and fields of nameless interface blocks.
    if (type.getInterfaceBlock() != nullptr)
    {
        // Calculate the block storage from the interface block automatically.  The fields inherit
        // from this.  Only blocks and arrays in blocks produce different SPIR-V types based on
        // block storage.
        const bool isBlock = type.isInterfaceBlock() || type.getStruct();
        if (blockStorage == EbsUnspecified && (isBlock || type.isArray()))
        {
            blockStorage = GetBlockStorage(type);
        }

        // row_major can only be specified on an interface block or one of its fields.  The fields
        // will inherit this from the interface block itself.
        if (!isRowMajorQualifiedBlock && isBlock)
        {
            isRowMajorQualifiedBlock = type.getLayoutQualifier().matrixPacking == EmpRowMajor;
        }

        // Arrays of matrices in a uniform/buffer block may generate a different stride based on
        // whether they are row- or column-major.  Square matrices are trivially known not to
        // generate a different type.
        if (!isRowMajorQualifiedArray)
        {
            isRowMajorQualifiedArray = IsNonSquareRowMajorArrayInBlock(type, *this);
        }

        // Structs with bools, bool arrays, bool vectors and bools themselves are replaced with uint
        // when used in an interface block.
        if (!isOrHasBoolInInterfaceBlock)
        {
            isOrHasBoolInInterfaceBlock = type.isInterfaceBlockContainingType(EbtBool) ||
                                          type.isStructureContainingType(EbtBool) ||
                                          type.getBasicType() == EbtBool;
        }

        if (!isPatchIOBlock && type.isInterfaceBlock())
        {
            isPatchIOBlock =
                type.getQualifier() == EvqPatchIn || type.getQualifier() == EvqPatchOut;
        }
    }

    // |invariant| is significant for structs as the fields of the type are decorated with Invariant
    // in SPIR-V.  This is possible for outputs of struct type, or struct-typed fields of an
    // interface block.
    if (type.getStruct() != nullptr)
    {
        isInvariantBlock = isInvariantBlock || IsInvariant(type, compiler);
    }
}

void SpirvTypeSpec::onArrayElementSelection(bool isElementTypeBlock, bool isElementTypeArray)
{
    // No difference in type for non-block non-array types in std140 and std430 block storage.
    if (!isElementTypeBlock && !isElementTypeArray)
    {
        blockStorage = EbsUnspecified;
    }

    // No difference in type for non-array types in std140 and std430 block storage.
    if (!isElementTypeArray)
    {
        isRowMajorQualifiedArray = false;
    }
}

void SpirvTypeSpec::onBlockFieldSelection(const TType &fieldType)
{
    // Patch is never recursively applied.
    isPatchIOBlock = false;

    if (fieldType.getStruct() == nullptr)
    {
        // If the field is not a block, no difference if the parent block was invariant or
        // row-major.
        isRowMajorQualifiedArray = IsNonSquareRowMajorArrayInBlock(fieldType, *this);
        isInvariantBlock         = false;
        isRowMajorQualifiedBlock = false;

        // If the field is not an array, no difference in storage block.
        if (!fieldType.isArray())
        {
            blockStorage = EbsUnspecified;
        }

        if (fieldType.getBasicType() != EbtBool)
        {
            isOrHasBoolInInterfaceBlock = false;
        }
    }
    else
    {
        // Apply row-major only to structs that contain matrices.
        isRowMajorQualifiedBlock =
            IsBlockFieldRowMajorQualified(fieldType, isRowMajorQualifiedBlock) &&
            fieldType.isStructureContainingMatrices();

        // Structs without bools aren't affected by |isOrHasBoolInInterfaceBlock|.
        if (isOrHasBoolInInterfaceBlock)
        {
            isOrHasBoolInInterfaceBlock = fieldType.isStructureContainingType(EbtBool);
        }
    }
}

void SpirvTypeSpec::onMatrixColumnSelection()
{
    // The matrix types are never differentiated, so neither would be their columns.
    ASSERT(!isInvariantBlock && !isRowMajorQualifiedBlock && !isRowMajorQualifiedArray &&
           !isOrHasBoolInInterfaceBlock && blockStorage == EbsUnspecified);
}

void SpirvTypeSpec::onVectorComponentSelection()
{
    // The vector types are never differentiated, so neither would be their components.  The only
    // exception is bools in interface blocks, in which case the component and the vector are
    // similarly differentiated.
    ASSERT(!isInvariantBlock && !isRowMajorQualifiedBlock && !isRowMajorQualifiedArray &&
           blockStorage == EbsUnspecified);
}

SPIRVBuilder::SPIRVBuilder(TCompiler *compiler,
                           const ShCompileOptions &compileOptions,
                           ShHashFunction64 hashFunction,
                           NameMap &nameMap)
    : mCompiler(compiler),
      mCompileOptions(compileOptions),
      mShaderType(gl::FromGLenum<gl::ShaderType>(compiler->getShaderType())),
      mNextAvailableId(1),
      mHashFunction(hashFunction),
      mNameMap(nameMap),
      mNextUnusedBinding(0),
      mNextUnusedInputLocation(0),
      mNextUnusedOutputLocation(0)
{
    // The Shader capability is always defined.
    addCapability(spv::CapabilityShader);

    // Add Geometry or Tessellation capabilities based on shader type.
    if (mCompiler->getShaderType() == GL_GEOMETRY_SHADER)
    {
        addCapability(spv::CapabilityGeometry);
    }
    else if (mCompiler->getShaderType() == GL_TESS_CONTROL_SHADER_EXT ||
             mCompiler->getShaderType() == GL_TESS_EVALUATION_SHADER_EXT)
    {
        addCapability(spv::CapabilityTessellation);
    }

    mExtInstImportIdStd = getNewId({});
}

spirv::IdRef SPIRVBuilder::getNewId(const SpirvDecorations &decorations)
{
    spirv::IdRef newId = mNextAvailableId;
    mNextAvailableId   = spirv::IdRef(mNextAvailableId + 1);

    for (const spv::Decoration decoration : decorations)
    {
        spirv::WriteDecorate(&mSpirvDecorations, newId, decoration, {});
    }

    return newId;
}

SpirvType SPIRVBuilder::getSpirvType(const TType &type, const SpirvTypeSpec &typeSpec) const
{
    SpirvType spirvType;
    spirvType.type                = type.getBasicType();
    spirvType.primarySize         = type.getNominalSize();
    spirvType.secondarySize       = type.getSecondarySize();
    spirvType.arraySizes          = type.getArraySizes();
    spirvType.imageInternalFormat = type.getLayoutQualifier().imageInternalFormat;

    switch (spirvType.type)
    {
        // External textures are treated as 2D textures in the vulkan back-end.
        case EbtSamplerExternalOES:
        case EbtSamplerExternal2DY2YEXT:
        // WEBGL video textures too.
        case EbtSamplerVideoWEBGL:
            spirvType.type = EbtSampler2D;
            break;
        // yuvCscStandardEXT is just a uint under the hood.
        case EbtYuvCscStandardEXT:
            spirvType.type = EbtUInt;
            break;
        default:
            break;
    }

    if (type.getStruct() != nullptr)
    {
        spirvType.block = type.getStruct();
    }
    else if (type.isInterfaceBlock())
    {
        spirvType.block = type.getInterfaceBlock();
    }

    // Automatically inherit or infer the type-specializing properties.
    spirvType.typeSpec = typeSpec;
    spirvType.typeSpec.inferDefaults(type, mCompiler);

    return spirvType;
}

const SpirvTypeData &SPIRVBuilder::getTypeData(const TType &type, const SpirvTypeSpec &typeSpec)
{
    SpirvType spirvType = getSpirvType(type, typeSpec);

    const TSymbol *block = nullptr;
    if (type.getStruct() != nullptr)
    {
        block = type.getStruct();
    }
    else if (type.isInterfaceBlock())
    {
        block = type.getInterfaceBlock();
    }

    return getSpirvTypeData(spirvType, block);
}

const SpirvTypeData &SPIRVBuilder::getTypeDataOverrideTypeSpec(const TType &type,
                                                               const SpirvTypeSpec &typeSpec)
{
    // This is a variant of getTypeData() where type spec is not automatically derived.  It's useful
    // in cast operations that specifically need to override the spec.
    SpirvType spirvType = getSpirvType(type, typeSpec);
    spirvType.typeSpec  = typeSpec;

    return getSpirvTypeData(spirvType, nullptr);
}

const SpirvTypeData &SPIRVBuilder::getSpirvTypeData(const SpirvType &type, const TSymbol *block)
{
    // Structs with bools generate a different type when used in an interface block (where the bool
    // is replaced with a uint).  The bool, bool vector and bool arrays too generate a different
    // type when nested in an interface block, but that type is the same as the equivalent uint
    // type.  To avoid defining duplicate uint types, we switch the basic type here to uint.  From
    // the outside, therefore bool in an interface block and uint look like different types, but
    // under the hood will be the same uint.
    if (type.block == nullptr && type.typeSpec.isOrHasBoolInInterfaceBlock)
    {
        ASSERT(type.type == EbtBool);

        SpirvType uintType                            = type;
        uintType.type                                 = EbtUInt;
        uintType.typeSpec.isOrHasBoolInInterfaceBlock = false;
        return getSpirvTypeData(uintType, block);
    }

    auto iter = mTypeMap.find(type);
    if (iter == mTypeMap.end())
    {
        SpirvTypeData newTypeData = declareType(type, block);

        iter = mTypeMap.insert({type, newTypeData}).first;
    }

    return iter->second;
}

spirv::IdRef SPIRVBuilder::getBasicTypeId(TBasicType basicType, size_t size)
{
    SpirvType type;
    type.type        = basicType;
    type.primarySize = static_cast<uint8_t>(size);
    return getSpirvTypeData(type, nullptr).id;
}

spirv::IdRef SPIRVBuilder::getTypePointerId(spirv::IdRef typeId, spv::StorageClass storageClass)
{
    SpirvIdAndStorageClass key{typeId, storageClass};

    auto iter = mTypePointerIdMap.find(key);
    if (iter == mTypePointerIdMap.end())
    {
        const spirv::IdRef typePointerId = getNewId({});

        spirv::WriteTypePointer(&mSpirvTypePointerDecls, typePointerId, storageClass, typeId);

        iter = mTypePointerIdMap.insert({key, typePointerId}).first;
    }

    return iter->second;
}

spirv::IdRef SPIRVBuilder::getFunctionTypeId(spirv::IdRef returnTypeId,
                                             const spirv::IdRefList &paramTypeIds)
{
    SpirvIdAndIdList key{returnTypeId, paramTypeIds};

    auto iter = mFunctionTypeIdMap.find(key);
    if (iter == mFunctionTypeIdMap.end())
    {
        const spirv::IdRef functionTypeId = getNewId({});

        spirv::WriteTypeFunction(&mSpirvFunctionTypeDecls, functionTypeId, returnTypeId,
                                 paramTypeIds);

        iter = mFunctionTypeIdMap.insert({key, functionTypeId}).first;
    }

    return iter->second;
}

SpirvDecorations SPIRVBuilder::getDecorations(const TType &type)
{
    const bool enablePrecision = !mCompileOptions.ignorePrecisionQualifiers;
    const TPrecision precision = type.getPrecision();

    SpirvDecorations decorations;

    // Handle precision.
    if (enablePrecision && (precision == EbpMedium || precision == EbpLow))
    {
        decorations.push_back(spv::DecorationRelaxedPrecision);
    }

    return decorations;
}

SpirvDecorations SPIRVBuilder::getArithmeticDecorations(const TType &type,
                                                        bool isPrecise,
                                                        TOperator op)
{
    SpirvDecorations decorations = getDecorations(type);

    // In GLSL, findMsb operates on a highp operand, while returning a lowp result.  In SPIR-V,
    // RelaxedPrecision cannot be applied on the Find*Msb instructions as that affects the operand
    // as well:
    //
    // > The RelaxedPrecision Decoration can be applied to:
    // > ...
    // > The Result <id> of an instruction that operates on numerical types, meaning the instruction
    // > is to operate at relaxed precision. The instruction's operands may also be truncated to the
    // > relaxed precision.
    // > ...
    //
    // findLSB() and bitCount() are in a similar situation.  Here, we remove RelaxedPrecision from
    // such problematic instructions.
    switch (op)
    {
        case EOpFindMSB:
        case EOpFindLSB:
        case EOpBitCount:
            // Currently getDecorations() only adds RelaxedPrecision, so removing the
            // RelaxedPrecision decoration is simply done by clearing the vector.
            ASSERT(decorations.empty() ||
                   (decorations.size() == 1 && decorations[0] == spv::DecorationRelaxedPrecision));
            decorations.clear();
            break;
        default:
            break;
    }

    // Handle |precise|.
    if (isPrecise)
    {
        decorations.push_back(spv::DecorationNoContraction);
    }

    return decorations;
}

spirv::IdRef SPIRVBuilder::getExtInstImportIdStd()
{
    ASSERT(mExtInstImportIdStd.valid());
    return mExtInstImportIdStd;
}

SpirvTypeData SPIRVBuilder::declareType(const SpirvType &type, const TSymbol *block)
{
    // Recursively declare the type.  Type id is allocated afterwards purely for better id order in
    // output.
    spirv::IdRef typeId;

    if (!type.arraySizes.empty())
    {
        // Declaring an array.  First, declare the type without the outermost array size, then
        // declare a new array type based on that.

        SpirvType subType  = type;
        subType.arraySizes = type.arraySizes.first(type.arraySizes.size() - 1);
        subType.typeSpec.onArrayElementSelection(subType.block != nullptr,
                                                 !subType.arraySizes.empty());

        const spirv::IdRef subTypeId = getSpirvTypeData(subType, block).id;

        const unsigned int length = type.arraySizes.back();

        if (length == 0)
        {
            // Storage buffers may include a dynamically-sized array, which is identified by it
            // having a length of 0.
            typeId = getNewId({});
            spirv::WriteTypeRuntimeArray(&mSpirvTypeAndConstantDecls, typeId, subTypeId);
        }
        else
        {
            const spirv::IdRef lengthId = getUintConstant(length);
            typeId                      = getNewId({});
            spirv::WriteTypeArray(&mSpirvTypeAndConstantDecls, typeId, subTypeId, lengthId);
        }
    }
    else if (type.block != nullptr)
    {
        // Declaring a block.  First, declare all the fields, then declare a struct based on the
        // list of field types.

        spirv::IdRefList fieldTypeIds;
        for (const TField *field : type.block->fields())
        {
            const TType &fieldType = *field->type();

            SpirvTypeSpec fieldTypeSpec = type.typeSpec;
            fieldTypeSpec.onBlockFieldSelection(fieldType);

            const SpirvType fieldSpirvType = getSpirvType(fieldType, fieldTypeSpec);
            const spirv::IdRef fieldTypeId =
                getSpirvTypeData(fieldSpirvType, fieldType.getStruct()).id;
            fieldTypeIds.push_back(fieldTypeId);
        }

        typeId = getNewId({});
        spirv::WriteTypeStruct(&mSpirvTypeAndConstantDecls, typeId, fieldTypeIds);
    }
    else if (IsSampler(type.type) && !type.isSamplerBaseImage)
    {
        // Declaring a sampler.  First, declare the non-sampled image and then a combined
        // image-sampler.

        SpirvType imageType          = type;
        imageType.isSamplerBaseImage = true;

        const spirv::IdRef nonSampledId = getSpirvTypeData(imageType, nullptr).id;

        typeId = getNewId({});
        spirv::WriteTypeSampledImage(&mSpirvTypeAndConstantDecls, typeId, nonSampledId);
    }
    else if (IsImage(type.type) || IsSubpassInputType(type.type) || type.isSamplerBaseImage)
    {
        // Declaring an image.

        spirv::IdRef sampledType;
        spv::Dim dim;
        spirv::LiteralInteger depth;
        spirv::LiteralInteger arrayed;
        spirv::LiteralInteger multisampled;
        spirv::LiteralInteger sampled;

        getImageTypeParameters(type.type, &sampledType, &dim, &depth, &arrayed, &multisampled,
                               &sampled);
        const spv::ImageFormat imageFormat = getImageFormat(type.imageInternalFormat);

        typeId = getNewId({});
        spirv::WriteTypeImage(&mSpirvTypeAndConstantDecls, typeId, sampledType, dim, depth, arrayed,
                              multisampled, sampled, imageFormat, nullptr);
    }
    else if (type.secondarySize > 1)
    {
        // Declaring a matrix.  Declare the column type first, then create a matrix out of it.

        SpirvType columnType     = type;
        columnType.primarySize   = columnType.secondarySize;
        columnType.secondarySize = 1;
        columnType.typeSpec.onMatrixColumnSelection();

        const spirv::IdRef columnTypeId = getSpirvTypeData(columnType, nullptr).id;

        typeId = getNewId({});
        spirv::WriteTypeMatrix(&mSpirvTypeAndConstantDecls, typeId, columnTypeId,
                               spirv::LiteralInteger(type.primarySize));
    }
    else if (type.primarySize > 1)
    {
        // Declaring a vector.  Declare the component type first, then create a vector out of it.

        SpirvType componentType   = type;
        componentType.primarySize = 1;
        componentType.typeSpec.onVectorComponentSelection();

        const spirv::IdRef componentTypeId = getSpirvTypeData(componentType, nullptr).id;

        typeId = getNewId({});
        spirv::WriteTypeVector(&mSpirvTypeAndConstantDecls, typeId, componentTypeId,
                               spirv::LiteralInteger(type.primarySize));
    }
    else
    {
        typeId = getNewId({});

        // Declaring a basic type.  There's a different instruction for each.
        switch (type.type)
        {
            case EbtVoid:
                spirv::WriteTypeVoid(&mSpirvTypeAndConstantDecls, typeId);
                break;
            case EbtFloat:
                spirv::WriteTypeFloat(&mSpirvTypeAndConstantDecls, typeId,
                                      spirv::LiteralInteger(32));
                break;
            case EbtDouble:
                // TODO: support desktop GLSL.  http://anglebug.com/6197
                UNIMPLEMENTED();
                break;
            case EbtInt:
                spirv::WriteTypeInt(&mSpirvTypeAndConstantDecls, typeId, spirv::LiteralInteger(32),
                                    spirv::LiteralInteger(1));
                break;
            case EbtUInt:
                spirv::WriteTypeInt(&mSpirvTypeAndConstantDecls, typeId, spirv::LiteralInteger(32),
                                    spirv::LiteralInteger(0));
                break;
            case EbtBool:
                spirv::WriteTypeBool(&mSpirvTypeAndConstantDecls, typeId);
                break;
            default:
                UNREACHABLE();
        }
    }

    // If this was a block declaration, add debug information for its type and field names.
    //
    // TODO: make this conditional to a compiler flag.  Instead of outputting the debug info
    // unconditionally and having the SPIR-V transformer remove them, it's better to avoid
    // generating them in the first place.  This both simplifies the transformer and reduces SPIR-V
    // binary size that gets written to disk cache.  http://anglebug.com/4889
    if (block != nullptr && type.arraySizes.empty())
    {
        spirv::WriteName(&mSpirvDebug, typeId, hashName(block).data());

        uint32_t fieldIndex = 0;
        for (const TField *field : type.block->fields())
        {
            spirv::WriteMemberName(&mSpirvDebug, typeId, spirv::LiteralInteger(fieldIndex++),
                                   hashFieldName(field).data());
        }
    }

    // Write decorations for interface block fields.
    if (type.typeSpec.blockStorage != EbsUnspecified)
    {
        // Cannot have opaque uniforms inside interface blocks.
        ASSERT(!IsOpaqueType(type.type));

        const bool isInterfaceBlock = block != nullptr && block->isInterfaceBlock();
        const bool isStd140         = type.typeSpec.blockStorage != EbsStd430;

        if (!type.arraySizes.empty() && !isInterfaceBlock)
        {
            // Write the ArrayStride decoration for arrays inside interface blocks.  An array of
            // interface blocks doesn't need a stride.
            const ShaderVariable var = SpirvTypeToShaderVariable(type);
            const uint32_t stride    = GetArrayStrideInBlock(var, isStd140);

            spirv::WriteDecorate(&mSpirvDecorations, typeId, spv::DecorationArrayStride,
                                 {spirv::LiteralInteger(stride)});
        }
        else if (type.arraySizes.empty() && type.block != nullptr)
        {
            // Write the Offset decoration for interface blocks and structs in them.
            const ShaderVariable var = SpirvTypeToShaderVariable(type);
            Encode(var, isStd140, typeId, &mSpirvDecorations);
        }
    }

    // Write other member decorations.
    if (block != nullptr && type.arraySizes.empty())
    {
        writeMemberDecorations(type, typeId);
    }

    return {typeId};
}

void SPIRVBuilder::getImageTypeParameters(TBasicType type,
                                          spirv::IdRef *sampledTypeOut,
                                          spv::Dim *dimOut,
                                          spirv::LiteralInteger *depthOut,
                                          spirv::LiteralInteger *arrayedOut,
                                          spirv::LiteralInteger *multisampledOut,
                                          spirv::LiteralInteger *sampledOut)
{
    TBasicType sampledType = EbtFloat;
    *dimOut                = IsSubpassInputType(type) ? spv::DimSubpassData : spv::Dim2D;
    bool isDepth           = false;
    bool isArrayed         = false;
    bool isMultisampled    = false;

    // Decompose the basic type into image properties
    switch (type)
    {
        // Float 2D Images
        case EbtSampler2D:
        case EbtImage2D:
        case EbtSubpassInput:
            break;
        case EbtSamplerExternalOES:
        case EbtSamplerExternal2DY2YEXT:
        case EbtSamplerVideoWEBGL:
            // These must have already been converted to EbtSampler2D.
            UNREACHABLE();
            break;
        case EbtSampler2DArray:
        case EbtImage2DArray:
            isArrayed = true;
            break;
        case EbtSampler2DMS:
        case EbtImage2DMS:
        case EbtSubpassInputMS:
            isMultisampled = true;
            break;
        case EbtSampler2DMSArray:
        case EbtImage2DMSArray:
            isArrayed      = true;
            isMultisampled = true;
            break;
        case EbtSampler2DShadow:
            isDepth = true;
            break;
        case EbtSampler2DArrayShadow:
            isDepth   = true;
            isArrayed = true;
            break;

        // Integer 2D images
        case EbtISampler2D:
        case EbtIImage2D:
        case EbtISubpassInput:
            sampledType = EbtInt;
            break;
        case EbtISampler2DArray:
        case EbtIImage2DArray:
            sampledType = EbtInt;
            isArrayed   = true;
            break;
        case EbtISampler2DMS:
        case EbtIImage2DMS:
        case EbtISubpassInputMS:
            sampledType    = EbtInt;
            isMultisampled = true;
            break;
        case EbtISampler2DMSArray:
        case EbtIImage2DMSArray:
            sampledType    = EbtInt;
            isArrayed      = true;
            isMultisampled = true;
            break;

        // Unsinged integer 2D images
        case EbtUSampler2D:
        case EbtUImage2D:
        case EbtUSubpassInput:
            sampledType = EbtUInt;
            break;
        case EbtUSampler2DArray:
        case EbtUImage2DArray:
            sampledType = EbtUInt;
            isArrayed   = true;
            break;
        case EbtUSampler2DMS:
        case EbtUImage2DMS:
        case EbtUSubpassInputMS:
            sampledType    = EbtUInt;
            isMultisampled = true;
            break;
        case EbtUSampler2DMSArray:
        case EbtUImage2DMSArray:
            sampledType    = EbtUInt;
            isArrayed      = true;
            isMultisampled = true;
            break;

        // 3D images
        case EbtSampler3D:
        case EbtImage3D:
            *dimOut = spv::Dim3D;
            break;
        case EbtISampler3D:
        case EbtIImage3D:
            sampledType = EbtInt;
            *dimOut     = spv::Dim3D;
            break;
        case EbtUSampler3D:
        case EbtUImage3D:
            sampledType = EbtUInt;
            *dimOut     = spv::Dim3D;
            break;

        // Float cube images
        case EbtSamplerCube:
        case EbtImageCube:
            *dimOut = spv::DimCube;
            break;
        case EbtSamplerCubeArray:
        case EbtImageCubeArray:
            *dimOut   = spv::DimCube;
            isArrayed = true;
            break;
        case EbtSamplerCubeArrayShadow:
            *dimOut   = spv::DimCube;
            isDepth   = true;
            isArrayed = true;
            break;
        case EbtSamplerCubeShadow:
            *dimOut = spv::DimCube;
            isDepth = true;
            break;

        // Integer cube images
        case EbtISamplerCube:
        case EbtIImageCube:
            sampledType = EbtInt;
            *dimOut     = spv::DimCube;
            break;
        case EbtISamplerCubeArray:
        case EbtIImageCubeArray:
            sampledType = EbtInt;
            *dimOut     = spv::DimCube;
            isArrayed   = true;
            break;

        // Unsigned integer cube images
        case EbtUSamplerCube:
        case EbtUImageCube:
            sampledType = EbtUInt;
            *dimOut     = spv::DimCube;
            break;
        case EbtUSamplerCubeArray:
        case EbtUImageCubeArray:
            sampledType = EbtUInt;
            *dimOut     = spv::DimCube;
            isArrayed   = true;
            break;

        // Float 1D images
        case EbtSampler1D:
        case EbtImage1D:
            *dimOut = spv::Dim1D;
            break;
        case EbtSampler1DArray:
        case EbtImage1DArray:
            *dimOut   = spv::Dim1D;
            isArrayed = true;
            break;
        case EbtSampler1DShadow:
            *dimOut = spv::Dim1D;
            isDepth = true;
            break;
        case EbtSampler1DArrayShadow:
            *dimOut   = spv::Dim1D;
            isDepth   = true;
            isArrayed = true;
            break;

        // Integer 1D images
        case EbtISampler1D:
        case EbtIImage1D:
            sampledType = EbtInt;
            *dimOut     = spv::Dim1D;
            break;
        case EbtISampler1DArray:
        case EbtIImage1DArray:
            sampledType = EbtInt;
            *dimOut     = spv::Dim1D;
            isArrayed   = true;
            break;

        // Unsigned integer 1D images
        case EbtUSampler1D:
        case EbtUImage1D:
            sampledType = EbtUInt;
            *dimOut     = spv::Dim1D;
            break;
        case EbtUSampler1DArray:
        case EbtUImage1DArray:
            sampledType = EbtUInt;
            *dimOut     = spv::Dim1D;
            isArrayed   = true;
            break;

        // Rect images
        case EbtSampler2DRect:
        case EbtImageRect:
            *dimOut = spv::DimRect;
            break;
        case EbtSampler2DRectShadow:
            *dimOut = spv::DimRect;
            isDepth = true;
            break;
        case EbtISampler2DRect:
        case EbtIImageRect:
            sampledType = EbtInt;
            *dimOut     = spv::DimRect;
            break;
        case EbtUSampler2DRect:
        case EbtUImageRect:
            sampledType = EbtUInt;
            *dimOut     = spv::DimRect;
            break;

        // Image buffers
        case EbtSamplerBuffer:
        case EbtImageBuffer:
            *dimOut = spv::DimBuffer;
            break;
        case EbtISamplerBuffer:
        case EbtIImageBuffer:
            sampledType = EbtInt;
            *dimOut     = spv::DimBuffer;
            break;
        case EbtUSamplerBuffer:
        case EbtUImageBuffer:
            sampledType = EbtUInt;
            *dimOut     = spv::DimBuffer;
            break;
        default:
            UNREACHABLE();
    }

    // Get id of the component type of the image
    SpirvType sampledSpirvType;
    sampledSpirvType.type = sampledType;

    *sampledTypeOut = getSpirvTypeData(sampledSpirvType, nullptr).id;

    const bool isSampledImage = IsSampler(type);

    // Set flags based on SPIR-V required values.  See OpTypeImage:
    //
    // - For depth:        0 = non-depth,      1 = depth
    // - For arrayed:      0 = non-arrayed,    1 = arrayed
    // - For multisampled: 0 = single-sampled, 1 = multisampled
    // - For sampled:      1 = sampled,        2 = storage
    //
    *depthOut        = spirv::LiteralInteger(isDepth ? 1 : 0);
    *arrayedOut      = spirv::LiteralInteger(isArrayed ? 1 : 0);
    *multisampledOut = spirv::LiteralInteger(isMultisampled ? 1 : 0);
    *sampledOut      = spirv::LiteralInteger(isSampledImage ? 1 : 2);

    // Add the necessary capability based on parameters.  The SPIR-V spec section 3.8 Dim specfies
    // the required capabilities:
    //
    //     Dim          Sampled         Storage            Storage Array
    //     --------------------------------------------------------------
    //     1D           Sampled1D       Image1D
    //     2D           Shader                             ImageMSArray
    //     3D
    //     Cube         Shader                             ImageCubeArray
    //     Rect         SampledRect     ImageRect
    //     Buffer       SampledBuffer   ImageBuffer
    //
    // Additionally, the SubpassData Dim requires the InputAttachment capability.
    //
    // Note that the Shader capability is always unconditionally added.
    //
    switch (*dimOut)
    {
        case spv::Dim1D:
            addCapability(isSampledImage ? spv::CapabilitySampled1D : spv::CapabilityImage1D);
            break;
        case spv::Dim2D:
            if (!isSampledImage && isArrayed && isMultisampled)
            {
                addCapability(spv::CapabilityImageMSArray);
            }
            break;
        case spv::Dim3D:
            break;
        case spv::DimCube:
            if (!isSampledImage && isArrayed)
            {
                addCapability(spv::CapabilityImageCubeArray);
            }
            break;
        case spv::DimRect:
            addCapability(isSampledImage ? spv::CapabilitySampledRect : spv::CapabilityImageRect);
            break;
        case spv::DimBuffer:
            addCapability(isSampledImage ? spv::CapabilitySampledBuffer
                                         : spv::CapabilityImageBuffer);
            break;
        case spv::DimSubpassData:
            addCapability(spv::CapabilityInputAttachment);
            break;
        default:
            UNREACHABLE();
    }
}

spv::ImageFormat SPIRVBuilder::getImageFormat(TLayoutImageInternalFormat imageInternalFormat)
{
    switch (imageInternalFormat)
    {
        case EiifUnspecified:
            return spv::ImageFormatUnknown;
        case EiifRGBA32F:
            return spv::ImageFormatRgba32f;
        case EiifRGBA16F:
            return spv::ImageFormatRgba16f;
        case EiifR32F:
            return spv::ImageFormatR32f;
        case EiifRGBA32UI:
            return spv::ImageFormatRgba32ui;
        case EiifRGBA16UI:
            return spv::ImageFormatRgba16ui;
        case EiifRGBA8UI:
            return spv::ImageFormatRgba8ui;
        case EiifR32UI:
            return spv::ImageFormatR32ui;
        case EiifRGBA32I:
            return spv::ImageFormatRgba32i;
        case EiifRGBA16I:
            return spv::ImageFormatRgba16i;
        case EiifRGBA8I:
            return spv::ImageFormatRgba8i;
        case EiifR32I:
            return spv::ImageFormatR32i;
        case EiifRGBA8:
            return spv::ImageFormatRgba8;
        case EiifRGBA8_SNORM:
            return spv::ImageFormatRgba8Snorm;
        default:
            UNREACHABLE();
            return spv::ImageFormatUnknown;
    }
}

spirv::IdRef SPIRVBuilder::getBoolConstant(bool value)
{
    uint32_t asInt = static_cast<uint32_t>(value);

    spirv::IdRef constantId = mBoolConstants[asInt];

    if (!constantId.valid())
    {
        SpirvType boolType;
        boolType.type = EbtBool;

        const spirv::IdRef boolTypeId = getSpirvTypeData(boolType, nullptr).id;

        mBoolConstants[asInt] = constantId = getNewId({});
        if (value)
        {
            spirv::WriteConstantTrue(&mSpirvTypeAndConstantDecls, boolTypeId, constantId);
        }
        else
        {
            spirv::WriteConstantFalse(&mSpirvTypeAndConstantDecls, boolTypeId, constantId);
        }
    }

    return constantId;
}

spirv::IdRef SPIRVBuilder::getBasicConstantHelper(uint32_t value,
                                                  TBasicType type,
                                                  angle::HashMap<uint32_t, spirv::IdRef> *constants)
{
    auto iter = constants->find(value);
    if (iter != constants->end())
    {
        return iter->second;
    }

    SpirvType spirvType;
    spirvType.type = type;

    const spirv::IdRef typeId     = getSpirvTypeData(spirvType, nullptr).id;
    const spirv::IdRef constantId = getNewId({});

    spirv::WriteConstant(&mSpirvTypeAndConstantDecls, typeId, constantId,
                         spirv::LiteralContextDependentNumber(value));

    return constants->insert({value, constantId}).first->second;
}

spirv::IdRef SPIRVBuilder::getUintConstant(uint32_t value)
{
    return getBasicConstantHelper(value, EbtUInt, &mUintConstants);
}

spirv::IdRef SPIRVBuilder::getIntConstant(int32_t value)
{
    uint32_t asUint = static_cast<uint32_t>(value);
    return getBasicConstantHelper(asUint, EbtInt, &mIntConstants);
}

spirv::IdRef SPIRVBuilder::getFloatConstant(float value)
{
    union
    {
        float f;
        uint32_t u;
    } asUint;
    asUint.f = value;
    return getBasicConstantHelper(asUint.u, EbtFloat, &mFloatConstants);
}

spirv::IdRef SPIRVBuilder::getNullConstant(spirv::IdRef typeId)
{
    if (typeId >= mNullConstants.size())
    {
        mNullConstants.resize(typeId + 1);
    }

    if (!mNullConstants[typeId].valid())
    {
        const spirv::IdRef constantId = getNewId({});
        mNullConstants[typeId]        = constantId;

        spirv::WriteConstantNull(&mSpirvTypeAndConstantDecls, typeId, constantId);
    }

    return mNullConstants[typeId];
}

spirv::IdRef SPIRVBuilder::getNullVectorConstantHelper(TBasicType type, int size)
{
    SpirvType vecType;
    vecType.type        = type;
    vecType.primarySize = static_cast<uint8_t>(size);

    return getNullConstant(getSpirvTypeData(vecType, nullptr).id);
}

spirv::IdRef SPIRVBuilder::getVectorConstantHelper(spirv::IdRef valueId, TBasicType type, int size)
{
    if (size == 1)
    {
        return valueId;
    }

    SpirvType vecType;
    vecType.type        = type;
    vecType.primarySize = static_cast<uint8_t>(size);

    const spirv::IdRef typeId = getSpirvTypeData(vecType, nullptr).id;
    const spirv::IdRefList valueIds(size, valueId);

    return getCompositeConstant(typeId, valueIds);
}

spirv::IdRef SPIRVBuilder::getUvecConstant(uint32_t value, int size)
{
    if (value == 0)
    {
        return getNullVectorConstantHelper(EbtUInt, size);
    }

    const spirv::IdRef valueId = getUintConstant(value);
    return getVectorConstantHelper(valueId, EbtUInt, size);
}

spirv::IdRef SPIRVBuilder::getIvecConstant(int32_t value, int size)
{
    if (value == 0)
    {
        return getNullVectorConstantHelper(EbtInt, size);
    }

    const spirv::IdRef valueId = getIntConstant(value);
    return getVectorConstantHelper(valueId, EbtInt, size);
}

spirv::IdRef SPIRVBuilder::getVecConstant(float value, int size)
{
    if (value == 0)
    {
        return getNullVectorConstantHelper(EbtFloat, size);
    }

    const spirv::IdRef valueId = getFloatConstant(value);
    return getVectorConstantHelper(valueId, EbtFloat, size);
}

spirv::IdRef SPIRVBuilder::getCompositeConstant(spirv::IdRef typeId, const spirv::IdRefList &values)
{
    SpirvIdAndIdList key{typeId, values};

    auto iter = mCompositeConstants.find(key);
    if (iter == mCompositeConstants.end())
    {
        const spirv::IdRef constantId = getNewId({});

        spirv::WriteConstantComposite(&mSpirvTypeAndConstantDecls, typeId, constantId, values);

        iter = mCompositeConstants.insert({key, constantId}).first;
    }

    return iter->second;
}

void SPIRVBuilder::startNewFunction(spirv::IdRef functionId, const TFunction *func)
{
    ASSERT(mSpirvCurrentFunctionBlocks.empty());

    // Add the first block of the function.
    mSpirvCurrentFunctionBlocks.emplace_back();
    mSpirvCurrentFunctionBlocks.back().labelId = getNewId({});

    // Output debug information.
    spirv::WriteName(&mSpirvDebug, functionId, hashFunctionName(func).data());
}

void SPIRVBuilder::assembleSpirvFunctionBlocks()
{
    // Take all the blocks and place them in the functions section of SPIR-V in sequence.
    for (const SpirvBlock &block : mSpirvCurrentFunctionBlocks)
    {
        // Every block must be properly terminated.
        ASSERT(block.isTerminated);

        // Generate the OpLabel instruction for the block.
        spirv::WriteLabel(&mSpirvFunctions, block.labelId);

        // Add the variable declarations if any.
        mSpirvFunctions.insert(mSpirvFunctions.end(), block.localVariables.begin(),
                               block.localVariables.end());

        // Add the body of the block.
        mSpirvFunctions.insert(mSpirvFunctions.end(), block.body.begin(), block.body.end());
    }

    // Clean up.
    mSpirvCurrentFunctionBlocks.clear();
}

spirv::IdRef SPIRVBuilder::declareVariable(spirv::IdRef typeId,
                                           spv::StorageClass storageClass,
                                           const SpirvDecorations &decorations,
                                           spirv::IdRef *initializerId,
                                           const char *name)
{
    const bool isFunctionLocal = storageClass == spv::StorageClassFunction;

    // Make sure storage class is consistent with where the variable is declared.
    ASSERT(!isFunctionLocal || !mSpirvCurrentFunctionBlocks.empty());

    // Function-local variables go in the first block of the function, while the rest are in the
    // global variables section.
    spirv::Blob *spirvSection = isFunctionLocal
                                    ? &mSpirvCurrentFunctionBlocks.front().localVariables
                                    : &mSpirvVariableDecls;

    const spirv::IdRef typePointerId = getTypePointerId(typeId, storageClass);
    const spirv::IdRef variableId    = getNewId(decorations);

    spirv::WriteVariable(spirvSection, typePointerId, variableId, storageClass, initializerId);

    // Output debug information.
    if (name)
    {
        spirv::WriteName(&mSpirvDebug, variableId, name);
    }

    return variableId;
}

spirv::IdRef SPIRVBuilder::declareSpecConst(TBasicType type, int id, const char *name)
{
    SpirvType spirvType;
    spirvType.type = type;

    const spirv::IdRef typeId      = getSpirvTypeData(spirvType, nullptr).id;
    const spirv::IdRef specConstId = getNewId({});

    // Note: all spec constants are 0 initialized by the translator.
    if (type == EbtBool)
    {
        spirv::WriteSpecConstantFalse(&mSpirvTypeAndConstantDecls, typeId, specConstId);
    }
    else
    {
        spirv::WriteSpecConstant(&mSpirvTypeAndConstantDecls, typeId, specConstId,
                                 spirv::LiteralContextDependentNumber(0));
    }

    // Add the SpecId decoration
    spirv::WriteDecorate(&mSpirvDecorations, specConstId, spv::DecorationSpecId,
                         {spirv::LiteralInteger(id)});

    // Output debug information.
    if (name)
    {
        spirv::WriteName(&mSpirvDebug, specConstId, name);
    }

    return specConstId;
}

void SPIRVBuilder::startConditional(size_t blockCount, bool isContinuable, bool isBreakable)
{
    mConditionalStack.emplace_back();
    SpirvConditional &conditional = mConditionalStack.back();

    // Create the requested number of block ids.
    conditional.blockIds.resize(blockCount);
    for (spirv::IdRef &blockId : conditional.blockIds)
    {
        blockId = getNewId({});
    }

    conditional.isContinuable = isContinuable;
    conditional.isBreakable   = isBreakable;

    // Don't automatically start the next block.  The caller needs to generate instructions based on
    // the ids that were just generated above.
}

void SPIRVBuilder::nextConditionalBlock()
{
    ASSERT(!mConditionalStack.empty());
    SpirvConditional &conditional = mConditionalStack.back();

    ASSERT(conditional.nextBlockToWrite < conditional.blockIds.size());
    const spirv::IdRef blockId = conditional.blockIds[conditional.nextBlockToWrite++];

    // The previous block must have properly terminated.
    ASSERT(isCurrentFunctionBlockTerminated());

    // Generate a new block.
    mSpirvCurrentFunctionBlocks.emplace_back();
    mSpirvCurrentFunctionBlocks.back().labelId = blockId;
}

void SPIRVBuilder::endConditional()
{
    ASSERT(!mConditionalStack.empty());

    // No blocks should be left.
    ASSERT(mConditionalStack.back().nextBlockToWrite == mConditionalStack.back().blockIds.size());

    mConditionalStack.pop_back();
}

bool SPIRVBuilder::isInLoop() const
{
    for (const SpirvConditional &conditional : mConditionalStack)
    {
        if (conditional.isContinuable)
        {
            return true;
        }
    }

    return false;
}

spirv::IdRef SPIRVBuilder::getBreakTargetId() const
{
    for (size_t index = mConditionalStack.size(); index > 0; --index)
    {
        const SpirvConditional &conditional = mConditionalStack[index - 1];

        if (conditional.isBreakable)
        {
            // The target of break; is always the merge block, and the merge block is always the
            // last block.
            return conditional.blockIds.back();
        }
    }

    UNREACHABLE();
    return spirv::IdRef{};
}

spirv::IdRef SPIRVBuilder::getContinueTargetId() const
{
    for (size_t index = mConditionalStack.size(); index > 0; --index)
    {
        const SpirvConditional &conditional = mConditionalStack[index - 1];

        if (conditional.isContinuable)
        {
            // The target of continue; is always the block before merge, so it's the one before
            // last.
            ASSERT(conditional.blockIds.size() > 2);
            return conditional.blockIds[conditional.blockIds.size() - 2];
        }
    }

    UNREACHABLE();
    return spirv::IdRef{};
}

uint32_t SPIRVBuilder::nextUnusedBinding()
{
    return mNextUnusedBinding++;
}

uint32_t SPIRVBuilder::nextUnusedInputLocation(uint32_t consumedCount)
{
    uint32_t nextUnused = mNextUnusedInputLocation;
    mNextUnusedInputLocation += consumedCount;
    return nextUnused;
}

uint32_t SPIRVBuilder::nextUnusedOutputLocation(uint32_t consumedCount)
{
    uint32_t nextUnused = mNextUnusedOutputLocation;
    mNextUnusedOutputLocation += consumedCount;
    return nextUnused;
}

bool SPIRVBuilder::isInvariantOutput(const TType &type) const
{
    return IsInvariant(type, mCompiler);
}

void SPIRVBuilder::addCapability(spv::Capability capability)
{
    mCapabilities.insert(capability);
}

void SPIRVBuilder::addExecutionMode(spv::ExecutionMode executionMode)
{
    mExecutionModes.insert(executionMode);
}

void SPIRVBuilder::addExtension(SPIRVExtensions extension)
{
    mExtensions.set(extension);
}

void SPIRVBuilder::setEntryPointId(spirv::IdRef id)
{
    ASSERT(!mEntryPointId.valid());
    mEntryPointId = id;
}

void SPIRVBuilder::addEntryPointInterfaceVariableId(spirv::IdRef id)
{
    mEntryPointInterfaceList.push_back(id);
}

void SPIRVBuilder::writePerVertexBuiltIns(const TType &type, spirv::IdRef typeId)
{
    ASSERT(type.isInterfaceBlock());
    const TInterfaceBlock *block = type.getInterfaceBlock();

    uint32_t fieldIndex = 0;
    for (const TField *field : block->fields())
    {
        spv::BuiltIn decorationValue = spv::BuiltInPosition;
        switch (field->type()->getQualifier())
        {
            case EvqPosition:
                decorationValue = spv::BuiltInPosition;
                break;
            case EvqPointSize:
                decorationValue = spv::BuiltInPointSize;
                break;
            case EvqClipDistance:
                decorationValue = spv::BuiltInClipDistance;
                break;
            case EvqCullDistance:
                decorationValue = spv::BuiltInCullDistance;
                break;
            default:
                UNREACHABLE();
        }

        spirv::WriteMemberDecorate(&mSpirvDecorations, typeId, spirv::LiteralInteger(fieldIndex++),
                                   spv::DecorationBuiltIn,
                                   {spirv::LiteralInteger(decorationValue)});
    }
}

void SPIRVBuilder::writeInterfaceVariableDecorations(const TType &type, spirv::IdRef variableId)
{
    const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();
    const bool isVarying                    = IsVarying(type.getQualifier());
    const bool needsSetBinding =
        !layoutQualifier.pushConstant &&
        (IsSampler(type.getBasicType()) ||
         (type.isInterfaceBlock() &&
          (type.getQualifier() == EvqUniform || type.getQualifier() == EvqBuffer)) ||
         IsImage(type.getBasicType()) || IsSubpassInputType(type.getBasicType()));
    const bool needsLocation = type.getQualifier() == EvqAttribute ||
                               type.getQualifier() == EvqVertexIn ||
                               type.getQualifier() == EvqFragmentOut || isVarying;
    const bool needsInputAttachmentIndex = IsSubpassInputType(type.getBasicType());
    const bool needsBlendIndex =
        type.getQualifier() == EvqFragmentOut && layoutQualifier.index >= 0;
    const bool needsYuvDecorate = mCompileOptions.addVulkanYUVLayoutQualifier &&
                                  type.getQualifier() == EvqFragmentOut && layoutQualifier.yuv;

    // If the resource declaration requires set & binding, add the DescriptorSet and Binding
    // decorations.
    if (needsSetBinding)
    {
        spirv::WriteDecorate(&mSpirvDecorations, variableId, spv::DecorationDescriptorSet,
                             {spirv::LiteralInteger(0)});
        spirv::WriteDecorate(&mSpirvDecorations, variableId, spv::DecorationBinding,
                             {spirv::LiteralInteger(nextUnusedBinding())});
    }

    if (needsLocation)
    {
        const unsigned int locationCount =
            CalculateVaryingLocationCount(type, gl::ToGLenum(mShaderType));
        const uint32_t location = IsShaderIn(type.getQualifier())
                                      ? nextUnusedInputLocation(locationCount)
                                      : nextUnusedOutputLocation(locationCount);

        spirv::WriteDecorate(&mSpirvDecorations, variableId, spv::DecorationLocation,
                             {spirv::LiteralInteger(location)});
    }

    // If the resource declaration is an input attachment, add the InputAttachmentIndex decoration.
    if (needsInputAttachmentIndex)
    {
        spirv::WriteDecorate(&mSpirvDecorations, variableId, spv::DecorationInputAttachmentIndex,
                             {spirv::LiteralInteger(layoutQualifier.inputAttachmentIndex)});
    }

    if (needsBlendIndex)
    {
        spirv::WriteDecorate(&mSpirvDecorations, variableId, spv::DecorationIndex,
                             {spirv::LiteralInteger(layoutQualifier.index)});
    }

    if (needsYuvDecorate)
    {
        // WIP in spec
        const spv::Decoration yuvDecorate = static_cast<spv::Decoration>(6088);
        spirv::WriteDecorate(&mSpirvDecorations, variableId, yuvDecorate,
                             {spirv::LiteralInteger(layoutQualifier.index)});
    }

    // Handle interpolation and auxiliary decorations on varyings
    if (isVarying)
    {
        writeInterpolationDecoration(type.getQualifier(), variableId,
                                     std::numeric_limits<uint32_t>::max());
    }
}

void SPIRVBuilder::writeBranchConditional(spirv::IdRef conditionValue,
                                          spirv::IdRef trueBlock,
                                          spirv::IdRef falseBlock,
                                          spirv::IdRef mergeBlock)
{
    // Generate the following:
    //
    //     OpSelectionMerge %mergeBlock None
    //     OpBranchConditional %conditionValue %trueBlock %falseBlock
    //
    spirv::WriteSelectionMerge(getSpirvCurrentFunctionBlock(), mergeBlock,
                               spv::SelectionControlMaskNone);
    spirv::WriteBranchConditional(getSpirvCurrentFunctionBlock(), conditionValue, trueBlock,
                                  falseBlock, {});
    terminateCurrentFunctionBlock();

    // Start the true or false block, whichever exists.
    nextConditionalBlock();
}

void SPIRVBuilder::writeBranchConditionalBlockEnd()
{
    if (!isCurrentFunctionBlockTerminated())
    {
        // Insert a branch to the merge block at the end of each if-else block, unless the block is
        // already terminated, such as with a return or discard.
        const spirv::IdRef mergeBlock = getCurrentConditional()->blockIds.back();

        spirv::WriteBranch(getSpirvCurrentFunctionBlock(), mergeBlock);
        terminateCurrentFunctionBlock();
    }

    // Move on to the next block.
    nextConditionalBlock();
}

void SPIRVBuilder::writeLoopHeader(spirv::IdRef branchToBlock,
                                   spirv::IdRef continueBlock,
                                   spirv::IdRef mergeBlock)
{
    // First, jump to the header block:
    //
    //     OpBranch %header
    //
    const spirv::IdRef headerBlock = mConditionalStack.back().blockIds[0];
    spirv::WriteBranch(getSpirvCurrentFunctionBlock(), headerBlock);
    terminateCurrentFunctionBlock();

    // Start the header block.
    nextConditionalBlock();

    // Generate the following:
    //
    //     OpLoopMerge %mergeBlock %continueBlock None
    //     OpBranch %branchToBlock (%cond or if do-while, %body)
    //
    spirv::WriteLoopMerge(getSpirvCurrentFunctionBlock(), mergeBlock, continueBlock,
                          spv::LoopControlMaskNone);
    spirv::WriteBranch(getSpirvCurrentFunctionBlock(), branchToBlock);
    terminateCurrentFunctionBlock();

    // Start the next block, which is either %cond or %body.
    nextConditionalBlock();
}

void SPIRVBuilder::writeLoopConditionEnd(spirv::IdRef conditionValue,
                                         spirv::IdRef branchToBlock,
                                         spirv::IdRef mergeBlock)
{
    // Generate the following:
    //
    //     OpBranchConditional %conditionValue %branchToBlock %mergeBlock
    //
    // %branchToBlock is either %body or if do-while, %header
    //
    spirv::WriteBranchConditional(getSpirvCurrentFunctionBlock(), conditionValue, branchToBlock,
                                  mergeBlock, {});
    terminateCurrentFunctionBlock();

    // Start the next block, which is either %continue or %body.
    nextConditionalBlock();
}

void SPIRVBuilder::writeLoopContinueEnd(spirv::IdRef headerBlock)
{
    // Generate the following:
    //
    //     OpBranch %headerBlock
    //
    spirv::WriteBranch(getSpirvCurrentFunctionBlock(), headerBlock);
    terminateCurrentFunctionBlock();

    // Start the next block, which is %body.
    nextConditionalBlock();
}

void SPIRVBuilder::writeLoopBodyEnd(spirv::IdRef continueBlock)
{
    // Generate the following:
    //
    //     OpBranch %continueBlock
    //
    // This is only done if the block isn't already terminated in another way, such as with an
    // unconditional continue/etc at the end of the loop.
    if (!isCurrentFunctionBlockTerminated())
    {
        spirv::WriteBranch(getSpirvCurrentFunctionBlock(), continueBlock);
        terminateCurrentFunctionBlock();
    }

    // Start the next block, which is %merge or if while, %continue.
    nextConditionalBlock();
}

void SPIRVBuilder::writeSwitch(spirv::IdRef conditionValue,
                               spirv::IdRef defaultBlock,
                               const spirv::PairLiteralIntegerIdRefList &targetPairList,
                               spirv::IdRef mergeBlock)
{
    // Generate the following:
    //
    //     OpSelectionMerge %mergeBlock None
    //     OpSwitch %conditionValue %defaultBlock A %ABlock B %BBlock ...
    //
    spirv::WriteSelectionMerge(getSpirvCurrentFunctionBlock(), mergeBlock,
                               spv::SelectionControlMaskNone);
    spirv::WriteSwitch(getSpirvCurrentFunctionBlock(), conditionValue, defaultBlock,
                       targetPairList);
    terminateCurrentFunctionBlock();

    // Start the next case block.
    nextConditionalBlock();
}

void SPIRVBuilder::writeSwitchCaseBlockEnd()
{
    if (!isCurrentFunctionBlockTerminated())
    {
        // If a case does not end in branch, insert a branch to the next block, implementing
        // fallthrough.  For the last block, the branch target would automatically be the merge
        // block.
        const SpirvConditional *conditional = getCurrentConditional();
        const spirv::IdRef nextBlock        = conditional->blockIds[conditional->nextBlockToWrite];

        spirv::WriteBranch(getSpirvCurrentFunctionBlock(), nextBlock);
        terminateCurrentFunctionBlock();
    }

    // Move on to the next block.
    nextConditionalBlock();
}

void SPIRVBuilder::writeMemberDecorations(const SpirvType &type, spirv::IdRef typeId)
{
    ASSERT(type.block != nullptr);

    uint32_t fieldIndex = 0;

    for (const TField *field : type.block->fields())
    {
        const TType &fieldType = *field->type();

        // Add invariant decoration if any.
        if (type.typeSpec.isInvariantBlock || fieldType.isInvariant())
        {
            spirv::WriteMemberDecorate(&mSpirvDecorations, typeId,
                                       spirv::LiteralInteger(fieldIndex), spv::DecorationInvariant,
                                       {});
        }

        // Add matrix decorations if any.
        if (fieldType.isMatrix())
        {
            // ColMajor or RowMajor
            const bool isRowMajor =
                IsBlockFieldRowMajorQualified(fieldType, type.typeSpec.isRowMajorQualifiedBlock);
            spirv::WriteMemberDecorate(
                &mSpirvDecorations, typeId, spirv::LiteralInteger(fieldIndex),
                isRowMajor ? spv::DecorationRowMajor : spv::DecorationColMajor, {});
        }

        // Add interpolation and auxiliary decorations
        writeInterpolationDecoration(fieldType.getQualifier(), typeId, fieldIndex);

        // Add patch decoration if any.
        if (type.typeSpec.isPatchIOBlock)
        {
            spirv::WriteMemberDecorate(&mSpirvDecorations, typeId,
                                       spirv::LiteralInteger(fieldIndex), spv::DecorationPatch, {});
        }

        // Add other decorations.
        SpirvDecorations decorations = getDecorations(fieldType);
        for (const spv::Decoration decoration : decorations)
        {
            spirv::WriteMemberDecorate(&mSpirvDecorations, typeId,
                                       spirv::LiteralInteger(fieldIndex), decoration, {});
        }

        ++fieldIndex;
    }
}

void SPIRVBuilder::writeInterpolationDecoration(TQualifier qualifier,
                                                spirv::IdRef id,
                                                uint32_t fieldIndex)
{
    spv::Decoration decoration = spv::DecorationMax;

    switch (qualifier)
    {
        case EvqSmooth:
        case EvqSmoothOut:
        case EvqSmoothIn:
            // No decoration in SPIR-V for smooth, this is the default interpolation.
            return;

        case EvqFlat:
        case EvqFlatOut:
        case EvqFlatIn:
            decoration = spv::DecorationFlat;
            break;

        case EvqNoPerspective:
        case EvqNoPerspectiveOut:
        case EvqNoPerspectiveIn:
            decoration = spv::DecorationNoPerspective;
            break;

        case EvqCentroid:
        case EvqCentroidOut:
        case EvqCentroidIn:
            decoration = spv::DecorationCentroid;
            break;

        case EvqSample:
        case EvqSampleOut:
        case EvqSampleIn:
            decoration = spv::DecorationSample;
            addCapability(spv::CapabilitySampleRateShading);
            break;

        default:
            return;
    }

    if (fieldIndex != std::numeric_limits<uint32_t>::max())
    {
        spirv::WriteMemberDecorate(&mSpirvDecorations, id, spirv::LiteralInteger(fieldIndex),
                                   decoration, {});
    }
    else
    {
        spirv::WriteDecorate(&mSpirvDecorations, id, decoration, {});
    }
}

ImmutableString SPIRVBuilder::hashName(const TSymbol *symbol)
{
    return HashName(symbol, mHashFunction, &mNameMap);
}

ImmutableString SPIRVBuilder::hashTypeName(const TType &type)
{
    return GetTypeName(type, mHashFunction, &mNameMap);
}

ImmutableString SPIRVBuilder::hashFieldName(const TField *field)
{
    ASSERT(field->symbolType() != SymbolType::Empty);
    if (field->symbolType() == SymbolType::UserDefined)
    {
        return HashName(field->name(), mHashFunction, &mNameMap);
    }

    return field->name();
}

ImmutableString SPIRVBuilder::hashFunctionName(const TFunction *func)
{
    if (func->isMain())
    {
        return func->name();
    }

    return hashName(func);
}

spirv::Blob SPIRVBuilder::getSpirv()
{
    ASSERT(mConditionalStack.empty());

    spirv::Blob result;

    // Reserve a minimum amount of memory.
    //
    //   5 for header +
    //   a number of capabilities +
    //   size of already generated instructions.
    //
    // The actual size is larger due to other metadata instructions such as extensions,
    // OpExtInstImport, OpEntryPoint, OpExecutionMode etc.
    result.reserve(5 + mCapabilities.size() * 2 + mSpirvDebug.size() + mSpirvDecorations.size() +
                   mSpirvTypeAndConstantDecls.size() + mSpirvTypePointerDecls.size() +
                   mSpirvFunctionTypeDecls.size() + mSpirvVariableDecls.size() +
                   mSpirvFunctions.size());

    // Generate the SPIR-V header.
    spirv::WriteSpirvHeader(&result, mNextAvailableId);

    // Generate metadata in the following order:
    //
    // - OpCapability instructions.
    for (spv::Capability capability : mCapabilities)
    {
        spirv::WriteCapability(&result, capability);
    }

    // - OpExtension instructions
    writeExtensions(&result);

    // - OpExtInstImport
    spirv::WriteExtInstImport(&result, getExtInstImportIdStd(), "GLSL.std.450");

    // - OpMemoryModel
    spirv::WriteMemoryModel(&result, spv::AddressingModelLogical, spv::MemoryModelGLSL450);

    // - OpEntryPoint
    constexpr gl::ShaderMap<spv::ExecutionModel> kExecutionModels = {
        {gl::ShaderType::Vertex, spv::ExecutionModelVertex},
        {gl::ShaderType::TessControl, spv::ExecutionModelTessellationControl},
        {gl::ShaderType::TessEvaluation, spv::ExecutionModelTessellationEvaluation},
        {gl::ShaderType::Geometry, spv::ExecutionModelGeometry},
        {gl::ShaderType::Fragment, spv::ExecutionModelFragment},
        {gl::ShaderType::Compute, spv::ExecutionModelGLCompute},
    };
    spirv::WriteEntryPoint(&result, kExecutionModels[mShaderType], mEntryPointId, "main",
                           mEntryPointInterfaceList);

    // - OpExecutionMode instructions
    writeExecutionModes(&result);

    // - OpSource and OpSourceExtension instructions.
    //
    // This is to support debuggers and capture/replay tools and isn't strictly necessary.
    spirv::WriteSource(&result, spv::SourceLanguageGLSL, spirv::LiteralInteger(450), nullptr,
                       nullptr);
    writeSourceExtensions(&result);

    // Append the already generated sections in order
    result.insert(result.end(), mSpirvDebug.begin(), mSpirvDebug.end());
    result.insert(result.end(), mSpirvDecorations.begin(), mSpirvDecorations.end());
    result.insert(result.end(), mSpirvTypeAndConstantDecls.begin(),
                  mSpirvTypeAndConstantDecls.end());
    result.insert(result.end(), mSpirvTypePointerDecls.begin(), mSpirvTypePointerDecls.end());
    result.insert(result.end(), mSpirvFunctionTypeDecls.begin(), mSpirvFunctionTypeDecls.end());
    result.insert(result.end(), mSpirvVariableDecls.begin(), mSpirvVariableDecls.end());
    result.insert(result.end(), mSpirvFunctions.begin(), mSpirvFunctions.end());

    result.shrink_to_fit();
    return result;
}

void SPIRVBuilder::writeExecutionModes(spirv::Blob *blob)
{
    switch (mShaderType)
    {
        case gl::ShaderType::Fragment:
            spirv::WriteExecutionMode(blob, mEntryPointId, spv::ExecutionModeOriginUpperLeft, {});

            if (mCompiler->isEarlyFragmentTestsSpecified())
            {
                spirv::WriteExecutionMode(blob, mEntryPointId, spv::ExecutionModeEarlyFragmentTests,
                                          {});
            }

            break;

        case gl::ShaderType::TessControl:
            spirv::WriteExecutionMode(
                blob, mEntryPointId, spv::ExecutionModeOutputVertices,
                {spirv::LiteralInteger(mCompiler->getTessControlShaderOutputVertices())});
            break;

        case gl::ShaderType::TessEvaluation:
        {
            const spv::ExecutionMode inputExecutionMode = GetTessEvalInputExecutionMode(
                mCompiler->getTessEvaluationShaderInputPrimitiveType());
            const spv::ExecutionMode spacingExecutionMode = GetTessEvalSpacingExecutionMode(
                mCompiler->getTessEvaluationShaderInputVertexSpacingType());
            const spv::ExecutionMode orderingExecutionMode = GetTessEvalOrderingExecutionMode(
                mCompiler->getTessEvaluationShaderInputOrderingType());

            spirv::WriteExecutionMode(blob, mEntryPointId, inputExecutionMode, {});
            spirv::WriteExecutionMode(blob, mEntryPointId, spacingExecutionMode, {});
            spirv::WriteExecutionMode(blob, mEntryPointId, orderingExecutionMode, {});
            if (mCompiler->getTessEvaluationShaderInputPointType() == EtetPointMode)
            {
                spirv::WriteExecutionMode(blob, mEntryPointId, spv::ExecutionModePointMode, {});
            }
            break;
        }

        case gl::ShaderType::Geometry:
        {
            const spv::ExecutionMode inputExecutionMode =
                GetGeometryInputExecutionMode(mCompiler->getGeometryShaderInputPrimitiveType());
            const spv::ExecutionMode outputExecutionMode =
                GetGeometryOutputExecutionMode(mCompiler->getGeometryShaderOutputPrimitiveType());

            // max_vertices=0 is not valid in Vulkan
            const int maxVertices = std::max(1, mCompiler->getGeometryShaderMaxVertices());

            spirv::WriteExecutionMode(blob, mEntryPointId, inputExecutionMode, {});
            spirv::WriteExecutionMode(blob, mEntryPointId, outputExecutionMode, {});
            spirv::WriteExecutionMode(blob, mEntryPointId, spv::ExecutionModeOutputVertices,
                                      {spirv::LiteralInteger(maxVertices)});
            spirv::WriteExecutionMode(
                blob, mEntryPointId, spv::ExecutionModeInvocations,
                {spirv::LiteralInteger(mCompiler->getGeometryShaderInvocations())});

            break;
        }

        case gl::ShaderType::Compute:
        {
            const sh::WorkGroupSize &localSize = mCompiler->getComputeShaderLocalSize();
            spirv::WriteExecutionMode(
                blob, mEntryPointId, spv::ExecutionModeLocalSize,
                {spirv::LiteralInteger(localSize[0]), spirv::LiteralInteger(localSize[1]),
                 spirv::LiteralInteger(localSize[2])});
            break;
        }

        default:
            break;
    }

    // Add any execution modes that were added due to built-ins used in the shader.
    for (spv::ExecutionMode executionMode : mExecutionModes)
    {
        spirv::WriteExecutionMode(blob, mEntryPointId, executionMode, {});
    }
}

void SPIRVBuilder::writeExtensions(spirv::Blob *blob)
{
    for (SPIRVExtensions extension : mExtensions)
    {
        switch (extension)
        {
            case SPIRVExtensions::MultiviewOVR:
                spirv::WriteExtension(blob, "SPV_KHR_multiview");
                break;
            case SPIRVExtensions::FragmentShaderInterlockARB:
                spirv::WriteExtension(blob, "SPV_EXT_fragment_shader_interlock");
                break;
            default:
                UNREACHABLE();
        }
    }
}

void SPIRVBuilder::writeSourceExtensions(spirv::Blob *blob)
{
    for (SPIRVExtensions extension : mExtensions)
    {
        switch (extension)
        {
            case SPIRVExtensions::MultiviewOVR:
                spirv::WriteSourceExtension(blob, "GL_OVR_multiview");
                break;
            case SPIRVExtensions::FragmentShaderInterlockARB:
                spirv::WriteSourceExtension(blob, "GL_ARB_fragment_shader_interlock");
                break;
            default:
                UNREACHABLE();
        }
    }
}

}  // namespace sh
