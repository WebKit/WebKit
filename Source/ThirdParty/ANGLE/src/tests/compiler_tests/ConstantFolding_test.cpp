//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ConstantFolding_test.cpp:
//   Tests for constant folding
//

#include <vector>

#include "angle_gl.h"
#include "gtest/gtest.h"
#include "GLSLANG/ShaderLang.h"
#include "compiler/translator/PoolAlloc.h"
#include "compiler/translator/TranslatorESSL.h"

template <typename T>
class ConstantFinder : public TIntermTraverser
{
  public:
    ConstantFinder(const std::vector<T> &constantVector)
        : TIntermTraverser(true, false, false),
          mConstantVector(constantVector),
          mFaultTolerance(T()),
          mFound(false)
    {}

    ConstantFinder(const std::vector<T> &constantVector, const T &faultTolerance)
        : TIntermTraverser(true, false, false),
        mConstantVector(constantVector),
        mFaultTolerance(faultTolerance),
        mFound(false)
    {}

    ConstantFinder(const T &value)
        : TIntermTraverser(true, false, false),
          mFaultTolerance(T()),
          mFound(false)
    {
        mConstantVector.push_back(value);
    }

    void visitConstantUnion(TIntermConstantUnion *node)
    {
        if (node->getType().getObjectSize() == mConstantVector.size())
        {
            bool found = true;
            for (size_t i = 0; i < mConstantVector.size(); i++)
            {
                if (!isEqual(node->getUnionArrayPointer()[i], mConstantVector[i]))
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                mFound = found;
            }
        }
    }

    bool found() const { return mFound; }

  private:
    bool isEqual(const TConstantUnion &node, const float &value) const
    {
        return mFaultTolerance >= fabsf(node.getFConst() - value);
    }

    bool isEqual(const TConstantUnion &node, const int &value) const
    {
        ASSERT(mFaultTolerance < std::numeric_limits<int>::max());
        // abs() returns 0 at least on some platforms when the minimum int value is passed in (it
        // doesn't have a positive counterpart).
        return mFaultTolerance >= abs(node.getIConst() - value) &&
               (node.getIConst() - value) != std::numeric_limits<int>::min();
    }

    bool isEqual(const TConstantUnion &node, const unsigned int &value) const
    {
        ASSERT(mFaultTolerance < static_cast<unsigned int>(std::numeric_limits<int>::max()));
        return static_cast<int>(mFaultTolerance) >=
                   abs(static_cast<int>(node.getUConst() - value)) &&
               static_cast<int>(node.getUConst() - value) != std::numeric_limits<int>::min();
    }

    bool isEqual(const TConstantUnion &node, const bool &value) const
    {
        return node.getBConst() == value;
    }

    std::vector<T> mConstantVector;
    T mFaultTolerance;
    bool mFound;
};

class ConstantFoldingTest : public testing::Test
{
  public:
    ConstantFoldingTest() {}

  protected:
    virtual void SetUp()
    {
        allocator.push();
        SetGlobalPoolAllocator(&allocator);
        ShBuiltInResources resources;
        ShInitBuiltInResources(&resources);

        mTranslatorESSL = new TranslatorESSL(GL_FRAGMENT_SHADER, SH_GLES3_SPEC);
        ASSERT_TRUE(mTranslatorESSL->Init(resources));
    }

    virtual void TearDown()
    {
        delete mTranslatorESSL;
        SetGlobalPoolAllocator(NULL);
        allocator.pop();
    }

    void compile(const std::string& shaderString)
    {
        const char *shaderStrings[] = { shaderString.c_str() };

        mASTRoot = mTranslatorESSL->compileTreeForTesting(shaderStrings, 1, SH_OBJECT_CODE);
        if (!mASTRoot)
        {
            TInfoSink &infoSink = mTranslatorESSL->getInfoSink();
            FAIL() << "Shader compilation into ESSL failed " << infoSink.info.c_str();
        }
    }

    template <typename T>
    bool constantFoundInAST(T constant)
    {
        ConstantFinder<T> finder(constant);
        mASTRoot->traverse(&finder);
        return finder.found();
    }

    template <typename T>
    bool constantVectorFoundInAST(const std::vector<T> &constantVector)
    {
        ConstantFinder<T> finder(constantVector);
        mASTRoot->traverse(&finder);
        return finder.found();
    }

    template <typename T>
    bool constantColumnMajorMatrixFoundInAST(const std::vector<T> &constantMatrix)
    {
        return constantVectorFoundInAST(constantMatrix);
    }

    template <typename T>
    bool constantVectorNearFoundInAST(const std::vector<T> &constantVector, const T &faultTolerance)
    {
        ConstantFinder<T> finder(constantVector, faultTolerance);
        mASTRoot->traverse(&finder);
        return finder.found();
    }

  private:
    TranslatorESSL *mTranslatorESSL;
    TIntermNode *mASTRoot;

    TPoolAllocator allocator;
};

TEST_F(ConstantFoldingTest, FoldIntegerAdd)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out int my_Int;\n"
        "void main() {\n"
        "   const int i = 1124 + 5;\n"
        "   my_Int = i;\n"
        "}\n";
    compile(shaderString);
    ASSERT_FALSE(constantFoundInAST(1124));
    ASSERT_FALSE(constantFoundInAST(5));
    ASSERT_TRUE(constantFoundInAST(1129));
}

TEST_F(ConstantFoldingTest, FoldIntegerSub)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out int my_Int;\n"
        "void main() {\n"
        "   const int i = 1124 - 5;\n"
        "   my_Int = i;\n"
        "}\n";
    compile(shaderString);
    ASSERT_FALSE(constantFoundInAST(1124));
    ASSERT_FALSE(constantFoundInAST(5));
    ASSERT_TRUE(constantFoundInAST(1119));
}

TEST_F(ConstantFoldingTest, FoldIntegerMul)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out int my_Int;\n"
        "void main() {\n"
        "   const int i = 1124 * 5;\n"
        "   my_Int = i;\n"
        "}\n";
    compile(shaderString);
    ASSERT_FALSE(constantFoundInAST(1124));
    ASSERT_FALSE(constantFoundInAST(5));
    ASSERT_TRUE(constantFoundInAST(5620));
}

TEST_F(ConstantFoldingTest, FoldIntegerDiv)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out int my_Int;\n"
        "void main() {\n"
        "   const int i = 1124 / 5;\n"
        "   my_Int = i;\n"
        "}\n";
    compile(shaderString);
    ASSERT_FALSE(constantFoundInAST(1124));
    ASSERT_FALSE(constantFoundInAST(5));
    // Rounding mode of division is undefined in the spec but ANGLE can be expected to round down.
    ASSERT_TRUE(constantFoundInAST(224));
}

TEST_F(ConstantFoldingTest, FoldIntegerModulus)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out int my_Int;\n"
        "void main() {\n"
        "   const int i = 1124 % 5;\n"
        "   my_Int = i;\n"
        "}\n";
    compile(shaderString);
    ASSERT_FALSE(constantFoundInAST(1124));
    ASSERT_FALSE(constantFoundInAST(5));
    ASSERT_TRUE(constantFoundInAST(4));
}

TEST_F(ConstantFoldingTest, FoldVectorCrossProduct)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec3 my_Vec3;"
        "void main() {\n"
        "   const vec3 v3 = cross(vec3(1.0f, 1.0f, 1.0f), vec3(1.0f, -1.0f, 1.0f));\n"
        "   my_Vec3 = v3;\n"
        "}\n";
    compile(shaderString);
    std::vector<float> input1(3, 1.0f);
    ASSERT_FALSE(constantVectorFoundInAST(input1));
    std::vector<float> input2;
    input2.push_back(1.0f);
    input2.push_back(-1.0f);
    input2.push_back(1.0f);
    ASSERT_FALSE(constantVectorFoundInAST(input2));
    std::vector<float> result;
    result.push_back(2.0f);
    result.push_back(0.0f);
    result.push_back(-2.0f);
    ASSERT_TRUE(constantVectorFoundInAST(result));
}

// FoldMxNMatrixInverse tests check if the matrix 'inverse' operation
// on MxN matrix is constant folded when argument is constant expression and also
// checks the correctness of the result returned by the constant folding operation.
// All the matrices including matrices in the shader code are in column-major order.
TEST_F(ConstantFoldingTest, Fold2x2MatrixInverse)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float i;\n"
        "out vec2 my_Vec;\n"
        "void main() {\n"
        "   const mat2 m2 = inverse(mat2(2.0f, 3.0f,\n"
        "                                5.0f, 7.0f));\n"
        "   mat2 m = m2 * mat2(i);\n"
        "   my_Vec = m[0];\n"
        "}\n";
    compile(shaderString);
    float inputElements[] =
    {
        2.0f, 3.0f,
        5.0f, 7.0f
    };
    std::vector<float> input(inputElements, inputElements + 4);
    ASSERT_FALSE(constantColumnMajorMatrixFoundInAST(input));
    float outputElements[] =
    {
        -7.0f, 3.0f,
        5.0f, -2.0f
    };
    std::vector<float> result(outputElements, outputElements + 4);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}

// Check if the matrix 'inverse' operation on 3x3 matrix is constant folded.
TEST_F(ConstantFoldingTest, Fold3x3MatrixInverse)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float i;\n"
        "out vec3 my_Vec;\n"
        "void main() {\n"
        "   const mat3 m3 = inverse(mat3(11.0f, 13.0f, 19.0f,\n"
        "                                23.0f, 29.0f, 31.0f,\n"
        "                                37.0f, 41.0f, 43.0f));\n"
        "   mat3 m = m3 * mat3(i);\n"
        "   my_Vec = m3[0];\n"
        "}\n";
    compile(shaderString);
    float inputElements[] =
    {
        11.0f, 13.0f, 19.0f,
        23.0f, 29.0f, 31.0f,
        37.0f, 41.0f, 43.0f
    };
    std::vector<float> input(inputElements, inputElements + 9);
    ASSERT_FALSE(constantVectorFoundInAST(input));
    float outputElements[] =
    {
        3.0f / 85.0f, -11.0f / 34.0f, 37.0f / 170.0f,
        -79.0f / 340.0f, 23.0f / 68.0f, -12.0f / 85.0f,
        13.0f / 68.0f, -3.0f / 68.0f, -1.0f / 34.0f
    };
    std::vector<float> result(outputElements, outputElements + 9);
    const float floatFaultTolerance = 0.000001f;
    ASSERT_TRUE(constantVectorNearFoundInAST(result, floatFaultTolerance));
}

// Check if the matrix 'inverse' operation on 4x4 matrix is constant folded.
TEST_F(ConstantFoldingTest, Fold4x4MatrixInverse)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float i;\n"
        "out vec4 my_Vec;\n"
        "void main() {\n"
        "   const mat4 m4 = inverse(mat4(29.0f, 31.0f, 37.0f, 41.0f,\n"
        "                                43.0f, 47.0f, 53.0f, 59.0f,\n"
        "                                61.0f, 67.0f, 71.0f, 73.0f,\n"
        "                                79.0f, 83.0f, 89.0f, 97.0f));\n"
        "   mat4 m = m4 * mat4(i);\n"
        "   my_Vec = m[0];\n"
        "}\n";
    compile(shaderString);
    float inputElements[] =
    {
        29.0f, 31.0f, 37.0f, 41.0f,
        43.0f, 47.0f, 53.0f, 59.0f,
        61.0f, 67.0f, 71.0f, 73.0f,
        79.0f, 83.0f, 89.0f, 97.0f
    };
    std::vector<float> input(inputElements, inputElements + 16);
    ASSERT_FALSE(constantVectorFoundInAST(input));
    float outputElements[] =
    {
        43.0f / 126.0f, -11.0f / 21.0f, -2.0f / 21.0f, 31.0f / 126.0f,
        -5.0f / 7.0f, 9.0f / 14.0f, 1.0f / 14.0f, -1.0f / 7.0f,
        85.0f / 126.0f, -11.0f / 21.0f, 43.0f / 210.0f, -38.0f / 315.0f,
        -2.0f / 7.0f, 5.0f / 14.0f, -6.0f / 35.0f, 3.0f / 70.0f
    };
    std::vector<float> result(outputElements, outputElements + 16);
    const float floatFaultTolerance = 0.00001f;
    ASSERT_TRUE(constantVectorNearFoundInAST(result, floatFaultTolerance));
}

// FoldMxNMatrixDeterminant tests check if the matrix 'determinant' operation
// on MxN matrix is constant folded when argument is constant expression and also
// checks the correctness of the result returned by the constant folding operation.
// All the matrices including matrices in the shader code are in column-major order.
TEST_F(ConstantFoldingTest, Fold2x2MatrixDeterminant)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out float my_Float;"
        "void main() {\n"
        "   const float f = determinant(mat2(2.0f, 3.0f,\n"
        "                                    5.0f, 7.0f));\n"
        "   my_Float = f;\n"
        "}\n";
    compile(shaderString);
    float inputElements[] =
    {
        2.0f, 3.0f,
        5.0f, 7.0f
    };
    std::vector<float> input(inputElements, inputElements + 4);
    ASSERT_FALSE(constantColumnMajorMatrixFoundInAST(input));
    ASSERT_TRUE(constantFoundInAST(-1.0f));
}

// Check if the matrix 'determinant' operation on 3x3 matrix is constant folded.
TEST_F(ConstantFoldingTest, Fold3x3MatrixDeterminant)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out float my_Float;"
        "void main() {\n"
        "   const float f = determinant(mat3(11.0f, 13.0f, 19.0f,\n"
             "                               23.0f, 29.0f, 31.0f,\n"
        "                                    37.0f, 41.0f, 43.0f));\n"
        "   my_Float = f;\n"
        "}\n";
    compile(shaderString);
    float inputElements[] =
    {
        11.0f, 13.0f, 19.0f,
        23.0f, 29.0f, 31.0f,
        37.0f, 41.0f, 43.0f
    };
    std::vector<float> input(inputElements, inputElements + 9);
    ASSERT_FALSE(constantColumnMajorMatrixFoundInAST(input));
    ASSERT_TRUE(constantFoundInAST(-680.0f));
}

// Check if the matrix 'determinant' operation on 4x4 matrix is constant folded.
TEST_F(ConstantFoldingTest, Fold4x4MatrixDeterminant)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out float my_Float;"
        "void main() {\n"
        "   const float f = determinant(mat4(29.0f, 31.0f, 37.0f, 41.0f,\n"
        "                                    43.0f, 47.0f, 53.0f, 59.0f,\n"
        "                                    61.0f, 67.0f, 71.0f, 73.0f,\n"
        "                                    79.0f, 83.0f, 89.0f, 97.0f));\n"
        "   my_Float = f;\n"
        "}\n";
    compile(shaderString);
    float inputElements[] =
    {
        29.0f, 31.0f, 37.0f, 41.0f,
        43.0f, 47.0f, 53.0f, 59.0f,
        61.0f, 67.0f, 71.0f, 73.0f,
        79.0f, 83.0f, 89.0f, 97.0f
    };
    std::vector<float> input(inputElements, inputElements + 16);
    ASSERT_FALSE(constantColumnMajorMatrixFoundInAST(input));
    ASSERT_TRUE(constantFoundInAST(-2520.0f));
}

// Check if the matrix 'transpose' operation on 3x3 matrix is constant folded.
// All the matrices including matrices in the shader code are in column-major order.
TEST_F(ConstantFoldingTest, Fold3x3MatrixTranspose)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float i;\n"
        "out vec3 my_Vec;\n"
        "void main() {\n"
        "   const mat3 m3 = transpose(mat3(11.0f, 13.0f, 19.0f,\n"
        "                                  23.0f, 29.0f, 31.0f,\n"
        "                                  37.0f, 41.0f, 43.0f));\n"
        "   mat3 m = m3 * mat3(i);\n"
        "   my_Vec = m[0];\n"
        "}\n";
    compile(shaderString);
    float inputElements[] =
    {
        11.0f, 13.0f, 19.0f,
        23.0f, 29.0f, 31.0f,
        37.0f, 41.0f, 43.0f
    };
    std::vector<float> input(inputElements, inputElements + 9);
    ASSERT_FALSE(constantColumnMajorMatrixFoundInAST(input));
    float outputElements[] =
    {
        11.0f, 23.0f, 37.0f,
        13.0f, 29.0f, 41.0f,
        19.0f, 31.0f, 43.0f
    };
    std::vector<float> result(outputElements, outputElements + 9);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}

// Test that 0xFFFFFFFF wraps to -1 when parsed as integer.
// This is featured in the examples of ESSL3 section 4.1.3. ESSL3 section 12.42
// means that any 32-bit unsigned integer value is a valid literal.
TEST_F(ConstantFoldingTest, ParseWrappedHexIntLiteral)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "precision highp int;\n"
        "uniform int inInt;\n"
        "out vec4 my_Vec;\n"
        "void main() {\n"
        "   const int i = 0xFFFFFFFF;\n"
        "   my_Vec = vec4(i * inInt);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(-1));
}

// Test that 3000000000 wraps to -1294967296 when parsed as integer.
// This is featured in the examples of GLSL 4.5, and ESSL behavior should match
// desktop GLSL when it comes to integer parsing.
TEST_F(ConstantFoldingTest, ParseWrappedDecimalIntLiteral)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "precision highp int;\n"
        "uniform int inInt;\n"
        "out vec4 my_Vec;\n"
        "void main() {\n"
        "   const int i = 3000000000;\n"
        "   my_Vec = vec4(i * inInt);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(-1294967296));
}

// Test that 0xFFFFFFFFu is parsed correctly as an unsigned integer literal.
// This is featured in the examples of ESSL3 section 4.1.3. ESSL3 section 12.42
// means that any 32-bit unsigned integer value is a valid literal.
TEST_F(ConstantFoldingTest, ParseMaxUintLiteral)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "precision highp int;\n"
        "uniform uint inInt;\n"
        "out vec4 my_Vec;\n"
        "void main() {\n"
        "   const uint i = 0xFFFFFFFFu;\n"
        "   my_Vec = vec4(i * inInt);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0xFFFFFFFFu));
}

// Test that unary minus applied to unsigned int is constant folded correctly.
// This is featured in the examples of ESSL3 section 4.1.3. ESSL3 section 12.42
// means that any 32-bit unsigned integer value is a valid literal.
TEST_F(ConstantFoldingTest, FoldUnaryMinusOnUintLiteral)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "precision highp int;\n"
        "uniform uint inInt;\n"
        "out vec4 my_Vec;\n"
        "void main() {\n"
        "   const uint i = -1u;\n"
        "   my_Vec = vec4(i * inInt);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0xFFFFFFFFu));
}

// Test that constant mat2 initialization with a mat2 parameter works correctly.
TEST_F(ConstantFoldingTest, FoldMat2ConstructorTakingMat2)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float mult;\n"
        "void main() {\n"
        "   const mat2 cm = mat2(mat2(0.0, 1.0, 2.0, 3.0));\n"
        "   mat2 m = cm * mult;\n"
        "   gl_FragColor = vec4(m[0], m[1]);\n"
        "}\n";
    compile(shaderString);
    float outputElements[] =
    {
        0.0f, 1.0f,
        2.0f, 3.0f
    };
    std::vector<float> result(outputElements, outputElements + 4);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}

// Test that constant mat2 initialization with an int parameter works correctly.
TEST_F(ConstantFoldingTest, FoldMat2ConstructorTakingScalar)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float mult;\n"
        "void main() {\n"
        "   const mat2 cm = mat2(3);\n"
        "   mat2 m = cm * mult;\n"
        "   gl_FragColor = vec4(m[0], m[1]);\n"
        "}\n";
    compile(shaderString);
    float outputElements[] =
    {
        3.0f, 0.0f,
        0.0f, 3.0f
    };
    std::vector<float> result(outputElements, outputElements + 4);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}

// Test that constant mat2 initialization with a mix of parameters works correctly.
TEST_F(ConstantFoldingTest, FoldMat2ConstructorTakingMix)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float mult;\n"
        "void main() {\n"
        "   const mat2 cm = mat2(-1, vec2(0.0, 1.0), vec4(2.0));\n"
        "   mat2 m = cm * mult;\n"
        "   gl_FragColor = vec4(m[0], m[1]);\n"
        "}\n";
    compile(shaderString);
    float outputElements[] =
    {
        -1.0, 0.0f,
        1.0f, 2.0f
    };
    std::vector<float> result(outputElements, outputElements + 4);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}

// Test that constant mat2 initialization with a mat3 parameter works correctly.
TEST_F(ConstantFoldingTest, FoldMat2ConstructorTakingMat3)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float mult;\n"
        "void main() {\n"
        "   const mat2 cm = mat2(mat3(0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0));\n"
        "   mat2 m = cm * mult;\n"
        "   gl_FragColor = vec4(m[0], m[1]);\n"
        "}\n";
    compile(shaderString);
    float outputElements[] =
    {
        0.0f, 1.0f,
        3.0f, 4.0f
    };
    std::vector<float> result(outputElements, outputElements + 4);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}

// Test that constant mat4x3 initialization with a mat3x2 parameter works correctly.
TEST_F(ConstantFoldingTest, FoldMat4x3ConstructorTakingMat3x2)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform float mult;\n"
        "out vec4 my_FragColor;\n"
        "void main() {\n"
        "   const mat4x3 cm = mat4x3(mat3x2(1.0, 2.0,\n"
        "                                   3.0, 4.0,\n"
        "                                   5.0, 6.0));\n"
        "   mat4x3 m = cm * mult;\n"
        "   my_FragColor = vec4(m[0], m[1][0]);\n"
        "}\n";
    compile(shaderString);
    float outputElements[] =
    {
        1.0f, 2.0f, 0.0f,
        3.0f, 4.0f, 0.0f,
        5.0f, 6.0f, 1.0f,
        0.0f, 0.0f, 0.0f
    };
    std::vector<float> result(outputElements, outputElements + 12);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}


// Test that constant mat2 initialization with a vec4 parameter works correctly.
TEST_F(ConstantFoldingTest, FoldMat2ConstructorTakingVec4)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform float mult;\n"
        "void main() {\n"
        "   const mat2 cm = mat2(vec4(0.0, 1.0, 2.0, 3.0));\n"
        "   mat2 m = cm * mult;\n"
        "   gl_FragColor = vec4(m[0], m[1]);\n"
        "}\n";
    compile(shaderString);
    float outputElements[] =
    {
        0.0f, 1.0f,
        2.0f, 3.0f
    };
    std::vector<float> result(outputElements, outputElements + 4);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}

// Test that equality comparison of two different structs with a nested struct inside returns false.
TEST_F(ConstantFoldingTest, FoldNestedDifferentStructEqualityComparison)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "struct nested {\n"
        "    float f\n;"
        "};\n"
        "struct S {\n"
        "    nested a;\n"
        "    float f;\n"
        "};\n"
        "uniform vec4 mult;\n"
        "void main()\n"
        "{\n"
        "    const S s1 = S(nested(0.0), 2.0);\n"
        "    const S s2 = S(nested(0.0), 3.0);\n"
        "    gl_FragColor = (s1 == s2 ? 1.0 : 0.5) * mult;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0.5f));
}

// Test that equality comparison of two identical structs with a nested struct inside returns true.
TEST_F(ConstantFoldingTest, FoldNestedIdenticalStructEqualityComparison)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "struct nested {\n"
        "    float f\n;"
        "};\n"
        "struct S {\n"
        "    nested a;\n"
        "    float f;\n"
        "    int i;\n"
        "};\n"
        "uniform vec4 mult;\n"
        "void main()\n"
        "{\n"
        "    const S s1 = S(nested(0.0), 2.0, 3);\n"
        "    const S s2 = S(nested(0.0), 2.0, 3);\n"
        "    gl_FragColor = (s1 == s2 ? 1.0 : 0.5) * mult;\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(1.0f));
}

// Test that right elements are chosen from non-square matrix
TEST_F(ConstantFoldingTest, FoldNonSquareMatrixIndexing)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = mat3x4(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)[1];\n"
        "}\n";
    compile(shaderString);
    float outputElements[] = {4.0f, 5.0f, 6.0f, 7.0f};
    std::vector<float> result(outputElements, outputElements + 4);
    ASSERT_TRUE(constantVectorFoundInAST(result));
}

// Test that folding outer product of vectors with non-matching lengths works.
TEST_F(ConstantFoldingTest, FoldNonSquareOuterProduct)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    mat3x2 prod = outerProduct(vec2(2.0, 3.0), vec3(5.0, 7.0, 11.0));\n"
        "    my_FragColor = vec4(prod[0].x);\n"
        "}\n";
    compile(shaderString);
    // clang-format off
    float outputElements[] =
    {
        10.0f, 15.0f,
        14.0f, 21.0f,
        22.0f, 33.0f
    };
    // clang-format on
    std::vector<float> result(outputElements, outputElements + 6);
    ASSERT_TRUE(constantColumnMajorMatrixFoundInAST(result));
}

// Test that folding bit shift left with non-matching signedness works.
TEST_F(ConstantFoldingTest, FoldBitShiftLeftDifferentSignedness)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    uint u = 0xffffffffu << 31;\n"
        "    my_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0x80000000u));
}

// Test that folding bit shift right with non-matching signedness works.
TEST_F(ConstantFoldingTest, FoldBitShiftRightDifferentSignedness)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    uint u = 0xffffffffu >> 30;\n"
        "    my_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0x3u));
}

// Test that folding signed bit shift right extends the sign bit.
// ESSL 3.00.6 section 5.9 Expressions.
TEST_F(ConstantFoldingTest, FoldBitShiftRightExtendSignBit)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    const int i = 0x8fffe000 >> 6;\n"
        "    uint u = uint(i);"
        "    my_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    // The bits of the operand are 0x8fffe000 = 1000 1111 1111 1111 1110 0000 0000 0000
    // After shifting, they become              1111 1110 0011 1111 1111 1111 1000 0000 = 0xfe3fff80
    ASSERT_TRUE(constantFoundInAST(0xfe3fff80u));
}

// Signed bit shift left should interpret its operand as a bit pattern. As a consequence a number
// may turn from positive to negative when shifted left.
// ESSL 3.00.6 section 5.9 Expressions.
TEST_F(ConstantFoldingTest, FoldBitShiftLeftInterpretedAsBitPattern)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    const int i = 0x1fffffff << 3;\n"
        "    uint u = uint(i);"
        "    my_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0xfffffff8u));
}

// Test that dividing the minimum signed integer by -1 works.
// ESSL 3.00.6 section 4.1.3 Integers:
// "However, for the case where the minimum representable value is divided by -1, it is allowed to
// return either the minimum representable value or the maximum representable value."
TEST_F(ConstantFoldingTest, FoldDivideMinimumIntegerByMinusOne)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    int i = 0x80000000 / (-1);\n"
        "    my_FragColor = vec4(i);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0x7fffffff) || constantFoundInAST(-0x7fffffff - 1));
}

// Test that folding an unsigned integer addition that overflows works.
// ESSL 3.00.6 section 4.1.3 Integers:
// "For all precisions, operations resulting in overflow or underflow will not cause any exception,
// nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result where
// n is the size in bits of the integer."
TEST_F(ConstantFoldingTest, FoldUnsignedIntegerAddOverflow)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    uint u = 0xffffffffu + 43u;\n"
        "    my_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(42u));
}

// Test that folding a signed integer addition that overflows works.
// ESSL 3.00.6 section 4.1.3 Integers:
// "For all precisions, operations resulting in overflow or underflow will not cause any exception,
// nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result where
// n is the size in bits of the integer."
TEST_F(ConstantFoldingTest, FoldSignedIntegerAddOverflow)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    int i = 0x7fffffff + 4;\n"
        "    my_FragColor = vec4(i);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(-0x7ffffffd));
}

// Test that folding an unsigned integer subtraction that overflows works.
// ESSL 3.00.6 section 4.1.3 Integers:
// "For all precisions, operations resulting in overflow or underflow will not cause any exception,
// nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result where
// n is the size in bits of the integer."
TEST_F(ConstantFoldingTest, FoldUnsignedIntegerDiffOverflow)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    uint u = 0u - 5u;\n"
        "    my_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0xfffffffbu));
}

// Test that folding a signed integer subtraction that overflows works.
// ESSL 3.00.6 section 4.1.3 Integers:
// "For all precisions, operations resulting in overflow or underflow will not cause any exception,
// nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result where
// n is the size in bits of the integer."
TEST_F(ConstantFoldingTest, FoldSignedIntegerDiffOverflow)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    int i = -0x7fffffff - 7;\n"
        "    my_FragColor = vec4(i);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0x7ffffffa));
}

// Test that folding an unsigned integer multiplication that overflows works.
// ESSL 3.00.6 section 4.1.3 Integers:
// "For all precisions, operations resulting in overflow or underflow will not cause any exception,
// nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result where
// n is the size in bits of the integer."
TEST_F(ConstantFoldingTest, FoldUnsignedIntegerMultiplyOverflow)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    uint u = 0xffffffffu * 10u;\n"
        "    my_FragColor = vec4(u);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(0xfffffff6u));
}

// Test that folding a signed integer multiplication that overflows works.
// ESSL 3.00.6 section 4.1.3 Integers:
// "For all precisions, operations resulting in overflow or underflow will not cause any exception,
// nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result where
// n is the size in bits of the integer."
TEST_F(ConstantFoldingTest, FoldSignedIntegerMultiplyOverflow)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    int i = 0x7fffffff * 42;\n"
        "    my_FragColor = vec4(i);\n"
        "}\n";
    compile(shaderString);
    ASSERT_TRUE(constantFoundInAST(-42));
}

// Test that folding of negating the minimum representable integer works. Note that in the test
// "0x80000000" is a negative literal, and the minus sign before it is the negation operator.
// ESSL 3.00.6 section 4.1.3 Integers:
// "For all precisions, operations resulting in overflow or underflow will not cause any exception,
// nor will they saturate, rather they will 'wrap' to yield the low-order n bits of the result where
// n is the size in bits of the integer."
TEST_F(ConstantFoldingTest, FoldMinimumSignedIntegerNegation)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    int i = -0x80000000;\n"
        "    my_FragColor = vec4(i);\n"
        "}\n";
    compile(shaderString);
    // Negating the minimum signed integer overflows the positive range, so it wraps back to itself.
    ASSERT_TRUE(constantFoundInAST(-0x7fffffff - 1));
}
