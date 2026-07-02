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
 * \brief Attribute location test
 *//*--------------------------------------------------------------------*/

#include "es2fAttribLocationTests.hpp"

#include "glsAttributeLocationTests.hpp"

#include "deStringUtil.hpp"
#include "gluDefs.hpp"
#include "gluRenderContext.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"
#include "tcuTestLog.hpp"

#include <vector>

using namespace deqp::gls::AttributeLocationTestUtil;
using std::vector;

namespace deqp
{
namespace gles2
{
namespace Functional
{

TestCaseGroup *createAttributeLocationTests(Context &context)
{
    const AttribType types[] = {AttribType("float", 1, GL_FLOAT),     AttribType("vec2", 1, GL_FLOAT_VEC2),
                                AttribType("vec3", 1, GL_FLOAT_VEC3), AttribType("vec4", 1, GL_FLOAT_VEC4),

                                AttribType("mat2", 2, GL_FLOAT_MAT2), AttribType("mat3", 3, GL_FLOAT_MAT3),
                                AttribType("mat4", 4, GL_FLOAT_MAT4)};

    TestCaseGroup *const root = new TestCaseGroup(context, "attribute_location", "Attribute location tests");

    // Basic bind attribute tests
    {
        TestCaseGroup *const bindAttributeGroup = new TestCaseGroup(context, "bind", "Basic attribute binding tests.");

        root->addChild(bindAttributeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];
            bindAttributeGroup->addChild(
                new gls::BindAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Bind max number of attributes
    {
        TestCaseGroup *const bindMaxAttributeGroup =
            new TestCaseGroup(context, "bind_max_attributes", "Test using maximum attributes with bind.");

        root->addChild(bindMaxAttributeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];
            bindMaxAttributeGroup->addChild(
                new gls::BindMaxAttributesTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Test aliasing
    {
        TestCaseGroup *const aliasingGroup =
            new TestCaseGroup(context, "bind_aliasing", "Test attribute location aliasing with bind.");

        root->addChild(aliasingGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];

            // Simple aliasing cases
            aliasingGroup->addChild(
                new gls::BindAliasingAttributeTest(context.getTestContext(), context.getRenderContext(), type));

            // For types which occupy more than one location. Alias second location.
            if (type.getLocationSize() > 1)
                aliasingGroup->addChild(
                    new gls::BindAliasingAttributeTest(context.getTestContext(), context.getRenderContext(), type, 1));

            // Use more than maximum attributes with conditional aliasing
            aliasingGroup->addChild(
                new gls::BindMaxAliasingAttributeTest(context.getTestContext(), context.getRenderContext(), type));

            // Use more than maximum attributes with inactive attributes
            aliasingGroup->addChild(
                new gls::BindInactiveAliasingAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Test filling holes in attribute location
    {
        TestCaseGroup *const holeGroup = new TestCaseGroup(
            context, "bind_hole", "Bind all, but one attribute and leave hole in location space for it.");

        root->addChild(holeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];

            // Bind first location, leave hole size of type and fill rest of locations
            holeGroup->addChild(
                new gls::BindHoleAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Test binding at different times
    {
        TestCaseGroup *const bindTimeGroup =
            new TestCaseGroup(context, "bind_time", "Bind time tests. Test binding at different stages.");

        root->addChild(bindTimeGroup);

        bindTimeGroup->addChild(
            new gls::PreAttachBindAttributeTest(context.getTestContext(), context.getRenderContext()));
        bindTimeGroup->addChild(
            new gls::PreLinkBindAttributeTest(context.getTestContext(), context.getRenderContext()));
        bindTimeGroup->addChild(
            new gls::PostLinkBindAttributeTest(context.getTestContext(), context.getRenderContext()));
        bindTimeGroup->addChild(new gls::BindRelinkAttributeTest(context.getTestContext(), context.getRenderContext()));
        bindTimeGroup->addChild(
            new gls::BindReattachAttributeTest(context.getTestContext(), context.getRenderContext()));
    }

    // Test relinking program
    {
        TestCaseGroup *const relinkHoleGroup = new TestCaseGroup(
            context, "bind_relink_hole", "Test relinking with moving hole in attribute location space.");

        root->addChild(relinkHoleGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];

            relinkHoleGroup->addChild(
                new gls::BindRelinkHoleAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    return root;
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
