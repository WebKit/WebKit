//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IndexDataManagerPerfTest:
//   Performance test for index buffer management.
//

#include "ANGLEPerfTest.h"

#include <gmock/gmock.h>

#include "angle_unittests_utils.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/IndexBuffer.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"

using namespace testing;

namespace
{

class MockIndexBuffer : public rx::IndexBuffer
{
  public:
    MockIndexBuffer(unsigned int bufferSize, GLenum indexType)
        : mBufferSize(bufferSize), mIndexType(indexType)
    {
    }

    MOCK_METHOD3(initialize, gl::Error(unsigned int, GLenum, bool));
    MOCK_METHOD3(mapBuffer, gl::Error(unsigned int, unsigned int, void **));
    MOCK_METHOD0(unmapBuffer, gl::Error());
    MOCK_METHOD0(discard, gl::Error());
    MOCK_METHOD2(setSize, gl::Error(unsigned int, GLenum));

    // inlined for speed
    GLenum getIndexType() const override { return mIndexType; }
    unsigned int getBufferSize() const override { return mBufferSize; }

  private:
    unsigned int mBufferSize;
    GLenum mIndexType;
};

class MockBufferFactoryD3D : public rx::BufferFactoryD3D
{
  public:
    MockBufferFactoryD3D(unsigned int bufferSize, GLenum indexType)
        : mBufferSize(bufferSize), mIndexType(indexType)
    {
    }

    MOCK_METHOD0(createVertexBuffer, rx::VertexBuffer *());
    MOCK_CONST_METHOD1(getVertexConversionType, rx::VertexConversionType(gl::VertexFormatType));
    MOCK_CONST_METHOD1(getVertexComponentType, GLenum(gl::VertexFormatType));
    MOCK_CONST_METHOD4(getVertexSpaceRequired,
                       gl::ErrorOrResult<unsigned int>(const gl::VertexAttribute &,
                                                       const gl::VertexBinding &,
                                                       GLsizei,
                                                       GLsizei));

    // Dependency injection
    rx::IndexBuffer *createIndexBuffer() override
    {
        return new MockIndexBuffer(mBufferSize, mIndexType);
    }

  private:
    unsigned int mBufferSize;
    GLenum mIndexType;
};

class MockBufferD3D : public rx::BufferD3D
{
  public:
    MockBufferD3D(rx::BufferFactoryD3D *factory) : BufferD3D(mockState, factory), mData() {}

    // BufferImpl
    gl::Error setData(const gl::Context *context,
                      gl::BufferBinding target,
                      const void *data,
                      size_t size,
                      gl::BufferUsage) override
    {
        mData.resize(size);
        if (data && size > 0)
        {
            memcpy(&mData[0], data, size);
        }
        return gl::NoError();
    }

    MOCK_METHOD5(setSubData,
                 gl::Error(const gl::Context *, gl::BufferBinding, const void *, size_t, size_t));
    MOCK_METHOD5(copySubData,
                 gl::Error(const gl::Context *, BufferImpl *, GLintptr, GLintptr, GLsizeiptr));
    MOCK_METHOD3(map, gl::Error(const gl::Context *context, GLenum, void **));
    MOCK_METHOD5(mapRange, gl::Error(const gl::Context *, size_t, size_t, GLbitfield, void **));
    MOCK_METHOD2(unmap, gl::Error(const gl::Context *context, GLboolean *));

    // BufferD3D
    MOCK_METHOD1(markTransformFeedbackUsage, gl::Error(const gl::Context *));

    // inlined for speed
    bool supportsDirectBinding() const override { return false; }
    size_t getSize() const override { return mData.size(); }

    gl::Error getData(const gl::Context *context, const uint8_t **outData) override
    {
        *outData = &mData[0];
        return gl::NoError();
    }

  private:
    gl::BufferState mockState;
    std::vector<uint8_t> mData;
};

class MockGLFactoryD3D : public rx::MockGLFactory
{
  public:
    MockGLFactoryD3D(MockBufferFactoryD3D *bufferFactory) : mBufferFactory(bufferFactory) {}

    rx::BufferImpl *createBuffer(const gl::BufferState &state) override
    {
        MockBufferD3D *mockBufferD3D = new MockBufferD3D(mBufferFactory);

        EXPECT_CALL(*mBufferFactory, createVertexBuffer())
            .WillOnce(Return(nullptr))
            .RetiresOnSaturation();
        mockBufferD3D->initializeStaticData(nullptr);

        return mockBufferD3D;
    }

    MockBufferFactoryD3D *mBufferFactory;
};

class IndexDataManagerPerfTest : public ANGLEPerfTest
{
  public:
    IndexDataManagerPerfTest();

    void step() override;

    rx::IndexDataManager mIndexDataManager;
    GLsizei mIndexCount;
    unsigned int mBufferSize;
    MockBufferFactoryD3D mMockBufferFactory;
    MockGLFactoryD3D mMockGLFactory;
    gl::Buffer mIndexBuffer;
};

IndexDataManagerPerfTest::IndexDataManagerPerfTest()
    : ANGLEPerfTest("IndexDataManger", "_run"),
      mIndexDataManager(&mMockBufferFactory),
      mIndexCount(4000),
      mBufferSize(mIndexCount * sizeof(GLushort)),
      mMockBufferFactory(mBufferSize, GL_UNSIGNED_SHORT),
      mMockGLFactory(&mMockBufferFactory),
      mIndexBuffer(&mMockGLFactory, 1)
{
    std::vector<GLushort> indexData(mIndexCount);
    for (GLsizei index = 0; index < mIndexCount; ++index)
    {
        indexData[index] = static_cast<GLushort>(index);
    }
    EXPECT_FALSE(mIndexBuffer
                     .bufferData(nullptr, gl::BufferBinding::Array, &indexData[0],
                                 indexData.size() * sizeof(GLushort), gl::BufferUsage::StaticDraw)
                     .isError());
}

void IndexDataManagerPerfTest::step()
{
    rx::TranslatedIndexData translatedIndexData;
    gl::IndexRange indexRange;
    for (unsigned int iteration = 0; iteration < 100; ++iteration)
    {
        (void)mIndexBuffer.getIndexRange(nullptr, GL_UNSIGNED_SHORT, 0, mIndexCount, false,
                                         &indexRange);
        (void)mIndexDataManager.prepareIndexData(nullptr, GL_UNSIGNED_SHORT, GL_UNSIGNED_SHORT,
                                                 mIndexCount, &mIndexBuffer, nullptr,
                                                 &translatedIndexData);
    }
}

TEST_F(IndexDataManagerPerfTest, Run)
{
    run();
}

}  // anonymous namespace
