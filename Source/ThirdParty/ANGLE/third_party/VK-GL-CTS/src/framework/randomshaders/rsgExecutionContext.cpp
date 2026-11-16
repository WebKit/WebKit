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
 * \brief Shader Evaluation Context.
 *//*--------------------------------------------------------------------*/

#include "rsgExecutionContext.hpp"

namespace rsg
{

ExecMaskStorage::ExecMaskStorage(bool initVal)
{
    for (int i = 0; i < EXEC_VEC_WIDTH; i++)
        m_data[i].as<bool>() = initVal;
}

ExecValueAccess ExecMaskStorage::getValue(void)
{
    return ExecValueAccess(VariableType::getScalarType(VariableType::TYPE_BOOL), m_data);
}

ExecConstValueAccess ExecMaskStorage::getValue(void) const
{
    return ExecConstValueAccess(VariableType::getScalarType(VariableType::TYPE_BOOL), m_data);
}

ExecutionContext::ExecutionContext(const Sampler2DMap &samplers2D, const SamplerCubeMap &samplersCube)
    : m_samplers2D(samplers2D)
    , m_samplersCube(samplersCube)
{
    // Initialize execution mask to true
    ExecMaskStorage initVal(true);
    pushExecutionMask(initVal.getValue());
}

ExecutionContext::~ExecutionContext(void)
{
    for (VarValueMap::iterator i = m_varValues.begin(); i != m_varValues.end(); i++)
        delete i->second;
    m_varValues.clear();
}

ExecValueAccess ExecutionContext::getValue(const Variable *variable)
{
    ExecValueStorage *storage = m_varValues[variable];

    if (!storage)
    {
        storage               = new ExecValueStorage(variable->getType());
        m_varValues[variable] = storage;
    }

    return storage->getValue(variable->getType());
}

const Sampler2D &ExecutionContext::getSampler2D(const Variable *sampler) const
{
    const ExecValueStorage *samplerVal = m_varValues.find(sampler)->second;

    int samplerNdx = samplerVal->getValue(sampler->getType()).asInt(0);

    return m_samplers2D.find(samplerNdx)->second;
}

const SamplerCube &ExecutionContext::getSamplerCube(const Variable *sampler) const
{
    const ExecValueStorage *samplerVal = m_varValues.find(sampler)->second;

    int samplerNdx = samplerVal->getValue(sampler->getType()).asInt(0);

    return m_samplersCube.find(samplerNdx)->second;
}

void ExecutionContext::andExecutionMask(ExecConstValueAccess value)
{
    ExecMaskStorage tmp;
    ExecValueAccess newValue      = tmp.getValue();
    ExecConstValueAccess oldValue = getExecutionMask();

    for (int i = 0; i < EXEC_VEC_WIDTH; i++)
        newValue.asBool(i) = oldValue.asBool(i) && value.asBool(i);

    pushExecutionMask(newValue);
}

void ExecutionContext::pushExecutionMask(ExecConstValueAccess value)
{
    ExecMaskStorage tmp;
    tmp.getValue() = value.value();
    m_execMaskStack.push_back(tmp);
}

void ExecutionContext::popExecutionMask(void)
{
    m_execMaskStack.pop_back();
}

ExecConstValueAccess ExecutionContext::getExecutionMask(void) const
{
    return m_execMaskStack[m_execMaskStack.size() - 1].getValue();
}

void assignMasked(ExecValueAccess dst, ExecConstValueAccess src, ExecConstValueAccess mask)
{
    const VariableType &type = dst.getType();

    switch (type.getBaseType())
    {
    case VariableType::TYPE_ARRAY:
    {
        int numElems = type.getNumElements();
        for (int elemNdx = 0; elemNdx < numElems; elemNdx++)
            assignMasked(dst.arrayElement(elemNdx), src.arrayElement(elemNdx), mask);

        break;
    }

    case VariableType::TYPE_STRUCT:
    {
        int numMembers = (int)type.getMembers().size();
        for (int memberNdx = 0; memberNdx < numMembers; memberNdx++)
            assignMasked(dst.member(memberNdx), src.member(memberNdx), mask);

        break;
    }

    case VariableType::TYPE_FLOAT:
    case VariableType::TYPE_INT:
    case VariableType::TYPE_BOOL:
    case VariableType::TYPE_SAMPLER_2D:
    case VariableType::TYPE_SAMPLER_CUBE:
    {
        for (int elemNdx = 0; elemNdx < type.getNumElements(); elemNdx++)
        {
            ExecValueAccess dstElem      = dst.component(elemNdx);
            ExecConstValueAccess srcElem = src.component(elemNdx);

            for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
            {
                if (mask.asBool(compNdx))
                    dstElem.asScalar(compNdx) = srcElem.asScalar(compNdx);
            }
        }

        break;
    }

    default:
        DE_FATAL("Unsupported");
        break;
    }
}

} // namespace rsg
