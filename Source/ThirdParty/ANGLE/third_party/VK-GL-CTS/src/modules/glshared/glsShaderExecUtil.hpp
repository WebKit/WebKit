#ifndef _GLSSHADEREXECUTIL_HPP
#define _GLSSHADEREXECUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
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
 * \brief Shader execution utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluVarType.hpp"
#include "gluShaderUtil.hpp"

namespace tcu
{
class TestLog;
}

namespace glu
{
class RenderContext;
}

namespace deqp
{
namespace gls
{
namespace ShaderExecUtil
{

//! Shader input / output variable declaration.
struct Symbol
{
    std::string name;     //!< Symbol name.
    glu::VarType varType; //!< Symbol type.

    Symbol(void)
    {
    }
    Symbol(const std::string &name_, const glu::VarType &varType_) : name(name_), varType(varType_)
    {
    }
};

//! Complete shader specification.
struct ShaderSpec
{
    glu::GLSLVersion version; //!< Shader version.
    std::vector<Symbol> inputs;
    std::vector<Symbol> outputs;
    std::string
        globalDeclarations; //!< These are placed into global scope. Can contain uniform declarations for example.
    std::string source;     //!< Source snippet to be executed.

    ShaderSpec(void) : version(glu::GLSL_VERSION_300_ES)
    {
    }
};

//! Base class for shader executor.
class ShaderExecutor
{
public:
    virtual ~ShaderExecutor(void);

    //! Check if executor can be used.
    virtual bool isOk(void) const = 0;

    //! Log executor details (program etc.).
    virtual void log(tcu::TestLog &log) const = 0;

    //! Get program.
    virtual uint32_t getProgram(void) const = 0;

    //! Set this shader program current in context. Must be called before execute().
    virtual void useProgram(void);

    //! Execute active program. useProgram() must be called before this.
    virtual void execute(int numValues, const void *const *inputs, void *const *outputs) = 0;

protected:
    ShaderExecutor(const glu::RenderContext &renderCtx, const ShaderSpec &shaderSpec);

    const glu::RenderContext &m_renderCtx;

    std::vector<Symbol> m_inputs;
    std::vector<Symbol> m_outputs;
};

inline tcu::TestLog &operator<<(tcu::TestLog &log, const ShaderExecutor *executor)
{
    executor->log(log);
    return log;
}
inline tcu::TestLog &operator<<(tcu::TestLog &log, const ShaderExecutor &executor)
{
    executor.log(log);
    return log;
}

bool executorSupported(glu::ShaderType shaderType);
ShaderExecutor *createExecutor(const glu::RenderContext &renderCtx, glu::ShaderType shaderType,
                               const ShaderSpec &shaderSpec);

} // namespace ShaderExecUtil
} // namespace gls
} // namespace deqp

#endif // _GLSSHADEREXECUTIL_HPP
