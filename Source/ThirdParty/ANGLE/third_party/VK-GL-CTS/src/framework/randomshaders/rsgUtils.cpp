/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Utilities.
 *//*--------------------------------------------------------------------*/

#include "rsgUtils.hpp"

#include <set>
#include <string>

using std::set;
using std::string;
using std::vector;

namespace rsg
{

void addNewUniforms(vector<const ShaderInput *> &uniforms, set<string> &addedUniforms, const Shader &shader)
{
    const vector<ShaderInput *> &shaderUniforms = shader.getUniforms();
    for (vector<ShaderInput *>::const_iterator i = shaderUniforms.begin(); i != shaderUniforms.end(); i++)
    {
        const ShaderInput *uniform = *i;
        if (addedUniforms.find(uniform->getVariable()->getName()) == addedUniforms.end())
        {
            addedUniforms.insert(uniform->getVariable()->getName());
            uniforms.push_back(uniform);
        }
    }
}

void computeUnifiedUniforms(const Shader &vertexShader, const Shader &fragmentShader,
                            std::vector<const ShaderInput *> &uniforms)
{
    set<string> addedUniforms;
    addNewUniforms(uniforms, addedUniforms, vertexShader);
    addNewUniforms(uniforms, addedUniforms, fragmentShader);
}

void computeRandomValue(de::Random &rnd, ValueAccess dst, ConstValueRangeAccess valueRange)
{
    const VariableType &type = dst.getType();

    switch (type.getBaseType())
    {
    case VariableType::TYPE_FLOAT:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            const float quantizeStep     = 1.0f / 8.0f;
            float minVal                 = valueRange.component(ndx).getMin().asFloat();
            float maxVal                 = valueRange.component(ndx).getMax().asFloat();
            dst.component(ndx).asFloat() = getQuantizedFloat(rnd, minVal, maxVal, quantizeStep);
        }
        break;

    case VariableType::TYPE_BOOL:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            int minVal                  = valueRange.component(ndx).getMin().asBool() ? 1 : 0;
            int maxVal                  = valueRange.component(ndx).getMin().asBool() ? 1 : 0;
            dst.component(ndx).asBool() = rnd.getInt(minVal, maxVal) == 1;
        }
        break;

    case VariableType::TYPE_INT:
    case VariableType::TYPE_SAMPLER_2D:
    case VariableType::TYPE_SAMPLER_CUBE:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            int minVal                 = valueRange.component(ndx).getMin().asInt();
            int maxVal                 = valueRange.component(ndx).getMax().asInt();
            dst.component(ndx).asInt() = rnd.getInt(minVal, maxVal);
        }
        break;

    case VariableType::TYPE_ARRAY:
    {
        int numElements = type.getNumElements();
        for (int ndx = 0; ndx < numElements; ndx++)
            computeRandomValue(rnd, dst.arrayElement(ndx), valueRange.arrayElement(ndx));
        break;
    }

    case VariableType::TYPE_STRUCT:
    {
        int numMembers = (int)type.getMembers().size();
        for (int ndx = 0; ndx < numMembers; ndx++)
            computeRandomValue(rnd, dst.member(ndx), valueRange.member(ndx));
        break;
    }

    default:
        TCU_FAIL("Invalid type");
    }
}

void computeUniformValues(de::Random &rnd, std::vector<VariableValue> &values,
                          const std::vector<const ShaderInput *> &uniforms)
{
    DE_ASSERT(values.empty());
    for (vector<const ShaderInput *>::const_iterator i = uniforms.begin(); i != uniforms.end(); i++)
    {
        const ShaderInput *uniform = *i;
        values.push_back(VariableValue(uniform->getVariable()));
        computeRandomValue(rnd, values[values.size() - 1].getValue(), uniform->getValueRange());
    }
}

bool isUndefinedValueRange(ConstValueRangeAccess valueRange)
{
    switch (valueRange.getType().getBaseType())
    {
    case VariableType::TYPE_FLOAT:
    case VariableType::TYPE_INT:
    {
        bool isFloat  = valueRange.getType().getBaseType() == VariableType::TYPE_FLOAT;
        Scalar infMin = isFloat ? Scalar::min<float>() : Scalar::min<int>();
        Scalar infMax = isFloat ? Scalar::max<float>() : Scalar::max<int>();

        for (int ndx = 0; ndx < valueRange.getType().getNumElements(); ndx++)
        {
            if (valueRange.getMin().component(ndx).asScalar() != infMin ||
                valueRange.getMax().component(ndx).asScalar() != infMax)
                return false;
        }
        return true;
    }

    case VariableType::TYPE_BOOL:
        return false;

    default:
        TCU_FAIL("Unsupported type");
    }
}

VariableType computeRandomType(GeneratorState &state, int maxScalars)
{
    DE_ASSERT(maxScalars >= 1);

    static const VariableType::Type baseTypes[] = {
        VariableType::TYPE_BOOL, VariableType::TYPE_INT, VariableType::TYPE_FLOAT
        // \todo [pyry] Other types
    };

    VariableType::Type baseType = VariableType::TYPE_LAST;
    state.getRandom().choose(baseTypes, baseTypes + DE_LENGTH_OF_ARRAY(baseTypes), &baseType, 1);

    switch (baseType)
    {
    case VariableType::TYPE_BOOL:
    case VariableType::TYPE_INT:
    case VariableType::TYPE_FLOAT:
    {
        const int minVecLength = 1;
        const int maxVecLength = 4;
        return VariableType(baseType, state.getRandom().getInt(minVecLength, de::min(maxScalars, maxVecLength)));
    }

    default:
        DE_ASSERT(false);
        throw Exception("computeRandomType(): Unsupported type");
    }
}

void computeRandomValueRange(GeneratorState &state, ValueRangeAccess valueRange)
{
    const VariableType &type = valueRange.getType();
    de::Random &rnd          = state.getRandom();

    switch (type.getBaseType())
    {
    case VariableType::TYPE_BOOL:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            bool minVal                                 = rnd.getBool();
            bool maxVal                                 = minVal ? true : rnd.getBool();
            valueRange.getMin().component(ndx).asBool() = minVal;
            valueRange.getMax().component(ndx).asBool() = maxVal;
        }
        break;

    case VariableType::TYPE_INT:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            const int minIntVal   = -16;
            const int maxIntVal   = 16;
            const int maxRangeLen = maxIntVal - minIntVal;

            int rangeLen = rnd.getInt(0, maxRangeLen);
            int minVal   = minIntVal + rnd.getInt(0, maxRangeLen - rangeLen);
            int maxVal   = minVal + rangeLen;

            valueRange.getMin().component(ndx).asInt() = minVal;
            valueRange.getMax().component(ndx).asInt() = maxVal;
        }
        break;

    case VariableType::TYPE_FLOAT:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            const float step        = 0.1f;
            const int maxSteps      = 320;
            const float minFloatVal = -16.0f;

            int rangeLen = rnd.getInt(0, maxSteps);
            int minStep  = rnd.getInt(0, maxSteps - rangeLen);

            float minVal = minFloatVal + step * (float)minStep;
            float maxVal = minVal + step * (float)rangeLen;

            valueRange.getMin().component(ndx).asFloat() = minVal;
            valueRange.getMax().component(ndx).asFloat() = maxVal;
        }
        break;

    default:
        DE_ASSERT(false);
        throw Exception("computeRandomValueRange(): Unsupported type");
    }
}

int getTypeConstructorDepth(const VariableType &type)
{
    switch (type.getBaseType())
    {
    case VariableType::TYPE_STRUCT:
    {
        const vector<VariableType::Member> &members = type.getMembers();
        int maxDepth                                = 0;
        for (vector<VariableType::Member>::const_iterator i = members.begin(); i != members.end(); i++)
        {
            const VariableType &memberType = i->getType();
            int depth                      = 0;
            switch (memberType.getBaseType())
            {
            case VariableType::TYPE_STRUCT:
                depth = getTypeConstructorDepth(memberType);
                break;

            case VariableType::TYPE_BOOL:
            case VariableType::TYPE_FLOAT:
            case VariableType::TYPE_INT:
                depth = memberType.getNumElements() == 1 ? 1 : 2;
                break;

            default:
                DE_ASSERT(false);
                break;
            }

            maxDepth = de::max(maxDepth, depth);
        }
        return maxDepth + 1;
    }

    case VariableType::TYPE_BOOL:
    case VariableType::TYPE_FLOAT:
    case VariableType::TYPE_INT:
        return 2; // One node for ctor, another for value

    default:
        DE_ASSERT(false);
        return 0;
    }
}

int getConservativeValueExprDepth(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    // \todo [2011-03-22 pyry] Do a look-up into variable manager?
    DE_UNREF(state);
    return getTypeConstructorDepth(valueRange.getType());
}

static float computeRangeLengthSum(ConstValueRangeAccess valueRange)
{
    const VariableType &type = valueRange.getType();
    float rangeLength        = 0.0f;

    switch (type.getBaseType())
    {
    case VariableType::TYPE_FLOAT:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            float minVal = valueRange.component(ndx).getMin().asFloat();
            float maxVal = valueRange.component(ndx).getMax().asFloat();
            rangeLength += maxVal - minVal;
        }
        break;

    case VariableType::TYPE_BOOL:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            int minVal = valueRange.component(ndx).getMin().asBool() ? 1 : 0;
            int maxVal = valueRange.component(ndx).getMin().asBool() ? 1 : 0;
            rangeLength += (float)(maxVal - minVal);
        }
        break;

    case VariableType::TYPE_INT:
    case VariableType::TYPE_SAMPLER_2D:
    case VariableType::TYPE_SAMPLER_CUBE:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            int64_t minVal = valueRange.component(ndx).getMin().asInt();
            int64_t maxVal = valueRange.component(ndx).getMax().asInt();
            rangeLength += (float)((int)(maxVal - minVal));
        }
        break;

    case VariableType::TYPE_ARRAY:
    {
        int numElements = type.getNumElements();
        for (int ndx = 0; ndx < numElements; ndx++)
            rangeLength += computeRangeLengthSum(valueRange.arrayElement(ndx));
        break;
    }

    case VariableType::TYPE_STRUCT:
    {
        int numMembers = (int)type.getMembers().size();
        for (int ndx = 0; ndx < numMembers; ndx++)
            rangeLength += computeRangeLengthSum(valueRange.member(ndx));
        break;
    }

    default:
        TCU_FAIL("Invalid type");
    }

    return rangeLength;
}

float computeDynamicRangeWeight(ConstValueRangeAccess valueRange)
{
    const VariableType &type = valueRange.getType();
    float rangeLenSum        = computeRangeLengthSum(valueRange);
    int numScalars           = type.getScalarSize();

    return rangeLenSum / (float)numScalars;
}

} // namespace rsg
