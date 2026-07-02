#ifndef _ES2FBUFFERTESTUTIL_HPP
#define _ES2FBUFFERTESTUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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
 * \brief Buffer test utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestLog.hpp"
#include "gluCallLogWrapper.hpp"
#include "tes2TestCase.hpp"

#include <vector>
#include <set>

namespace glu
{
class ShaderProgram;
}

namespace deqp
{
namespace gles2
{
namespace Functional
{
namespace BufferTestUtil
{

// Helper functions.

void fillWithRandomBytes(uint8_t *ptr, int numBytes, uint32_t seed);
bool compareByteArrays(tcu::TestLog &log, const uint8_t *resPtr, const uint8_t *refPtr, int numBytes);
const char *getBufferTargetName(uint32_t target);
const char *getUsageHintName(uint32_t hint);

// Base class for buffer cases.

class BufferCase : public TestCase, public glu::CallLogWrapper
{
public:
    BufferCase(Context &context, const char *name, const char *description);
    virtual ~BufferCase(void);

    void init(void);
    void deinit(void);

    uint32_t genBuffer(void);
    void deleteBuffer(uint32_t buffer);
    void checkError(void);

private:
    // Resource handles for cleanup in case of unexpected iterate() termination.
    std::set<uint32_t> m_allocatedBuffers;
};

// Reference buffer.

class ReferenceBuffer
{
public:
    ReferenceBuffer(void)
    {
    }
    ~ReferenceBuffer(void)
    {
    }

    void setSize(int numBytes);
    void setData(int numBytes, const uint8_t *bytes);
    void setSubData(int offset, int numBytes, const uint8_t *bytes);

    uint8_t *getPtr(int offset = 0)
    {
        return &m_data[offset];
    }
    const uint8_t *getPtr(int offset = 0) const
    {
        return &m_data[offset];
    }

private:
    std::vector<uint8_t> m_data;
};

// Buffer verifier system.

enum VerifyType
{
    VERIFY_AS_VERTEX_ARRAY = 0,
    VERIFY_AS_INDEX_ARRAY,

    VERIFY_LAST
};

class BufferVerifierBase : public glu::CallLogWrapper
{
public:
    BufferVerifierBase(Context &context);
    virtual ~BufferVerifierBase(void)
    {
    }

    virtual int getMinSize(void) const                                                       = 0;
    virtual int getAlignment(void) const                                                     = 0;
    virtual bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes) = 0;

protected:
    Context &m_context;

private:
    BufferVerifierBase(const BufferVerifierBase &other);
    BufferVerifierBase &operator=(const BufferVerifierBase &other);
};

class BufferVerifier
{
public:
    BufferVerifier(Context &context, VerifyType verifyType);
    ~BufferVerifier(void);

    int getMinSize(void) const
    {
        return m_verifier->getMinSize();
    }
    int getAlignment(void) const
    {
        return m_verifier->getAlignment();
    }

    // \note Offset is applied to reference pointer as well.
    bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes);

private:
    BufferVerifier(const BufferVerifier &other);
    BufferVerifier &operator=(const BufferVerifier &other);

    BufferVerifierBase *m_verifier;
};

class VertexArrayVerifier : public BufferVerifierBase
{
public:
    VertexArrayVerifier(Context &context);
    ~VertexArrayVerifier(void);

    int getMinSize(void) const
    {
        return 3 * 4;
    }
    int getAlignment(void) const
    {
        return 1;
    }
    bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes);

private:
    glu::ShaderProgram *m_program;
    uint32_t m_posLoc;
    uint32_t m_byteVecLoc;
};

class IndexArrayVerifier : public BufferVerifierBase
{
public:
    IndexArrayVerifier(Context &context);
    ~IndexArrayVerifier(void);

    int getMinSize(void) const
    {
        return 2;
    }
    int getAlignment(void) const
    {
        return 1;
    }
    bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes);

private:
    glu::ShaderProgram *m_program;
    uint32_t m_posLoc;
    uint32_t m_colorLoc;
};

} // namespace BufferTestUtil
} // namespace Functional
} // namespace gles2
} // namespace deqp

#endif // _ES2FBUFFERTESTUTIL_HPP
