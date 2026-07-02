
/*-------------------------------------------------------------------------
 * drawElements Quality Program Reference Renderer
 * -----------------------------------------------
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
 * \brief Shader interfaces.
 *//*--------------------------------------------------------------------*/

#include "rrShaders.hpp"

namespace rr
{

GeometryShader::GeometryShader(size_t numVaryingInputs, size_t numVaryingOutputs, GeometryShaderInputType inputType,
                               GeometryShaderOutputType outputType, size_t verticesOut, size_t numInvocations)
    : m_inputType(inputType)
    , m_outputType(outputType)
    , m_numVerticesOut(verticesOut)
    , m_numInvocations(numInvocations)
    , m_inputs(numVaryingInputs)
    , m_outputs(numVaryingOutputs)
{
}

} // namespace rr
