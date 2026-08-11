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
 * \brief Buffer copying tests.
 *//*--------------------------------------------------------------------*/

#include "es3fBufferCopyTests.hpp"
#include "glsBufferTestUtil.hpp"
#include "tcuTestLog.hpp"
#include "deMemory.h"
#include "deString.h"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <algorithm>

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

class BasicBufferCopyCase : public BufferCase
{
public:
    BasicBufferCopyCase(Context &context, const char *name, const char *desc, uint32_t srcTarget, int srcSize,
                        uint32_t srcHint, uint32_t dstTarget, int dstSize, uint32_t dstHint, int copySrcOffset,
                        int copyDstOffset, int copySize, VerifyType verifyType)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_srcTarget(srcTarget)
        , m_srcSize(srcSize)
        , m_srcHint(srcHint)
        , m_dstTarget(dstTarget)
        , m_dstSize(dstSize)
        , m_dstHint(dstHint)
        , m_copySrcOffset(copySrcOffset)
        , m_copyDstOffset(copyDstOffset)
        , m_copySize(copySize)
        , m_verifyType(verifyType)
    {
        DE_ASSERT(de::inBounds(m_copySrcOffset, 0, m_srcSize) &&
                  de::inRange(m_copySrcOffset + m_copySize, m_copySrcOffset, m_srcSize));
        DE_ASSERT(de::inBounds(m_copyDstOffset, 0, m_dstSize) &&
                  de::inRange(m_copyDstOffset + m_copySize, m_copyDstOffset, m_dstSize));
    }

    IterateResult iterate(void)
    {
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verifyType);
        ReferenceBuffer srcRef;
        ReferenceBuffer dstRef;
        uint32_t srcBuf  = 0;
        uint32_t dstBuf  = 0;
        uint32_t srcSeed = deStringHash(getName()) ^ 0xabcd;
        uint32_t dstSeed = deStringHash(getName()) ^ 0xef01;
        bool isOk        = true;

        srcRef.setSize(m_srcSize);
        fillWithRandomBytes(srcRef.getPtr(), m_srcSize, srcSeed);

        dstRef.setSize(m_dstSize);
        fillWithRandomBytes(dstRef.getPtr(), m_dstSize, dstSeed);

        // Create source buffer and fill with data.
        srcBuf = genBuffer();
        glBindBuffer(m_srcTarget, srcBuf);
        glBufferData(m_srcTarget, m_srcSize, srcRef.getPtr(), m_srcHint);
        GLU_CHECK_MSG("glBufferData");

        // Create destination buffer and fill with data.
        dstBuf = genBuffer();
        glBindBuffer(m_dstTarget, dstBuf);
        glBufferData(m_dstTarget, m_dstSize, dstRef.getPtr(), m_dstHint);
        GLU_CHECK_MSG("glBufferData");

        // Verify both buffers before executing copy.
        isOk = verifier.verify(srcBuf, srcRef.getPtr(), 0, m_srcSize, m_srcTarget) && isOk;
        isOk = verifier.verify(dstBuf, dstRef.getPtr(), 0, m_dstSize, m_dstTarget) && isOk;

        // Execute copy.
        deMemcpy(dstRef.getPtr() + m_copyDstOffset, srcRef.getPtr() + m_copySrcOffset, m_copySize);

        glBindBuffer(m_srcTarget, srcBuf);
        glBindBuffer(m_dstTarget, dstBuf);
        glCopyBufferSubData(m_srcTarget, m_dstTarget, m_copySrcOffset, m_copyDstOffset, m_copySize);
        GLU_CHECK_MSG("glCopyBufferSubData");

        // Verify both buffers after copy.
        isOk = verifier.verify(srcBuf, srcRef.getPtr(), 0, m_srcSize, m_srcTarget) && isOk;
        isOk = verifier.verify(dstBuf, dstRef.getPtr(), 0, m_dstSize, m_dstTarget) && isOk;

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_srcTarget;
    int m_srcSize;
    uint32_t m_srcHint;

    uint32_t m_dstTarget;
    int m_dstSize;
    uint32_t m_dstHint;

    int m_copySrcOffset;
    int m_copyDstOffset;
    int m_copySize;

    VerifyType m_verifyType;
};

// Case B: same buffer, take range as parameter

class SingleBufferCopyCase : public BufferCase
{
public:
    SingleBufferCopyCase(Context &context, const char *name, const char *desc, uint32_t srcTarget, uint32_t dstTarget,
                         uint32_t hint, VerifyType verifyType)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_srcTarget(srcTarget)
        , m_dstTarget(dstTarget)
        , m_hint(hint)
        , m_verifyType(verifyType)
    {
    }

    IterateResult iterate(void)
    {
        const int size = 1000;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verifyType);
        ReferenceBuffer ref;
        uint32_t buf      = 0;
        uint32_t baseSeed = deStringHash(getName());
        bool isOk         = true;

        ref.setSize(size);

        // Create buffer.
        buf = genBuffer();
        glBindBuffer(m_srcTarget, buf);

        static const struct
        {
            int srcOffset;
            int dstOffset;
            int copySize;
        } copyRanges[] = {
            {57, 701, 101},  // Non-adjecent, from low to high.
            {640, 101, 101}, // Non-adjecent, from high to low.
            {0, 500, 500},   // Lower half to upper half.
            {500, 0, 500}    // Upper half to lower half.
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(copyRanges) && isOk; ndx++)
        {
            int srcOffset = copyRanges[ndx].srcOffset;
            int dstOffset = copyRanges[ndx].dstOffset;
            int copySize  = copyRanges[ndx].copySize;

            fillWithRandomBytes(ref.getPtr(), size, baseSeed ^ deInt32Hash(ndx));

            // Fill with data.
            glBindBuffer(m_srcTarget, buf);
            glBufferData(m_srcTarget, size, ref.getPtr(), m_hint);
            GLU_CHECK_MSG("glBufferData");

            // Execute copy.
            deMemcpy(ref.getPtr() + dstOffset, ref.getPtr() + srcOffset, copySize);

            glBindBuffer(m_dstTarget, buf);
            glCopyBufferSubData(m_srcTarget, m_dstTarget, srcOffset, dstOffset, copySize);
            GLU_CHECK_MSG("glCopyBufferSubData");

            // Verify buffer after copy.
            isOk = verifier.verify(buf, ref.getPtr(), 0, size, m_dstTarget) && isOk;
        }

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_srcTarget;
    uint32_t m_dstTarget;
    uint32_t m_hint;

    VerifyType m_verifyType;
};

BufferCopyTests::BufferCopyTests(Context &context) : TestCaseGroup(context, "copy", "Buffer copy tests")
{
}

BufferCopyTests::~BufferCopyTests(void)
{
}

void BufferCopyTests::init(void)
{
    static const uint32_t bufferTargets[] = {
        GL_ARRAY_BUFFER,      GL_COPY_READ_BUFFER,    GL_COPY_WRITE_BUFFER,         GL_ELEMENT_ARRAY_BUFFER,
        GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER};

    // .basic
    {
        tcu::TestCaseGroup *basicGroup = new tcu::TestCaseGroup(m_testCtx, "basic", "Basic buffer copy cases");
        addChild(basicGroup);

        for (int srcTargetNdx = 0; srcTargetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); srcTargetNdx++)
        {
            for (int dstTargetNdx = 0; dstTargetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); dstTargetNdx++)
            {
                if (srcTargetNdx == dstTargetNdx)
                    continue;

                uint32_t srcTarget  = bufferTargets[srcTargetNdx];
                uint32_t dstTarget  = bufferTargets[dstTargetNdx];
                const int size      = 1017;
                const uint32_t hint = GL_STATIC_DRAW;
                VerifyType verify   = VERIFY_AS_VERTEX_ARRAY;
                string name         = string(getBufferTargetName(srcTarget)) + "_" + getBufferTargetName(dstTarget);

                basicGroup->addChild(new BasicBufferCopyCase(m_context, name.c_str(), "", srcTarget, size, hint,
                                                             dstTarget, size, hint, 0, 0, size, verify));
            }
        }
    }

    // .subrange
    {
        tcu::TestCaseGroup *subrangeGroup = new tcu::TestCaseGroup(m_testCtx, "subrange", "Buffer subrange copy tests");
        addChild(subrangeGroup);

        static const struct
        {
            const char *name;
            int srcSize;
            int dstSize;
            int srcOffset;
            int dstOffset;
            int copySize;
        } cases[] = {//                        srcSize        dstSize        srcOffs        dstOffs        copySize
                     {"middle", 1000, 1000, 250, 250, 500},      {"small_to_large", 100, 1000, 0, 409, 100},
                     {"large_to_small", 1000, 100, 409, 0, 100}, {"low_to_high_1", 1000, 1000, 0, 500, 500},
                     {"low_to_high_2", 997, 1027, 0, 701, 111},  {"high_to_low_1", 1000, 1000, 500, 0, 500},
                     {"high_to_low_2", 1027, 997, 701, 17, 111}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cases); ndx++)
        {
            uint32_t srcTarget = GL_COPY_READ_BUFFER;
            uint32_t dstTarget = GL_COPY_WRITE_BUFFER;
            uint32_t hint      = GL_STATIC_DRAW;
            VerifyType verify  = VERIFY_AS_VERTEX_ARRAY;

            subrangeGroup->addChild(new BasicBufferCopyCase(
                m_context, cases[ndx].name, "", srcTarget, cases[ndx].srcSize, hint, dstTarget, cases[ndx].dstSize,
                hint, cases[ndx].srcOffset, cases[ndx].dstOffset, cases[ndx].copySize, verify));
        }
    }

    // .single_buffer
    {
        tcu::TestCaseGroup *singleBufGroup =
            new tcu::TestCaseGroup(m_testCtx, "single_buffer", "Copies within single buffer");
        addChild(singleBufGroup);

        for (int srcTargetNdx = 0; srcTargetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); srcTargetNdx++)
        {
            for (int dstTargetNdx = 0; dstTargetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); dstTargetNdx++)
            {
                if (srcTargetNdx == dstTargetNdx)
                    continue;

                uint32_t srcTarget  = bufferTargets[srcTargetNdx];
                uint32_t dstTarget  = bufferTargets[dstTargetNdx];
                const uint32_t hint = GL_STATIC_DRAW;
                VerifyType verify   = VERIFY_AS_VERTEX_ARRAY;
                string name         = string(getBufferTargetName(srcTarget)) + "_" + getBufferTargetName(dstTarget);

                singleBufGroup->addChild(
                    new SingleBufferCopyCase(m_context, name.c_str(), "", srcTarget, dstTarget, hint, verify));
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
