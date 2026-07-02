#ifndef _RSGEXECUTIONCONTEXT_HPP
#define _RSGEXECUTIONCONTEXT_HPP
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
 * \brief Shader Execution Context.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgVariable.hpp"
#include "rsgVariableValue.hpp"
#include "rsgSamplers.hpp"

#include <vector>
#include <map>

namespace rsg
{

constexpr int EXEC_VEC_WIDTH = 64;

typedef ConstStridedValueAccess<EXEC_VEC_WIDTH> ExecConstValueAccess;
typedef StridedValueAccess<EXEC_VEC_WIDTH> ExecValueAccess;
typedef ValueStorage<EXEC_VEC_WIDTH> ExecValueStorage;

typedef std::map<const Variable *, ExecValueStorage *> VarValueMap;

class ExecMaskStorage
{
public:
    ExecMaskStorage(bool initVal = true);

    ExecValueAccess getValue(void);
    ExecConstValueAccess getValue(void) const;

private:
    Scalar m_data[EXEC_VEC_WIDTH];
};

class ExecutionContext
{
public:
    ExecutionContext(const Sampler2DMap &samplers2D, const SamplerCubeMap &samplersCube);
    ~ExecutionContext(void);

    ExecValueAccess getValue(const Variable *variable);
    const Sampler2D &getSampler2D(const Variable *variable) const;
    const SamplerCube &getSamplerCube(const Variable *variable) const;

    ExecConstValueAccess getExecutionMask(void) const;

    void andExecutionMask(ExecConstValueAccess value); // Pushes computed value
    void pushExecutionMask(ExecConstValueAccess value);

    void popExecutionMask(void);

protected:
    ExecutionContext(const ExecutionContext &other);
    ExecutionContext &operator=(const ExecutionContext &other);

    VarValueMap m_varValues;
    const Sampler2DMap &m_samplers2D;
    const SamplerCubeMap &m_samplersCube;
    std::vector<ExecMaskStorage> m_execMaskStack;
};

void assignMasked(ExecValueAccess dst, ExecConstValueAccess src, ExecConstValueAccess mask);

} // namespace rsg

#endif // _RSGEXECUTIONCONTEXT_HPP
