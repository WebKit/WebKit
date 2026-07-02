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
 * \brief Buffer map tests.
 *//*--------------------------------------------------------------------*/

#include "es3fBufferMapTests.hpp"
#include "glsBufferTestUtil.hpp"
#include "tcuTestLog.hpp"
#include "deMemory.h"
#include "deString.h"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <algorithm>

using std::set;
using std::string;
using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Functional
{

using namespace gls::BufferTestUtil;

// Test cases.

class BufferMapReadCase : public BufferCase
{
public:
    BufferMapReadCase(Context &context, const char *name, const char *desc, uint32_t bufferTarget, uint32_t usage,
                      int bufferSize, int mapOffset, int mapSize, WriteType write)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_bufferTarget(bufferTarget)
        , m_usage(usage)
        , m_bufferSize(bufferSize)
        , m_mapOffset(mapOffset)
        , m_mapSize(mapSize)
        , m_write(write)
    {
    }

    IterateResult iterate(void)
    {
        TestLog &log      = m_testCtx.getLog();
        uint32_t dataSeed = deStringHash(getName());
        ReferenceBuffer refBuf;
        BufferWriter writer(m_renderCtx, m_testCtx.getLog(), m_write);
        bool isOk = false;

        // Setup reference data.
        refBuf.setSize(m_bufferSize);
        fillWithRandomBytes(refBuf.getPtr(), m_bufferSize, dataSeed);

        uint32_t buf = genBuffer();
        glBindBuffer(m_bufferTarget, buf);
        glBufferData(m_bufferTarget, m_bufferSize, nullptr, m_usage);
        writer.write(buf, 0, m_bufferSize, refBuf.getPtr(), m_bufferTarget);

        glBindBuffer(m_bufferTarget, buf);
        void *ptr = glMapBufferRange(m_bufferTarget, m_mapOffset, m_mapSize, GL_MAP_READ_BIT);
        GLU_CHECK_MSG("glMapBufferRange");
        TCU_CHECK(ptr);

        isOk = compareByteArrays(log, (const uint8_t *)ptr, refBuf.getPtr(m_mapOffset), m_mapSize);

        glUnmapBuffer(m_bufferTarget);
        GLU_CHECK_MSG("glUnmapBuffer");

        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_bufferTarget;
    uint32_t m_usage;
    int m_bufferSize;
    int m_mapOffset;
    int m_mapSize;
    WriteType m_write;
};

class BufferMapWriteCase : public BufferCase
{
public:
    BufferMapWriteCase(Context &context, const char *name, const char *desc, uint32_t bufferTarget, uint32_t usage,
                       int size, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_bufferTarget(bufferTarget)
        , m_usage(usage)
        , m_size(size)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        uint32_t dataSeed = deStringHash(getName());
        ReferenceBuffer refBuf;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);

        // Setup reference data.
        refBuf.setSize(m_size);
        fillWithRandomBytes(refBuf.getPtr(), m_size, dataSeed);

        uint32_t buf = genBuffer();
        glBindBuffer(m_bufferTarget, buf);
        glBufferData(m_bufferTarget, m_size, nullptr, m_usage);

        void *ptr = glMapBufferRange(m_bufferTarget, 0, m_size, GL_MAP_WRITE_BIT);
        GLU_CHECK_MSG("glMapBufferRange");
        TCU_CHECK(ptr);

        fillWithRandomBytes((uint8_t *)ptr, m_size, dataSeed);
        glUnmapBuffer(m_bufferTarget);
        GLU_CHECK_MSG("glUnmapBuffer");

        bool isOk = verifier.verify(buf, refBuf.getPtr(), 0, m_size, m_bufferTarget);
        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_bufferTarget;
    uint32_t m_usage;
    int m_size;
    VerifyType m_verify;
};

class BufferPartialMapWriteCase : public BufferCase
{
public:
    BufferPartialMapWriteCase(Context &context, const char *name, const char *desc, uint32_t bufferTarget,
                              uint32_t usage, int bufferSize, int mapOffset, int mapSize, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_bufferTarget(bufferTarget)
        , m_usage(usage)
        , m_bufferSize(bufferSize)
        , m_mapOffset(mapOffset)
        , m_mapSize(mapSize)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        uint32_t dataSeed = deStringHash(getName());
        ReferenceBuffer refBuf;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);

        // Setup reference data.
        refBuf.setSize(m_bufferSize);
        fillWithRandomBytes(refBuf.getPtr(), m_bufferSize, dataSeed);

        uint32_t buf = genBuffer();
        glBindBuffer(m_bufferTarget, buf);
        glBufferData(m_bufferTarget, m_bufferSize, refBuf.getPtr(), m_usage);

        // Do reference map.
        fillWithRandomBytes(refBuf.getPtr(m_mapOffset), m_mapSize, dataSeed & 0xabcdef);

        void *ptr = glMapBufferRange(m_bufferTarget, m_mapOffset, m_mapSize, GL_MAP_WRITE_BIT);
        GLU_CHECK_MSG("glMapBufferRange");
        TCU_CHECK(ptr);

        deMemcpy(ptr, refBuf.getPtr(m_mapOffset), m_mapSize);
        glUnmapBuffer(m_bufferTarget);
        GLU_CHECK_MSG("glUnmapBuffer");

        bool isOk = verifier.verify(buf, refBuf.getPtr(), 0, m_bufferSize, m_bufferTarget);
        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_bufferTarget;
    uint32_t m_usage;
    int m_bufferSize;
    int m_mapOffset;
    int m_mapSize;
    VerifyType m_verify;
};

class BufferMapInvalidateCase : public BufferCase
{
public:
    BufferMapInvalidateCase(Context &context, const char *name, const char *desc, uint32_t bufferTarget, uint32_t usage,
                            bool partialWrite, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_bufferTarget(bufferTarget)
        , m_usage(usage)
        , m_partialWrite(partialWrite)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        uint32_t dataSeed = deStringHash(getName());
        uint32_t buf      = 0;
        ReferenceBuffer refBuf;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);
        const int bufferSize   = 1300;
        const int mapOffset    = 200;
        const int mapSize      = 1011;
        const int mapWriteOffs = m_partialWrite ? 91 : 0;
        const int verifyOffset = mapOffset + mapWriteOffs;
        const int verifySize   = mapSize - mapWriteOffs;

        // Setup reference data.
        refBuf.setSize(bufferSize);
        fillWithRandomBytes(refBuf.getPtr(), bufferSize, dataSeed);

        buf = genBuffer();
        glBindBuffer(m_bufferTarget, buf);
        glBufferData(m_bufferTarget, bufferSize, refBuf.getPtr(), m_usage);

        // Do reference map.
        fillWithRandomBytes(refBuf.getPtr(mapOffset + mapWriteOffs), mapSize - mapWriteOffs, dataSeed & 0xabcdef);

        void *ptr =
            glMapBufferRange(m_bufferTarget, mapOffset, mapSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        GLU_CHECK_MSG("glMapBufferRange");
        TCU_CHECK(ptr);

        deMemcpy((uint8_t *)ptr + mapWriteOffs, refBuf.getPtr(mapOffset + mapWriteOffs), mapSize - mapWriteOffs);
        glUnmapBuffer(m_bufferTarget);
        GLU_CHECK_MSG("glUnmapBuffer");

        bool isOk = verifier.verify(buf, refBuf.getPtr(), verifyOffset, verifySize, m_bufferTarget);
        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_bufferTarget;
    uint32_t m_usage;
    bool m_partialWrite;
    VerifyType m_verify;
};

class BufferMapPartialInvalidateCase : public BufferCase
{
public:
    BufferMapPartialInvalidateCase(Context &context, const char *name, const char *desc, uint32_t bufferTarget,
                                   uint32_t usage, bool partialWrite, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_bufferTarget(bufferTarget)
        , m_usage(usage)
        , m_partialWrite(partialWrite)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        uint32_t dataSeed = deStringHash(getName());
        uint32_t buf      = 0;
        ReferenceBuffer refBuf;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);
        const int bufferSize   = 1300;
        const int mapOffset    = 200;
        const int mapSize      = 1011;
        const int mapWriteOffs = m_partialWrite ? 91 : 0;
        const int verifyOffset = m_partialWrite ? mapOffset + mapWriteOffs : 0;
        const int verifySize   = bufferSize - verifyOffset;

        // Setup reference data.
        refBuf.setSize(bufferSize);
        fillWithRandomBytes(refBuf.getPtr(), bufferSize, dataSeed);

        buf = genBuffer();
        glBindBuffer(m_bufferTarget, buf);
        glBufferData(m_bufferTarget, bufferSize, refBuf.getPtr(), m_usage);

        // Do reference map.
        fillWithRandomBytes(refBuf.getPtr(mapOffset + mapWriteOffs), mapSize - mapWriteOffs, dataSeed & 0xabcdef);

        void *ptr =
            glMapBufferRange(m_bufferTarget, mapOffset, mapSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
        GLU_CHECK_MSG("glMapBufferRange");
        TCU_CHECK(ptr);

        deMemcpy((uint8_t *)ptr + mapWriteOffs, refBuf.getPtr(mapOffset + mapWriteOffs), mapSize - mapWriteOffs);
        glUnmapBuffer(m_bufferTarget);
        GLU_CHECK_MSG("glUnmapBuffer");

        bool isOk = verifier.verify(buf, refBuf.getPtr(), verifyOffset, verifySize, m_bufferTarget);
        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_bufferTarget;
    uint32_t m_usage;
    bool m_partialWrite;
    VerifyType m_verify;
};

class BufferMapExplicitFlushCase : public BufferCase
{
public:
    BufferMapExplicitFlushCase(Context &context, const char *name, const char *desc, uint32_t bufferTarget,
                               uint32_t usage, bool partialWrite, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_bufferTarget(bufferTarget)
        , m_usage(usage)
        , m_partialWrite(partialWrite)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        uint32_t dataSeed = deStringHash(getName());
        uint32_t buf      = 0;
        ReferenceBuffer refBuf;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);
        const int bufferSize = 1300;
        const int mapOffset  = 200;
        const int mapSize    = 1011;
        const int sliceAOffs = m_partialWrite ? 1 : 0;
        const int sliceASize = m_partialWrite ? 96 : 473;
        const int sliceBOffs = m_partialWrite ? 503 : sliceAOffs + sliceASize;
        const int sliceBSize = mapSize - sliceBOffs;
        bool isOk            = true;

        // Setup reference data.
        refBuf.setSize(bufferSize);
        fillWithRandomBytes(refBuf.getPtr(), bufferSize, dataSeed);

        buf = genBuffer();
        glBindBuffer(m_bufferTarget, buf);
        glBufferData(m_bufferTarget, bufferSize, refBuf.getPtr(), m_usage);

        // Do reference map.
        fillWithRandomBytes(refBuf.getPtr(mapOffset), mapSize, dataSeed & 0xabcdef);

        void *ptr = glMapBufferRange(m_bufferTarget, mapOffset, mapSize, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
        GLU_CHECK_MSG("glMapBufferRange");
        TCU_CHECK(ptr);

        deMemcpy(ptr, refBuf.getPtr(mapOffset), mapSize);

        glFlushMappedBufferRange(m_bufferTarget, sliceAOffs, sliceASize);
        GLU_CHECK_MSG("glFlushMappedBufferRange");
        glFlushMappedBufferRange(m_bufferTarget, sliceBOffs, sliceBSize);
        GLU_CHECK_MSG("glFlushMappedBufferRange");

        glUnmapBuffer(m_bufferTarget);
        GLU_CHECK_MSG("glUnmapBuffer");

        if (m_partialWrite)
        {
            if (!verifier.verify(buf, refBuf.getPtr(), mapOffset + sliceAOffs, sliceASize, m_bufferTarget))
                isOk = false;

            if (!verifier.verify(buf, refBuf.getPtr(), mapOffset + sliceBOffs, sliceBSize, m_bufferTarget))
                isOk = false;
        }
        else
        {
            if (!verifier.verify(buf, refBuf.getPtr(), mapOffset, mapSize, m_bufferTarget))
                isOk = false;
        }

        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_bufferTarget;
    uint32_t m_usage;
    bool m_partialWrite;
    VerifyType m_verify;
};

class BufferMapUnsyncWriteCase : public BufferCase
{
public:
    BufferMapUnsyncWriteCase(Context &context, const char *name, const char *desc, uint32_t bufferTarget,
                             uint32_t usage)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_bufferTarget(bufferTarget)
        , m_usage(usage)
    {
    }

    IterateResult iterate(void)
    {
        VertexArrayVerifier verifier(m_renderCtx, m_testCtx.getLog());
        uint32_t dataSeed = deStringHash(getName());
        ReferenceBuffer refBuf;
        uint32_t buf   = 0;
        bool isOk      = true;
        const int size = 1200;

        // Setup reference data.
        refBuf.setSize(size);
        fillWithRandomBytes(refBuf.getPtr(), size, dataSeed);

        buf = genBuffer();
        glBindBuffer(m_bufferTarget, buf);
        glBufferData(m_bufferTarget, size, refBuf.getPtr(), m_usage);

        // Use for rendering.
        if (!verifier.verify(buf, refBuf.getPtr(), 0, size))
            isOk = false;
        // \note ReadPixels() implies Finish

        glBindBuffer(m_bufferTarget, buf);
        void *ptr = glMapBufferRange(m_bufferTarget, 0, size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
        GLU_CHECK_MSG("glMapBufferRange");
        TCU_CHECK(ptr);

        fillWithRandomBytes(refBuf.getPtr(), size, dataSeed & 0xabcdef);
        deMemcpy(ptr, refBuf.getPtr(), size);

        glUnmapBuffer(m_bufferTarget);
        GLU_CHECK_MSG("glUnmapBuffer");

        // Synchronize.
        glFinish();

        if (!verifier.verify(buf, refBuf.getPtr(), 0, size))
            isOk = false;

        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_bufferTarget;
    uint32_t m_usage;
};

class BufferMapReadWriteCase : public BufferCase
{
public:
    BufferMapReadWriteCase(Context &context, const char *name, const char *desc, uint32_t bufferTarget, uint32_t usage,
                           int bufferSize, int mapOffset, int mapSize, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_bufferTarget(bufferTarget)
        , m_usage(usage)
        , m_bufferSize(bufferSize)
        , m_mapOffset(mapOffset)
        , m_mapSize(mapSize)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        TestLog &log      = m_testCtx.getLog();
        uint32_t dataSeed = deStringHash(getName());
        uint32_t buf      = 0;
        ReferenceBuffer refBuf;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);
        bool isOk = true;

        // Setup reference data.
        refBuf.setSize(m_bufferSize);
        fillWithRandomBytes(refBuf.getPtr(), m_bufferSize, dataSeed);

        buf = genBuffer();
        glBindBuffer(m_bufferTarget, buf);
        glBufferData(m_bufferTarget, m_bufferSize, refBuf.getPtr(), m_usage);

        // Verify before mapping.
        if (!verifier.verify(buf, refBuf.getPtr(), 0, m_bufferSize, m_bufferTarget))
            isOk = false;

        glBindBuffer(m_bufferTarget, buf);
        void *ptr = glMapBufferRange(m_bufferTarget, m_mapOffset, m_mapSize, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
        GLU_CHECK_MSG("glMapBufferRange");
        TCU_CHECK(ptr);

        // Compare mapped ptr.
        if (!compareByteArrays(log, (const uint8_t *)ptr, refBuf.getPtr(m_mapOffset), m_mapSize))
            isOk = false;

        fillWithRandomBytes(refBuf.getPtr(m_mapOffset), m_mapSize, dataSeed & 0xabcdef);
        deMemcpy(ptr, refBuf.getPtr(m_mapOffset), m_mapSize);

        glUnmapBuffer(m_bufferTarget);
        GLU_CHECK_MSG("glUnmapBuffer");

        // Compare final buffer.
        if (!verifier.verify(buf, refBuf.getPtr(), 0, m_bufferSize, m_bufferTarget))
            isOk = false;

        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_bufferTarget;
    uint32_t m_usage;
    int m_bufferSize;
    int m_mapOffset;
    int m_mapSize;
    VerifyType m_verify;
};

BufferMapTests::BufferMapTests(Context &context) : TestCaseGroup(context, "map", "Buffer map tests")
{
}

BufferMapTests::~BufferMapTests(void)
{
}

void BufferMapTests::init(void)
{
    static const uint32_t bufferTargets[] = {
        GL_ARRAY_BUFFER,      GL_COPY_READ_BUFFER,    GL_COPY_WRITE_BUFFER,         GL_ELEMENT_ARRAY_BUFFER,
        GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER};

    static const uint32_t usageHints[] = {GL_STREAM_DRAW,  GL_STREAM_READ,  GL_STREAM_COPY,
                                          GL_STATIC_DRAW,  GL_STATIC_READ,  GL_STATIC_COPY,
                                          GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, GL_DYNAMIC_COPY};

    static const struct
    {
        const char *name;
        WriteType write;
    } bufferDataSources[] = {{"sub_data", WRITE_BUFFER_SUB_DATA}, {"map_write", WRITE_BUFFER_WRITE_MAP}};

    static const struct
    {
        const char *name;
        VerifyType verify;
    } bufferUses[] = {{"map_read", VERIFY_BUFFER_READ_MAP},
                      {"render_as_vertex_array", VERIFY_AS_VERTEX_ARRAY},
                      {"render_as_index_array", VERIFY_AS_INDEX_ARRAY}};

    // .read
    {
        tcu::TestCaseGroup *mapReadGroup =
            new tcu::TestCaseGroup(m_testCtx, "read", "Buffer read using glMapBufferRange()");
        addChild(mapReadGroup);

        // .[data src]
        for (int srcNdx = 0; srcNdx < DE_LENGTH_OF_ARRAY(bufferDataSources); srcNdx++)
        {
            WriteType write                = bufferDataSources[srcNdx].write;
            tcu::TestCaseGroup *writeGroup = new tcu::TestCaseGroup(m_testCtx, bufferDataSources[srcNdx].name, "");
            mapReadGroup->addChild(writeGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                uint32_t target       = bufferTargets[targetNdx];
                const uint32_t hint   = GL_STATIC_READ;
                const int size        = 1019;
                const int partialOffs = 17;
                const int partialSize = 501;

                writeGroup->addChild(new BufferMapReadCase(m_context,
                                                           (string(getBufferTargetName(target)) + "_full").c_str(), "",
                                                           target, hint, size, 0, size, write));
                writeGroup->addChild(new BufferMapReadCase(m_context,
                                                           (string(getBufferTargetName(target)) + "_partial").c_str(),
                                                           "", target, hint, size, partialOffs, partialSize, write));
            }
        }

        // .usage_hints
        {
            tcu::TestCaseGroup *hintsGroup =
                new tcu::TestCaseGroup(m_testCtx, "usage_hints", "Different usage hints with glMapBufferRange()");
            mapReadGroup->addChild(hintsGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                for (int hintNdx = 0; hintNdx < DE_LENGTH_OF_ARRAY(usageHints); hintNdx++)
                {
                    uint32_t target = bufferTargets[targetNdx];
                    uint32_t hint   = usageHints[hintNdx];
                    const int size  = 1019;
                    string name     = string(getBufferTargetName(target)) + "_" + getUsageHintName(hint);

                    hintsGroup->addChild(new BufferMapReadCase(m_context, name.c_str(), "", target, hint, size, 0, size,
                                                               WRITE_BUFFER_SUB_DATA));
                }
            }
        }
    }

    // .write
    {
        tcu::TestCaseGroup *mapWriteGroup =
            new tcu::TestCaseGroup(m_testCtx, "write", "Buffer write using glMapBufferRange()");
        addChild(mapWriteGroup);

        // .[verify type]
        for (int useNdx = 0; useNdx < DE_LENGTH_OF_ARRAY(bufferUses); useNdx++)
        {
            VerifyType verify            = bufferUses[useNdx].verify;
            tcu::TestCaseGroup *useGroup = new tcu::TestCaseGroup(m_testCtx, bufferUses[useNdx].name, "");
            mapWriteGroup->addChild(useGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                uint32_t target       = bufferTargets[targetNdx];
                uint32_t hint         = GL_STATIC_DRAW;
                const int size        = 1019;
                const int partialOffs = 17;
                const int partialSize = 501;
                string name           = string(getBufferTargetName(target)) + "_" + getUsageHintName(hint);

                useGroup->addChild(new BufferMapWriteCase(m_context,
                                                          (string(getBufferTargetName(target)) + "_full").c_str(), "",
                                                          target, hint, size, verify));
                useGroup->addChild(
                    new BufferPartialMapWriteCase(m_context, (string(getBufferTargetName(target)) + "_partial").c_str(),
                                                  "", target, hint, size, partialOffs, partialSize, verify));
            }
        }

        // .usage_hints
        {
            tcu::TestCaseGroup *hintsGroup = new tcu::TestCaseGroup(m_testCtx, "usage_hints", "Usage hints");
            mapWriteGroup->addChild(hintsGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                for (int hintNdx = 0; hintNdx < DE_LENGTH_OF_ARRAY(usageHints); hintNdx++)
                {
                    uint32_t target = bufferTargets[targetNdx];
                    uint32_t hint   = usageHints[hintNdx];
                    const int size  = 1019;
                    string name     = string(getBufferTargetName(target)) + "_" + getUsageHintName(hint);

                    hintsGroup->addChild(new BufferMapWriteCase(m_context, name.c_str(), "", target, hint, size,
                                                                VERIFY_AS_VERTEX_ARRAY));
                }
            }
        }

        // .invalidate
        {
            tcu::TestCaseGroup *invalidateGroup = new tcu::TestCaseGroup(m_testCtx, "invalidate", "Buffer invalidate");
            mapWriteGroup->addChild(invalidateGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                uint32_t target = bufferTargets[targetNdx];
                uint32_t hint   = GL_STATIC_DRAW;

                invalidateGroup->addChild(
                    new BufferMapInvalidateCase(m_context, (string(getBufferTargetName(target)) + "_write_all").c_str(),
                                                "", target, hint, false, VERIFY_AS_VERTEX_ARRAY));
                invalidateGroup->addChild(new BufferMapInvalidateCase(
                    m_context, (string(getBufferTargetName(target)) + "_write_partial").c_str(), "", target, hint, true,
                    VERIFY_AS_VERTEX_ARRAY));
            }
        }

        // .partial_invalidate
        {
            tcu::TestCaseGroup *invalidateGroup =
                new tcu::TestCaseGroup(m_testCtx, "partial_invalidate", "Partial invalidate");
            mapWriteGroup->addChild(invalidateGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                uint32_t target = bufferTargets[targetNdx];
                uint32_t hint   = GL_STATIC_DRAW;

                invalidateGroup->addChild(new BufferMapPartialInvalidateCase(
                    m_context, (string(getBufferTargetName(target)) + "_write_all").c_str(), "", target, hint, false,
                    VERIFY_AS_VERTEX_ARRAY));
                invalidateGroup->addChild(new BufferMapPartialInvalidateCase(
                    m_context, (string(getBufferTargetName(target)) + "_write_partial").c_str(), "", target, hint, true,
                    VERIFY_AS_VERTEX_ARRAY));
            }
        }

        // .explicit_flush
        {
            tcu::TestCaseGroup *flushGroup = new tcu::TestCaseGroup(m_testCtx, "explicit_flush", "Explicit flush");
            mapWriteGroup->addChild(flushGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                uint32_t target = bufferTargets[targetNdx];
                uint32_t hint   = GL_STATIC_DRAW;

                flushGroup->addChild(
                    new BufferMapExplicitFlushCase(m_context, (string(getBufferTargetName(target)) + "_all").c_str(),
                                                   "", target, hint, false, VERIFY_AS_VERTEX_ARRAY));
                flushGroup->addChild(new BufferMapExplicitFlushCase(
                    m_context, (string(getBufferTargetName(target)) + "_partial").c_str(), "", target, hint, true,
                    VERIFY_AS_VERTEX_ARRAY));
            }
        }

        // .unsynchronized
        {
            tcu::TestCaseGroup *unsyncGroup = new tcu::TestCaseGroup(m_testCtx, "unsynchronized", "Unsynchronized map");
            mapWriteGroup->addChild(unsyncGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                uint32_t target = bufferTargets[targetNdx];
                uint32_t hint   = GL_STATIC_DRAW;

                unsyncGroup->addChild(
                    new BufferMapUnsyncWriteCase(m_context, getBufferTargetName(target), "", target, hint));
            }
        }
    }

    // .read_write
    {
        tcu::TestCaseGroup *mapReadWriteGroup =
            new tcu::TestCaseGroup(m_testCtx, "read_write", "Buffer read and write using glMapBufferRange()");
        addChild(mapReadWriteGroup);

        // .[verify type]
        for (int useNdx = 0; useNdx < DE_LENGTH_OF_ARRAY(bufferUses); useNdx++)
        {
            VerifyType verify            = bufferUses[useNdx].verify;
            tcu::TestCaseGroup *useGroup = new tcu::TestCaseGroup(m_testCtx, bufferUses[useNdx].name, "");
            mapReadWriteGroup->addChild(useGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                uint32_t target       = bufferTargets[targetNdx];
                uint32_t hint         = GL_STATIC_DRAW;
                const int size        = 1019;
                const int partialOffs = 17;
                const int partialSize = 501;
                string name           = string(getBufferTargetName(target)) + "_" + getUsageHintName(hint);

                useGroup->addChild(new BufferMapReadWriteCase(m_context,
                                                              (string(getBufferTargetName(target)) + "_full").c_str(),
                                                              "", target, hint, size, 0, size, verify));
                useGroup->addChild(
                    new BufferMapReadWriteCase(m_context, (string(getBufferTargetName(target)) + "_partial").c_str(),
                                               "", target, hint, size, partialOffs, partialSize, verify));
            }
        }

        // .usage_hints
        {
            tcu::TestCaseGroup *hintsGroup = new tcu::TestCaseGroup(m_testCtx, "usage_hints", "Usage hints");
            mapReadWriteGroup->addChild(hintsGroup);

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
            {
                for (int hintNdx = 0; hintNdx < DE_LENGTH_OF_ARRAY(usageHints); hintNdx++)
                {
                    uint32_t target = bufferTargets[targetNdx];
                    uint32_t hint   = usageHints[hintNdx];
                    const int size  = 1019;
                    string name     = string(getBufferTargetName(target)) + "_" + getUsageHintName(hint);

                    hintsGroup->addChild(new BufferMapReadWriteCase(m_context, name.c_str(), "", target, hint, size, 0,
                                                                    size, VERIFY_AS_VERTEX_ARRAY));
                }
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
