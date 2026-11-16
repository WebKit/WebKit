/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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

#include "es3fAttribLocationTests.hpp"

#include "glsAttributeLocationTests.hpp"

#include "glw.h"

using namespace deqp::gls::AttributeLocationTestUtil;
using std::vector;

namespace deqp
{
namespace gles3
{
namespace Functional
{

TestCaseGroup *createAttributeLocationTests(Context &context)
{
    const AttribType types[] = {AttribType("float", 1, GL_FLOAT),
                                AttribType("vec2", 1, GL_FLOAT_VEC2),
                                AttribType("vec3", 1, GL_FLOAT_VEC3),
                                AttribType("vec4", 1, GL_FLOAT_VEC4),

                                AttribType("mat2", 2, GL_FLOAT_MAT2),
                                AttribType("mat3", 3, GL_FLOAT_MAT3),
                                AttribType("mat4", 4, GL_FLOAT_MAT4),

                                AttribType("int", 1, GL_INT),
                                AttribType("ivec2", 1, GL_INT_VEC2),
                                AttribType("ivec3", 1, GL_INT_VEC3),
                                AttribType("ivec4", 1, GL_INT_VEC4),

                                AttribType("uint", 1, GL_UNSIGNED_INT),
                                AttribType("uvec2", 1, GL_UNSIGNED_INT_VEC2),
                                AttribType("uvec3", 1, GL_UNSIGNED_INT_VEC3),
                                AttribType("uvec4", 1, GL_UNSIGNED_INT_VEC4),

                                AttribType("mat2x2", 2, GL_FLOAT_MAT2),
                                AttribType("mat2x3", 2, GL_FLOAT_MAT2x3),
                                AttribType("mat2x4", 2, GL_FLOAT_MAT2x4),

                                AttribType("mat3x2", 3, GL_FLOAT_MAT3x2),
                                AttribType("mat3x3", 3, GL_FLOAT_MAT3),
                                AttribType("mat3x4", 3, GL_FLOAT_MAT3x4),

                                AttribType("mat4x2", 4, GL_FLOAT_MAT4x2),
                                AttribType("mat4x3", 4, GL_FLOAT_MAT4x3),
                                AttribType("mat4x4", 4, GL_FLOAT_MAT4)};

    const AttribType es2Types[] = {AttribType("float", 1, GL_FLOAT),     AttribType("vec2", 1, GL_FLOAT_VEC2),
                                   AttribType("vec3", 1, GL_FLOAT_VEC3), AttribType("vec4", 1, GL_FLOAT_VEC4),

                                   AttribType("mat2", 2, GL_FLOAT_MAT2), AttribType("mat3", 3, GL_FLOAT_MAT3),
                                   AttribType("mat4", 4, GL_FLOAT_MAT4)};

    TestCaseGroup *const root = new TestCaseGroup(context, "attribute_location", "Attribute location tests");

    // Basic bind attribute tests
    {
        TestCaseGroup *const bindAttributeGroup =
            new TestCaseGroup(context, "bind", "Basic bind attribute location tests.");

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
            new TestCaseGroup(context, "bind_max_attributes", "Use bind with maximum number of attributes.");

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
            new TestCaseGroup(context, "bind_aliasing", "Test binding aliasing locations.");

        root->addChild(aliasingGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(es2Types); typeNdx++)
        {
            const AttribType &type = es2Types[typeNdx];

            // Simple aliasing cases
            aliasingGroup->addChild(
                new gls::BindAliasingAttributeTest(context.getTestContext(), context.getRenderContext(), type));

            // For types which occupy more than one location. Alias second location.
            if (type.getLocationSize() > 1)
                aliasingGroup->addChild(
                    new gls::BindAliasingAttributeTest(context.getTestContext(), context.getRenderContext(), type, 1));

            // Use more than maximum attributes with aliasing
            aliasingGroup->addChild(
                new gls::BindMaxAliasingAttributeTest(context.getTestContext(), context.getRenderContext(), type));

            // Use more than maximum attributes but inactive
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

    // Basic layout location attribute tests
    {
        TestCaseGroup *const layoutAttributeGroup =
            new TestCaseGroup(context, "layout", "Basic layout location tests.");

        root->addChild(layoutAttributeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];
            layoutAttributeGroup->addChild(
                new gls::LocationAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Test max attributes with layout locations
    {
        TestCaseGroup *const layoutMaxAttributeGroup = new TestCaseGroup(
            context, "layout_max_attributes", "Maximum attributes used with layout location qualifiers.");

        root->addChild(layoutMaxAttributeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];
            layoutMaxAttributeGroup->addChild(
                new gls::LocationMaxAttributesTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Test filling holes in attribute location
    {
        TestCaseGroup *const holeGroup =
            new TestCaseGroup(context, "layout_hole",
                              "Define layout location for all, but one attribute consuming max attribute locations.");

        root->addChild(holeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];

            // Location first location, leave hole size of type and fill rest of locations
            holeGroup->addChild(
                new gls::LocationHoleAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Basic mixed mixed attribute tests
    {
        TestCaseGroup *const mixedAttributeGroup = new TestCaseGroup(context, "mixed", "Basic mixed location tests.");

        root->addChild(mixedAttributeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];
            mixedAttributeGroup->addChild(
                new gls::MixedAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    {
        TestCaseGroup *const mixedMaxAttributeGroup = new TestCaseGroup(
            context, "mixed_max_attributes", "Maximum attributes used with mixed binding and layout qualifiers.");

        root->addChild(mixedMaxAttributeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];
            mixedMaxAttributeGroup->addChild(
                new gls::MixedMaxAttributesTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Test mixed binding at different times
    {
        TestCaseGroup *const mixedTimeGroup =
            new TestCaseGroup(context, "mixed_time", "Bind time tests. Test binding at different stages.");

        root->addChild(mixedTimeGroup);

        mixedTimeGroup->addChild(
            new gls::PreAttachMixedAttributeTest(context.getTestContext(), context.getRenderContext()));
        mixedTimeGroup->addChild(
            new gls::PreLinkMixedAttributeTest(context.getTestContext(), context.getRenderContext()));
        mixedTimeGroup->addChild(
            new gls::PostLinkMixedAttributeTest(context.getTestContext(), context.getRenderContext()));
        mixedTimeGroup->addChild(
            new gls::MixedRelinkAttributeTest(context.getTestContext(), context.getRenderContext()));
        mixedTimeGroup->addChild(
            new gls::MixedReattachAttributeTest(context.getTestContext(), context.getRenderContext()));
    }

    {
        TestCaseGroup *const holeGroup = new TestCaseGroup(
            context, "mixed_hole",
            "Use layout location qualifiers and binding. Leave hole in location space for only free attribute.");

        root->addChild(holeGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];

            holeGroup->addChild(
                new gls::MixedHoleAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Test hole in location space that moves when relinking
    {
        TestCaseGroup *const relinkBindHoleGroup = new TestCaseGroup(
            context, "bind_relink_hole", "Test relinking with moving hole in attribute location space.");

        root->addChild(relinkBindHoleGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];

            relinkBindHoleGroup->addChild(
                new gls::BindRelinkHoleAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    // Test hole in location space that moves when relinking
    {
        TestCaseGroup *const relinkMixedHoleGroup = new TestCaseGroup(
            context, "mixed_relink_hole", "Test relinking with moving hole in attribute location space.");

        root->addChild(relinkMixedHoleGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); typeNdx++)
        {
            const AttribType &type = types[typeNdx];

            relinkMixedHoleGroup->addChild(
                new gls::MixedRelinkHoleAttributeTest(context.getTestContext(), context.getRenderContext(), type));
        }
    }

    return root;
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
