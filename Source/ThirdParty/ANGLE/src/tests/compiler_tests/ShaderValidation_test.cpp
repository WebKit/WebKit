//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderValidation_test.cpp:
//   Tests that malformed shaders fail compilation, and that correct shaders pass compilation.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "GLSLANG/ShaderLang.h"
#include "gtest/gtest.h"
#include "tests/test_utils/ShaderCompileTreeTest.h"

using namespace sh;

// Tests that don't target a specific version of the API spec (sometimes there are minor
// differences). They choose the shader spec version with version directives.
class FragmentShaderValidationTest : public ShaderCompileTreeTest
{
  public:
    FragmentShaderValidationTest() {}

  protected:
    void initResources(ShBuiltInResources *resources) override { resources->MaxDrawBuffers = 8; }

    ::GLenum getShaderType() const override { return GL_FRAGMENT_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_GLES3_1_SPEC; }
};

// Tests that don't target a specific version of the API spec (sometimes there are minor
// differences). They choose the shader spec version with version directives.
class VertexShaderValidationTest : public ShaderCompileTreeTest
{
  public:
    VertexShaderValidationTest() {}

  protected:
    ::GLenum getShaderType() const override { return GL_VERTEX_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_GLES3_1_SPEC; }
};

class WebGL2FragmentShaderValidationTest : public ShaderCompileTreeTest
{
  public:
    WebGL2FragmentShaderValidationTest() {}

  protected:
    ::GLenum getShaderType() const override { return GL_FRAGMENT_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_WEBGL2_SPEC; }
};

class WebGL1FragmentShaderValidationTest : public ShaderCompileTreeTest
{
  public:
    WebGL1FragmentShaderValidationTest() {}

  protected:
    ::GLenum getShaderType() const override { return GL_FRAGMENT_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_WEBGL_SPEC; }
};

class ComputeShaderValidationTest : public ShaderCompileTreeTest
{
  public:
    ComputeShaderValidationTest() {}

  protected:
    ::GLenum getShaderType() const override { return GL_COMPUTE_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_GLES3_1_SPEC; }
};

class ComputeShaderEnforcePackingValidationTest : public ComputeShaderValidationTest
{
  public:
    ComputeShaderEnforcePackingValidationTest() {}

  protected:
    void initResources(ShBuiltInResources *resources) override
    {
        resources->MaxComputeUniformComponents = kMaxComputeUniformComponents;

        // We need both MaxFragmentUniformVectors and MaxFragmentUniformVectors smaller than
        // MaxComputeUniformComponents / 4.
        resources->MaxVertexUniformVectors   = 16;
        resources->MaxFragmentUniformVectors = 16;
    }

    void SetUp() override
    {
        mCompileOptions.enforcePackingRestrictions = true;
        ShaderCompileTreeTest::SetUp();
    }

    // It is unnecessary to use a very large MaxComputeUniformComponents in this test.
    static constexpr GLint kMaxComputeUniformComponents = 128;
};

class GeometryShaderValidationTest : public ShaderCompileTreeTest
{
  public:
    GeometryShaderValidationTest() {}

  protected:
    void initResources(ShBuiltInResources *resources) override
    {
        resources->EXT_geometry_shader = 1;
    }
    ::GLenum getShaderType() const override { return GL_GEOMETRY_SHADER_EXT; }
    ShShaderSpec getShaderSpec() const override { return SH_GLES3_1_SPEC; }
};

class FragmentShaderEXTGeometryShaderValidationTest : public FragmentShaderValidationTest
{
  public:
    FragmentShaderEXTGeometryShaderValidationTest() {}

  protected:
    void initResources(ShBuiltInResources *resources) override
    {
        resources->EXT_geometry_shader = 1;
    }
};

// The local_size layout qualifier is only available in compute shaders.
TEST_F(GeometryShaderValidationTest, InvalidUseOfLocalSizeX)
{
    const std::string &shaderString1 =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        layout (points, local_size_x = 15) in;
        layout (points, max_vertices = 2) out;
        void main()
        {
        })";

    const std::string &shaderString2 =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        layout (points) in;
        layout (invocations = 2, local_size_x = 15) in;
        layout (points, max_vertices = 2) out;
        void main()
        {
        })";

    const std::string &shaderString3 =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        layout (points) in;
        layout (points, local_size_x = 15, max_vertices = 2) out;
        void main()
        {
        })";

    const std::string &shaderString4 =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        layout (points) in;
        layout (points) out;
        layout (max_vertices = 2, local_size_x = 15) out;
        void main()
        {
        })";
    if (compile(shaderString1) || compile(shaderString2) || compile(shaderString3) ||
        compile(shaderString4))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is a compile time error to use the gl_WorkGroupSize constant if
// the local size has not been declared yet.
// GLSL ES 3.10 Revision 4, 7.1.3 Compute Shader Special Variables
TEST_F(ComputeShaderValidationTest, InvalidUsageOfWorkGroupSize)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "   uvec3 WorkGroupSize = gl_WorkGroupSize;\n"
        "}\n"
        "layout(local_size_x = 12) in;\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The test covers the compute shader built-in variables and constants.
TEST_F(ComputeShaderValidationTest, CorrectUsageOfComputeBuiltins)
{
    const std::string &shaderString =
        R"(#version 310 es
        layout(local_size_x=4, local_size_y=3, local_size_z=2) in;
        layout(rgba32ui) uniform highp writeonly uimage2D imageOut;
        void main()
        {
            uvec3 temp1 = gl_NumWorkGroups;
            uvec3 temp2 = gl_WorkGroupSize;
            uvec3 temp3 = gl_WorkGroupID;
            uvec3 temp4 = gl_LocalInvocationID;
            uvec3 temp5 = gl_GlobalInvocationID;
            uint  temp6 = gl_LocalInvocationIndex;
            imageStore(imageOut, ivec2(0), uvec4(temp1 + temp2 + temp3 + temp4 + temp5, temp6));
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// It is illegal to write to a special variable.
TEST_F(ComputeShaderValidationTest, SpecialVariableNumWorkGroups)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 12) in;\n"
        "void main()\n"
        "{\n"
        "   gl_NumWorkGroups = uvec3(1); \n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is illegal to write to a special variable.
TEST_F(ComputeShaderValidationTest, SpecialVariableWorkGroupID)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 12) in;\n"
        "void main()\n"
        "{\n"
        "   gl_WorkGroupID = uvec3(1); \n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is illegal to write to a special variable.
TEST_F(ComputeShaderValidationTest, SpecialVariableLocalInvocationID)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 12) in;\n"
        "void main()\n"
        "{\n"
        "   gl_LocalInvocationID = uvec3(1); \n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is illegal to write to a special variable.
TEST_F(ComputeShaderValidationTest, SpecialVariableGlobalInvocationID)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 12) in;\n"
        "void main()\n"
        "{\n"
        "   gl_GlobalInvocationID = uvec3(1); \n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is illegal to write to a special variable.
TEST_F(ComputeShaderValidationTest, SpecialVariableLocalInvocationIndex)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 12) in;\n"
        "void main()\n"
        "{\n"
        "   gl_LocalInvocationIndex = 1; \n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is illegal to write to a special variable.
TEST_F(ComputeShaderValidationTest, SpecialVariableWorkGroupSize)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 12) in;\n"
        "void main()\n"
        "{\n"
        "   gl_WorkGroupSize = uvec3(1); \n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is illegal to apply an unary operator to a sampler.
TEST_F(FragmentShaderValidationTest, SamplerUnaryOperator)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform sampler2D s;\n"
        "void main()\n"
        "{\n"
        "   -s;\n"
        "   gl_FragColor = vec4(0);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invariant cannot be used with a work group size declaration.
TEST_F(ComputeShaderValidationTest, InvariantBlockSize)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "invariant layout(local_size_x = 15) in;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invariant cannot be used with a non-output variable in ESSL3.
TEST_F(FragmentShaderValidationTest, InvariantNonOuput)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "invariant int value;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invariant cannot be used with a non-output variable in ESSL3.
// ESSL 3.00.6 section 4.8: This applies even if the declaration is empty.
TEST_F(FragmentShaderValidationTest, InvariantNonOuputEmptyDeclaration)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "invariant in float;\n"
        "void main() {}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invariant declaration should follow the following format "invariant <out variable name>".
// Test having an incorrect qualifier in the invariant declaration.
TEST_F(FragmentShaderValidationTest, InvariantDeclarationWithStorageQualifier)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 foo;\n"
        "invariant centroid foo;\n"
        "void main() {\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invariant declaration should follow the following format "invariant <out variable name>".
// Test having an incorrect precision qualifier in the invariant declaration.
TEST_F(FragmentShaderValidationTest, InvariantDeclarationWithPrecisionQualifier)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 foo;\n"
        "invariant highp foo;\n"
        "void main() {\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invariant declaration should follow the following format "invariant <out variable name>".
// Test having an incorrect layout qualifier in the invariant declaration.
TEST_F(FragmentShaderValidationTest, InvariantDeclarationWithLayoutQualifier)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 foo;\n"
        "invariant layout(location=0) foo;\n"
        "void main() {\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Variable declaration with both invariant and layout qualifiers is not valid in the formal grammar
// provided in the ESSL 3.00 spec. ESSL 3.10 starts allowing this combination, but ESSL 3.00 should
// still disallow it.
TEST_F(FragmentShaderValidationTest, VariableDeclarationWithInvariantAndLayoutQualifierESSL300)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "invariant layout(location = 0) out vec4 my_FragColor;\n"
        "void main() {\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Bit shift with a rhs value > 31 has an undefined result in the GLSL spec. Detecting an undefined
// result at compile time should not generate an error either way.
// ESSL 3.00.6 section 5.9.
TEST_F(FragmentShaderValidationTest, ShiftBy32)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        out uint my_out;
        void main() {
           my_out = 1u << 32u;
        })";
    if (compile(shaderString))
    {
        if (!hasWarning())
        {
            FAIL() << "Shader compilation succeeded without warnings, expecting warning:\n"
                   << mInfoLog;
        }
    }
    else
    {
        FAIL() << "Shader compilation failed, expecting success with warning:\n" << mInfoLog;
    }
}

// Bit shift with a rhs value < 0 has an undefined result in the GLSL spec. Detecting an undefined
// result at compile time should not generate an error either way.
// ESSL 3.00.6 section 5.9.
TEST_F(FragmentShaderValidationTest, ShiftByNegative)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        out uint my_out;
        void main() {
           my_out = 1u << (-1);
        })";
    if (compile(shaderString))
    {
        if (!hasWarning())
        {
            FAIL() << "Shader compilation succeeded without warnings, expecting warning:\n"
                   << mInfoLog;
        }
    }
    else
    {
        FAIL() << "Shader compilation failed, expecting success with warning:\n" << mInfoLog;
    }
}

// Test that pruning empty declarations from loop init expression works.
TEST_F(FragmentShaderValidationTest, EmptyDeclarationAsLoopInit)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    int i = 0;\n"
        "    for (int; i < 3; i++)\n"
        "    {\n"
        "        my_FragColor = vec4(i);\n"
        "    }\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}
// r32f, r32i, r32ui do not require either the writeonly or readonly memory qualifiers.
// GLSL ES 3.10, Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, ImageR32FNoMemoryQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "in vec4 myInput;\n"
        "layout(r32f) uniform image2D myImage;\n"
        "void main() {\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Images which do not have r32f, r32i or r32ui as internal format, must have readonly or writeonly
// specified.
// GLSL ES 3.10, Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, ImageRGBA32FWithIncorrectMemoryQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "in vec4 myInput;\n"
        "layout(rgba32f) uniform image2D myImage;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is a compile-time error to call imageStore when the image is qualified as readonly.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, StoreInReadOnlyImage)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "in vec4 myInput;\n"
        "layout(r32f) uniform readonly image2D myImage;\n"
        "void main() {\n"
        "   imageStore(myImage, ivec2(0), vec4(1.0));\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is a compile-time error to call imageLoad when the image is qualified as writeonly.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, LoadFromWriteOnlyImage)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "in vec4 myInput;\n"
        "layout(r32f) uniform writeonly image2D myImage;\n"
        "void main() {\n"
        "   imageLoad(myImage, ivec2(0));\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is a compile-time error to call imageStore when the image is qualified as readonly.
// Test to make sure this is validated correctly for images in arrays.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, StoreInReadOnlyImageArray)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "in vec4 myInput;\n"
        "layout(r32f) uniform readonly image2D myImage[2];\n"
        "void main() {\n"
        "   imageStore(myImage[0], ivec2(0), vec4(1.0));\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It is a compile-time error to call imageStore when the image is qualified as readonly.
// Test to make sure that checking this doesn't crash when validating an image in a struct.
// Image in a struct in itself isn't accepted by the parser, but error recovery still results in
// an image in the struct.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, StoreInReadOnlyImageInStruct)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "in vec4 myInput;\n"
        "uniform struct S {\n"
        "    layout(r32f) readonly image2D myImage;\n"
        "} s;\n"
        "void main() {\n"
        "   imageStore(s.myImage, ivec2(0), vec4(1.0));\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// A valid declaration and usage of an image3D.
TEST_F(FragmentShaderValidationTest, ValidImage3D)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image3D;\n"
        "in vec4 myInput;\n"
        "layout(rgba32f) uniform readonly image3D myImage;\n"
        "void main() {\n"
        "   imageLoad(myImage, ivec3(0));\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// A valid declaration and usage of an imageCube.
TEST_F(FragmentShaderValidationTest, ValidImageCube)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump imageCube;\n"
        "in vec4 myInput;\n"
        "layout(rgba32f) uniform readonly imageCube myImage;\n"
        "void main() {\n"
        "   imageLoad(myImage, ivec3(0));\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// A valid declaration and usage of an image2DArray.
TEST_F(FragmentShaderValidationTest, ValidImage2DArray)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2DArray;\n"
        "in vec4 myInput;\n"
        "layout(rgba32f) uniform readonly image2DArray myImage;\n"
        "void main() {\n"
        "   imageLoad(myImage, ivec3(0));\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Images cannot be l-values.
// GLSL ES 3.10 Revision 4, 4.1.7 Opaque Types
TEST_F(FragmentShaderValidationTest, ImageLValueFunctionDefinitionInOut)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "void myFunc(inout image2D someImage) {}\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Cannot assign to images.
// GLSL ES 3.10 Revision 4, 4.1.7 Opaque Types
TEST_F(FragmentShaderValidationTest, ImageAssignment)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(rgba32f) uniform readonly image2D myImage;\n"
        "layout(rgba32f) uniform readonly image2D myImage2;\n"
        "void main() {\n"
        "   myImage = myImage2;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Passing an image qualifier to a function should not be able to discard the readonly qualifier.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, ReadOnlyQualifierMissingInFunctionArgument)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(rgba32f) uniform readonly image2D myImage;\n"
        "void myFunc(in image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Passing an image qualifier to a function should not be able to discard the readonly qualifier.
// Test with an image from an array.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, ReadOnlyQualifierMissingInFunctionArgumentArray)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(rgba32f) uniform readonly image2D myImage[2];\n"
        "void myFunc(in image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage[0]);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Passing an image qualifier to a function should not be able to discard the readonly qualifier.
// Test that validation doesn't crash on this for an image in a struct.
// Image in a struct in itself isn't accepted by the parser, but error recovery still results in
// an image in the struct.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, ReadOnlyQualifierMissingInFunctionArgumentStruct)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "uniform struct S {\n"
        "    layout(r32f) readonly image2D myImage;\n"
        "} s;\n"
        "void myFunc(in image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(s.myImage);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Passing an image qualifier to a function should not be able to discard the writeonly qualifier.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, WriteOnlyQualifierMissingInFunctionArgument)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(rgba32f) uniform writeonly image2D myImage;\n"
        "void myFunc(in image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Passing an image parameter as an argument to another function should not be able to discard the
// writeonly qualifier.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, DiscardWriteonlyInFunctionBody)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(rgba32f) uniform writeonly image2D myImage;\n"
        "void myFunc1(in image2D someImage) {}\n"
        "void myFunc2(in writeonly image2D someImage) { myFunc1(someImage); }\n"
        "void main() {\n"
        "   myFunc2(myImage);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The memory qualifiers for the image declaration and function argument match and the test should
// pass.
TEST_F(FragmentShaderValidationTest, CorrectImageMemoryQualifierSpecified)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(r32f) uniform image2D myImage;\n"
        "void myFunc(in image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage);\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// The test adds additional qualifiers to the argument in the function header.
// This is correct since no memory qualifiers are discarded upon the function call.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, CorrectImageMemoryQualifierSpecified2)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(r32f) uniform image2D myImage;\n"
        "void myFunc(in readonly writeonly image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage);\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Images are not allowed in structs.
// GLSL ES 3.10 Revision 4, 4.1.8 Structures
TEST_F(FragmentShaderValidationTest, ImageInStruct)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "struct myStruct { layout(r32f) image2D myImage; };\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Images are not allowed in interface blocks.
// GLSL ES 3.10 Revision 4, 4.3.9 Interface Blocks
TEST_F(FragmentShaderValidationTest, ImageInInterfaceBlock)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "uniform myBlock { layout(r32f) image2D myImage; };\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Readonly used with an interface block.
TEST_F(FragmentShaderValidationTest, ReadonlyWithInterfaceBlock)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "uniform readonly myBlock { float something; };\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Readonly used with an invariant.
TEST_F(FragmentShaderValidationTest, ReadonlyWithInvariant)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 something;\n"
        "invariant readonly something;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Readonly used with a member of a structure.
TEST_F(FragmentShaderValidationTest, ReadonlyWithStructMember)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 something;\n"
        "struct MyStruct { readonly float myMember; };\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It should not be possible to use an internal format layout qualifier with an interface block.
TEST_F(FragmentShaderValidationTest, ImageInternalFormatWithInterfaceBlock)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 something;\n"
        "layout(rgba32f) uniform MyStruct { float myMember; };\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// It should not be possible to use an internal format layout qualifier with a uniform without a
// type.
TEST_F(FragmentShaderValidationTest, ImageInternalFormatInGlobalLayoutQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 something;\n"
        "layout(rgba32f) uniform;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// ESSL 1.00 section 4.1.7.
// Samplers are not allowed as operands for most operations. Test this for ternary operator.
TEST_F(FragmentShaderValidationTest, SamplerAsTernaryOperand)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform bool u;\n"
        "uniform sampler2D s1;\n"
        "uniform sampler2D s2;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(u ? s1 : s2, vec2(0, 0));\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// ESSL 1.00.17 section 4.5.2.
// ESSL 3.00.6 section 4.5.3.
// Precision must be specified for floats. Test this with a declaration with no qualifiers.
TEST_F(FragmentShaderValidationTest, FloatDeclarationNoQualifiersNoPrecision)
{
    const std::string &shaderString =
        "vec4 foo = vec4(0.0);\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = foo;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Precision must be specified for floats. Test this with a function argument no qualifiers.
TEST_F(FragmentShaderValidationTest, FloatDeclarationNoQualifiersNoPrecisionFunctionArg)
{
    const std::string &shaderString = R"(
int c(float x)
{
    return int(x);
}
void main()
{
    c(5.0);
})";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Check compiler doesn't crash on incorrect unsized array declarations.
TEST_F(FragmentShaderValidationTest, IncorrectUnsizedArray)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "float foo[] = 0.0;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    foo[0] = 1.0;\n"
        "    my_FragColor = vec4(foo[0]);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Check compiler doesn't crash when a bvec is on the right hand side of a logical operator.
// ESSL 3.00.6 section 5.9.
TEST_F(FragmentShaderValidationTest, LogicalOpRHSIsBVec)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "void main()\n"
        "{\n"
        "    bool b;\n"
        "    bvec3 b3;\n"
        "    b && b3;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Check compiler doesn't crash when there's an unsized array constructor with no parameters.
// ESSL 3.00.6 section 4.1.9: Array size must be greater than zero.
TEST_F(FragmentShaderValidationTest, UnsizedArrayConstructorNoParameters)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "void main()\n"
        "{\n"
        "    int[]();\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Passing an image parameter as an argument to another function should not be able to discard the
// coherent qualifier.
TEST_F(FragmentShaderValidationTest, CoherentQualifierMissingInFunctionArgument)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(r32f) uniform coherent image2D myImage;\n"
        "void myFunc(in image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Passing an image parameter as an argument to another function should not be able to discard the
// volatile qualifier.
TEST_F(FragmentShaderValidationTest, VolatileQualifierMissingInFunctionArgument)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(r32f) uniform volatile image2D myImage;\n"
        "void myFunc(in image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The restrict qualifier can be discarded from a function argument.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, RestrictQualifierDiscardedInFunctionArgument)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(r32f) uniform restrict image2D myImage;\n"
        "void myFunc(in image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage);\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Function image arguments can be overqualified.
// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
TEST_F(FragmentShaderValidationTest, OverqualifyingImageParameter)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(r32f) uniform image2D myImage;\n"
        "void myFunc(in coherent volatile image2D someImage) {}\n"
        "void main() {\n"
        "   myFunc(myImage);\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that work group size can be used to size arrays.
// GLSL ES 3.10.4 section 7.1.3 Compute Shader Special Variables
TEST_F(ComputeShaderValidationTest, WorkGroupSizeAsArraySize)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 5, local_size_y = 3, local_size_z = 1) in;\n"
        "void main()\n"
        "{\n"
        "    int[gl_WorkGroupSize.x] a = int[5](0, 0, 0, 0, 0);\n"
        "    int[gl_WorkGroupSize.y] b = int[3](0, 0, 0);\n"
        "    int[gl_WorkGroupSize.z] c = int[1](0);\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Shared memory variables cannot be used inside a vertex shader.
// GLSL ES 3.10 Revision 4, 4.3.8 Shared Variables
TEST_F(VertexShaderValidationTest, VertexShaderSharedMemory)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec4 i;\n"
        "shared float myShared[10];\n"
        "void main() {\n"
        "    gl_Position = i;\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Shared memory variables cannot be used inside a fragment shader.
// GLSL ES 3.10 Revision 4, 4.3.8 Shared Variables
TEST_F(FragmentShaderValidationTest, FragmentShaderSharedMemory)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "shared float myShared[10];\n"
        "out vec4 color;\n"
        "void main() {\n"
        "   color = vec4(1.0);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Shared memory cannot be combined with any other storage qualifier.
TEST_F(ComputeShaderValidationTest, UniformSharedMemory)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "uniform shared float myShared[100];\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Correct usage of shared memory variables.
TEST_F(ComputeShaderValidationTest, CorrectUsageOfSharedMemory)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "shared float myShared[100];\n"
        "void main() {\n"
        "   myShared[gl_LocalInvocationID.x] = 1.0;\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Shared memory variables cannot be initialized.
// GLSL ES 3.10 Revision 4, 4.3.8 Shared Variables
TEST_F(ComputeShaderValidationTest, SharedVariableInitialization)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "shared int myShared = 0;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Local variables cannot be qualified as shared.
// GLSL ES 3.10 Revision 4, 4.3 Storage Qualifiers
TEST_F(ComputeShaderValidationTest, SharedMemoryInFunctionBody)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "void func() {\n"
        "   shared int myShared;\n"
        "}\n"
        "void main() {\n"
        "   func();\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Struct members cannot be qualified as shared.
TEST_F(ComputeShaderValidationTest, SharedMemoryInStruct)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "struct MyStruct {\n"
        "   shared int myShared;\n"
        "};\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Interface block members cannot be qualified as shared.
TEST_F(ComputeShaderValidationTest, SharedMemoryInInterfaceBlock)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "uniform Myblock {\n"
        "   shared int myShared;\n"
        "};\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The shared qualifier cannot be used with any other qualifier.
TEST_F(ComputeShaderValidationTest, SharedWithInvariant)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "invariant shared int myShared;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The shared qualifier cannot be used with any other qualifier.
TEST_F(ComputeShaderValidationTest, SharedWithMemoryQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "readonly shared int myShared;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The shared qualifier cannot be used with any other qualifier.
TEST_F(ComputeShaderValidationTest, SharedGlobalLayoutDeclaration)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 5) in;\n"
        "layout(row_major) shared mat4;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Declaring a function with the same name as a built-in from a higher ESSL version should not cause
// a redeclaration error.
TEST_F(FragmentShaderValidationTest, BuiltinESSL31FunctionDeclaredInESSL30Shader)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "void imageSize() {}\n"
        "void main() {\n"
        "   imageSize();\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Attempting to declare num_views without enabling OVR_multiview.
TEST_F(VertexShaderValidationTest, InvalidNumViews)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "layout (num_views = 2) in;\n"
        "void main() {\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// memoryBarrierShared is only available in a compute shader.
// GLSL ES 3.10 Revision 4, 8.15 Shader Memory Control Functions
TEST_F(FragmentShaderValidationTest, InvalidUseOfMemoryBarrierShared)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "void main() {\n"
        "    memoryBarrierShared();\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// groupMemoryBarrier is only available in a compute shader.
// GLSL ES 3.10 Revision 4, 8.15 Shader Memory Control Functions
TEST_F(FragmentShaderValidationTest, InvalidUseOfGroupMemoryBarrier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "void main() {\n"
        "    groupMemoryBarrier();\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// barrier can be used in a compute shader.
// GLSL ES 3.10 Revision 4, 8.14 Shader Invocation Control Functions
TEST_F(ComputeShaderValidationTest, ValidUseOfBarrier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 15) in;\n"
        "void main() {\n"
        "   barrier();\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success " << mInfoLog;
    }
}

// memoryBarrierImage() can be used in all GLSL ES 3.10 shaders.
// GLSL ES 3.10 Revision 4, 8.15 Shader Memory Control Functions
TEST_F(FragmentShaderValidationTest, ValidUseOfMemoryBarrierImageInFragmentShader)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision highp image2D;\n"
        "layout(r32f) uniform image2D myImage;\n"
        "void main() {\n"
        "    imageStore(myImage, ivec2(0), vec4(1.0));\n"
        "    memoryBarrierImage();\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success " << mInfoLog;
    }
}

// checks that gsampler2DMS is not supported in version lower than 310
TEST_F(FragmentShaderValidationTest, Sampler2DMSInESSL300Shader)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "uniform highp sampler2DMS s;\n"
        "void main() {}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Declare main() with incorrect parameters.
// ESSL 3.00.6 section 6.1 Function Definitions.
TEST_F(FragmentShaderValidationTest, InvalidMainPrototypeParameters)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "void main(int a);\n"
        "void main() {}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Regression test for a crash in the empty constructor of unsized array
// of a structure with non-basic fields fields. Test with "void".
TEST_F(FragmentShaderValidationTest, VoidFieldStructUnsizedArrayEmptyConstructor)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "struct S {void a;};"
        "void main() {S s[] = S[]();}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Regression test for a crash in the empty constructor of unsized array
// of a structure with non-basic fields fields. Test with something other than "void".
TEST_F(FragmentShaderValidationTest, SamplerFieldStructUnsizedArrayEmptyConstructor)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "struct S {sampler2D a;};"
        "void main() {S s[] = S[]();}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Checks that odd array initialization syntax is an error, and does not produce
// an ASSERT failure.
TEST_F(VertexShaderValidationTest, InvalidArrayConstruction)
{
    const std::string &shaderString =
        "struct S { mediump float i; mediump int ggb; };\n"
        "void main() {\n"
        "  S s[2];\n"
        "  s = S[](s.x, 0.0);\n"
        "  gl_Position = vec4(1, 0, 0, 1);\n"
        "}";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Correct usage of image binding layout qualifier.
TEST_F(ComputeShaderValidationTest, CorrectImageBindingLayoutQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump image2D;\n"
        "layout(local_size_x = 5) in;\n"
        "layout(binding = 1, rgba32f) writeonly uniform image2D myImage;\n"
        "void main()\n"
        "{\n"
        "   imageStore(myImage, ivec2(gl_LocalInvocationID.xy), vec4(1.0));\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success " << mInfoLog;
    }
}

// Incorrect use of "binding" on a global layout qualifier.
TEST_F(ComputeShaderValidationTest, IncorrectGlobalBindingLayoutQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 5, binding = 0) in;\n"
        "void main() {}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Incorrect use of "binding" on a struct field layout qualifier.
TEST_F(ComputeShaderValidationTest, IncorrectStructFieldBindingLayoutQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(local_size_x = 1) in;\n"
        "struct S\n"
        "{\n"
        "  layout(binding = 0) float f;\n"
        "};\n"
        "void main() {}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Variable binding layout qualifier is set to a negative value. 0xffffffff wraps around to -1
// according to the integer parsing rules.
TEST_F(FragmentShaderValidationTest, ImageBindingUnitNegative)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(rgba32f, binding = 0xffffffff) writeonly uniform mediump image2D myImage;\n"
        "out vec4 outFrag;\n"
        "void main()\n"
        "{\n"
        "   outFrag = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Image binding layout qualifier value is greater than the maximum image binding.
TEST_F(FragmentShaderValidationTest, ImageBindingUnitTooBig)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(rgba32f, binding = 9999) writeonly uniform mediump image2D myImage;\n"
        "out vec4 outFrag;\n"
        "void main()\n"
        "{\n"
        "   outFrag = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Uniform variable binding is set on a non-opaque type.
TEST_F(FragmentShaderValidationTest, NonOpaqueUniformBinding)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(binding = 0) uniform float myFloat;\n"
        "out vec4 outFrag;\n"
        "void main()\n"
        "{\n"
        "   outFrag = vec4(myFloat);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Uniform variable binding is set on a sampler type.
// ESSL 3.10 section 4.4.5 Opaque Uniform Layout Qualifiers.
TEST_F(FragmentShaderValidationTest, SamplerUniformBinding)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(binding = 0) uniform mediump sampler2D mySampler;\n"
        "out vec4 outFrag;\n"
        "void main()\n"
        "{\n"
        "   outFrag = vec4(0.0);\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success " << mInfoLog;
    }
}

// Uniform variable binding is set on a sampler type in an ESSL 3.00 shader.
// The binding layout qualifier was added in ESSL 3.10, so this is incorrect.
TEST_F(FragmentShaderValidationTest, SamplerUniformBindingESSL300)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "layout(binding = 0) uniform mediump sampler2D mySampler;\n"
        "out vec4 outFrag;\n"
        "void main()\n"
        "{\n"
        "   outFrag = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Attempting to construct a struct containing a void array should fail without asserting.
TEST_F(FragmentShaderValidationTest, ConstructStructContainingVoidArray)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 outFrag;\n"
        "struct S\n"
        "{\n"
        "    void A[1];\n"
        "} s = S();\n"
        "void main()\n"
        "{\n"
        "    outFrag = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure " << mInfoLog;
    }
}

// Uniforms can't have location in ESSL 3.00.
// Test this with an empty declaration (ESSL 3.00.6 section 4.8: The combinations of qualifiers that
// cause compile-time or link-time errors are the same whether or not the declaration is empty).
TEST_F(FragmentShaderValidationTest, UniformLocationEmptyDeclaration)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "layout(location=0) uniform float;\n"
        "void main() {}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test function parameters of opaque type can't be l-value too.
TEST_F(FragmentShaderValidationTest, OpaqueParameterCanNotBeLValue)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "uniform sampler2D s;\n"
        "void foo(sampler2D as) {\n"
        "    as = s;\n"
        "}\n"
        "void main() {}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test samplers must not be operands in expressions, except for array indexing, structure field
// selection and parentheses(ESSL 3.00 Secion 4.1.7).
TEST_F(FragmentShaderValidationTest, InvalidExpressionForSamplerOperands)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "uniform sampler2D s;\n"
        "uniform sampler2D s2;\n"
        "void main() {\n"
        "    s + s2;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test interface blocks as invalid operands to a binary expression.
TEST_F(FragmentShaderValidationTest, InvalidInterfaceBlockBinaryExpression)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "uniform U\n"
        "{\n"
        "    int foo; \n"
        "} u;\n"
        "void main()\n"
        "{\n"
        "    u + u;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test interface block as an invalid operand to an unary expression.
TEST_F(FragmentShaderValidationTest, InvalidInterfaceBlockUnaryExpression)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "uniform U\n"
        "{\n"
        "    int foo; \n"
        "} u;\n"
        "void main()\n"
        "{\n"
        "    +u;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test interface block as an invalid operand to a ternary expression.
// Note that the spec is not very explicit on this, but it makes sense to forbid this.
TEST_F(FragmentShaderValidationTest, InvalidInterfaceBlockTernaryExpression)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "uniform U\n"
        "{\n"
        "    int foo; \n"
        "} u;\n"
        "void main()\n"
        "{\n"
        "    true ? u : u;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that "buffer" and "shared" are valid identifiers in version lower than GLSL ES 3.10.
TEST_F(FragmentShaderValidationTest, BufferAndSharedAsIdentifierOnES3)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision highp float;
        out vec4 my_out;
        void main()
        {
            int buffer = 1;
            int shared = 2;
            my_out = vec4(buffer + shared);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that a struct can not be used as a constructor argument for a scalar.
TEST_F(FragmentShaderValidationTest, StructAsBoolConstructorArgument)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "struct my_struct\n"
        "{\n"
        "    float f;\n"
        "};\n"
        "my_struct a = my_struct(1.0);\n"
        "void main(void)\n"
        "{\n"
        "    bool test = bool(a);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a compute shader can be compiled with MAX_COMPUTE_UNIFORM_COMPONENTS uniform
// components.
TEST_F(ComputeShaderEnforcePackingValidationTest, MaxComputeUniformComponents)
{
    GLint uniformVectorCount = kMaxComputeUniformComponents / 4;

    std::ostringstream ostream;
    ostream << "#version 310 es\n"
               "layout(local_size_x = 1) in;\n";

    for (GLint i = 0; i < uniformVectorCount; ++i)
    {
        ostream << "uniform vec4 u_value" << i << ";\n";
    }

    ostream << "void main()\n"
               "{\n";

    for (GLint i = 0; i < uniformVectorCount; ++i)
    {
        ostream << "    vec4 v" << i << " = u_value" << i << ";\n";
    }

    ostream << "}\n";

    if (!compile(ostream.str()))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that a function can't be declared with a name starting with "gl_". Note that it's important
// that the function is not being called.
TEST_F(FragmentShaderValidationTest, FunctionDeclaredWithReservedName)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "void gl_();\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a function can't be defined with a name starting with "gl_". Note that it's important
// that the function is not being called.
TEST_F(FragmentShaderValidationTest, FunctionDefinedWithReservedName)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "void gl_()\n"
        "{\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that ops with mismatching operand types are disallowed and don't result in an assert.
// This makes sure that constant folding doesn't fetch invalid union values in case operand types
// mismatch.
TEST_F(FragmentShaderValidationTest, InvalidOpsWithConstantOperandsDontAssert)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    float f1 = 0.5 / 2;\n"
        "    float f2 = true + 0.5;\n"
        "    float f3 = float[2](0.0, 1.0)[1.0];\n"
        "    float f4 = float[2](0.0, 1.0)[true];\n"
        "    float f5 = true ? 1.0 : 0;\n"
        "    float f6 = 1.0 ? 1.0 : 2.0;\n"
        "    my_FragColor = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that case labels with invalid types don't assert
TEST_F(FragmentShaderValidationTest, CaseLabelsWithInvalidTypesDontAssert)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform int i;\n"
        "void main()\n"
        "{\n"
        "    float f = 0.0;\n"
        "    switch (i)\n"
        "    {\n"
        "        case 0u:\n"
        "            f = 0.0;\n"
        "        case true:\n"
        "            f = 1.0;\n"
        "        case 2.0:\n"
        "            f = 2.0;\n"
        "    }\n"
        "    my_FragColor = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that using an array as an index is not allowed.
TEST_F(FragmentShaderValidationTest, ArrayAsIndex)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    int i[2] = int[2](0, 1);\n"
        "    float f[2] = float[2](2.0, 3.0);\n"
        "    my_FragColor = vec4(f[i]);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that using an array as an array size is not allowed.
TEST_F(FragmentShaderValidationTest, ArrayAsArraySize)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    const int i[2] = int[2](1, 2);\n"
        "    float f[i];\n"
        "    my_FragColor = vec4(f[0]);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The input primitive layout qualifier is only available in geometry shaders.
TEST_F(VertexShaderValidationTest, InvalidUseOfInputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(points) in vec4 myInput;\n"
        "out vec4 myOutput;\n"
        "void main() {\n"
        "   myOutput = myInput;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The input primitive layout qualifier is only available in geometry shaders.
TEST_F(FragmentShaderValidationTest, InvalidUseOfInputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(points) in vec4 myInput;\n"
        "out vec4 myOutput;\n"
        "void main() {\n"
        "   myOutput = myInput;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The input primitive layout qualifier is only available in geometry shaders.
TEST_F(ComputeShaderValidationTest, InvalidUseOfInputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(points, local_size_x = 12) in;\n"
        "void main()\n"
        "{\n"
        "   uvec3 WorkGroupSize = gl_WorkGroupSize;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The output primitive layout qualifier is only available in geometry shaders.
TEST_F(VertexShaderValidationTest, InvalidUseOfOutputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec4 myInput;\n"
        "layout(points) out vec4 myOutput;\n"
        "void main() {\n"
        "   myOutput = myInput;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The output primitive layout qualifier is only available in geometry shaders.
TEST_F(FragmentShaderValidationTest, InvalidUseOfOutputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec4 myInput;\n"
        "layout(points) out vec4 myOutput;\n"
        "void main() {\n"
        "   myOutput = myInput;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The 'invocations' layout qualifier is only available in geometry shaders.
TEST_F(VertexShaderValidationTest, InvalidUseOfInvocations)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout (invocations = 3) in vec4 myInput;\n"
        "out vec4 myOutput;\n"
        "void main() {\n"
        "   myOutput = myInput;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The 'invocations' layout qualifier is only available in geometry shaders.
TEST_F(FragmentShaderValidationTest, InvalidUseOfInvocations)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout (invocations = 3) in vec4 myInput;\n"
        "out vec4 myOutput;\n"
        "void main() {\n"
        "   myOutput = myInput;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The 'invocations' layout qualifier is only available in geometry shaders.
TEST_F(ComputeShaderValidationTest, InvalidUseOfInvocations)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(invocations = 3, local_size_x = 12) in;\n"
        "void main()\n"
        "{\n"
        "   uvec3 WorkGroupSize = gl_WorkGroupSize;\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The 'max_vertices' layout qualifier is only available in geometry shaders.
TEST_F(VertexShaderValidationTest, InvalidUseOfMaxVertices)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec4 myInput;\n"
        "layout(max_vertices = 3) out vec4 myOutput;\n"
        "void main() {\n"
        "   myOutput = myInput;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// The 'max_vertices' layout qualifier is only available in geometry shaders.
TEST_F(FragmentShaderValidationTest, InvalidUseOfMaxVertices)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec4 myInput;\n"
        "layout(max_vertices = 3) out vec4 myOutput;\n"
        "void main() {\n"
        "   myOutput = myInput;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that using the same variable name twice in function parameters fails without crashing.
TEST_F(FragmentShaderValidationTest, RedefinedParamInFunctionHeader)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void foo(int a, float a)\n"
        "{\n"
        "    return;\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(0.0);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that using gl_ViewportIndex is not allowed in an ESSL 3.10 shader.
TEST_F(VertexShaderValidationTest, ViewportIndexInESSL310)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(gl_ViewportIndex);\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that gl_PrimitiveID is valid in fragment shader with 'GL_EXT_geometry_shader' declared.
TEST_F(FragmentShaderEXTGeometryShaderValidationTest, PrimitiveIDWithExtension)
{
    const std::string &shaderString =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        precision mediump float;
        layout(location = 0) out mediump vec4 fragColor;
        void main(void)
        {
            vec4 data = vec4(0.1, 0.2, 0.3, 0.4);
            float value = data[gl_PrimitiveID % 4];
            fragColor = vec4(value, 0, 0, 1);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that gl_PrimitiveID is invalid in fragment shader without 'GL_EXT_geometry_shader' declared.
TEST_F(FragmentShaderEXTGeometryShaderValidationTest, PrimitiveIDWithoutExtension)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision mediump float;
        layout(location = 0) out mediump vec4 fragColor;
        void main(void)
        {
            vec4 data = vec4(0.1, 0.2, 0.3, 0.4);
            float value = data[gl_PrimitiveID % 4];
            fragColor = vec4(value, 0, 0, 1);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that gl_PrimitiveID cannot be l-value in fragment shader.
TEST_F(FragmentShaderEXTGeometryShaderValidationTest, AssignValueToPrimitiveID)
{
    const std::string &shaderString =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        precision mediump float;
        layout(location = 0) out mediump vec4 fragColor;
        void main(void)
        {
            gl_PrimitiveID = 1;
            fragColor = vec4(1.0, 0.0, 0.0, 1.0);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that gl_Layer is valid in fragment shader with 'GL_EXT_geometry_shader' declared.
TEST_F(FragmentShaderEXTGeometryShaderValidationTest, LayerWithExtension)
{
    const std::string &shaderString =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        precision mediump float;
        layout(location = 0) out mediump vec4 fragColor;
        void main(void)
        {
            vec4 data = vec4(0.1, 0.2, 0.3, 0.4);
            float value = data[gl_Layer % 4];
            fragColor = vec4(value, 0, 0, 1);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that gl_Layer is invalid in fragment shader without 'GL_EXT_geometry_shader' declared.
TEST_F(FragmentShaderEXTGeometryShaderValidationTest, LayerWithoutExtension)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision mediump float;
        layout(location = 0) out mediump vec4 fragColor;
        void main(void)
        {
            vec4 data = vec4(0.1, 0.2, 0.3, 0.4);
            float value = data[gl_Layer % 4];
            fragColor = vec4(value, 0, 0, 1);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that gl_Layer cannot be l-value in fragment shader.
TEST_F(FragmentShaderEXTGeometryShaderValidationTest, AssignValueToLayer)
{
    const std::string &shaderString =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        precision mediump float;
        layout(location = 0) out mediump vec4 fragColor;
        void main(void)
        {
            gl_Layer = 1;
            fragColor = vec4(1.0, 0.0, 0.0, 1.0);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that all built-in constants defined in GL_EXT_geometry_shader can be used in fragment shader
// with 'GL_EXT_geometry_shader' declared.
TEST_F(FragmentShaderEXTGeometryShaderValidationTest, GeometryShaderBuiltInConstants)
{
    const std::string &kShaderHeader =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        precision mediump float;
        layout(location = 0) out mediump vec4 fragColor;
        void main(void)
        {
            int val = )";

    const std::array<std::string, 9> kGeometryShaderBuiltinConstants = {{
        "gl_MaxGeometryInputComponents",
        "gl_MaxGeometryOutputComponents",
        "gl_MaxGeometryImageUniforms",
        "gl_MaxGeometryTextureImageUnits",
        "gl_MaxGeometryOutputVertices",
        "gl_MaxGeometryTotalOutputComponents",
        "gl_MaxGeometryUniformComponents",
        "gl_MaxGeometryAtomicCounters",
        "gl_MaxGeometryAtomicCounterBuffers",
    }};

    const std::string &kShaderTail =
        R"(;
            fragColor = vec4(val, 0, 0, 1);
        })";

    for (const std::string &kGSBuiltinConstant : kGeometryShaderBuiltinConstants)
    {
        std::ostringstream ostream;
        ostream << kShaderHeader << kGSBuiltinConstant << kShaderTail;
        if (!compile(ostream.str()))
        {
            FAIL() << "Shader compilation failed, expecting success: \n" << mInfoLog;
        }
    }
}

// Test that any built-in constants defined in GL_EXT_geometry_shader cannot be used in fragment
// shader without 'GL_EXT_geometry_shader' declared.
TEST_F(FragmentShaderEXTGeometryShaderValidationTest,
       GeometryShaderBuiltInConstantsWithoutExtension)
{
    const std::string &kShaderHeader =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout(location = 0) out mediump vec4 fragColor;\n"
        "void main(void)\n"
        "{\n"
        "    int val = ";

    const std::array<std::string, 9> kGeometryShaderBuiltinConstants = {{
        "gl_MaxGeometryInputComponents",
        "gl_MaxGeometryOutputComponents",
        "gl_MaxGeometryImageUniforms",
        "gl_MaxGeometryTextureImageUnits",
        "gl_MaxGeometryOutputVertices",
        "gl_MaxGeometryTotalOutputComponents",
        "gl_MaxGeometryUniformComponents",
        "gl_MaxGeometryAtomicCounters",
        "gl_MaxGeometryAtomicCounterBuffers",
    }};

    const std::string &kShaderTail =
        ";\n"
        "    fragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    for (const std::string &kGSBuiltinConstant : kGeometryShaderBuiltinConstants)
    {
        std::ostringstream ostream;
        ostream << kShaderHeader << kGSBuiltinConstant << kShaderTail;
        if (compile(ostream.str()))
        {
            FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
        }
    }
}

// Test that declaring and using an interface block with 'const' qualifier is not allowed.
TEST_F(VertexShaderValidationTest, InterfaceBlockUsingConstQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "const block\n"
        "{\n"
        "    vec2 value;\n"
        "} ConstBlock[2];\n"
        "void main()\n"
        "{\n"
        "    int i = 0;\n"
        "    vec2 value1 = ConstBlock[i].value;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that using shader io blocks without declaration of GL_EXT_shader_io_block is not allowed.
TEST_F(VertexShaderValidationTest, IOBlockWithoutExtension)
{
    const std::string &shaderString =
        R"(#version 310 es
        out block
        {
            vec2 value;
        } VSOutput[2];
        void main()
        {
            int i = 0;
            vec2 value1 = VSOutput[i].value;
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that using shader io blocks without declaration of GL_EXT_shader_io_block is not allowed.
TEST_F(FragmentShaderValidationTest, IOBlockWithoutExtension)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision mediump float;
        in block
        {
            vec4 i_color;
        } FSInput[2];
        out vec4 o_color;
        void main()
        {
            int i = 0;
            o_color = FSInput[i].i_color;
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a shader input with 'flat' qualifier cannot be used as l-value.
TEST_F(FragmentShaderValidationTest, AssignValueToFlatIn)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "flat in float value;\n"
        "out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "    value = 1.0;\n"
        "    o_color = vec4(1.0, 0.0, 0.0, 1.0);"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a shader input with 'smooth' qualifier cannot be used as l-value.
TEST_F(FragmentShaderValidationTest, AssignValueToSmoothIn)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "smooth in float value;\n"
        "out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "    value = 1.0;\n"
        "    o_color = vec4(1.0, 0.0, 0.0, 1.0);"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a shader input with 'centroid' qualifier cannot be used as l-value.
TEST_F(FragmentShaderValidationTest, AssignValueToCentroidIn)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "centroid in float value;\n"
        "out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "    value = 1.0;\n"
        "    o_color = vec4(1.0, 0.0, 0.0, 1.0);"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that shader compilation fails if the component argument is dynamic.
TEST_F(FragmentShaderValidationTest, DynamicComponentTextureGather)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump sampler2D;\n"
        "uniform sampler2D tex;\n"
        "out vec4 o_color;\n"
        "uniform int uComp;\n"
        "void main()\n"
        "{\n"
        "    o_color = textureGather(tex, vec2(0), uComp);"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that shader compilation fails if the component argument to textureGather has a negative
// value.
TEST_F(FragmentShaderValidationTest, TextureGatherNegativeComponent)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump sampler2D;\n"
        "uniform sampler2D tex;\n"
        "out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "    o_color = textureGather(tex, vec2(0), -1);"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that shader compilation fails if the component argument to textureGather has a value greater
// than 3.
TEST_F(FragmentShaderValidationTest, TextureGatherTooGreatComponent)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump sampler2D;\n"
        "uniform sampler2D tex;\n"
        "out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "    o_color = textureGather(tex, vec2(0), 4);"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that shader compilation fails if the offset is less than the minimum value.
TEST_F(FragmentShaderValidationTest, TextureGatherTooGreatOffset)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "precision mediump sampler2D;\n"
        "uniform sampler2D tex;\n"
        "out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "    o_color = textureGatherOffset(tex, vec2(0), ivec2(-100), 2);"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that it isn't allowed to use 'location' layout qualifier on GLSL ES 3.0 vertex shader
// outputs.
TEST_F(VertexShaderValidationTest, UseLocationOnVertexOutES30)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "in vec4 v1;\n"
        "layout (location = 1) out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that using 'location' layout qualifier on vertex shader outputs is legal in GLSL ES 3.1
// shaders.
TEST_F(VertexShaderValidationTest, UseLocationOnVertexOutES31)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "in vec4 v1;\n"
        "layout (location = 1) out vec4 o_color1;\n"
        "layout (location = 2) out vec4 o_color2;\n"
        "out vec3 v3;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it isn't allowed to use 'location' layout qualifier on GLSL ES 3.0 fragment shader
// inputs.
TEST_F(FragmentShaderValidationTest, UseLocationOnFragmentInES30)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "layout (location = 0) in vec4 v_color1;\n"
        "layout (location = 0) out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that using 'location' layout qualifier on fragment shader inputs is legal in GLSL ES 3.1
// shaders.
TEST_F(FragmentShaderValidationTest, UseLocationOnFragmentInES31)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "layout (location = 0) in mat4 v_mat;\n"
        "layout (location = 4) in vec4 v_color1;\n"
        "in vec2 v_color2;\n"
        "layout (location = 0) out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that declaring outputs of a vertex shader with same location causes a compile error.
TEST_F(VertexShaderValidationTest, DeclareSameLocationOnVertexOut)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "in float i_value;\n"
        "layout (location = 1) out vec4 o_color1;\n"
        "layout (location = 1) out vec4 o_color2;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that declaring inputs of a fragment shader with same location causes a compile error.
TEST_F(FragmentShaderValidationTest, DeclareSameLocationOnFragmentIn)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in float i_value;\n"
        "layout (location = 1) in vec4 i_color1;\n"
        "layout (location = 1) in vec4 i_color2;\n"
        "layout (location = 0) out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that the location of an element of an array conflicting with other output varyings in a
// vertex shader causes a compile error.
TEST_F(VertexShaderValidationTest, LocationConflictsnOnArrayElement)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "in float i_value;\n"
        "layout (location = 0) out vec4 o_color1[3];\n"
        "layout (location = 1) out vec4 o_color2;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that the location of an element of a matrix conflicting with other output varyings in a
// vertex shader causes a compile error.
TEST_F(VertexShaderValidationTest, LocationConflictsOnMatrixElement)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "in float i_value;\n"
        "layout (location = 0) out mat4 o_mvp;\n"
        "layout (location = 2) out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that the location of an element of a struct conflicting with other output varyings in a
// vertex shader causes a compile error.
TEST_F(VertexShaderValidationTest, LocationConflictsOnStructElement)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "in float i_value;\n"
        "struct S\n"
        "{\n"
        "    float value1;\n"
        "    vec3 value2;\n"
        "};\n"
        "layout (location = 0) out S s_in;"
        "layout (location = 1) out vec4 o_color;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that declaring inputs of a vertex shader with a location larger than GL_MAX_VERTEX_ATTRIBS
// causes a compile error.
TEST_F(VertexShaderValidationTest, AttributeLocationOutOfRange)
{
    // Assumes 1000 >= GL_MAX_VERTEX_ATTRIBS.
    // Current OpenGL and Direct3D implementations support up to 32.

    const std::string &shaderString =
        "#version 300 es\n"
        "layout (location = 1000) in float i_value;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a block can follow the final case in a switch statement.
// GLSL ES 3.00.5 section 6 and the grammar suggest that an empty block is a statement.
TEST_F(FragmentShaderValidationTest, SwitchFinalCaseHasEmptyBlock)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision mediump float;
        uniform int i;
        void main()
        {
            switch (i)
            {
                case 0:
                    break;
                default:
                    {}
            }
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that an empty declaration can follow the final case in a switch statement.
TEST_F(FragmentShaderValidationTest, SwitchFinalCaseHasEmptyDeclaration)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision mediump float;
        uniform int i;
        void main()
        {
            switch (i)
            {
                case 0:
                    break;
                default:
                    float;
            }
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// The final case in a switch statement can't be empty in ESSL 3.10 either. This is the intent of
// the spec though public spec in early 2018 didn't reflect this yet.
TEST_F(FragmentShaderValidationTest, SwitchFinalCaseEmptyESSL310)
{
    const std::string &shaderString =
        R"(#version 310 es

        precision mediump float;
        uniform int i;
        void main()
        {
            switch (i)
            {
                case 0:
                    break;
                default:
            }
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that fragment shader cannot declare unsized inputs.
TEST_F(FragmentShaderValidationTest, UnsizedInputs)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in float i_value[];\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that unsized struct members are not allowed.
TEST_F(FragmentShaderValidationTest, UnsizedStructMember)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision highp float;
        out vec4 color;

        struct S
        {
            int[] foo;
        };

        void main()
        {
            color = vec4(1.0);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that unsized parameters without a name are not allowed.
// GLSL ES 3.10 section 6.1 Function Definitions.
TEST_F(FragmentShaderValidationTest, UnsizedNamelessParameter)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision highp float;
        out vec4 color;

        void foo(int[]);

        void main()
        {
            color = vec4(1.0);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that partially unsized array of arrays constructor sizes are validated.
TEST_F(FragmentShaderValidationTest, PartiallyUnsizedArrayOfArraysConstructor)
{
    const std::string &shaderString =
        R"(#version 310 es

        precision highp float;
        out vec4 color;

        void main()
        {
            int a[][] = int[2][](int[1](1));
            color = vec4(a[0][0]);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that duplicate field names in a struct declarator list are validated.
TEST_F(FragmentShaderValidationTest, DuplicateFieldNamesInStructDeclaratorList)
{
    const std::string &shaderString =
        R"(precision mediump float;

        struct S {
            float f, f;
        };

        void main()
        {
            gl_FragColor = vec4(1.0);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that an empty statement is not allowed in switch before the first case.
TEST_F(FragmentShaderValidationTest, EmptyStatementInSwitchBeforeFirstCase)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision mediump float;
        uniform int u_zero;
        out vec4 my_FragColor;

        void main()
        {
            switch(u_zero)
            {
                    ;
                case 0:
                    my_FragColor = vec4(0.0);
                default:
                    my_FragColor = vec4(1.0);
            }
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a nameless struct definition is not allowed as a function parameter type.
// ESSL 3.00.6 section 12.10. ESSL 3.10 January 2016 section 13.10.
TEST_F(FragmentShaderValidationTest, NamelessStructDefinitionAsParameterType)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        float foo(struct { float field; } f)
        {
            return f.field;
        }

        void main()
        {
            my_FragColor = vec4(0, 1, 0, 1);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a named struct definition is not allowed as a function parameter type.
// ESSL 3.00.6 section 12.10. ESSL 3.10 January 2016 section 13.10.
TEST_F(FragmentShaderValidationTest, NamedStructDefinitionAsParameterType)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        float foo(struct S { float field; } f)
        {
            return f.field;
        }

        void main()
        {
            my_FragColor = vec4(0, 1, 0, 1);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a named struct definition is not allowed as a function parameter type.
// ESSL 3.00.6 section 12.10. ESSL 3.10 January 2016 section 13.10.
TEST_F(FragmentShaderValidationTest, StructDefinitionAsTypeOfParameterWithoutName)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        float foo(struct S { float field; } /* no parameter name */)
        {
            return 1.0;
        }

        void main()
        {
            my_FragColor = vec4(0, 1, 0, 1);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that an unsized const array doesn't assert.
TEST_F(FragmentShaderValidationTest, UnsizedConstArray)
{
    const std::string &shaderString =
        R"(#version 300 es

        void main()
        {
            const int t[];
            t[0];
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that the value passed to the mem argument of an atomic memory function can be a shared
// variable.
TEST_F(ComputeShaderValidationTest, AtomicAddWithSharedVariable)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(local_size_x = 5) in;
        shared uint myShared;

        void main() {
            atomicAdd(myShared, 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it is acceptable to pass an element of an array to the mem argument of an atomic memory
// function, as long as the underlying array is a buffer or shared variable.
TEST_F(ComputeShaderValidationTest, AtomicAddWithSharedVariableArray)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(local_size_x = 5) in;
        shared uint myShared[2];

        void main() {
            atomicAdd(myShared[0], 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it is acceptable to pass a single component of a vector to the mem argument of an
// atomic memory function, as long as the underlying vector is a buffer or shared variable.
TEST_F(ComputeShaderValidationTest, AtomicAddWithSharedVariableVector)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(local_size_x = 5) in;
        shared uvec4 myShared;

        void main() {
            atomicAdd(myShared[0], 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that the value passed to the mem argument of an atomic memory function can be a buffer
// variable.
TEST_F(FragmentShaderValidationTest, AtomicAddWithBufferVariable)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(std140) buffer bufferName1{
            uint u1;
        };

        void main()
        {
            atomicAdd(u1, 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it is acceptable to pass an element of an array to the mem argument of an atomic memory
// function, as long as the underlying array is a buffer or shared variable.
TEST_F(FragmentShaderValidationTest, AtomicAddWithBufferVariableArrayElement)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(std140) buffer bufferName1{
            uint u1[2];
        };

        void main()
        {
            atomicAdd(u1[0], 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it is acceptable to pass a member of a shader storage block instance to the mem
// argument of an atomic memory function.
TEST_F(FragmentShaderValidationTest, AtomicAddWithBufferVariableInBlockInstance)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(std140) buffer bufferName{
            uint u1;
        } instanceName;

        void main()
        {
            atomicAdd(instanceName.u1, 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it is acceptable to pass a member of a shader storage block instance array to the mem
// argument of an atomic memory function.
TEST_F(FragmentShaderValidationTest, AtomicAddWithBufferVariableInBlockInstanceArray)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(std140) buffer bufferName{
            uint u1;
        } instanceName[1];

        void main()
        {
            atomicAdd(instanceName[0].u1, 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it is acceptable to pass an element of an array  of a shader storage block instance to
// the mem argument of an atomic memory function.
TEST_F(FragmentShaderValidationTest, AtomicAddWithElementOfArrayInBlockInstance)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(std140) buffer blockName {
            uint data[2];
        } instanceName;

        void main()
        {
            atomicAdd(instanceName.data[0], 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it is not allowed to pass an atomic counter variable to the mem argument of an atomic
// memory function.
TEST_F(FragmentShaderValidationTest, AtomicAddWithAtomicCounter)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(binding = 0, offset = 4) uniform atomic_uint ac;

        void main()
        {
            atomicAdd(ac, 2u);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that it is not allowed to pass an element of an atomic counter array to the mem argument of
// an atomic memory function.
TEST_F(FragmentShaderValidationTest, AtomicAddWithAtomicCounterArray)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(binding = 0, offset = 4) uniform atomic_uint ac[2];

        void main()
        {
            atomicAdd(ac[0], 2u);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that it is not allowed to pass a local uint value to the mem argument of an atomic memory
// function.
TEST_F(FragmentShaderValidationTest, AtomicAddWithNonStorageVariable)
{
    const std::string &shaderString =
        R"(#version 310 es

        void main()
        {
            uint test = 1u;
            atomicAdd(test, 2u);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that it is acceptable to pass a swizzle of a member of a shader storage block to the mem
// argument of an atomic memory function.
TEST_F(FragmentShaderValidationTest, AtomicAddWithSwizzle)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(std140) buffer bufferName{
            uvec4 u1[2];
        } instanceName[3];

        void main()
        {
            atomicAdd(instanceName[2].u1[1].y, 2u);
        })";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that it is not allowed to pass an expression that does not constitute of indexing, field
// selection or swizzle to the mem argument of an atomic memory function.
TEST_F(FragmentShaderValidationTest, AtomicAddWithNonIndexNonSwizzleExpression)
{
    const std::string &shaderString =
        R"(#version 310 es

        layout(std140) buffer bufferName{
            uint u1[2];
        } instanceName[3];

        void main()
        {
            atomicAdd(instanceName[2].u1[1] + 1u, 2u);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that negative indexing of a matrix doesn't result in an assert.
TEST_F(FragmentShaderValidationTest, MatrixNegativeIndex)
{
    const std::string &shaderString =
        R"(
        precision mediump float;

        void main()
        {
            gl_FragColor = mat4(1.0)[-1];
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Global variable initializers need to be constant expressions. Test with assigning a ternary
// expression that ANGLE can fold.
TEST_F(FragmentShaderValidationTest, AssignConstantFoldedFromNonConstantTernaryToGlobal)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        uniform float u;
        float f = true ? 1.0 : u;

        out vec4 my_FragColor;

        void main()
        {
           my_FragColor = vec4(f);
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Global variable initializers need to be constant expressions. Test with assigning a ternary
// expression that ANGLE can fold.
TEST_F(FragmentShaderValidationTest,
       AssignConstantArrayVariableFoldedFromNonConstantTernaryToGlobal)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        uniform float u[2];
        const float c[2] = float[2](1.0, 2.0);
        float f[2] = true ? c : u;

        out vec4 my_FragColor;

        void main()
        {
           my_FragColor = vec4(f[0], f[1], 0.0, 1.0);
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test going past the struct nesting limit while simultaneously using invalid nested struct
// definitions. This makes sure that the code generating an error message about going past the
// struct nesting limit does not access the name of a nameless struct definition.
TEST_F(WebGL1FragmentShaderValidationTest, StructNestingLimitWithNestedStructDefinitions)
{
    const std::string &shaderString =
        R"(
        precision mediump float;

        struct
        {
            struct
            {
                struct
                {
                    struct
                    {
                        struct
                        {
                            struct
                            {
                                float f;
                            } s5;
                        } s4;
                    } s3;
                } s2;
            } s1;
        } s0;

        void main(void)
        {
            gl_FragColor = vec4(s0.s1.s2.s3.s4.s5.f);
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that the result of a sequence operator is not a constant-expression.
// ESSL 3.00 section 12.43.
TEST_F(FragmentShaderValidationTest, CommaReturnsNonConstant)
{
    const std::string &shaderString =
        R"(#version 300 es

        precision highp float;
        out vec4 my_FragColor;

        void main(void)
        {
            const int i = (0, 0);
            my_FragColor = vec4(i);
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that the result of indexing into an array constructor with some non-constant arguments is
// not a constant expression.
TEST_F(FragmentShaderValidationTest,
       IndexingIntoArrayConstructorWithNonConstantArgumentsIsNotConstantExpression)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision highp float;
        uniform float u;
        out float my_FragColor;
        void main()
        {
            const float f = float[2](u, 1.0)[1];
            my_FragColor = f;
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that the type of an initializer of a constant variable needs to match.
TEST_F(FragmentShaderValidationTest, ConstantInitializerTypeMismatch)
{
    const std::string &shaderString =
        R"(
        precision mediump float;
        const float f = 0;

        void main()
        {
            gl_FragColor = vec4(f);
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that redeclaring a built-in is an error in ESSL 1.00. ESSL 1.00.17 section 4.2.6 disallows
// "redefinition" of built-ins - it's not very explicit about redeclaring them, but we treat this as
// an error. The redeclaration cannot serve any purpose since it can't be accompanied by a
// definition.
TEST_F(FragmentShaderValidationTest, RedeclaringBuiltIn)
{
    const std::string &shaderString =
        R"(
        precision mediump float;
        float sin(float x);

        void main()
        {
            gl_FragColor = vec4(0.0);
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Redefining a built-in that is not available in the current shader stage is assumed to be not an
// error. Test with redefining groupMemoryBarrier() in fragment shader. The built-in
// groupMemoryBarrier() is only available in compute shaders.
TEST_F(FragmentShaderValidationTest, RedeclaringBuiltInFromAnotherShaderStage)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision mediump float;
        out vec4 my_FragColor;
        float groupMemoryBarrier() { return 1.0; }

        void main()
        {
            my_FragColor = vec4(groupMemoryBarrier());
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that standard derivative functions that are in core ESSL 3.00 compile successfully.
TEST_F(FragmentShaderValidationTest, ESSL300StandardDerivatives)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        in vec4 iv;
        out vec4 my_FragColor;

        void main()
        {
            vec4 v4 = vec4(0.0);
            v4 += fwidth(iv);
            v4 += dFdx(iv);
            v4 += dFdy(iv);
            my_FragColor = v4;
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that vertex shader built-in gl_Position is not accessible in fragment shader.
TEST_F(FragmentShaderValidationTest, GlPosition)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        in vec4 iv;
        out vec4 my_FragColor;

        void main()
        {
            gl_Position = iv;
            my_FragColor = iv;
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that compute shader built-in gl_LocalInvocationID is not accessible in fragment shader.
TEST_F(FragmentShaderValidationTest, GlLocalInvocationID)
{
    const std::string &shaderString =
        R"(#version 310 es
        precision mediump float;
        out vec3 my_FragColor;

        void main()
        {
            my_FragColor = vec3(gl_LocalInvocationID);
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that fragment shader built-in gl_FragCoord is not accessible in vertex shader.
TEST_F(VertexShaderValidationTest, GlFragCoord)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;

        void main()
        {
            gl_Position = vec4(gl_FragCoord);
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a long sequence of repeated swizzling on an l-value does not cause a stack overflow.
TEST_F(VertexShaderValidationTest, LValueRepeatedSwizzle)
{
    std::stringstream shaderString;
    shaderString << R"(#version 300 es
        precision mediump float;

        uniform vec2 u;

        void main()
        {
            vec2 f;
            f)";
    for (int i = 0; i < 1000; ++i)
    {
        shaderString << ".yx.yx";
    }
    shaderString << R"( = vec2(0.0);
        })";

    if (!compile(shaderString.str()))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that swizzling that contains duplicate components can't form an l-value, even if it is
// swizzled again so that the final result does not contain duplicate components.
TEST_F(VertexShaderValidationTest, LValueSwizzleDuplicateComponents)
{

    const std::string &shaderString = R"(#version 300 es
        precision mediump float;

        void main()
        {
            vec2 f;
            (f.xxyy).xz = vec2(0.0);
        })";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that a fragment shader with nested if statements without braces compiles successfully.
TEST_F(FragmentShaderValidationTest, HandleIfInnerIfStatementAlwaysTriviallyPruned)
{
    const std::string &shaderString =
        R"(precision mediump float;
        void main()
        {
            if (true)
                if (false)
                    gl_FragColor = vec4(0.0);
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that a fragment shader with an if statement nested in a loop without braces compiles
// successfully.
TEST_F(FragmentShaderValidationTest, HandleLoopInnerIfStatementAlwaysTriviallyPruned)
{
    const std::string &shaderString =
        R"(precision mediump float;
        void main()
        {
            while (false)
                if (false)
                    gl_FragColor = vec4(0.0);
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that declaring both gl_FragColor and gl_FragData invariant is not an error. The GLSL ES 1.00
// spec only disallows writing to both of them. ANGLE extends this validation to also cover reads,
// but it makes sense not to treat declaring them both invariant as an error.
TEST_F(FragmentShaderValidationTest, DeclareBothBuiltInFragmentOutputsInvariant)
{
    const std::string &shaderString =
        R"(
        invariant gl_FragColor;
        invariant gl_FragData;
        precision mediump float;
        void main()
        {
            gl_FragColor = vec4(0.0);
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that a case cannot be placed inside a block nested inside a switch statement. GLSL ES 3.10
// section 6.2.
TEST_F(FragmentShaderValidationTest, CaseInsideBlock)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        uniform int u;
        out vec4 my_FragColor;
        void main()
        {
            switch (u)
            {
                case 1:
                {
                    case 0:
                        my_FragColor = vec4(0.0);
                }
                default:
                    my_FragColor = vec4(1.0);
            }
        })";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test using a value from a constant array as a case label.
TEST_F(FragmentShaderValidationTest, ValueFromConstantArrayAsCaseLabel)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        uniform int u;
        const int[3] arr = int[3](2, 1, 0);
        out vec4 my_FragColor;
        void main()
        {
            switch (u)
            {
                case arr[1]:
                    my_FragColor = vec4(0.0);
                case 2:
                case 0:
                default:
                    my_FragColor = vec4(1.0);
            }
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test using a value from a constant array as a fragment output index.
TEST_F(FragmentShaderValidationTest, ValueFromConstantArrayAsFragmentOutputIndex)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        uniform int u;
        const int[3] arr = int[3](4, 1, 0);
        out vec4 my_FragData[2];
        void main()
        {
            my_FragData[arr[1]] = vec4(0.0);
            my_FragData[arr[2]] = vec4(0.0);
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test using a value from a constant array as an array size.
TEST_F(FragmentShaderValidationTest, ValueFromConstantArrayAsArraySize)
{
    const std::string &shaderString =
        R"(#version 300 es
        precision mediump float;
        uniform int u;
        const int[3] arr = int[3](0, 2, 0);
        const int[arr[1]] arr2 = int[2](2, 1);
        out vec4 my_FragColor;
        void main()
        {
            my_FragColor = vec4(arr2[1]);
        })";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that an invalid struct with void fields doesn't crash or assert when used in a comma
// operator. This is a regression test.
TEST_F(FragmentShaderValidationTest, InvalidStructWithVoidFieldsInComma)
{
    // The struct needed the two fields for the bug to repro.
    const std::string &shaderString =
        R"(#version 300 es
precision highp float;

struct T { void a[8], c; };

void main() {
    0.0, T();
})";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that layout(early_fragment_tests) in; is valid in fragment shader
TEST_F(FragmentShaderValidationTest, ValidEarlyFragmentTests)
{
    constexpr char kShaderString[] =
        R"(#version 310 es
        precision mediump float;
        layout(early_fragment_tests) in;
        out vec4 color;
        void main()
        {
            color = vec4(0.0);
        })";
    if (!compile(kShaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that layout(early_fragment_tests=x) in; is invalid
TEST_F(FragmentShaderValidationTest, InvalidValueForEarlyFragmentTests)
{
    constexpr char kShaderString[] =
        R"(#version 310 es
        precision mediump float;
        layout(early_fragment_tests=1) in;
        out vec4 color;
        void main()
        {
            color = vec4(0.0);
        })";
    if (compile(kShaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that layout(early_fragment_tests) in varying; is invalid
TEST_F(FragmentShaderValidationTest, InvalidEarlyFragmentTestsOnVariableDecl)
{
    constexpr char kShaderString[] =
        R"(#version 310 es
        precision mediump float;
        layout(early_fragment_tests) in vec4 v;
        out vec4 color;
        void main()
        {
            color = v;
        })";
    if (compile(kShaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that layout(early_fragment_tests) in; is invalid in vertex shader
TEST_F(VertexShaderValidationTest, InvalidEarlyFragmentTests)
{
    constexpr char kShaderString[] =
        R"(#version 310 es
        layout(early_fragment_tests) in;
        void main()
        {
            gl_Position = vec4(0.0);
        })";
    if (compile(kShaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that layout(early_fragment_tests) in; is invalid in compute shader
TEST_F(ComputeShaderValidationTest, InvalidEarlyFragmentTests)
{
    constexpr char kShaderString[] =
        R"(#version 310 es
        layout(local_size_x = 1) in;
        layout(early_fragment_tests) in;
        void main() {})";
    if (compile(kShaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that layout(x) in; only accepts x=early_fragment_tests.
TEST_F(FragmentShaderValidationTest, NothingButEarlyFragmentTestsWithInWithoutVariableDecl)
{
    const char *noValueQualifiers[] = {
        "shared",      "packed",
        "std140",      "std430",
        "row_major",   "col_major",
        "location",    "yuv",
        "rgba32f",     "rgba16f",
        "r32f",        "rgba8",
        "rgba8_snorm", "rgba32i",
        "rgba16i",     "rgba8i",
        "r32i",        "rgba32ui",
        "rgba16ui",    "rgba8ui",
        "r32ui",       "points",
        "lines",       "lines_adjacency",
        "triangles",   "triangles_adjacency",
        "line_strip",  "triangle_strip",
    };

    const char *withValueQualifiers[] = {
        "location",     "binding",   "offset",      "local_size_x", "local_size_y",
        "local_size_z", "num_views", "invocations", "max_vertices", "index",
    };

    constexpr char kShaderStringPre[] =
        R"(#version 310 es
        precision mediump float;
        layout()";
    constexpr char kShaderStringPost[] =
        R"() in;
        out vec4 color;
        void main()
        {
            color = vec4(0.0);
        })";

    // Make sure the method of constructing shaders is valid.
    const std::string validShaderString =
        kShaderStringPre + std::string("early_fragment_tests") + kShaderStringPost;
    if (!compile(validShaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }

    for (size_t i = 0; i < ArraySize(noValueQualifiers); ++i)
    {
        const std::string shaderString =
            kShaderStringPre + std::string(noValueQualifiers[i]) + kShaderStringPost;

        if (compile(shaderString))
        {
            FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
        }
    }

    for (size_t i = 0; i < ArraySize(withValueQualifiers); ++i)
    {
        const std::string shaderString =
            kShaderStringPre + std::string(withValueQualifiers[i]) + "=1" + kShaderStringPost;

        if (compile(shaderString))
        {
            FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
        }
    }
}
