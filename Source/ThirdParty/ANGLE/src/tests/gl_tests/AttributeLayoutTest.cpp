//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AttributeLayoutTest:
//   Test various layouts of vertex attribute data:
//   - in memory, in buffer object, or combination of both
//   - sequential or interleaved
//   - various combinations of data types

#include <vector>

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

// Test will draw these four triangles.
// clang-format off
constexpr double kTriangleData[] = {
    // xy       rgb
    0,0,        1,1,0,
    -1,+1,      1,1,0,
    +1,+1,      1,1,0,

    0,0,        0,1,0,
    +1,+1,      0,1,0,
    +1,-1,      0,1,0,

    0,0,        0,1,1,
    +1,-1,      0,1,1,
    -1,-1,      0,1,1,

    0,0,        1,0,1,
    -1,-1,      1,0,1,
    -1,+1,      1,0,1,
};
// clang-format on

constexpr size_t kNumVertices = ArraySize(kTriangleData) / 5;

// Vertex data source description.
class VertexData
{
  public:
    VertexData(int dimension, const double *data, unsigned offset, unsigned stride)
        : mDimension(dimension), mData(data), mOffset(offset), mStride(stride)
    {}
    int getDimension() const { return mDimension; }
    double getValue(unsigned vertexNumber, int component) const
    {
        return mData[mOffset + mStride * vertexNumber + component];
    }

  private:
    int mDimension;
    const double *mData;
    // offset and stride in doubles
    unsigned mOffset;
    unsigned mStride;
};

// A container for one or more vertex attributes.
class Container
{
  public:
    static constexpr size_t kSize = 1024;

    void open(void) { memset(mMemory, 0xff, kSize); }
    void *getDestination(size_t offset) { return mMemory + offset; }
    virtual void close(void) {}
    virtual ~Container() {}
    virtual const char *getAddress() = 0;
    virtual GLuint getBuffer()       = 0;

  protected:
    char mMemory[kSize];
};

// Vertex attribute data in client memory.
class Memory : public Container
{
  public:
    const char *getAddress() override { return mMemory; }
    GLuint getBuffer() override { return 0; }
};

// Vertex attribute data in buffer object.
class Buffer : public Container
{
  public:
    void close(void) override
    {
        glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(mMemory), mMemory, GL_STATIC_DRAW);
    }

    const char *getAddress() override { return nullptr; }
    GLuint getBuffer() override { return mBuffer; }

  protected:
    GLBuffer mBuffer;
};

// Encapsulate the storage, layout, format and data of a vertex attribute.
struct Attrib
{
    void openContainer(void) const { mContainer->open(); }

    void fillContainer(void) const
    {
        for (unsigned i = 0; i < kNumVertices; ++i)
        {
            for (int j = 0; j < mData.getDimension(); ++j)
            {
                size_t destOffset = mOffset + mStride * i + mCTypeSize * j;
                if (destOffset + mCTypeSize > Container::kSize)
                    FAIL() << "test case does not fit container";

                double value = mData.getValue(i, j);
                if (mGLType == GL_FIXED)
                    value *= 1 << 16;
                else if (mNormalized)
                {
                    if (value < mMinIn || value > mMaxIn)
                        FAIL() << "test data does not fit format";
                    value = (value - mMinIn) * mScale + mMinOut;
                }

                mStore(value, mContainer->getDestination(destOffset));
            }
        }
    }

    void closeContainer(void) const { mContainer->close(); }

    void enable(unsigned index) const
    {
        glBindBuffer(GL_ARRAY_BUFFER, mContainer->getBuffer());
        glVertexAttribPointer(index, mData.getDimension(), mGLType, mNormalized, mStride,
                              mContainer->getAddress() + mOffset);
        EXPECT_GL_NO_ERROR();
        glEnableVertexAttribArray(index);
    }

    bool inClientMemory(void) const { return mContainer->getAddress() != nullptr; }

    std::shared_ptr<Container> mContainer;
    unsigned mOffset;
    unsigned mStride;
    const VertexData &mData;
    void (*mStore)(double value, void *dest);
    GLenum mGLType;
    GLboolean mNormalized;
    size_t mCTypeSize;
    double mMinIn;
    double mMaxIn;
    double mMinOut;
    double mScale;
};

// Change type and store.
template <class T>
void Store(double value, void *dest)
{
    T v = static_cast<T>(value);
    memcpy(dest, &v, sizeof(v));
}

// Function object that makes Attrib structs according to a vertex format.
template <class CType, GLenum GLType, bool Normalized>
class Format
{
    static_assert(!(Normalized && GLType == GL_FLOAT), "Normalized float does not make sense.");

  public:
    Format(bool es3) : mES3(es3) {}

    Attrib operator()(std::shared_ptr<Container> container,
                      unsigned offset,
                      unsigned stride,
                      const VertexData &data) const
    {
        double minIn    = 0;
        double maxIn    = 1;
        double minOut   = std::numeric_limits<CType>::min();
        double rangeOut = std::numeric_limits<CType>::max() - minOut;

        if (std::is_signed<CType>::value)
        {
            minIn = -1;
            maxIn = +1;
            if (mES3)
            {
                minOut += 1;
                rangeOut -= 1;
            }
        }

        return {
            container,  offset,        stride, data,  Store<CType>, GLType,
            Normalized, sizeof(CType), minIn,  maxIn, minOut,       rangeOut / (maxIn - minIn),
        };
    }

  protected:
    const bool mES3;
};

typedef std::vector<Attrib> TestCase;

void PrepareTestCase(const TestCase &tc)
{
    for (const Attrib &a : tc)
        a.openContainer();
    for (const Attrib &a : tc)
        a.fillContainer();
    for (const Attrib &a : tc)
        a.closeContainer();
    unsigned i = 0;
    for (const Attrib &a : tc)
        a.enable(i++);
}

class AttributeLayoutTest : public ANGLETest
{
  protected:
    AttributeLayoutTest()
        : mProgram(0), mCoord(2, kTriangleData, 0, 5), mColor(3, kTriangleData, 2, 5)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void GetTestCases(void);

    void SetUp() override
    {
        ANGLETest::SetUp();

        glClearColor(.2f, .2f, .2f, .0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);

        constexpr char kVS[] =
            "attribute mediump vec2 coord;\n"
            "attribute mediump vec3 color;\n"
            "varying mediump vec3 vcolor;\n"
            "void main(void)\n"
            "{\n"
            "    gl_Position = vec4(coord, 0, 1);\n"
            "    vcolor = color;\n"
            "}\n";

        constexpr char kFS[] =
            "varying mediump vec3 vcolor;\n"
            "void main(void)\n"
            "{\n"
            "    gl_FragColor = vec4(vcolor, 0);\n"
            "}\n";

        mProgram = CompileProgram(kVS, kFS);
        ASSERT_NE(0u, mProgram);
        glUseProgram(mProgram);

        glGenBuffers(1, &mIndexBuffer);

        GetTestCases();
    }

    void TearDown() override
    {
        mTestCases.clear();
        glDeleteProgram(mProgram);
        glDeleteBuffers(1, &mIndexBuffer);
        ANGLETest::TearDown();
    }

    virtual bool Skip(const TestCase &) { return false; }
    virtual void Draw(int firstVertex, unsigned vertexCount, const GLushort *indices) = 0;

    void Run(bool drawFirstTriangle)
    {
        glViewport(0, 0, getWindowWidth(), getWindowHeight());
        glUseProgram(mProgram);

        for (unsigned i = 0; i < mTestCases.size(); ++i)
        {
            if (mTestCases[i].size() == 0 || Skip(mTestCases[i]))
                continue;

            PrepareTestCase(mTestCases[i]);

            glClear(GL_COLOR_BUFFER_BIT);

            std::string testCase;
            if (drawFirstTriangle)
            {
                Draw(0, kNumVertices, mIndices);
                testCase = "draw";
            }
            else
            {
                Draw(3, kNumVertices - 3, mIndices + 3);
                testCase = "skip";
            }

            testCase += " first triangle case ";
            int w = getWindowWidth() / 4;
            int h = getWindowHeight() / 4;
            if (drawFirstTriangle)
            {
                EXPECT_PIXEL_EQ(w * 2, h * 3, 255, 255, 0, 0) << testCase << i;
            }
            else
            {
                EXPECT_PIXEL_EQ(w * 2, h * 3, 51, 51, 51, 0) << testCase << i;
            }
            EXPECT_PIXEL_EQ(w * 3, h * 2, 0, 255, 0, 0) << testCase << i;
            EXPECT_PIXEL_EQ(w * 2, h * 1, 0, 255, 255, 0) << testCase << i;
            EXPECT_PIXEL_EQ(w * 1, h * 2, 255, 0, 255, 0) << testCase << i;
        }
    }

    static const GLushort mIndices[kNumVertices];

    GLuint mProgram;
    GLuint mIndexBuffer;

    std::vector<TestCase> mTestCases;

    VertexData mCoord;
    VertexData mColor;
};
const GLushort AttributeLayoutTest::mIndices[kNumVertices] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

void AttributeLayoutTest::GetTestCases(void)
{
    const bool es3 = getClientMajorVersion() >= 3;

    Format<GLfloat, GL_FLOAT, false> Float(es3);
    Format<GLint, GL_FIXED, false> Fixed(es3);

    Format<GLbyte, GL_BYTE, false> SByte(es3);
    Format<GLubyte, GL_UNSIGNED_BYTE, false> UByte(es3);
    Format<GLshort, GL_SHORT, false> SShort(es3);
    Format<GLushort, GL_UNSIGNED_SHORT, false> UShort(es3);
    Format<GLint, GL_INT, false> SInt(es3);
    Format<GLuint, GL_UNSIGNED_INT, false> UInt(es3);

    Format<GLbyte, GL_BYTE, true> NormSByte(es3);
    Format<GLubyte, GL_UNSIGNED_BYTE, true> NormUByte(es3);
    Format<GLshort, GL_SHORT, true> NormSShort(es3);
    Format<GLushort, GL_UNSIGNED_SHORT, true> NormUShort(es3);
    Format<GLint, GL_INT, true> NormSInt(es3);
    Format<GLuint, GL_UNSIGNED_INT, true> NormUInt(es3);

    std::shared_ptr<Container> M0 = std::make_shared<Memory>();
    std::shared_ptr<Container> M1 = std::make_shared<Memory>();
    std::shared_ptr<Container> B0 = std::make_shared<Buffer>();
    std::shared_ptr<Container> B1 = std::make_shared<Buffer>();

    // 0. two buffers
    mTestCases.push_back({Float(B0, 0, 8, mCoord), Float(B1, 0, 12, mColor)});

    // 1. two memory
    mTestCases.push_back({Float(M0, 0, 8, mCoord), Float(M1, 0, 12, mColor)});

    // 2. one memory, sequential
    mTestCases.push_back({Float(M0, 0, 8, mCoord), Float(M0, 96, 12, mColor)});

    // 3. one memory, interleaved
    mTestCases.push_back({Float(M0, 0, 20, mCoord), Float(M0, 8, 20, mColor)});

    // 4. buffer and memory
    mTestCases.push_back({Float(B0, 0, 8, mCoord), Float(M0, 0, 12, mColor)});

    // 5. stride != size
    mTestCases.push_back({Float(B0, 0, 16, mCoord), Float(B1, 0, 12, mColor)});

    // 6-9. byte/short
    mTestCases.push_back({SByte(M0, 0, 20, mCoord), UByte(M0, 10, 20, mColor)});
    mTestCases.push_back({SShort(M0, 0, 20, mCoord), UShort(M0, 8, 20, mColor)});
    mTestCases.push_back({NormSByte(M0, 0, 8, mCoord), NormUByte(M0, 4, 8, mColor)});
    mTestCases.push_back({NormSShort(M0, 0, 20, mCoord), NormUShort(M0, 8, 20, mColor)});

    // 10. one buffer, sequential
    mTestCases.push_back({Fixed(B0, 0, 8, mCoord), Float(B0, 96, 12, mColor)});

    // 11. one buffer, interleaved
    mTestCases.push_back({Fixed(B0, 0, 20, mCoord), Float(B0, 8, 20, mColor)});

    // 12. memory and buffer, float and integer
    mTestCases.push_back({Float(M0, 0, 8, mCoord), SByte(B0, 0, 12, mColor)});

    // 13. buffer and memory, unusual offset and stride
    mTestCases.push_back({Float(B0, 11, 13, mCoord), Float(M0, 23, 17, mColor)});

    // 14-15. remaining ES3 formats
    if (es3)
    {
        mTestCases.push_back({SInt(M0, 0, 40, mCoord), UInt(M0, 16, 40, mColor)});
        // Fails on Nexus devices (anglebug.com/2641)
        if (!IsNexus5X() && !IsNexus6P())
            mTestCases.push_back({NormSInt(M0, 0, 40, mCoord), NormUInt(M0, 16, 40, mColor)});
    }
}

class AttributeLayoutNonIndexed : public AttributeLayoutTest
{
    void Draw(int firstVertex, unsigned vertexCount, const GLushort *indices) override
    {
        glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
    }
};

class AttributeLayoutMemoryIndexed : public AttributeLayoutTest
{
    void Draw(int firstVertex, unsigned vertexCount, const GLushort *indices) override
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_SHORT, indices);
    }
};

class AttributeLayoutBufferIndexed : public AttributeLayoutTest
{
    void Draw(int firstVertex, unsigned vertexCount, const GLushort *indices) override
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(*mIndices) * vertexCount, indices,
                     GL_STATIC_DRAW);
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_SHORT, nullptr);
    }
};

TEST_P(AttributeLayoutNonIndexed, Test)
{
    Run(true);
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsOpenGL());
    Run(false);
}

TEST_P(AttributeLayoutMemoryIndexed, Test)
{
    Run(true);
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && (IsOpenGL() || IsD3D11_FL93()));
    Run(false);
}

TEST_P(AttributeLayoutBufferIndexed, Test)
{
    Run(true);
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && (IsOpenGL() || IsD3D11_FL93()));
    Run(false);
}

#define PARAMS                                                                            \
    ES2_VULKAN(), ES2_OPENGL(), ES2_D3D9(), ES2_D3D11(), ES2_D3D11_FL9_3(), ES3_OPENGL(), \
        ES2_OPENGLES(), ES3_OPENGLES()

ANGLE_INSTANTIATE_TEST(AttributeLayoutNonIndexed, PARAMS);
ANGLE_INSTANTIATE_TEST(AttributeLayoutMemoryIndexed, PARAMS);
ANGLE_INSTANTIATE_TEST(AttributeLayoutBufferIndexed, PARAMS);

}  // anonymous namespace
