//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeleteBuffersTest:
//   Tests buffer deletion while buffers are still referenced by pending GPU work
//   and driver copy-on-write operations.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include <cstdint>
#include <vector>

using namespace angle;

namespace
{

class DeleteBuffersTest : public ANGLETest<>
{
  protected:
    static constexpr int kNumLargeBuffers                       = 4;
    static constexpr size_t kLargeBufferSizeMB                  = 64;
    static constexpr int kNumDonorBuffers                       = 192;
    static constexpr int kNumSecondaryBuffers                   = 13;
    static constexpr int kNumPreloadIterations                  = 16;
    static constexpr int kMaxCycles                             = 8;
    static constexpr std::array<int, 3> kBaseTargetDonorIndices = {43, 44, 45};
    static constexpr std::array<int, 8> kTargetIndexOffsets     = {0, 1, -1, 2, -2, 3, -3, 4};

    DeleteBuffersTest()
    {
        setWindowWidth(16);
        setWindowHeight(16);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setHardenedContextEnabled(true);
    }

    void testSetUp() override
    {
        constexpr char kVS[] = R"(#version 300 es
layout(location = 0) in vec4 a;
void main()
{
    gl_Position = vec4(a.x * 1e-6, a.y * 1e-6, 0.0, 1.0);
    gl_PointSize = 1.0;
})";

        constexpr char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 o;
void main()
{
    o = vec4(1.0, 0.0, 0.0, 1.0);
})";

        mProgram.makeRaster(kVS, kFS);
        ASSERT_TRUE(mProgram.valid());
        glUseProgram(mProgram);
        glViewport(0, 0, 16, 16);

        // Allocate persistent buffers so internal driver allocator chunks remain stable
        // across iterations.
        glBindBuffer(GL_ARRAY_BUFFER, mScratchBuffer);
        glBufferData(GL_ARRAY_BUFFER, 64, nullptr, GL_DYNAMIC_DRAW);

        mPrimeBuffer = makeBuffer(65536);
        glBindBuffer(GL_ARRAY_BUFFER, mScratchBuffer);
        glBindBuffer(GL_COPY_READ_BUFFER, mPrimeBuffer);
        const float zeroFloats[4] = {};
        glBufferSubData(GL_COPY_READ_BUFFER, 32, sizeof(zeroFloats), zeroFloats);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        for (GLsync fence : mFences)
        {
            glDeleteSync(fence);
        }
        mFences.clear();

        mPersistentControlBuffer.reset();
        mPersistentControlBufferInitialized = false;
        mPrimeBuffer.reset();
        mScratchBuffer.reset();
        mProgram.reset();
    }

    const void *getPatternData(size_t numBytes)
    {
        size_t numWords = numBytes / sizeof(uint32_t);
        if (mPatternBuffer.size() < numWords)
        {
            size_t oldSize = mPatternBuffer.size();
            mPatternBuffer.resize(numWords);
            for (size_t i = oldSize; i < numWords; ++i)
            {
                mPatternBuffer[i] = 0x9e3779b9u ^ (static_cast<uint32_t>(i) * 2654435761u);
            }
        }
        return mPatternBuffer.data();
    }

    GLBuffer makeBuffer(size_t bytes)
    {
        GLBuffer buffer;
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), getPatternData(bytes),
                     GL_DYNAMIC_DRAW);
        return buffer;
    }

    void readDraw(GLBuffer &buffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
        glDrawArrays(GL_POINTS, 0, 1);
    }

    void recordSubmissionBoundary()
    {
        mFences.push_back(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
    }

    bool pollFence(GLsync fence, int timeoutMs)
    {
        constexpr GLuint64 kNsPerMs = 1000000ULL;
        GLenum result               = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT,
                                                       static_cast<GLuint64>(timeoutMs) * kNsPerMs);
        return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED;
    }

    void runQueuedDeletionCycle(int cycle)
    {
        const float zeroFloats[4] = {};
        std::array<int, 3> targetDonorIndices;
        for (int i = 0; i < 3; ++i)
        {
            targetDonorIndices[i] = kBaseTargetDonorIndices[i] + kTargetIndexOffsets[cycle % 8];
        }

        // Preparation: allocate and reference large buffers in draw calls so subsequent
        // partial updates queue copy-on-write work in the driver.
        std::vector<GLBuffer> largeBuffers;
        largeBuffers.reserve(kNumLargeBuffers);
        for (int i = 0; i < kNumLargeBuffers; ++i)
        {
            GLBuffer largeBuffer = makeBuffer(kLargeBufferSizeMB << 20);
            readDraw(largeBuffer);
            largeBuffers.push_back(std::move(largeBuffer));
        }
        if (!mPersistentControlBufferInitialized)
        {
            mPersistentControlBuffer            = makeBuffer(65536);
            mPersistentControlBufferInitialized = true;
            readDraw(mPersistentControlBuffer);
        }
        glBindBuffer(GL_ARRAY_BUFFER, mScratchBuffer);

        // Pre-load secondary buffers with tracked read references across multiple
        // submission boundaries.
        std::vector<GLBuffer> secondaryBuffers;
        secondaryBuffers.reserve(kNumSecondaryBuffers);
        for (int k = 0; k < kNumSecondaryBuffers; ++k)
        {
            GLBuffer secondaryBuffer = makeBuffer(4096);
            readDraw(secondaryBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, mScratchBuffer);
            glBindBuffer(GL_COPY_READ_BUFFER, secondaryBuffer);
            glBufferSubData(GL_COPY_READ_BUFFER, 200, sizeof(zeroFloats), zeroFloats);
            secondaryBuffers.push_back(std::move(secondaryBuffer));
        }
        recordSubmissionBoundary();
        for (int iter = 0; iter < kNumPreloadIterations; ++iter)
        {
            for (GLBuffer &secondaryBuffer : secondaryBuffers)
            {
                readDraw(secondaryBuffer);
            }
            glBindBuffer(GL_ARRAY_BUFFER, mScratchBuffer);
            recordSubmissionBoundary();
        }

        // Allocate donor buffers and reference the target indices in draw calls.
        std::vector<GLBuffer> donorBuffers;
        donorBuffers.reserve(kNumDonorBuffers);
        for (int i = 0; i < kNumDonorBuffers; ++i)
        {
            donorBuffers.push_back(makeBuffer(256));
        }
        for (int donorIndex : targetDonorIndices)
        {
            readDraw(donorBuffers[donorIndex]);
        }
        glBindBuffer(GL_ARRAY_BUFFER, mScratchBuffer);

        GLsync settleFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        pollFence(settleFence, 8000);
        glDeleteSync(settleFence);
        ASSERT_GL_NO_ERROR();

        // Queue copy-on-write operations on the large buffers and the target donor buffers
        // (which may store raw pointers in the driver), then free the donor buffers while
        // the copy-on-write work is still pending.
        for (GLBuffer &largeBuffer : largeBuffers)
        {
            glBindBuffer(GL_COPY_READ_BUFFER, largeBuffer);
            glBufferSubData(GL_COPY_READ_BUFFER, 32, sizeof(zeroFloats), zeroFloats);
        }
        for (int donorIndex : targetDonorIndices)
        {
            glBindBuffer(GL_COPY_READ_BUFFER, donorBuffers[donorIndex]);
            glBufferSubData(GL_COPY_READ_BUFFER, 32, sizeof(zeroFloats), zeroFloats);
        }
        for (int i = kNumDonorBuffers - 1; i >= 0; --i)
        {
            donorBuffers[i].reset();
        }

        // Issue additional read draws on the secondary buffers to allocate new tracking entries.
        for (GLBuffer &secondaryBuffer : secondaryBuffers)
        {
            readDraw(secondaryBuffer);
        }
        glBindBuffer(GL_ARRAY_BUFFER, mScratchBuffer);
        recordSubmissionBoundary();
        for (int batch = 1; batch <= 15; ++batch)
        {
            for (GLBuffer &secondaryBuffer : secondaryBuffers)
            {
                readDraw(secondaryBuffer);
            }
            glBindBuffer(GL_ARRAY_BUFFER, mScratchBuffer);
            recordSubmissionBoundary();
        }

        // Wait for the pending copy-on-write queue to drain.
        glBindBuffer(GL_COPY_READ_BUFFER, largeBuffers[kNumLargeBuffers - 1]);
        glBufferSubData(GL_COPY_READ_BUFFER, 256, sizeof(zeroFloats), zeroFloats);
        ASSERT_GL_NO_ERROR();

        // Respecify and delete the secondary buffers to traverse their tracking structures.
        for (int k = 0; k < kNumSecondaryBuffers; ++k)
        {
            glBindBuffer(GL_COPY_READ_BUFFER, secondaryBuffers[k]);
            glBufferData(GL_COPY_READ_BUFFER, 8192, nullptr, GL_DYNAMIC_DRAW);
            ASSERT_GL_NO_ERROR();
        }
        for (GLBuffer &secondaryBuffer : secondaryBuffers)
        {
            secondaryBuffer.reset();
        }
        ASSERT_GL_NO_ERROR();

        // Clean up resources allocated in this cycle before starting the next iteration.
        for (GLBuffer &largeBuffer : largeBuffers)
        {
            largeBuffer.reset();
        }
        for (GLsync fence : mFences)
        {
            glDeleteSync(fence);
        }
        mFences.clear();
        ASSERT_GL_NO_ERROR();
    }

    GLBuffer makeElementBuffer(size_t bytes)
    {
        std::vector<uint32_t> indices(bytes / sizeof(uint32_t));
        for (size_t i = 0; i < indices.size(); ++i)
        {
            indices[i] = (i % 4 == 1) ? ~0u : static_cast<uint32_t>(i & 63);
        }
        GLBuffer elementBuffer;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), indices.data(),
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return elementBuffer;
    }

    void drawElementsWithEBO(GLVertexArray &vao, GLBuffer &ebo, GLBuffer &scratchElementBuffer)
    {
        glBindVertexArray(vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, nullptr);
        // Rebind the persistent scratch element buffer and perform a 1-point indexed draw so
        // ANGLE's VertexArrayGL::updateElementArrayBufferBinding releases its reference to `ebo`
        // and unbinds `ebo` from the native VAO in the driver.
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scratchElementBuffer);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_SHORT, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void runElementBufferCopyAndDeleteSequence(int cycle,
                                               GLVertexArray &vao,
                                               GLBuffer &posBuffer,
                                               GLBuffer &scratchElementBuffer,
                                               GLBuffer &copySrcBuffer)
    {
        GLint prevProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
        glUseProgram(mProgram);
        glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        const float zeroFloats[4] = {};
        std::array<int, 3> targetDonorIndices;
        for (int i = 0; i < 3; ++i)
        {
            targetDonorIndices[i] = kBaseTargetDonorIndices[i] + kTargetIndexOffsets[cycle % 8];
        }

        static constexpr size_t kLargeElementBufferSizeMB = 8;
        std::vector<GLBuffer> largeBuffers;
        largeBuffers.reserve(kNumLargeBuffers);
        for (int i = 0; i < kNumLargeBuffers; ++i)
        {
            GLBuffer largeBuffer = makeBuffer(kLargeElementBufferSizeMB << 20);
            readDraw(largeBuffer);
            largeBuffers.push_back(std::move(largeBuffer));
        }
        if (!mPersistentControlBufferInitialized)
        {
            mPersistentControlBuffer            = makeBuffer(65536);
            mPersistentControlBufferInitialized = true;
            readDraw(mPersistentControlBuffer);
        }
        readDraw(mScratchBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, posBuffer);

        // Restore attribute 0 on vao to posBuffer after readDraw modified the default VAO.
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindVertexArray(0);

        // Allocate secondary GL_ELEMENT_ARRAY_BUFFER objects, reference them via glDrawElements,
        // and update them via glCopyBufferSubData.
        std::vector<GLBuffer> secondaryEbos;
        secondaryEbos.reserve(kNumSecondaryBuffers);
        for (int k = 0; k < kNumSecondaryBuffers; ++k)
        {
            GLBuffer secondaryEbo = makeElementBuffer(4096);
            drawElementsWithEBO(vao, secondaryEbo, scratchElementBuffer);
            glBindBuffer(GL_COPY_READ_BUFFER, copySrcBuffer);
            glBindBuffer(GL_COPY_WRITE_BUFFER, secondaryEbo);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 200, 16);
            glBindBuffer(GL_COPY_READ_BUFFER, 0);
            glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
            secondaryEbos.push_back(std::move(secondaryEbo));
        }
        recordSubmissionBoundary();
        for (int iter = 0; iter < kNumPreloadIterations; ++iter)
        {
            for (GLBuffer &secondaryEbo : secondaryEbos)
            {
                drawElementsWithEBO(vao, secondaryEbo, scratchElementBuffer);
            }
            recordSubmissionBoundary();
        }

        // Allocate donor GL_ELEMENT_ARRAY_BUFFER objects and reference the target indices in
        // glDrawElements calls.
        std::vector<GLBuffer> donorEbos;
        donorEbos.reserve(kNumDonorBuffers);
        for (int i = 0; i < kNumDonorBuffers; ++i)
        {
            donorEbos.push_back(makeElementBuffer(256));
        }
        for (int donorIndex : targetDonorIndices)
        {
            drawElementsWithEBO(vao, donorEbos[donorIndex], scratchElementBuffer);
        }

        GLsync settleFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        pollFence(settleFence, 8000);
        glDeleteSync(settleFence);
        ASSERT_GL_NO_ERROR();

        // Queue copy-on-write operations on the large buffers and update the target
        // GL_ELEMENT_ARRAY_BUFFER objects via glCopyBufferSubData, then delete the element
        // buffers while the copy-on-write work is still pending.
        for (GLBuffer &largeBuffer : largeBuffers)
        {
            glBindBuffer(GL_COPY_READ_BUFFER, largeBuffer);
            glBufferSubData(GL_COPY_READ_BUFFER, 32, sizeof(zeroFloats), zeroFloats);
        }
        glBindBuffer(GL_COPY_READ_BUFFER, copySrcBuffer);
        for (int donorIndex : targetDonorIndices)
        {
            glBindBuffer(GL_COPY_WRITE_BUFFER, donorEbos[donorIndex]);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 32, 16);
        }
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
        for (int i = kNumDonorBuffers - 1; i >= 0; --i)
        {
            donorEbos[i].reset();
        }

        for (GLBuffer &secondaryEbo : secondaryEbos)
        {
            drawElementsWithEBO(vao, secondaryEbo, scratchElementBuffer);
        }
        recordSubmissionBoundary();
        for (int batch = 1; batch <= 15; ++batch)
        {
            for (GLBuffer &secondaryEbo : secondaryEbos)
            {
                drawElementsWithEBO(vao, secondaryEbo, scratchElementBuffer);
            }
            recordSubmissionBoundary();
        }

        glBindBuffer(GL_COPY_READ_BUFFER, largeBuffers[kNumLargeBuffers - 1]);
        glBufferSubData(GL_COPY_READ_BUFFER, 256, sizeof(zeroFloats), zeroFloats);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        ASSERT_GL_NO_ERROR();

        for (int k = 0; k < kNumSecondaryBuffers; ++k)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, secondaryEbos[k]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 8192, nullptr, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            ASSERT_GL_NO_ERROR();
        }
        for (GLBuffer &secondaryEbo : secondaryEbos)
        {
            secondaryEbo.reset();
        }
        ASSERT_GL_NO_ERROR();

        glFinish();
        for (GLBuffer &largeBuffer : largeBuffers)
        {
            largeBuffer.reset();
        }
        for (GLsync fence : mFences)
        {
            glDeleteSync(fence);
        }
        mFences.clear();
        glFinish();
        glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        glUseProgram(prevProgram);
        ASSERT_GL_NO_ERROR();
    }

    void runDrawElementsAndDeleteSequences(GLVertexArray &vao,
                                           GLBuffer &posBuffer,
                                           GLBuffer &scratchElementBuffer,
                                           GLTransformFeedback &tfObject,
                                           GLBuffer &tfBuffer)
    {
        // Sequence 1: temporary GL_ARRAY_BUFFER with vertexAttribDivisor,
        // glDrawElementsInstanced, rebind position buffer, 4x glDrawElements, delete both buffers.
        {
            static constexpr uint32_t kIndices[24] = {
                22, 60, 6,   50, 45, ~0u, 5,  25, 56, 52, 22, 25,
                6,  18, ~0u, 26, 29, 35,  10, 40, 62, 23, 21, 62,
            };
            std::vector<float> tempAttribs(256, 0.25f);
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);

            GLBuffer instanceAttribBuffer;
            glBindBuffer(GL_ARRAY_BUFFER, instanceAttribBuffer);
            glBufferData(GL_ARRAY_BUFFER, tempAttribs.size() * sizeof(float), tempAttribs.data(),
                         GL_STATIC_DRAW);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            glVertexAttribDivisor(0, 1);
            glDrawElementsInstanced(GL_POINTS, 24, GL_UNSIGNED_INT, nullptr, 3);

            glVertexAttribDivisor(0, 0);
            glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            for (int drawIter = 0; drawIter < 4; ++drawIter)
            {
                glDrawElements(GL_POINTS, 24, GL_UNSIGNED_INT, nullptr);
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 2: unsigned byte index buffer with two glCopyBufferSubData updates
        // and glDrawElementsInstanced at non-zero offset.
        {
            static constexpr uint8_t kIndices[16] = {
                0, 7, 14, 21, 255, 35, 42, 49, 56, 255, 6, 13, 20, 27, 255, 41,
            };
            static constexpr uint8_t kCopySrc[1] = {0};
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);

            GLBuffer copySrcBuffer;
            glBindBuffer(GL_COPY_READ_BUFFER, copySrcBuffer);
            glBufferData(GL_COPY_READ_BUFFER, sizeof(kCopySrc), kCopySrc, GL_STATIC_DRAW);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ELEMENT_ARRAY_BUFFER, 0, 15, 1);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ELEMENT_ARRAY_BUFFER, 0, 4, 1);
            glBindBuffer(GL_COPY_READ_BUFFER, 0);
            glDrawElementsInstanced(GL_TRIANGLE_STRIP, 13, GL_UNSIGNED_BYTE,
                                    reinterpret_cast<const void *>(3), 1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 3: respecifying GL_ELEMENT_ARRAY_BUFFER from uint8_t to uint32_t
        // via glBufferData followed by repeated glDrawElementsInstanced.
        {
            static constexpr uint8_t kIndices8[16] = {
                0, 1, 2, 3, 4, 5, 6, 7, 255, 9, 10, 11, 12, 13, 14, 15,
            };
            static constexpr uint32_t kIndices32[16] = {
                0, ~0u, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices8), kIndices8, GL_DYNAMIC_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices32), kIndices32, GL_DYNAMIC_DRAW);
            glDrawElementsInstanced(GL_TRIANGLE_STRIP, 16, GL_UNSIGNED_INT, nullptr, 1);
            glDrawElementsInstanced(GL_TRIANGLE_STRIP, 16, GL_UNSIGNED_INT, nullptr, 1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 4: partial update via glBufferSubData on GL_ELEMENT_ARRAY_BUFFER
        // followed by glDrawElementsInstanced and 3x glDrawElements.
        {
            static constexpr uint32_t kIndices[63] = {
                0,  1,  2,   3,   4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
                16, 17, 18,  19,  20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                32, 33, 34,  35,  36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                48, 49, ~0u, ~0u, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
            };
            static constexpr uint32_t kSubData[32] = {
                48, 7,  ~0u, 11,  50, ~0u, 29,  ~0u, 35,  ~0u, ~0u, ~0u, 18,  ~0u, 38,  ~0u,
                30, 30, ~0u, ~0u, 60, 49,  ~0u, ~0u, ~0u, ~0u, 62,  ~0u, ~0u, ~0u, ~0u, 62,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 60, sizeof(kSubData), kSubData);
            glDrawElementsInstanced(GL_LINES, 15, GL_UNSIGNED_INT,
                                    reinterpret_cast<const void *>(4), 2);
            for (int drawIter = 0; drawIter < 3; ++drawIter)
            {
                glDrawElements(GL_TRIANGLES, 17, GL_UNSIGNED_INT,
                               reinterpret_cast<const void *>(4));
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 5: Transform feedback active and paused with glDrawElementsInstanced
        // and glDrawElements, followed by index buffer deletion.
        {
            static constexpr uint16_t kIndices[16] = {
                12, 65535, 65535, 65535, 8, 39, 23, 18, 47, 58, 0, 62, 1, 2, 3, 65535,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);

            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfObject);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tfBuffer);
            glBeginTransformFeedback(GL_POINTS);
            glPauseTransformFeedback();
            glDrawElementsInstanced(GL_POINTS, 10, GL_UNSIGNED_SHORT,
                                    reinterpret_cast<const void *>(4), 7);
            glDrawElementsInstanced(GL_POINTS, 10, GL_UNSIGNED_SHORT,
                                    reinterpret_cast<const void *>(4), 7);
            glDrawElements(GL_TRIANGLE_FAN, 16, GL_UNSIGNED_SHORT, nullptr);
            glResumeTransformFeedback();
            glEndTransformFeedback();
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 6: secondary vertex attribute buffer bound to attribute 1
        // with large divisor, glDrawElementsInstanced + 4x glDrawElements, then delete both.
        {
            std::vector<uint16_t> indices(100, 65535);
            indices[1] = 2;
            indices[2] = 3;
            std::vector<float> tempAttribs(128, 0.5f);
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t), indices.data(),
                         GL_DYNAMIC_DRAW);

            GLBuffer instanceAttribBuffer;
            glBindBuffer(GL_ARRAY_BUFFER, instanceAttribBuffer);
            glBufferData(GL_ARRAY_BUFFER, tempAttribs.size() * sizeof(float), tempAttribs.data(),
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            glVertexAttribDivisor(1, 65535);
            glDrawElementsInstanced(GL_LINES, 98, GL_UNSIGNED_SHORT,
                                    reinterpret_cast<const void *>(2), 2);
            glVertexAttribDivisor(1, 0);
            glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            for (int drawIter = 0; drawIter < 4; ++drawIter)
            {
                glDrawElements(GL_LINES, 98, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(2));
            }
            glDisableVertexAttribArray(1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 7: delete GL_ELEMENT_ARRAY_BUFFER while bound to VAO, immediately
        // create and bind a new one, draw with glDrawElements and glDrawElementsInstanced, delete.
        {
            static constexpr uint16_t kIndices[63] = {
                1, 65535, 65535, 1,     0,     7, 7, 65535, 65535, 0, 7, 1, 65535, 8, 7, 65535,
                1, 0,     8,     65535, 7,     8, 1, 65535, 7,     7, 7, 1, 8,     8, 1, 0,
                1, 65535, 0,     1,     65535, 0, 8, 1,     65535, 8, 8, 1, 7,     7, 0, 7,
                7, 65535, 8,     0,     0,     0, 1, 1,     8,     7, 8, 1, 0,     1, 0,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);
            indexBuffer.reset();

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);
            glDrawElements(GL_TRIANGLE_FAN, 50, GL_UNSIGNED_SHORT,
                           reinterpret_cast<const void *>(6));
            glDrawElementsInstanced(GL_LINE_LOOP, 50, GL_UNSIGNED_SHORT,
                                    reinterpret_cast<const void *>(6), 2);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 8: resize GL_ELEMENT_ARRAY_BUFFER larger and smaller with glBufferData
        // between glDrawElements and glDrawElementsInstanced calls, then delete.
        {
            static constexpr uint8_t kIndices[16] = {
                0, 7, 14, 21, 255, 35, 42, 49, 56, 255, 6, 13, 20, 27, 255, 41,
            };
            std::vector<uint8_t> indices64(64, 1);
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);
            glDrawElements(GL_LINE_STRIP, 13, GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(3));
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices64.size(), indices64.data(),
                         GL_DYNAMIC_DRAW);
            glDrawElements(GL_LINE_STRIP, 64, GL_UNSIGNED_BYTE, nullptr);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 1, kIndices);
            glDrawElementsInstanced(GL_LINE_STRIP, 13, GL_UNSIGNED_BYTE,
                                    reinterpret_cast<const void *>(3), 1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 9: unsigned byte index buffer with glDrawElements before and after
        // glCopyBufferSubData, plus glDrawElementsInstanced, followed by buffer deletion.
        {
            static constexpr uint8_t kIndices[64] = {
                62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255,
                62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255,
                62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255,
                62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255, 62, 255,
            };
            static constexpr uint8_t kCopySrc[10] = {
                255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);

            GLBuffer copySrcBuffer;
            glBindBuffer(GL_COPY_READ_BUFFER, copySrcBuffer);
            glBufferData(GL_COPY_READ_BUFFER, sizeof(kCopySrc), kCopySrc, GL_STATIC_DRAW);

            glDrawElements(GL_TRIANGLE_STRIP, 35, GL_UNSIGNED_BYTE, nullptr);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ELEMENT_ARRAY_BUFFER, 0, 50, 10);
            glBindBuffer(GL_COPY_READ_BUFFER, 0);
            glDrawElements(GL_TRIANGLE_STRIP, 35, GL_UNSIGNED_BYTE, nullptr);
            glDrawElementsInstanced(GL_TRIANGLES, 64, GL_UNSIGNED_BYTE, nullptr, 1);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 10: unsigned short index buffer with overlapping glCopyBufferSubData
        // regions followed by glDrawElements and glDrawElementsInstanced and buffer deletion.
        {
            static constexpr uint16_t kIndices[33] = {
                48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
                48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
            };
            static constexpr uint16_t kCopySrc[11] = {
                1, 1, 65535, 0, 1, 8, 0, 7, 65535, 8, 1,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);

            GLBuffer copySrcBuffer;
            glBindBuffer(GL_COPY_READ_BUFFER, copySrcBuffer);
            glBufferData(GL_COPY_READ_BUFFER, sizeof(kCopySrc), kCopySrc, GL_STATIC_DRAW);

            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ELEMENT_ARRAY_BUFFER, 0, 20, 22);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ELEMENT_ARRAY_BUFFER, 0, 28, 22);
            glBindBuffer(GL_COPY_READ_BUFFER, 0);
            glDrawElements(GL_TRIANGLE_FAN, 33, GL_UNSIGNED_SHORT, nullptr);
            glDrawElementsInstanced(GL_TRIANGLE_FAN, 33, GL_UNSIGNED_SHORT, nullptr, 2);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 11: GL_LINE_LOOP with GL_UNSIGNED_INT and two overlapping glCopyBufferSubData
        // writes injecting 0xFFFFFFFF primitive-restart markers, followed by glDrawElements and
        // delete.
        {
            static constexpr uint32_t kIndices[12] = {
                0, 7, 8, 8, 1, 7, 1, 1, 1, 8, 7, 63,
            };
            static constexpr uint32_t kCopySrc[4] = {
                ~0u,
                ~0u,
                ~0u,
                3,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);

            GLBuffer copySrcBuffer;
            glBindBuffer(GL_COPY_READ_BUFFER, copySrcBuffer);
            glBufferData(GL_COPY_READ_BUFFER, sizeof(kCopySrc), kCopySrc, GL_STATIC_DRAW);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ELEMENT_ARRAY_BUFFER, 0, 32, 16);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ELEMENT_ARRAY_BUFFER, 0, 24, 16);
            glBindBuffer(GL_COPY_READ_BUFFER, 0);

            glDrawElements(GL_LINE_LOOP, 11, GL_UNSIGNED_INT, reinterpret_cast<const void *>(4));
            glDrawElements(GL_LINE_LOOP, 11, GL_UNSIGNED_INT, reinterpret_cast<const void *>(4));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 12: GL_LINE_LOOP with GL_UNSIGNED_INT containing multiple 0xFFFFFFFF
        // primitive-restart indices drawn via glDrawElements and glDrawElementsInstanced, then
        // immediately deleted.
        {
            static constexpr uint32_t kIndices[24] = {
                0,  1,  2,  3,  4,  ~0u, 6,  7,  8,  9,  10, ~0u,
                12, 13, 14, 15, 16, ~0u, 18, 19, 20, 21, 22, ~0u,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_DYNAMIC_DRAW);
            glDrawElements(GL_LINE_LOOP, 24, GL_UNSIGNED_INT, nullptr);
            glDrawElementsInstanced(GL_LINE_LOOP, 24, GL_UNSIGNED_INT, nullptr, 2);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 13: GL_LINE_LOOP respecifying GL_ELEMENT_ARRAY_BUFFER from uint32_t (with
        // 0xFFFFFFFF) to uint16_t (with 0xFFFF) via glBufferData, then glDrawElementsInstanced and
        // delete.
        {
            static constexpr uint32_t kIndices32[24] = {
                0,  1,  2,  3,  4,   5,  6,  7,  8,  9,  10, 11,
                12, 13, 14, 15, ~0u, 17, 18, 19, 20, 21, 22, 23,
            };
            static constexpr uint16_t kIndices16[24] = {
                0,  1,  2,  3,  4,  5,  6,  7,  65535, 9,  10, 11,
                12, 13, 14, 15, 16, 17, 18, 19, 20,    21, 22, 23,
            };
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices32), kIndices32, GL_DYNAMIC_DRAW);
            glDrawElements(GL_LINE_LOOP, 24, GL_UNSIGNED_INT, nullptr);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices16), kIndices16, GL_DYNAMIC_DRAW);
            glDrawElementsInstanced(GL_LINE_LOOP, 13, GL_UNSIGNED_SHORT,
                                    reinterpret_cast<const void *>(2), 1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Sequence 14: Delete GL_ELEMENT_ARRAY_BUFFER while bound to VAO, recreate with 255
        // uint16_t indices containing 64 primitive-restart markers (65535), draw 5x GL_LINE_LOOP,
        // then delete.
        {
            std::array<uint16_t, 255> indices;
            for (int k = 0; k < 255; ++k)
            {
                indices[k] = (k % 4 == 1) ? 65535 : static_cast<uint16_t>(k & 63);
            }
            GLBuffer indexBuffer;
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_DYNAMIC_DRAW);
            indexBuffer.reset();

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_DYNAMIC_DRAW);
            for (int drawIter = 0; drawIter < 5; ++drawIter)
            {
                glDrawElements(GL_LINE_LOOP, 110, GL_UNSIGNED_SHORT,
                               reinterpret_cast<const void *>(6));
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scratchElementBuffer);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_SHORT, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    GLProgram mProgram;
    GLBuffer mScratchBuffer;
    GLBuffer mPrimeBuffer;
    GLBuffer mPersistentControlBuffer;
    bool mPersistentControlBufferInitialized = false;
    std::vector<uint32_t> mPatternBuffer;
    std::vector<GLsync> mFences;
};

// Verify that deleting buffers while referenced by pending GPU work and copy-on-write
// operations does not crash or lose the context.
TEST_P(DeleteBuffersTest, QueuedDeletionDoesNotCrash)
{
    // Too slow on SwiftShader
    ANGLE_SKIP_TEST_IF(isSwiftshader());

    for (int cycle = 0; cycle < kMaxCycles; ++cycle)
    {
        ASSERT_NO_FATAL_FAILURE(runQueuedDeletionCycle(cycle));
    }
    EXPECT_GL_NO_ERROR();
}

// Verify that repeatedly drawing with glDrawElements and glDrawElementsInstanced, updating
// buffers via glCopyBufferSubData/glBufferSubData/glBufferData, and deleting the buffers
// does not crash or lose the context.
TEST_P(DeleteBuffersTest, DeleteAfterDrawElementsDoesNotCrash)
{
    // Too slow on SwiftShader
    ANGLE_SKIP_TEST_IF(isSwiftshader());

    static constexpr char kVS[] = R"(#version 300 es
layout(location = 0) in vec2 p;
out float v_tf;
void main()
{
    gl_Position = vec4(p, 0.0, 1.0);
    gl_PointSize = 1.0;
    v_tf = p.x + p.y;
})";

    static constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 c;
void main()
{
    c = vec4(1.0);
})";

    std::vector<std::string> tfVaryings = {"v_tf"};
    ANGLE_GL_PROGRAM_TRANSFORM_FEEDBACK(program, kVS, kFS, tfVaryings, GL_INTERLEAVED_ATTRIBS);
    glUseProgram(program);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    std::vector<float> positions(64 * 2);
    for (int i = 0; i < 64; ++i)
    {
        positions[i * 2]     = (static_cast<float>(i % 8) / 3.5f) - 1.0f;
        positions[i * 2 + 1] = (static_cast<float>(i / 8) / 3.5f) - 1.0f;
    }

    GLBuffer posBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(),
                 GL_STATIC_DRAW);

    static constexpr uint16_t kScratchIndices[4] = {0, 1, 2, 0};
    GLBuffer scratchElementBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scratchElementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kScratchIndices), kScratchIndices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    static constexpr uint32_t kCopySrcIndices[4] = {~0u, ~0u, ~0u, 3};
    GLBuffer copySrcBuffer;
    glBindBuffer(GL_COPY_READ_BUFFER, copySrcBuffer);
    glBufferData(GL_COPY_READ_BUFFER, sizeof(kCopySrcIndices), kCopySrcIndices, GL_STATIC_DRAW);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);

    GLVertexArray vao;
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    GLBuffer tfBuffer;
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, tfBuffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 4096, nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    GLTransformFeedback tfObject;
    EXPECT_GL_NO_ERROR();

    for (int i = 0; i < 1000; ++i)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        runDrawElementsAndDeleteSequences(vao, posBuffer, scratchElementBuffer, tfObject, tfBuffer);
        if (i < 4)
        {
            ASSERT_NO_FATAL_FAILURE(runElementBufferCopyAndDeleteSequence(
                i, vao, posBuffer, scratchElementBuffer, copySrcBuffer));
        }
        if (i % 5 == 4)
        {
            glFinish();
        }
    }
    glFinish();
    EXPECT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST_ES3_AND(DeleteBuffersTest,
                               ES3_OPENGL().enable(Feature::DeferGlDeleteBuffers),
                               ES3_OPENGLES().enable(Feature::DeferGlDeleteBuffers));

}  // anonymous namespace
