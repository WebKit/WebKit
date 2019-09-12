//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WEBGL_multiview_test.cpp:
//   Test that shaders with gl_ViewID_OVR are validated correctly.
//

#include "GLSLANG/ShaderLang.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "tests/test_utils/ShaderCompileTreeTest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

namespace
{

class SymbolOccurrenceCounter : public TIntermTraverser
{
  public:
    SymbolOccurrenceCounter() : TIntermTraverser(true, false, false), mNumberOfOccurrences(0u) {}

    void visitSymbol(TIntermSymbol *node) override
    {
        if (shouldCountSymbol(node))
        {
            ++mNumberOfOccurrences;
        }
    }

    virtual bool shouldCountSymbol(const TIntermSymbol *node) const = 0;

    unsigned getNumberOfOccurrences() const { return mNumberOfOccurrences; }

  private:
    unsigned mNumberOfOccurrences;
};

class SymbolOccurrenceCounterByQualifier : public SymbolOccurrenceCounter
{
  public:
    SymbolOccurrenceCounterByQualifier(TQualifier symbolQualifier)
        : mSymbolQualifier(symbolQualifier)
    {}

    bool shouldCountSymbol(const TIntermSymbol *node) const override
    {
        return node->getQualifier() == mSymbolQualifier;
    }

  private:
    TQualifier mSymbolQualifier;
};

class SymbolOccurrenceCounterByName : public SymbolOccurrenceCounter
{
  public:
    SymbolOccurrenceCounterByName(const ImmutableString &symbolName) : mSymbolName(symbolName) {}

    bool shouldCountSymbol(const TIntermSymbol *node) const override
    {
        return node->variable().symbolType() != SymbolType::Empty && node->getName() == mSymbolName;
    }

  private:
    ImmutableString mSymbolName;
};

class SymbolOccurrenceCounterByNameAndQualifier : public SymbolOccurrenceCounter
{
  public:
    SymbolOccurrenceCounterByNameAndQualifier(const ImmutableString &symbolName,
                                              TQualifier qualifier)
        : mSymbolName(symbolName), mSymbolQualifier(qualifier)
    {}

    bool shouldCountSymbol(const TIntermSymbol *node) const override
    {
        return node->variable().symbolType() != SymbolType::Empty &&
               node->getName() == mSymbolName && node->getQualifier() == mSymbolQualifier;
    }

  private:
    ImmutableString mSymbolName;
    TQualifier mSymbolQualifier;
};

class OVRMultiviewVertexShaderTest : public ShaderCompileTreeTest
{
  public:
    OVRMultiviewVertexShaderTest() {}

  protected:
    ::GLenum getShaderType() const override { return GL_VERTEX_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_WEBGL3_SPEC; }
    void initResources(ShBuiltInResources *resources) override
    {
        resources->OVR_multiview2 = 1;
        resources->MaxViewsOVR    = 4;
    }
};

class OVRMultiviewFragmentShaderTest : public ShaderCompileTreeTest
{
  public:
    OVRMultiviewFragmentShaderTest() {}

  protected:
    ::GLenum getShaderType() const override { return GL_FRAGMENT_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_WEBGL3_SPEC; }
    void initResources(ShBuiltInResources *resources) override
    {
        resources->OVR_multiview2 = 1;
        resources->MaxViewsOVR    = 4;
    }
};

class OVRMultiviewOutputCodeTest : public MatchOutputCodeTest
{
  public:
    OVRMultiviewOutputCodeTest(sh::GLenum shaderType)
        : MatchOutputCodeTest(shaderType, 0, SH_ESSL_OUTPUT)
    {
        addOutputType(SH_GLSL_COMPATIBILITY_OUTPUT);

        getResources()->OVR_multiview2 = 1;
        getResources()->MaxViewsOVR    = 4;
    }

    void requestHLSLOutput()
    {
#if defined(ANGLE_ENABLE_HLSL)
        addOutputType(SH_HLSL_4_1_OUTPUT);
#endif
    }

    bool foundInAllGLSLCode(const char *str)
    {
        return foundInGLSLCode(str) && foundInESSLCode(str);
    }

    bool foundInHLSLCode(const char *stringToFind) const
    {
#if defined(ANGLE_ENABLE_HLSL)
        return foundInCode(SH_HLSL_4_1_OUTPUT, stringToFind);
#else
        return true;
#endif
    }
};

class OVRMultiviewVertexShaderOutputCodeTest : public OVRMultiviewOutputCodeTest
{
  public:
    OVRMultiviewVertexShaderOutputCodeTest() : OVRMultiviewOutputCodeTest(GL_VERTEX_SHADER) {}
};

class OVRMultiviewFragmentShaderOutputCodeTest : public OVRMultiviewOutputCodeTest
{
  public:
    OVRMultiviewFragmentShaderOutputCodeTest() : OVRMultiviewOutputCodeTest(GL_FRAGMENT_SHADER) {}
};

class OVRMultiviewComputeShaderOutputCodeTest : public OVRMultiviewOutputCodeTest
{
  public:
    OVRMultiviewComputeShaderOutputCodeTest() : OVRMultiviewOutputCodeTest(GL_COMPUTE_SHADER) {}
};

void VariableOccursNTimes(TIntermBlock *root,
                          const ImmutableString &varName,
                          const TQualifier varQualifier,
                          unsigned n)
{
    // Check that there are n occurrences of the variable with the given name and qualifier.
    SymbolOccurrenceCounterByNameAndQualifier viewIDByNameAndQualifier(varName, varQualifier);
    root->traverse(&viewIDByNameAndQualifier);
    EXPECT_EQ(n, viewIDByNameAndQualifier.getNumberOfOccurrences());

    // Check that there are n occurrences of the variable with the given name. By this we guarantee
    // that there are no other occurrences of the variable with the same name but different
    // qualifier.
    SymbolOccurrenceCounterByName viewIDByName(varName);
    root->traverse(&viewIDByName);
    EXPECT_EQ(n, viewIDByName.getNumberOfOccurrences());
}

// Unsupported GL_OVR_multiview extension directive (GL_OVR_multiview2 spec only exposes
// GL_OVR_multiview2).
TEST_F(OVRMultiviewVertexShaderTest, InvalidMultiview2)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invalid combination of non-matching num_views declarations.
TEST_F(OVRMultiviewVertexShaderTest, InvalidNumViewsMismatch)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "layout(num_views = 1) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Invalid value zero for num_views.
TEST_F(OVRMultiviewVertexShaderTest, InvalidNumViewsZero)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 0) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Too large value for num_views.
TEST_F(OVRMultiviewVertexShaderTest, InvalidNumViewsGreaterThanMax)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 5) in;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.x = (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "    gl_Position.yzw = vec3(0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Valid use of gl_ViewID_OVR.
TEST_F(OVRMultiviewVertexShaderTest, ViewIDUsed)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "layout(num_views = 2) in;  // Duplicated on purpose\n"
        "in vec4 pos;\n"
        "out float myOutput;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0u)\n"
        "    {\n"
        "        gl_Position = pos;\n"
        "        myOutput = 1.0;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position = pos + vec4(1.0, 0.0, 0.0, 0.0);\n"
        "        myOutput = 2.0;\n"
        "    }\n"
        "    gl_Position += (gl_ViewID_OVR == 0u) ? 1.0 : 0.0;\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Read gl_FragCoord in a OVR_multiview2 fragment shader.
TEST_F(OVRMultiviewFragmentShaderTest, ReadOfFragCoord)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "precision highp float;\n"
        "out vec4 outColor;\n"
        "void main()\n"
        "{\n"
        "    outColor = vec4(gl_FragCoord.xy, 0, 1);\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Read gl_ViewID_OVR in an OVR_multiview2 fragment shader.
TEST_F(OVRMultiviewFragmentShaderTest, ReadOfViewID)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "precision highp float;\n"
        "out vec4 outColor;\n"
        "void main()\n"
        "{\n"
        "    outColor = vec4(gl_ViewID_OVR, 0, 0, 1);\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Correct use of GL_OVR_multiview2 macro.
TEST_F(OVRMultiviewVertexShaderTest, UseOfExtensionMacro)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#ifdef GL_OVR_multiview2\n"
        "#if (GL_OVR_multiview2 == 1)\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "}\n"
        "#endif\n"
        "#endif\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Test that gl_ViewID_OVR can't be used as an l-value.
TEST_F(OVRMultiviewVertexShaderTest, ViewIdAsLValue)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "void foo(out uint u)\n"
        "{\n"
        "    u = 3u;\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    foo(gl_ViewID_OVR);\n"
        "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that compiling an ESSL 1.00 shader with multiview support fails.
TEST_F(OVRMultiviewVertexShaderTest, ESSL1Shader)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0)\n"
        "    {\n"
        "        gl_Position = vec4(-1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that compiling an ESSL 1.00 shader with an unsupported global layout qualifier fails.
TEST_F(OVRMultiviewVertexShaderTest, ESSL1ShaderUnsupportedGlobalLayoutQualifier)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "layout(std140) uniform;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0)\n"
        "    {\n"
        "        gl_Position = vec4(-1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that compiling an ESSL 1.00 vertex shader with an unsupported input storage qualifier fails.
TEST_F(OVRMultiviewVertexShaderTest, ESSL1ShaderUnsupportedInputStorageQualifier)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "in vec4 pos;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0)\n"
        "    {\n"
        "        gl_Position = vec4(-1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_Position = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that compiling an ESSL 1.00 fragment shader with an unsupported input storage qualifier
// fails.
TEST_F(OVRMultiviewFragmentShaderTest, ESSL1ShaderUnsupportedInStorageQualifier)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "precision highp float;\n"
        "in vec4 color;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0)\n"
        "    {\n"
        "        gl_FragColor = color;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_FragColor = color + vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Test that gl_InstanceID gets correctly replaced by InstanceID. gl_InstanceID should only be used
// twice: once to initialize ViewID_OVR and once for InstanceID. The number of occurrences of
// InstanceID in the AST should be the sum of two and the number of occurrences of gl_InstanceID
// before any renaming.
TEST_F(OVRMultiviewVertexShaderTest, GLInstanceIDIsRenamed)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "flat out int myInstance;\n"
        "out float myInstanceF;\n"
        "out float myInstanceF2;\n"
        "void main()\n"
        "{\n"
        "   gl_Position.x = gl_ViewID_OVR == 0u ? 0. : 1.;\n"
        "   gl_Position.yzw = vec3(0., 0., 1.);\n"
        "   myInstance = gl_InstanceID;\n"
        "   myInstanceF = float(gl_InstanceID) + .5;\n"
        "   myInstanceF2 = float(gl_InstanceID) + .1;\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    compileAssumeSuccess(shaderString);

    SymbolOccurrenceCounterByName glInstanceIDByName(ImmutableString("gl_InstanceID"));
    mASTRoot->traverse(&glInstanceIDByName);
    EXPECT_EQ(2u, glInstanceIDByName.getNumberOfOccurrences());

    SymbolOccurrenceCounterByQualifier glInstanceIDByQualifier(EvqInstanceID);
    mASTRoot->traverse(&glInstanceIDByQualifier);
    EXPECT_EQ(2u, glInstanceIDByQualifier.getNumberOfOccurrences());

    SymbolOccurrenceCounterByName instanceIDByName(ImmutableString("InstanceID"));
    mASTRoot->traverse(&instanceIDByName);
    EXPECT_EQ(5u, instanceIDByName.getNumberOfOccurrences());
}

// Test that gl_ViewID_OVR gets correctly replaced by ViewID_OVR. gl_ViewID_OVR should not be found
// by either name or qualifier. The number of occurrences of ViewID_OVR in the AST should be the sum
// of two and the number of occurrences of gl_ViewID_OVR before any renaming.
TEST_F(OVRMultiviewVertexShaderTest, GLViewIDIsRenamed)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 2) in;\n"
        "flat out uint a;\n"
        "void main()\n"
        "{\n"
        "   gl_Position.x = gl_ViewID_OVR == 0u ? 0. : 1.;\n"
        "   gl_Position.yzw = vec3(0., 0., 1.);\n"
        "   a = gl_ViewID_OVR == 0u ? (gl_ViewID_OVR+2u) : gl_ViewID_OVR;\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    compileAssumeSuccess(shaderString);

    SymbolOccurrenceCounterByName glViewIDOVRByName(ImmutableString("gl_ViewID_OVR"));
    mASTRoot->traverse(&glViewIDOVRByName);
    EXPECT_EQ(0u, glViewIDOVRByName.getNumberOfOccurrences());

    SymbolOccurrenceCounterByQualifier glViewIDOVRByQualifier(EvqViewIDOVR);
    mASTRoot->traverse(&glViewIDOVRByQualifier);
    EXPECT_EQ(0u, glViewIDOVRByQualifier.getNumberOfOccurrences());

    SymbolOccurrenceCounterByNameAndQualifier viewIDByNameAndQualifier(
        ImmutableString("ViewID_OVR"), EvqFlatOut);
    mASTRoot->traverse(&viewIDByNameAndQualifier);
    EXPECT_EQ(6u, viewIDByNameAndQualifier.getNumberOfOccurrences());
}

// The test checks that ViewID_OVR and InstanceID have the correct initializers based on the
// number of views.
TEST_F(OVRMultiviewVertexShaderOutputCodeTest, ViewIDAndInstanceIDHaveCorrectValues)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 3) in;\n"
        "flat out int myInstance;\n"
        "void main()\n"
        "{\n"
        "   gl_Position.x = gl_ViewID_OVR == 0u ? 0. : 1.;\n"
        "   gl_Position.yzw = vec3(0., 0., 1.);\n"
        "   myInstance = gl_InstanceID;\n"
        "}\n";
    requestHLSLOutput();
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW);

    EXPECT_TRUE(foundInAllGLSLCode("ViewID_OVR = (uint(gl_InstanceID) % 3u)"));
    EXPECT_TRUE(foundInAllGLSLCode("InstanceID = int((uint(gl_InstanceID) / 3u))"));

    EXPECT_TRUE(foundInHLSLCode("ViewID_OVR = (uint_ctor(gl_InstanceID) % 3)"));
#if defined(ANGLE_ENABLE_HLSL)
    EXPECT_FALSE(foundInHLSLCode("_ViewID_OVR = (uint_ctor(gl_InstanceID) % 3)"));
#endif
    EXPECT_TRUE(foundInHLSLCode("InstanceID = int_ctor((uint_ctor(gl_InstanceID) / 3))"));
}

// The test checks that the directive enabling GL_OVR_multiview2 is not outputted if the extension
// is emulated.
TEST_F(OVRMultiviewVertexShaderOutputCodeTest, StrippedOVRMultiviewDirective)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 3) in;\n"
        "void main()\n"
        "{\n"
        "}\n";
    // The directive must not be present if any of the multiview emulation options are set.
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW);
    EXPECT_FALSE(foundInESSLCode("GL_OVR_multiview2"));
    EXPECT_FALSE(foundInGLSLCode("GL_OVR_multiview2"));

    // The directive should be outputted from the ESSL translator with none of the options being
    // set.
    compile(shaderString);
    EXPECT_TRUE(foundInESSLCode("GL_OVR_multiview2"));
}

// Test that ViewID_OVR is declared as a flat input variable in an ESSL 3.00 fragment shader.
TEST_F(OVRMultiviewFragmentShaderTest, ViewIDDeclaredAsFlatInput)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "void main()\n"
        "{\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    compileAssumeSuccess(shaderString);
    VariableOccursNTimes(mASTRoot, ImmutableString("ViewID_OVR"), EvqFlatIn, 1u);
}

// Test that ViewID_OVR is declared as a flat output variable in an ESSL 1.00 vertex shader.
TEST_F(OVRMultiviewVertexShaderTest, ViewIDDeclaredAsFlatOutput)
{
    const std::string &shaderString =
        "#extension GL_OVR_multiview2 : require\n"
        "void main()\n"
        "{\n"
        "}\n";
    mExtraCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    compileAssumeSuccess(shaderString);
    VariableOccursNTimes(mASTRoot, ImmutableString("ViewID_OVR"), EvqFlatOut, 2u);
}

// The test checks that the GL_NV_viewport_array2 extension is emitted in a vertex shader if the
// SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER option is set.
TEST_F(OVRMultiviewVertexShaderOutputCodeTest, ViewportArray2IsEmitted)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 3) in;\n"
        "void main()\n"
        "{\n"
        "}\n";
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW |
                              SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER);
    EXPECT_TRUE(foundInAllGLSLCode("#extension GL_NV_viewport_array2 : require"));
}

// The test checks that the GL_NV_viewport_array2 extension is not emitted in a vertex shader if the
// OVR_multiview2 extension is not requested in the shader source even if the
// SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER option is set.
TEST_F(OVRMultiviewVertexShaderOutputCodeTest, ViewportArray2IsNotEmitted)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "void main()\n"
        "{\n"
        "}\n";
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW |
                              SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER);
    EXPECT_FALSE(foundInGLSLCode("#extension GL_NV_viewport_array2"));
    EXPECT_FALSE(foundInESSLCode("#extension GL_NV_viewport_array2"));
}

// The test checks that the GL_NV_viewport_array2 extension is not emitted in a fragment shader if
// the SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER option is set.
TEST_F(OVRMultiviewFragmentShaderOutputCodeTest, ViewportArray2IsNotEmitted)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "void main()\n"
        "{\n"
        "}\n";
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW |
                              SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER);
    EXPECT_FALSE(foundInGLSLCode("#extension GL_NV_viewport_array2"));
    EXPECT_FALSE(foundInESSLCode("#extension GL_NV_viewport_array2"));
}

// The test checks that the GL_NV_viewport_array2 extension is not emitted in a compute shader if
// the SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER option is set.
TEST_F(OVRMultiviewComputeShaderOutputCodeTest, ViewportArray2IsNotEmitted)
{
    const std::string &shaderString =
        R"(#version 310 es
        #extension GL_OVR_multiview2 : require
        void main()
        {
        })";
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW |
                              SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER);
    EXPECT_FALSE(foundInGLSLCode("#extension GL_NV_viewport_array2"));
    EXPECT_FALSE(foundInESSLCode("#extension GL_NV_viewport_array2"));
}

// The test checks that the viewport index is selected after the initialization of ViewID_OVR for
// GLSL and ESSL ouputs.
TEST_F(OVRMultiviewVertexShaderOutputCodeTest, GlViewportIndexIsSet)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 3) in;\n"
        "void main()\n"
        "{\n"
        "}\n";
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW |
                              SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER);

    std::vector<const char *> expectedStrings = {"ViewID_OVR = (uint(gl_InstanceID) % 3u)",
                                                 "gl_ViewportIndex = int(ViewID_OVR)"};
    EXPECT_TRUE(foundInCodeInOrder(SH_ESSL_OUTPUT, expectedStrings));
    EXPECT_TRUE(foundInCodeInOrder(SH_GLSL_COMPATIBILITY_OUTPUT, expectedStrings));
}

// The test checks that the layer is selected after the initialization of ViewID_OVR for
// GLSL and ESSL ouputs.
TEST_F(OVRMultiviewVertexShaderOutputCodeTest, GlLayerIsSet)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "#extension GL_OVR_multiview2 : require\n"
        "layout(num_views = 3) in;\n"
        "void main()\n"
        "{\n"
        "}\n";
    compile(shaderString, SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW |
                              SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER);

    std::vector<const char *> expectedStrings = {
        "ViewID_OVR = (uint(gl_InstanceID) % 3u)",
        "gl_Layer = (int(ViewID_OVR) + multiviewBaseViewLayerIndex)"};
    EXPECT_TRUE(foundInCodeInOrder(SH_ESSL_OUTPUT, expectedStrings));
    EXPECT_TRUE(foundInCodeInOrder(SH_GLSL_COMPATIBILITY_OUTPUT, expectedStrings));
}

}  // namespace