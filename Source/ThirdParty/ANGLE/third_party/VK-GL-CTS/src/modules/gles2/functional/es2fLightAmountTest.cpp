/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
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
 * \brief Light amount test.
 *//*--------------------------------------------------------------------*/

#include "es2fLightAmountTest.hpp"
#include "tcuStringTemplate.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "tcuTestLog.hpp"
#include "deStringUtil.hpp"
#include "deInt32.h"
#include "deRandom.h"

#include <stdio.h>
#include <vector>

#include "glw.h"

using namespace std;

namespace deqp
{
namespace gles2
{
namespace Functional
{

const char *s_noLightsVertexShader = "uniform mat4 u_modelviewMatrix;\n"
                                     "uniform mat4 u_modelviewProjectionMatrix;\n"
                                     "uniform mat3 u_normalMatrix;\n"
                                     "\n"
                                     "attribute vec4 a_position;\n"
                                     "attribute vec3 a_normal;\n"
                                     "\n"
                                     "varying vec3 v_color;\n"
                                     "\n"
                                     "void main()\n"
                                     "{\n"
                                     "    v_color = vec3(0.0);\n"
                                     "    gl_Position = u_modelviewProjectionMatrix * a_position;\n"
                                     "}\n";

const char *s_vertexShaderTemplate =
    "struct Light\n"
    "{\n"
    "    vec3    position;\n"
    "    vec3    diffuse;\n"
    "    vec3    specular;\n"
    "    vec3    attenuation;\n"
    "};\n"
    "uniform Light u_lights[${NUM_DIR_LIGHTS} + ${NUM_OMNI_LIGHTS}];\n"
    "uniform mat4 u_modelviewMatrix;\n"
    "uniform mat4 u_modelviewProjectionMatrix;\n"
    "uniform mat3 u_normalMatrix;\n"
    "\n"
    "attribute vec4 a_position;\n"
    "attribute vec3 a_normal;\n"
    "\n"
    "varying vec3 v_color;\n"
    "\n"
    "float computeAttenuation(vec3 dirToLight, vec3 attenuation)\n"
    "{\n"
    "    float dist = length(dirToLight);\n"
    "    return 1.0 / (attenuation.x + attenuation.y*dist + attenuation.z*dist*dist);\n"
    "}\n"
    "\n"
    "vec3 computeDirLight(int ndx, vec3 position, vec3 normal)\n"
    "{\n"
    "    Light light = u_lights[ndx];\n"
    "    float cosAngle = dot(light.position, normal);\n"
    "    return cosAngle * light.diffuse;\n"
    "}\n"
    "\n"
    "vec3 computeOmniLight(int ndx, vec3 position, vec3 normal)\n"
    "{\n"
    "    Light light = u_lights[ndx];\n"
    "    vec3 dirToLight = light.position - position;\n"
    "    float cosAngle = dot(normalize(dirToLight), normal);\n"
    "    float atten = computeAttenuation(dirToLight, light.attenuation);\n"
    "    return atten * cosAngle * light.diffuse;\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec3 lightSpacePos = vec3(u_modelviewMatrix * a_position);\n"
    "    vec3 lightNormal = normalize(u_normalMatrix * a_normal);\n"
    "    vec3 color = vec3(0.0);\n"
    "    for (int i = 0; i < ${NUM_DIR_LIGHTS}; i++)\n"
    "        color += computeDirLight(i, lightSpacePos, lightNormal);\n"
    "    for (int i = 0; i < ${NUM_OMNI_LIGHTS}; i++)\n"
    "        color += computeOmniLight(${NUM_DIR_LIGHTS}+i, lightSpacePos, lightNormal);\n"
    "    v_color = color;\n"
    "    gl_Position = u_modelviewProjectionMatrix * a_position;\n"
    "}\n";

const char *s_fragmentShaderTemplate = "varying highp vec3 v_color;\n"
                                       "\n"
                                       "void main()\n"
                                       "{\n"
                                       "    gl_FragColor = vec4(v_color, 1.0);\n"
                                       "}\n";

class LightAmountCase : public TestCase
{
public:
    LightAmountCase(Context &context, const char *name, int numDirectionalLights, int numOmniLights, int numSpotLights)
        : TestCase(context, name, name)
        , m_numDirectionalLights(numDirectionalLights)
        , m_numOmniLights(numOmniLights)
        , m_numSpotLights(numSpotLights)
    {
    }

    virtual IterateResult iterate(void);

private:
    int m_numDirectionalLights;
    int m_numOmniLights;
    int m_numSpotLights;
};

TestCase::IterateResult LightAmountCase::iterate(void)
{
    GLU_CHECK_MSG("LightAmountTest::iterate() begin");

    string vertexShaderSource;
    string fragmentShaderSource;

    // Fill in shader template parameters.
    {
        bool hasAnyLights = ((m_numDirectionalLights + m_numOmniLights + m_numSpotLights) != 0);

        tcu::StringTemplate vertexTemplate(hasAnyLights ? s_vertexShaderTemplate : s_noLightsVertexShader);
        tcu::StringTemplate fragmentTemplate(s_fragmentShaderTemplate);

        map<string, string> params;
        params.insert(pair<string, string>("NUM_DIR_LIGHTS", de::toString(m_numDirectionalLights)));
        params.insert(pair<string, string>("NUM_OMNI_LIGHTS", de::toString(m_numOmniLights)));
        params.insert(pair<string, string>("NUM_SPOT_LIGHTS", de::toString(m_numSpotLights)));

        vertexShaderSource   = vertexTemplate.specialize(params);
        fragmentShaderSource = fragmentTemplate.specialize(params);
    }

    // Create shader and program objects.
    glu::ShaderProgram program(m_context.getRenderContext(),
                               glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
    m_testCtx.getLog() << program;

    // Draw something? Check results?
    glUseProgram(program.getProgram());

    bool testOk = program.isOk();

    GLU_CHECK_MSG("LightAmountTest::iterate() end");

    m_testCtx.setTestResult(testOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, testOk ? "Pass" : "Fail");
    return TestCase::STOP;
}

//

LightAmountTest::LightAmountTest(Context &context) : TestCaseGroup(context, "light_amount", "Light Amount Stress Tests")
{
}

LightAmountTest::~LightAmountTest(void)
{
}

void LightAmountTest::init(void)
{
    //                                        name                dir, omni, spot
    addChild(new LightAmountCase(m_context, "none", 0, 0, 0));
    addChild(new LightAmountCase(m_context, "1dir", 1, 0, 0));
    addChild(new LightAmountCase(m_context, "2dir", 2, 0, 0));
    addChild(new LightAmountCase(m_context, "4dir", 4, 0, 0));
    addChild(new LightAmountCase(m_context, "6dir", 6, 0, 0));
    addChild(new LightAmountCase(m_context, "8dir", 8, 0, 0));
    addChild(new LightAmountCase(m_context, "10dir", 10, 0, 0));
    addChild(new LightAmountCase(m_context, "12dir", 12, 0, 0));
    addChild(new LightAmountCase(m_context, "14dir", 14, 0, 0));
    addChild(new LightAmountCase(m_context, "16dir", 16, 0, 0));
    addChild(new LightAmountCase(m_context, "1omni", 0, 1, 0));
    addChild(new LightAmountCase(m_context, "2omni", 0, 2, 0));
    addChild(new LightAmountCase(m_context, "4omni", 0, 4, 0));
    addChild(new LightAmountCase(m_context, "6omni", 0, 6, 0));
    addChild(new LightAmountCase(m_context, "8omni", 0, 8, 0));
    addChild(new LightAmountCase(m_context, "10omni", 0, 10, 0));
    addChild(new LightAmountCase(m_context, "12omni", 0, 12, 0));
    addChild(new LightAmountCase(m_context, "14omni", 0, 14, 0));
    addChild(new LightAmountCase(m_context, "16omni", 0, 16, 0));
    // addChild(new LightAmountCase(m_context, "1spot", 0, 0, 1 ));
    // addChild(new LightAmountCase(m_context, "2spot", 0, 0, 2 ));
    // addChild(new LightAmountCase(m_context, "4spot", 0, 0, 4 ));
    // addChild(new LightAmountCase(m_context, "6spot", 0, 0, 6 ));
    // addChild(new LightAmountCase(m_context, "8spot", 0, 0, 8 ));
    // addChild(new LightAmountCase(m_context, "1dir_1omni", 1, 1, 0 ));
    // addChild(new LightAmountCase(m_context, "2dir_2omni", 2, 2, 0 ));
    // addChild(new LightAmountCase(m_context, "4dir_4omni", 4, 4, 0 ));
    // addChild(new LightAmountCase(m_context, "1dir_1spot", 1, 0, 1 ));
    // addChild(new LightAmountCase(m_context, "2dir_2spot", 2, 0, 2 ));
    // addChild(new LightAmountCase(m_context, "4dir_4spot", 4, 0, 4 ));
    // addChild(new LightAmountCase(m_context, "1omni_1spot", 0, 1, 1 ));
    // addChild(new LightAmountCase(m_context, "2omni_2spot", 0, 2, 2 ));
    // addChild(new LightAmountCase(m_context, "4omni_4spot", 0, 4, 4 ));
    // addChild(new LightAmountCase(m_context, "1dir_1omni_1spot", 1, 1, 1 ));
    // addChild(new LightAmountCase(m_context, "2dir_2omni_2spot", 2, 2, 2 ));
    // addChild(new LightAmountCase(m_context, "4dir_2omni_2spot", 4, 2, 2 ));
    // addChild(new LightAmountCase(m_context, "2dir_4omni_2spot", 2, 4, 2 ));
    // addChild(new LightAmountCase(m_context, "2dir_2omni_4spot", 2, 2, 4 ));
    // addChild(new LightAmountCase(m_context, "4dir_4omni_4spot", 4, 4, 4 ));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
