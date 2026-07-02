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
 * \brief Buffer data upload tests.
 *//*--------------------------------------------------------------------*/

#include "es3fBufferWriteTests.hpp"
#include "glsBufferTestUtil.hpp"
#include "tcuTestLog.hpp"
#include "gluStrUtil.hpp"
#include "deMemory.h"
#include "deString.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <algorithm>
#include <list>

using std::set;
using std::string;
using std::vector;
using tcu::IVec2;
using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Functional
{

using namespace gls::BufferTestUtil;

struct DataStoreSpec
{
    DataStoreSpec(void) : target(0), usage(0), size(0)
    {
    }

    DataStoreSpec(uint32_t target_, uint32_t usage_, int size_) : target(target_), usage(usage_), size(size_)
    {
    }

    uint32_t target;
    uint32_t usage;
    int size;
};

struct DataStoreSpecVecBuilder
{
    std::vector<DataStoreSpec> &list;

    DataStoreSpecVecBuilder(std::vector<DataStoreSpec> &list_) : list(list_)
    {
    }

    DataStoreSpecVecBuilder &operator<<(const DataStoreSpec &spec)
    {
        list.push_back(spec);
        return *this;
    }
};

struct RangeVecBuilder
{
    std::vector<tcu::IVec2> &list;

    RangeVecBuilder(std::vector<tcu::IVec2> &list_) : list(list_)
    {
    }

    RangeVecBuilder &operator<<(const tcu::IVec2 &vec)
    {
        list.push_back(vec);
        return *this;
    }
};

template <typename Iterator>
static bool isRangeListValid(Iterator begin, Iterator end)
{
    if (begin != end)
    {
        // Fetch first.
        tcu::IVec2 prev = *begin;
        ++begin;

        for (; begin != end; ++begin)
        {
            tcu::IVec2 cur = *begin;
            if (cur.x() <= prev.x() || cur.x() <= prev.x() + prev.y())
                return false;
            prev = cur;
        }
    }

    return true;
}

inline bool rangesIntersect(const tcu::IVec2 &a, const tcu::IVec2 &b)
{
    return de::inRange(a.x(), b.x(), b.x() + b.y()) || de::inRange(a.x() + a.y(), b.x(), b.x() + b.y()) ||
           de::inRange(b.x(), a.x(), a.x() + a.y()) || de::inRange(b.x() + b.y(), a.x(), a.x() + a.y());
}

inline tcu::IVec2 unionRanges(const tcu::IVec2 &a, const tcu::IVec2 &b)
{
    DE_ASSERT(rangesIntersect(a, b));

    int start = de::min(a.x(), b.x());
    int end   = de::max(a.x() + a.y(), b.x() + b.y());

    return tcu::IVec2(start, end - start);
}

//! Updates range list (start, len) with a new range.
std::vector<tcu::IVec2> addRangeToList(const std::vector<tcu::IVec2> &oldList, const tcu::IVec2 &newRange)
{
    DE_ASSERT(newRange.y() > 0);

    std::vector<tcu::IVec2> newList;
    std::vector<tcu::IVec2>::const_iterator oldListIter = oldList.begin();

    // Append ranges that end before the new range.
    for (; oldListIter != oldList.end() && oldListIter->x() + oldListIter->y() < newRange.x(); ++oldListIter)
        newList.push_back(*oldListIter);

    // Join any ranges that intersect new range
    {
        tcu::IVec2 curRange = newRange;
        while (oldListIter != oldList.end() && rangesIntersect(curRange, *oldListIter))
        {
            curRange = unionRanges(curRange, *oldListIter);
            ++oldListIter;
        }

        newList.push_back(curRange);
    }

    // Append remaining ranges.
    for (; oldListIter != oldList.end(); oldListIter++)
        newList.push_back(*oldListIter);

    DE_ASSERT(isRangeListValid(newList.begin(), newList.end()));

    return newList;
}

class BasicBufferDataCase : public BufferCase
{
public:
    BasicBufferDataCase(Context &context, const char *name, const char *desc, uint32_t target, uint32_t usage, int size,
                        VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_target(target)
        , m_usage(usage)
        , m_size(size)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        const uint32_t dataSeed = deStringHash(getName()) ^ 0x125;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);
        ReferenceBuffer refBuf;
        bool isOk = false;

        refBuf.setSize(m_size);
        fillWithRandomBytes(refBuf.getPtr(), m_size, dataSeed);

        uint32_t buf = genBuffer();
        glBindBuffer(m_target, buf);
        glBufferData(m_target, m_size, refBuf.getPtr(), m_usage);

        checkError();

        isOk = verifier.verify(buf, refBuf.getPtr(), 0, m_size, m_target);

        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_target;
    uint32_t m_usage;
    int m_size;
    VerifyType m_verify;
};

class RecreateBufferDataStoreCase : public BufferCase
{
public:
    RecreateBufferDataStoreCase(Context &context, const char *name, const char *desc, const DataStoreSpec *specs,
                                int numSpecs, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_specs(specs, specs + numSpecs)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        const uint32_t baseSeed = deStringHash(getName()) ^ 0xbeef;
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);
        ReferenceBuffer refBuf;
        const uint32_t buf = genBuffer();

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

        for (vector<DataStoreSpec>::const_iterator spec = m_specs.begin(); spec != m_specs.end(); spec++)
        {
            bool iterOk = false;

            refBuf.setSize(spec->size);
            fillWithRandomBytes(refBuf.getPtr(), spec->size,
                                baseSeed ^ deInt32Hash(spec->size + spec->target + spec->usage));

            glBindBuffer(spec->target, buf);
            glBufferData(spec->target, spec->size, refBuf.getPtr(), spec->usage);

            checkError();

            iterOk = verifier.verify(buf, refBuf.getPtr(), 0, spec->size, spec->target);

            if (!iterOk)
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
                break;
            }
        }

        deleteBuffer(buf);
        return STOP;
    }

private:
    std::vector<DataStoreSpec> m_specs;
    VerifyType m_verify;
};

class BasicBufferSubDataCase : public BufferCase
{
public:
    BasicBufferSubDataCase(Context &context, const char *name, const char *desc, uint32_t target, uint32_t usage,
                           int size, int subDataOffs, int subDataSize, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_target(target)
        , m_usage(usage)
        , m_size(size)
        , m_subDataOffs(subDataOffs)
        , m_subDataSize(subDataSize)
        , m_verify(verify)
    {
        DE_ASSERT(de::inBounds(subDataOffs, 0, size) && de::inRange(subDataOffs + subDataSize, 0, size));
    }

    IterateResult iterate(void)
    {
        const uint32_t dataSeed = deStringHash(getName());
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);
        ReferenceBuffer refBuf;
        bool isOk = false;

        refBuf.setSize(m_size);

        uint32_t buf = genBuffer();
        glBindBuffer(m_target, buf);

        // Initialize with glBufferData()
        fillWithRandomBytes(refBuf.getPtr(), m_size, dataSeed ^ 0x80354f);
        glBufferData(m_target, m_size, refBuf.getPtr(), m_usage);
        checkError();

        // Re-specify part of buffer
        fillWithRandomBytes(refBuf.getPtr() + m_subDataOffs, m_subDataSize, dataSeed ^ 0xfac425c);
        glBufferSubData(m_target, m_subDataOffs, m_subDataSize, refBuf.getPtr() + m_subDataOffs);

        isOk = verifier.verify(buf, refBuf.getPtr(), 0, m_size, m_target);

        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_target;
    uint32_t m_usage;
    int m_size;
    int m_subDataOffs;
    int m_subDataSize;
    VerifyType m_verify;
};

class SubDataToUndefinedCase : public BufferCase
{
public:
    SubDataToUndefinedCase(Context &context, const char *name, const char *desc, uint32_t target, uint32_t usage,
                           int size, const tcu::IVec2 *ranges, int numRanges, VerifyType verify)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_target(target)
        , m_usage(usage)
        , m_size(size)
        , m_ranges(ranges, ranges + numRanges)
        , m_verify(verify)
    {
    }

    IterateResult iterate(void)
    {
        const uint32_t dataSeed = deStringHash(getName());
        BufferVerifier verifier(m_renderCtx, m_testCtx.getLog(), m_verify);
        ReferenceBuffer refBuf;
        bool isOk = true;
        std::vector<tcu::IVec2> definedRanges;

        refBuf.setSize(m_size);

        uint32_t buf = genBuffer();
        glBindBuffer(m_target, buf);

        // Initialize storage with glBufferData()
        glBufferData(m_target, m_size, nullptr, m_usage);
        checkError();

        // Fill specified ranges with glBufferSubData()
        for (vector<tcu::IVec2>::const_iterator range = m_ranges.begin(); range != m_ranges.end(); range++)
        {
            fillWithRandomBytes(refBuf.getPtr() + range->x(), range->y(),
                                dataSeed ^ deInt32Hash(range->x() + range->y()));
            glBufferSubData(m_target, range->x(), range->y(), refBuf.getPtr() + range->x());

            // Mark range as defined
            definedRanges = addRangeToList(definedRanges, *range);
        }

        // Verify defined parts
        for (vector<tcu::IVec2>::const_iterator range = definedRanges.begin(); range != definedRanges.end(); range++)
        {
            if (!verifier.verify(buf, refBuf.getPtr(), range->x(), range->y(), m_target))
                isOk = false;
        }

        deleteBuffer(buf);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Buffer verification failed");
        return STOP;
    }

private:
    uint32_t m_target;
    uint32_t m_usage;
    int m_size;
    std::vector<tcu::IVec2> m_ranges;
    VerifyType m_verify;
};

class RandomBufferWriteCase : public BufferCase
{
public:
    RandomBufferWriteCase(Context &context, const char *name, const char *desc, uint32_t seed)
        : BufferCase(context.getTestContext(), context.getRenderContext(), name, desc)
        , m_seed(seed)
        , m_verifier(nullptr)
        , m_buffer(0)
        , m_curSize(0)
        , m_iterNdx(0)
    {
    }

    ~RandomBufferWriteCase(void)
    {
        delete m_verifier;
    }

    void init(void)
    {
        BufferCase::init();

        m_iterNdx  = 0;
        m_buffer   = genBuffer();
        m_curSize  = 0;
        m_verifier = new BufferVerifier(m_renderCtx, m_testCtx.getLog(), VERIFY_AS_VERTEX_ARRAY);

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }

    void deinit(void)
    {
        deleteBuffer(m_buffer);
        m_refBuffer.setSize(0);

        delete m_verifier;
        m_verifier = nullptr;

        BufferCase::deinit();
    }

    IterateResult iterate(void)
    {
        // Parameters.
        const int numIterations              = 5;
        const int uploadsPerIteration        = 7;
        const int minSize                    = 12;
        const int maxSize                    = 32 * 1024;
        const float respecifyProbability     = 0.07f;
        const float respecifyDataProbability = 0.2f;

        static const uint32_t bufferTargets[] = {
            GL_ARRAY_BUFFER,      GL_COPY_READ_BUFFER,    GL_COPY_WRITE_BUFFER,         GL_ELEMENT_ARRAY_BUFFER,
            GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER};

        static const uint32_t usageHints[] = {GL_STREAM_DRAW,  GL_STREAM_READ,  GL_STREAM_COPY,
                                              GL_STATIC_DRAW,  GL_STATIC_READ,  GL_STATIC_COPY,
                                              GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, GL_DYNAMIC_COPY};

        bool iterOk             = true;
        uint32_t curBoundTarget = GL_NONE;
        de::Random rnd(m_seed ^ deInt32Hash(m_iterNdx) ^ 0xacf92e);

        m_testCtx.getLog() << TestLog::Section(string("Iteration") + de::toString(m_iterNdx + 1),
                                               string("Iteration ") + de::toString(m_iterNdx + 1) + " / " +
                                                   de::toString(numIterations));

        for (int uploadNdx = 0; uploadNdx < uploadsPerIteration; uploadNdx++)
        {
            const uint32_t target = bufferTargets[rnd.getInt(0, DE_LENGTH_OF_ARRAY(bufferTargets) - 1)];
            const bool respecify  = m_curSize == 0 || rnd.getFloat() < respecifyProbability;

            if (target != curBoundTarget)
            {
                glBindBuffer(target, m_buffer);
                curBoundTarget = target;
            }

            if (respecify)
            {
                const int size          = rnd.getInt(minSize, maxSize);
                const uint32_t hint     = usageHints[rnd.getInt(0, DE_LENGTH_OF_ARRAY(usageHints) - 1)];
                const bool fillWithData = rnd.getFloat() < respecifyDataProbability;

                m_refBuffer.setSize(size);
                if (fillWithData)
                    fillWithRandomBytes(m_refBuffer.getPtr(), size, rnd.getUint32());

                glBufferData(target, size, fillWithData ? m_refBuffer.getPtr() : nullptr, hint);

                m_validRanges.clear();
                if (fillWithData)
                    m_validRanges.push_back(tcu::IVec2(0, size));

                m_curSize = size;
            }
            else
            {
                // \note Non-uniform size distribution.
                const int size =
                    de::clamp(deRoundFloatToInt32((float)m_curSize * deFloatPow(rnd.getFloat(0.0f, 0.7f), 3.0f)),
                              minSize, m_curSize);
                const int offset = rnd.getInt(0, m_curSize - size);

                fillWithRandomBytes(m_refBuffer.getPtr() + offset, size, rnd.getUint32());
                glBufferSubData(target, offset, size, m_refBuffer.getPtr() + offset);

                m_validRanges = addRangeToList(m_validRanges, tcu::IVec2(offset, size));
            }
        }

        // Check error.
        {
            uint32_t err = glGetError();
            if (err != GL_NO_ERROR)
                throw tcu::TestError(string("Got ") + glu::getErrorStr(err).toString());
        }

        // Verify valid ranges.
        for (vector<IVec2>::const_iterator range = m_validRanges.begin(); range != m_validRanges.end(); range++)
        {
            const uint32_t targetHint = GL_ARRAY_BUFFER;
            if (!m_verifier->verify(m_buffer, m_refBuffer.getPtr(), range->x(), range->y(), targetHint))
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Buffer verification failed");
                iterOk = false;
                break;
            }
        }

        m_testCtx.getLog() << TestLog::EndSection;

        DE_ASSERT(iterOk || m_testCtx.getTestResult() != QP_TEST_RESULT_PASS);

        m_iterNdx += 1;
        return (iterOk && m_iterNdx < numIterations) ? CONTINUE : STOP;
    }

private:
    uint32_t m_seed;

    BufferVerifier *m_verifier;
    uint32_t m_buffer;
    ReferenceBuffer m_refBuffer;
    std::vector<tcu::IVec2> m_validRanges;
    int m_curSize;
    int m_iterNdx;
};

BufferWriteTests::BufferWriteTests(Context &context) : TestCaseGroup(context, "write", "Buffer data upload tests")
{
}

BufferWriteTests::~BufferWriteTests(void)
{
}

void BufferWriteTests::init(void)
{
    static const uint32_t bufferTargets[] = {
        GL_ARRAY_BUFFER,      GL_COPY_READ_BUFFER,    GL_COPY_WRITE_BUFFER,         GL_ELEMENT_ARRAY_BUFFER,
        GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER};

    static const uint32_t usageHints[] = {GL_STREAM_DRAW,  GL_STREAM_READ,  GL_STREAM_COPY,
                                          GL_STATIC_DRAW,  GL_STATIC_READ,  GL_STATIC_COPY,
                                          GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, GL_DYNAMIC_COPY};

    // .basic
    {
        tcu::TestCaseGroup *const basicGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic", "Basic upload with glBufferData()");
        addChild(basicGroup);

        for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
        {
            for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usageHints); usageNdx++)
            {
                const uint32_t target   = bufferTargets[targetNdx];
                const uint32_t usage    = usageHints[usageNdx];
                const int size          = 1020;
                const VerifyType verify = VERIFY_AS_VERTEX_ARRAY;
                const string name       = string(getBufferTargetName(target)) + "_" + getUsageHintName(usage);

                basicGroup->addChild(new BasicBufferDataCase(m_context, name.c_str(), "", target, usage, size, verify));
            }
        }
    }

    // .recreate_store
    {
        tcu::TestCaseGroup *const recreateStoreGroup =
            new tcu::TestCaseGroup(m_testCtx, "recreate_store", "Data store recreate using glBufferData()");
        addChild(recreateStoreGroup);

#define RECREATE_STORE_CASE(NAME, DESC, SPECLIST)                                                                 \
    do                                                                                                            \
    {                                                                                                             \
        std::vector<DataStoreSpec> specs;                                                                         \
        DataStoreSpecVecBuilder builder(specs);                                                                   \
        builder SPECLIST;                                                                                         \
        recreateStoreGroup->addChild(new RecreateBufferDataStoreCase(m_context, #NAME, DESC, &specs[0],           \
                                                                     (int)specs.size(), VERIFY_AS_VERTEX_ARRAY)); \
    } while (false)

        RECREATE_STORE_CASE(identical_1, "Recreate with identical parameters",
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STATIC_DRAW, 996)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STATIC_DRAW, 996)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STATIC_DRAW, 996));

        RECREATE_STORE_CASE(identical_2, "Recreate with identical parameters",
                            << DataStoreSpec(GL_COPY_WRITE_BUFFER, GL_STATIC_DRAW, 72)
                            << DataStoreSpec(GL_COPY_WRITE_BUFFER, GL_STATIC_DRAW, 72)
                            << DataStoreSpec(GL_COPY_WRITE_BUFFER, GL_STATIC_DRAW, 72));

        RECREATE_STORE_CASE(different_target, "Recreate with different target",
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STATIC_DRAW, 504)
                            << DataStoreSpec(GL_COPY_READ_BUFFER, GL_STATIC_DRAW, 504)
                            << DataStoreSpec(GL_COPY_WRITE_BUFFER, GL_STATIC_DRAW, 504)
                            << DataStoreSpec(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, 504)
                            << DataStoreSpec(GL_PIXEL_PACK_BUFFER, GL_STATIC_DRAW, 504)
                            << DataStoreSpec(GL_PIXEL_UNPACK_BUFFER, GL_STATIC_DRAW, 504)
                            << DataStoreSpec(GL_TRANSFORM_FEEDBACK_BUFFER, GL_STATIC_DRAW, 504)
                            << DataStoreSpec(GL_UNIFORM_BUFFER, GL_STATIC_DRAW, 504)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STATIC_DRAW, 504));

        RECREATE_STORE_CASE(different_usage, "Recreate with different usage",
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_DRAW, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_COPY, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STATIC_READ, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_READ, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_DYNAMIC_COPY, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STATIC_COPY, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_DYNAMIC_READ, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STATIC_DRAW, 1644)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_DRAW, 1644));

        RECREATE_STORE_CASE(different_size, "Recreate with different size",
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_DRAW, 1024)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_DRAW, 12)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_DRAW, 3327)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_DRAW, 92)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_DRAW, 123795)
                            << DataStoreSpec(GL_ARRAY_BUFFER, GL_STREAM_DRAW, 571));

#undef RECREATE_STORE_CASE

        // Random cases.
        {
            const int numRandomCases    = 4;
            const int numUploadsPerCase = 10;
            const int minSize           = 12;
            const int maxSize           = 65536;
            const VerifyType verify     = VERIFY_AS_VERTEX_ARRAY;
            de::Random rnd(23921);

            for (int caseNdx = 0; caseNdx < numRandomCases; caseNdx++)
            {
                vector<DataStoreSpec> specs(numUploadsPerCase);

                for (vector<DataStoreSpec>::iterator spec = specs.begin(); spec != specs.end(); spec++)
                {
                    spec->target = bufferTargets[rnd.getInt(0, DE_LENGTH_OF_ARRAY(bufferTargets) - 1)];
                    spec->usage  = usageHints[rnd.getInt(0, DE_LENGTH_OF_ARRAY(usageHints) - 1)];
                    spec->size   = rnd.getInt(minSize, maxSize);
                }

                recreateStoreGroup->addChild(
                    new RecreateBufferDataStoreCase(m_context, (string("random_") + de::toString(caseNdx + 1)).c_str(),
                                                    "", &specs[0], (int)specs.size(), verify));
            }
        }
    }

    // .basic_subdata
    {
        tcu::TestCaseGroup *const basicGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_subdata", "Basic glBufferSubData() usage");
        addChild(basicGroup);

        for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(bufferTargets); targetNdx++)
        {
            for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usageHints); usageNdx++)
            {
                const uint32_t target   = bufferTargets[targetNdx];
                const uint32_t usage    = usageHints[usageNdx];
                const int size          = 1020;
                const VerifyType verify = VERIFY_AS_VERTEX_ARRAY;
                const string name       = string(getBufferTargetName(target)) + "_" + getUsageHintName(usage);

                basicGroup->addChild(new BasicBufferDataCase(m_context, name.c_str(), "", target, usage, size, verify));
            }
        }
    }

    // .partial_specify
    {
        tcu::TestCaseGroup *const partialSpecifyGroup = new tcu::TestCaseGroup(
            m_testCtx, "partial_specify", "Partial buffer data specification with glBufferSubData()");
        addChild(partialSpecifyGroup);

#define PARTIAL_SPECIFY_CASE(NAME, DESC, TARGET, USAGE, SIZE, RANGELIST)                                           \
    do                                                                                                             \
    {                                                                                                              \
        std::vector<tcu::IVec2> ranges;                                                                            \
        RangeVecBuilder builder(ranges);                                                                           \
        builder RANGELIST;                                                                                         \
        partialSpecifyGroup->addChild(new SubDataToUndefinedCase(                                                  \
            m_context, #NAME, DESC, TARGET, USAGE, SIZE, &ranges[0], (int)ranges.size(), VERIFY_AS_VERTEX_ARRAY)); \
    } while (false)

        PARTIAL_SPECIFY_CASE(whole_1, "Whole buffer specification with single glBufferSubData()", GL_ARRAY_BUFFER,
                             GL_STATIC_DRAW, 996, << IVec2(0, 996));
        PARTIAL_SPECIFY_CASE(whole_2, "Whole buffer specification with two calls", GL_UNIFORM_BUFFER, GL_DYNAMIC_READ,
                             1728, << IVec2(729, 999) << IVec2(0, 729));
        PARTIAL_SPECIFY_CASE(whole_3, "Whole buffer specification with three calls", GL_TRANSFORM_FEEDBACK_BUFFER,
                             GL_STREAM_COPY, 1944, << IVec2(0, 421) << IVec2(1421, 523) << IVec2(421, 1000));
        PARTIAL_SPECIFY_CASE(whole_4, "Whole buffer specification with three calls", GL_TRANSFORM_FEEDBACK_BUFFER,
                             GL_STREAM_COPY, 1200, << IVec2(0, 500) << IVec2(429, 200) << IVec2(513, 687));

        PARTIAL_SPECIFY_CASE(low_1, "Low part of buffer specified with single call", GL_ELEMENT_ARRAY_BUFFER,
                             GL_DYNAMIC_DRAW, 1000, << IVec2(0, 513));
        PARTIAL_SPECIFY_CASE(low_2, "Low part of buffer specified with two calls", GL_COPY_READ_BUFFER, GL_DYNAMIC_COPY,
                             996, << IVec2(0, 98) << IVec2(98, 511));
        PARTIAL_SPECIFY_CASE(low_3, "Low part of buffer specified with two calls", GL_COPY_READ_BUFFER, GL_DYNAMIC_COPY,
                             1200, << IVec2(0, 591) << IVec2(371, 400));

        PARTIAL_SPECIFY_CASE(high_1, "High part of buffer specified with single call", GL_COPY_WRITE_BUFFER,
                             GL_STATIC_COPY, 1000, << IVec2(500, 500));
        PARTIAL_SPECIFY_CASE(high_2, "High part of buffer specified with two calls", GL_TRANSFORM_FEEDBACK_BUFFER,
                             GL_STREAM_DRAW, 1200, << IVec2(600, 123) << IVec2(723, 477));
        PARTIAL_SPECIFY_CASE(high_3, "High part of buffer specified with two calls", GL_PIXEL_PACK_BUFFER,
                             GL_STREAM_READ, 1200, << IVec2(600, 200) << IVec2(601, 599));

        PARTIAL_SPECIFY_CASE(middle_1, "Middle part of buffer specified with single call", GL_PIXEL_UNPACK_BUFFER,
                             GL_STREAM_READ, 2500, << IVec2(1000, 799));
        PARTIAL_SPECIFY_CASE(middle_2, "Middle part of buffer specified with two calls", GL_ELEMENT_ARRAY_BUFFER,
                             GL_STATIC_DRAW, 2500, << IVec2(780, 220) << IVec2(1000, 500));
        PARTIAL_SPECIFY_CASE(middle_3, "Middle part of buffer specified with two calls", GL_ARRAY_BUFFER,
                             GL_STREAM_READ, 2500, << IVec2(780, 321) << IVec2(1000, 501));

#undef PARTIAL_SPECIFY_CASE
    }

    // .random
    {
        tcu::TestCaseGroup *const randomGroup =
            new tcu::TestCaseGroup(m_testCtx, "random", "Randomized buffer data cases");
        addChild(randomGroup);

        for (int i = 0; i < 10; i++)
            randomGroup->addChild(new RandomBufferWriteCase(m_context, de::toString(i).c_str(), "", deInt32Hash(i)));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
