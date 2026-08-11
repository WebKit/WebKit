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
 * \brief Shader generator.
 *//*--------------------------------------------------------------------*/

#include "rsgShaderGenerator.hpp"
#include "rsgFunctionGenerator.hpp"
#include "rsgToken.hpp"
#include "rsgPrettyPrinter.hpp"
#include "rsgUtils.hpp"
#include "deString.h"

#include <iterator>

using std::string;
using std::vector;

namespace rsg
{

ShaderGenerator::ShaderGenerator(GeneratorState &state) : m_state(state), m_varManager(state.getNameAllocator())
{
    state.setVariableManager(m_varManager);
}

ShaderGenerator::~ShaderGenerator(void)
{
}

namespace
{

const char *getFragColorName(const GeneratorState &state)
{
    switch (state.getProgramParameters().version)
    {
    case VERSION_100:
        return "gl_FragColor";
    case VERSION_300:
        return "dEQP_FragColor";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

void createAssignment(BlockStatement &block, const Variable *dstVar, const Variable *srcVar)
{
    VariableRead *varRead = new VariableRead(srcVar);
    try
    {
        block.addChild(new AssignStatement(dstVar, varRead));
    }
    catch (const std::exception &)
    {
        delete varRead;
        throw;
    }
}

const ValueEntry *findByName(VariableManager &varManager, const char *name)
{
    AnyEntry::Iterator iter = varManager.getBegin<AnyEntry>();
    AnyEntry::Iterator end  = varManager.getEnd<AnyEntry>();
    for (; iter != end; iter++)
    {
        const ValueEntry *entry = *iter;
        if (strcmp(entry->getVariable()->getName(), name) == 0)
            return entry;
    }
    return nullptr;
}

void genVertexPassthrough(GeneratorState &state, Shader &shader)
{
    // Create copies from shader inputs to outputs
    vector<const ValueEntry *> entries;
    std::copy(state.getVariableManager().getBegin<AnyEntry>(), state.getVariableManager().getEnd<AnyEntry>(),
              std::inserter(entries, entries.begin()));

    for (vector<const ValueEntry *>::const_iterator i = entries.begin(); i != entries.end(); i++)
    {
        const ValueEntry *entry = *i;
        const Variable *outVar  = entry->getVariable();
        std::string inVarName;

        if (outVar->getStorage() != Variable::STORAGE_SHADER_OUT)
            continue;

        // Name: a_[name], remove v_ -prefix if such exists
        inVarName = "a_";
        if (deStringBeginsWith(outVar->getName(), "v_"))
            inVarName += (outVar->getName() + 2);
        else
            inVarName += outVar->getName();

        Variable *inVar =
            state.getVariableManager().allocate(outVar->getType(), Variable::STORAGE_SHADER_IN, inVarName.c_str());

        // Update value range. This will be stored into shader input info.
        state.getVariableManager().setValue(inVar, entry->getValueRange());

        // Add assignment from input to output into main() body
        createAssignment(shader.getMain().getBody(), entry->getVariable(), inVar);
    }
}

void genFragmentPassthrough(GeneratorState &state, Shader &shader)
{
    // Add simple gl_FragColor = v_color; assignment
    const ValueEntry *fragColorEntry = findByName(state.getVariableManager(), getFragColorName(state));
    TCU_CHECK(fragColorEntry);

    Variable *inColorVariable = state.getVariableManager().allocate(fragColorEntry->getVariable()->getType(),
                                                                    Variable::STORAGE_SHADER_IN, "v_color");

    state.getVariableManager().setValue(inColorVariable, fragColorEntry->getValueRange());
    createAssignment(shader.getMain().getBody(), fragColorEntry->getVariable(), inColorVariable);
}

// Sets undefined (-inf..inf) components to some meaningful values. Used for sanitizing final shader input value ranges.
void fillUndefinedComponents(ValueRangeAccess valueRange)
{
    VariableType::Type baseType = valueRange.getType().getBaseType();
    TCU_CHECK(baseType == VariableType::TYPE_FLOAT || baseType == VariableType::TYPE_INT ||
              baseType == VariableType::TYPE_BOOL);

    for (int elemNdx = 0; elemNdx < valueRange.getType().getNumElements(); elemNdx++)
    {
        if (isUndefinedValueRange(valueRange.component(elemNdx)))
        {
            ValueAccess min = valueRange.component(elemNdx).getMin();
            ValueAccess max = valueRange.component(elemNdx).getMax();

            switch (baseType)
            {
            case VariableType::TYPE_FLOAT:
                min = 0.0f;
                max = 1.0f;
                break;
            case VariableType::TYPE_INT:
                min = 0;
                max = 1;
                break;
            case VariableType::TYPE_BOOL:
                min = false;
                max = true;
                break;
            default:
                DE_ASSERT(false);
            }
        }
    }
}

void fillUndefinedShaderInputs(vector<ShaderInput *> &inputs)
{
    for (vector<ShaderInput *>::iterator i = inputs.begin(); i != inputs.end(); i++)
    {
        if (!(*i)->getVariable()->getType().isSampler()) // Samplers are assigned at program-level.
            fillUndefinedComponents((*i)->getValueRange());
    }
}

} // namespace

void ShaderGenerator::generate(const ShaderParameters &shaderParams, Shader &shader,
                               const vector<ShaderInput *> &outputs)
{
    // Global scopes
    VariableScope &globalVariableScope = shader.getGlobalScope();
    ValueScope globalValueScope;

    // Init state
    m_state.setShader(shaderParams, shader);
    DE_ASSERT(m_state.getExpressionFlags() == 0);

    // Reserve some scalars for gl_Position & dEQP_Position
    ReservedScalars reservedScalars;
    if (shader.getType() == Shader::TYPE_VERTEX)
        m_state.getVariableManager().reserve(reservedScalars, 4 * 2);

    // Push global scopes
    m_varManager.pushVariableScope(globalVariableScope);
    m_varManager.pushValueScope(globalValueScope);

    // Init shader outputs.
    {
        for (vector<ShaderInput *>::const_iterator i = outputs.begin(); i != outputs.end(); i++)
        {
            const ShaderInput *input = *i;
            Variable *variable       = m_state.getVariableManager().allocate(
                input->getVariable()->getType(), Variable::STORAGE_SHADER_OUT, input->getVariable()->getName());

            m_state.getVariableManager().setValue(variable, input->getValueRange());
        }

        if (shader.getType() == Shader::TYPE_FRAGMENT)
        {
            // gl_FragColor
            // \todo [2011-11-22 pyry] Multiple outputs from fragment shader!
            Variable *fragColorVar = m_state.getVariableManager().allocate(
                VariableType(VariableType::TYPE_FLOAT, 4), Variable::STORAGE_SHADER_OUT, getFragColorName(m_state));
            ValueRange valueRange(fragColorVar->getType());

            valueRange.getMin() = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            valueRange.getMax() = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);

            fragColorVar->setLayoutLocation(0); // Bind color output to location 0 (applies to GLSL ES 3.0 onwards).

            m_state.getVariableManager().setValue(fragColorVar, valueRange.asAccess());
        }
    }

    // Construct shader code.
    {
        Function &main = shader.getMain();
        main.setReturnType(VariableType(VariableType::TYPE_VOID));

        if (shaderParams.randomize)
        {
            FunctionGenerator funcGen(m_state, main);

            // Mandate assignment into to all shader outputs in main()
            const vector<Variable *> &liveVars = globalVariableScope.getLiveVariables();
            for (vector<Variable *>::const_iterator i = liveVars.begin(); i != liveVars.end(); i++)
            {
                Variable *variable = *i;
                if (variable->getStorage() == Variable::STORAGE_SHADER_OUT)
                    funcGen.requireAssignment(variable);
            }

            funcGen.generate();
        }
        else
        {
            if (shader.getType() == Shader::TYPE_VERTEX)
                genVertexPassthrough(m_state, shader);
            else
            {
                DE_ASSERT(shader.getType() == Shader::TYPE_FRAGMENT);
                genFragmentPassthrough(m_state, shader);
            }
        }

        if (shader.getType() == Shader::TYPE_VERTEX)
        {
            // Add gl_Position = dEQP_Position;
            m_state.getVariableManager().release(reservedScalars);

            Variable *glPosVariable = m_state.getVariableManager().allocate(
                VariableType(VariableType::TYPE_FLOAT, 4), Variable::STORAGE_SHADER_OUT, "gl_Position");
            Variable *qpPosVariable = m_state.getVariableManager().allocate(
                VariableType(VariableType::TYPE_FLOAT, 4), Variable::STORAGE_SHADER_IN, "dEQP_Position");

            ValueRange valueRange(glPosVariable->getType());

            valueRange.getMin() = tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f);
            valueRange.getMax() = tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);

            m_state.getVariableManager().setValue(
                qpPosVariable,
                valueRange
                    .asAccess()); // \todo [2011-05-24 pyry] No expression should be able to use gl_Position or dEQP_Position..

            createAssignment(main.getBody(), glPosVariable, qpPosVariable);
        }
    }

    // Declare live global variables.
    {
        vector<Variable *> liveVariables;
        std::copy(globalVariableScope.getLiveVariables().begin(), globalVariableScope.getLiveVariables().end(),
                  std::inserter(liveVariables, liveVariables.begin()));

        vector<Variable *> createDeclarationStatementVars;

        for (vector<Variable *>::iterator i = liveVariables.begin(); i != liveVariables.end(); i++)
        {
            Variable *variable = *i;
            const char *name   = variable->getName();
            bool declare       = !deStringBeginsWith(name, "gl_"); // Do not declare built-in types.

            // Create input entries (store value range) if necessary
            vector<ShaderInput *> &inputs   = shader.getInputs();
            vector<ShaderInput *> &uniforms = shader.getUniforms();

            switch (variable->getStorage())
            {
            case Variable::STORAGE_SHADER_IN:
            {
                const ValueEntry *value = m_state.getVariableManager().getValue(variable);

                inputs.reserve(inputs.size() + 1);
                inputs.push_back(new ShaderInput(variable, value->getValueRange()));
                break;
            }

            case Variable::STORAGE_UNIFORM:
            {
                const ValueEntry *value = m_state.getVariableManager().getValue(variable);

                uniforms.reserve(uniforms.size() + 1);
                uniforms.push_back(new ShaderInput(variable, value->getValueRange()));
                break;
            }

            default:
                break;
            }

            if (declare)
                createDeclarationStatementVars.push_back(variable);
            else
            {
                // Just move to global scope without declaration statement.
                m_state.getVariableManager().declareVariable(variable);
            }
        }

        // All global initializers must be constant expressions, no variable allocation is allowed
        DE_ASSERT(m_state.getExpressionFlags() == 0);
        m_state.pushExpressionFlags(CONST_EXPR | NO_VAR_ALLOCATION);

        // Create declaration statements
        for (vector<Variable *>::iterator i = createDeclarationStatementVars.begin();
             i != createDeclarationStatementVars.end(); i++)
        {
            shader.getGlobalStatements().reserve(shader.getGlobalStatements().size());
            shader.getGlobalStatements().push_back(new DeclarationStatement(m_state, *i));
        }

        m_state.popExpressionFlags();
    }

    // Pop global scopes
    m_varManager.popVariableScope();
    m_varManager.popValueScope();

    // Fill undefined (unused) components in inputs with unused values
    fillUndefinedShaderInputs(shader.getInputs());
    fillUndefinedShaderInputs(shader.getUniforms());

    // Tokenize shader and write source
    {
        TokenStream tokenStr;
        shader.tokenize(m_state, tokenStr);

        std::ostringstream str;
        PrettyPrinter printer(str);

        // Append #version if necessary.
        if (m_state.getProgramParameters().version == VERSION_300)
            str << "#version 300 es\n";

        printer.append(tokenStr);
        shader.setSource(str.str().c_str());
    }
}

} // namespace rsg
