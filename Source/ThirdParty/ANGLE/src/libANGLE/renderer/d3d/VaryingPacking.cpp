//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VaryingPacking:
//   Class which describes a mapping from varyings to registers in D3D
//   for linking between shader stages.
//

#include "libANGLE/renderer/d3d/VaryingPacking.h"

#include "common/utilities.h"
#include "compiler/translator/blocklayoutHLSL.h"
#include "libANGLE/renderer/d3d/DynamicHLSL.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"

namespace rx
{

// Implementation of VaryingPacking::BuiltinVarying
VaryingPacking::BuiltinVarying::BuiltinVarying() : enabled(false), index(0), systemValue(false)
{
}

std::string VaryingPacking::BuiltinVarying::str() const
{
    return (systemValue ? semantic : (semantic + Str(index)));
}

void VaryingPacking::BuiltinVarying::enableSystem(const std::string &systemValueSemantic)
{
    enabled     = true;
    semantic    = systemValueSemantic;
    systemValue = true;
}

void VaryingPacking::BuiltinVarying::enable(const std::string &semanticVal, unsigned int indexVal)
{
    enabled  = true;
    semantic = semanticVal;
    index    = indexVal;
}

// Implementation of VaryingPacking
VaryingPacking::VaryingPacking(GLuint maxVaryingVectors)
    : mRegisterMap(maxVaryingVectors), mBuiltinInfo(SHADER_TYPE_MAX)
{
}

// Packs varyings into generic varying registers, using the algorithm from
// See [OpenGL ES Shading Language 1.00 rev. 17] appendix A section 7 page 111
// Also [OpenGL ES Shading Language 3.00 rev. 4] Section 11 page 119
// Returns false if unsuccessful.
bool VaryingPacking::packVarying(const PackedVarying &packedVarying)
{
    unsigned int varyingRows    = 0;
    unsigned int varyingColumns = 0;

    const auto &varying = *packedVarying.varying;

    // "Non - square matrices of type matCxR consume the same space as a square matrix of type matN
    // where N is the greater of C and R.Variables of type mat2 occupies 2 complete rows."
    // Here we are a bit more conservative and allow packing non-square matrices more tightly.
    // Make sure we use transposed matrix types to count registers correctly.
    ASSERT(!varying.isStruct());
    GLenum transposedType = gl::TransposeMatrixType(varying.type);
    varyingRows           = gl::VariableRowCount(transposedType);
    varyingColumns        = gl::VariableColumnCount(transposedType);

    // "Arrays of size N are assumed to take N times the size of the base type"
    varyingRows *= varying.elementCount();

    unsigned int maxVaryingVectors = static_cast<unsigned int>(mRegisterMap.size());

    // "For 2, 3 and 4 component variables packing is started using the 1st column of the 1st row.
    // Variables are then allocated to successive rows, aligning them to the 1st column."
    if (varyingColumns >= 2 && varyingColumns <= 4)
    {
        for (unsigned int row = 0; row <= maxVaryingVectors - varyingRows; ++row)
        {
            if (isFree(row, 0, varyingRows, varyingColumns))
            {
                insert(row, 0, packedVarying);
                return true;
            }
        }

        // "For 2 component variables, when there are no spare rows, the strategy is switched to
        // using the highest numbered row and the lowest numbered column where the variable will
        // fit."
        if (varyingColumns == 2)
        {
            for (unsigned int r = maxVaryingVectors - varyingRows + 1; r-- >= 1;)
            {
                if (isFree(r, 2, varyingRows, 2))
                {
                    insert(r, 2, packedVarying);
                    return true;
                }
            }
        }

        return false;
    }

    // "1 component variables have their own packing rule. They are packed in order of size, largest
    // first. Each variable is placed in the column that leaves the least amount of space in the
    // column and aligned to the lowest available rows within that column."
    ASSERT(varyingColumns == 1);
    unsigned int contiguousSpace[4]     = {0};
    unsigned int bestContiguousSpace[4] = {0};
    unsigned int totalSpace[4]          = {0};

    for (unsigned int row = 0; row < maxVaryingVectors; ++row)
    {
        for (unsigned int column = 0; column < 4; ++column)
        {
            if (mRegisterMap[row][column])
            {
                contiguousSpace[column] = 0;
            }
            else
            {
                contiguousSpace[column]++;
                totalSpace[column]++;

                if (contiguousSpace[column] > bestContiguousSpace[column])
                {
                    bestContiguousSpace[column] = contiguousSpace[column];
                }
            }
        }
    }

    unsigned int bestColumn = 0;
    for (unsigned int column = 1; column < 4; ++column)
    {
        if (bestContiguousSpace[column] >= varyingRows &&
            (bestContiguousSpace[bestColumn] < varyingRows ||
             totalSpace[column] < totalSpace[bestColumn]))
        {
            bestColumn = column;
        }
    }

    if (bestContiguousSpace[bestColumn] >= varyingRows)
    {
        for (unsigned int row = 0; row < maxVaryingVectors; row++)
        {
            if (isFree(row, bestColumn, varyingRows, 1))
            {
                for (unsigned int arrayIndex = 0; arrayIndex < varyingRows; ++arrayIndex)
                {
                    // If varyingRows > 1, it must be an array.
                    PackedVaryingRegister registerInfo;
                    registerInfo.packedVarying     = &packedVarying;
                    registerInfo.registerRow       = row + arrayIndex;
                    registerInfo.registerColumn    = bestColumn;
                    registerInfo.varyingArrayIndex = arrayIndex;
                    registerInfo.varyingRowIndex = 0;
                    mRegisterList.push_back(registerInfo);
                    mRegisterMap[row + arrayIndex][bestColumn] = true;
                }
                break;
            }
        }
        return true;
    }

    return false;
}

bool VaryingPacking::isFree(unsigned int registerRow,
                            unsigned int registerColumn,
                            unsigned int varyingRows,
                            unsigned int varyingColumns) const
{
    for (unsigned int row = 0; row < varyingRows; ++row)
    {
        ASSERT(registerRow + row < mRegisterMap.size());
        for (unsigned int column = 0; column < varyingColumns; ++column)
        {
            ASSERT(registerColumn + column < 4);
            if (mRegisterMap[registerRow + row][registerColumn + column])
            {
                return false;
            }
        }
    }

    return true;
}

void VaryingPacking::insert(unsigned int registerRow,
                            unsigned int registerColumn,
                            const PackedVarying &packedVarying)
{
    unsigned int varyingRows    = 0;
    unsigned int varyingColumns = 0;

    const auto &varying = *packedVarying.varying;
    ASSERT(!varying.isStruct());
    GLenum transposedType = gl::TransposeMatrixType(varying.type);
    varyingRows           = gl::VariableRowCount(transposedType);
    varyingColumns        = gl::VariableColumnCount(transposedType);

    PackedVaryingRegister registerInfo;
    registerInfo.packedVarying  = &packedVarying;
    registerInfo.registerColumn = registerColumn;

    for (unsigned int arrayElement = 0; arrayElement < varying.elementCount(); ++arrayElement)
    {
        for (unsigned int varyingRow = 0; varyingRow < varyingRows; ++varyingRow)
        {
            registerInfo.registerRow       = registerRow + (arrayElement * varyingRows) + varyingRow;
            registerInfo.varyingRowIndex   = varyingRow;
            registerInfo.varyingArrayIndex = arrayElement;
            mRegisterList.push_back(registerInfo);

            for (unsigned int columnIndex = 0; columnIndex < varyingColumns; ++columnIndex)
            {
                mRegisterMap[registerInfo.registerRow][registerColumn + columnIndex] = true;
            }
        }
    }
}

// See comment on packVarying.
bool VaryingPacking::packVaryings(gl::InfoLog &infoLog,
                                  const std::vector<PackedVarying> &packedVaryings,
                                  const std::vector<std::string> &transformFeedbackVaryings)
{
    std::set<std::string> uniqueVaryingNames;

    // "Variables are packed into the registers one at a time so that they each occupy a contiguous
    // subrectangle. No splitting of variables is permitted."
    for (const PackedVarying &packedVarying : packedVaryings)
    {
        const auto &varying = *packedVarying.varying;

        // Do not assign registers to built-in or unreferenced varyings
        if (varying.isBuiltIn() || (!varying.staticUse && !packedVarying.isStructField()))
        {
            continue;
        }

        ASSERT(!varying.isStruct());
        ASSERT(uniqueVaryingNames.count(varying.name) == 0);

        if (packVarying(packedVarying))
        {
            uniqueVaryingNames.insert(varying.name);
        }
        else
        {
            infoLog << "Could not pack varying " << varying.name;
            return false;
        }
    }

    for (const std::string &transformFeedbackVaryingName : transformFeedbackVaryings)
    {
        if (transformFeedbackVaryingName.compare(0, 3, "gl_") == 0)
        {
            // do not pack builtin XFB varyings
            continue;
        }

        for (const PackedVarying &packedVarying : packedVaryings)
        {
            const auto &varying = *packedVarying.varying;

            // Make sure transform feedback varyings aren't optimized out.
            if (uniqueVaryingNames.count(transformFeedbackVaryingName) == 0)
            {
                bool found = false;
                if (transformFeedbackVaryingName == varying.name)
                {
                    if (!packVarying(packedVarying))
                    {
                        infoLog << "Could not pack varying " << varying.name;
                        return false;
                    }

                    found = true;
                    break;
                }
                if (!found)
                {
                    infoLog << "Transform feedback varying " << transformFeedbackVaryingName
                            << " does not exist in the vertex shader.";
                    return false;
                }
            }
        }
    }

    // Sort the packed register list
    std::sort(mRegisterList.begin(), mRegisterList.end());

    // Assign semantic indices
    for (unsigned int semanticIndex = 0;
         semanticIndex < static_cast<unsigned int>(mRegisterList.size()); ++semanticIndex)
    {
        mRegisterList[semanticIndex].semanticIndex = semanticIndex;
    }

    return true;
}

unsigned int VaryingPacking::getRegisterCount() const
{
    unsigned int count = 0;

    for (const Register &reg : mRegisterMap)
    {
        if (reg.data[0] || reg.data[1] || reg.data[2] || reg.data[3])
        {
            ++count;
        }
    }

    if (mBuiltinInfo[SHADER_PIXEL].glFragCoord.enabled)
    {
        ++count;
    }

    if (mBuiltinInfo[SHADER_PIXEL].glPointCoord.enabled)
    {
        ++count;
    }

    return count;
}

void VaryingPacking::enableBuiltins(ShaderType shaderType,
                                    const ProgramD3DMetadata &programMetadata)
{
    int majorShaderModel = programMetadata.getRendererMajorShaderModel();
    bool position        = programMetadata.usesTransformFeedbackGLPosition();
    bool fragCoord       = programMetadata.usesFragCoord();
    bool pointCoord = shaderType == SHADER_VERTEX ? programMetadata.addsPointCoordToVertexShader()
                                                  : programMetadata.usesPointCoord();
    bool pointSize                  = programMetadata.usesSystemValuePointSize();
    bool hlsl4                      = (majorShaderModel >= 4);
    const std::string &userSemantic = GetVaryingSemantic(majorShaderModel, pointSize);

    unsigned int reservedSemanticIndex = getMaxSemanticIndex();

    BuiltinInfo *builtins = &mBuiltinInfo[shaderType];

    if (hlsl4)
    {
        builtins->dxPosition.enableSystem("SV_Position");
    }
    else if (shaderType == SHADER_PIXEL)
    {
        builtins->dxPosition.enableSystem("VPOS");
    }
    else
    {
        builtins->dxPosition.enableSystem("POSITION");
    }

    if (position)
    {
        builtins->glPosition.enable(userSemantic, reservedSemanticIndex++);
    }

    if (fragCoord)
    {
        builtins->glFragCoord.enable(userSemantic, reservedSemanticIndex++);
    }

    if (pointCoord)
    {
        // SM3 reserves the TEXCOORD semantic for point sprite texcoords (gl_PointCoord)
        // In D3D11 we manually compute gl_PointCoord in the GS.
        if (hlsl4)
        {
            builtins->glPointCoord.enable(userSemantic, reservedSemanticIndex++);
        }
        else
        {
            builtins->glPointCoord.enable("TEXCOORD", 0);
        }
    }

    // Special case: do not include PSIZE semantic in HLSL 3 pixel shaders
    if (pointSize && (shaderType != SHADER_PIXEL || hlsl4))
    {
        builtins->glPointSize.enableSystem("PSIZE");
    }
}

}  // namespace rx
