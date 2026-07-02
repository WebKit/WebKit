#ifndef _GLSBUFFERTESTUTIL_HPP
#define _GLSBUFFERTESTUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
#include "tcuTestCase.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluObjectWrapper.hpp"

#include <vector>
#include <set>

namespace tcu
{
class TestLog;
}

namespace glu
{
class RenderContext;
class ShaderProgram;
} // namespace glu

namespace deqp
{
namespace gls
{
namespace BufferTestUtil
{

// Helper functions.

void fillWithRandomBytes(uint8_t *ptr, int numBytes, uint32_t seed);
bool compareByteArrays(tcu::TestLog &log, const uint8_t *resPtr, const uint8_t *refPtr, int numBytes);
const char *getBufferTargetName(uint32_t target);
const char *getUsageHintName(uint32_t hint);

// Base class for buffer cases.

class BufferCase : public tcu::TestCase, public glu::CallLogWrapper
{
public:
    BufferCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *description);
    virtual ~BufferCase(void);

    void init(void);
    void deinit(void);

    uint32_t genBuffer(void);
    void deleteBuffer(uint32_t buffer);
    void checkError(void);

protected:
    glu::RenderContext &m_renderCtx;

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

// Buffer writer system.

enum WriteType
{
    WRITE_BUFFER_SUB_DATA = 0,
    WRITE_BUFFER_WRITE_MAP,
    WRITE_TRANSFORM_FEEDBACK,
    WRITE_PIXEL_PACK,

    WRITE_LAST
};

const char *getWriteTypeDescription(WriteType type);

class BufferWriterBase : protected glu::CallLogWrapper
{
public:
    BufferWriterBase(glu::RenderContext &renderCtx, tcu::TestLog &log);
    virtual ~BufferWriterBase(void)
    {
    }

    virtual int getMinSize(void) const                                                  = 0;
    virtual int getAlignment(void) const                                                = 0;
    virtual void write(uint32_t buffer, int offset, int numBytes, const uint8_t *bytes) = 0;
    virtual void write(uint32_t buffer, int offset, int numBytes, const uint8_t *bytes, uint32_t targetHint);

protected:
    glu::RenderContext &m_renderCtx;

private:
    BufferWriterBase(const BufferWriterBase &other);
    BufferWriterBase &operator=(const BufferWriterBase &other);
};

class BufferWriter
{
public:
    BufferWriter(glu::RenderContext &renderCtx, tcu::TestLog &log, WriteType writeType);
    ~BufferWriter(void);

    int getMinSize(void) const
    {
        return m_writer->getMinSize();
    }
    int getAlignment(void) const
    {
        return m_writer->getAlignment();
    }
    void write(uint32_t buffer, int offset, int numBytes, const uint8_t *bytes);
    void write(uint32_t buffer, int offset, int numBytes, const uint8_t *bytes, uint32_t targetHint);

private:
    BufferWriter(const BufferWriter &other);
    BufferWriter &operator=(const BufferWriter &other);

    BufferWriterBase *m_writer;
};

class BufferSubDataWriter : public BufferWriterBase
{
public:
    BufferSubDataWriter(glu::RenderContext &renderCtx, tcu::TestLog &log) : BufferWriterBase(renderCtx, log)
    {
    }
    ~BufferSubDataWriter(void)
    {
    }

    int getMinSize(void) const
    {
        return 1;
    }
    int getAlignment(void) const
    {
        return 1;
    }
    virtual void write(uint32_t buffer, int offset, int numBytes, const uint8_t *bytes);
    virtual void write(uint32_t buffer, int offset, int numBytes, const uint8_t *bytes, uint32_t target);
};

class BufferWriteMapWriter : public BufferWriterBase
{
public:
    BufferWriteMapWriter(glu::RenderContext &renderCtx, tcu::TestLog &log) : BufferWriterBase(renderCtx, log)
    {
    }
    ~BufferWriteMapWriter(void)
    {
    }

    int getMinSize(void) const
    {
        return 1;
    }
    int getAlignment(void) const
    {
        return 1;
    }
    virtual void write(uint32_t buffer, int offset, int numBytes, const uint8_t *bytes);
    virtual void write(uint32_t buffer, int offset, int numBytes, const uint8_t *bytes, uint32_t target);
};

// Buffer verifier system.

enum VerifyType
{
    VERIFY_AS_VERTEX_ARRAY = 0,
    VERIFY_AS_INDEX_ARRAY,
    VERIFY_AS_UNIFORM_BUFFER,
    VERIFY_AS_PIXEL_UNPACK_BUFFER,
    VERIFY_BUFFER_READ_MAP,

    VERIFY_LAST
};

const char *getVerifyTypeDescription(VerifyType type);

class BufferVerifierBase : public glu::CallLogWrapper
{
public:
    BufferVerifierBase(glu::RenderContext &renderCtx, tcu::TestLog &log);
    virtual ~BufferVerifierBase(void)
    {
    }

    virtual int getMinSize(void) const                                                       = 0;
    virtual int getAlignment(void) const                                                     = 0;
    virtual bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes) = 0;
    virtual bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes, uint32_t targetHint);

protected:
    glu::RenderContext &m_renderCtx;
    tcu::TestLog &m_log;

private:
    BufferVerifierBase(const BufferVerifierBase &other);
    BufferVerifierBase &operator=(const BufferVerifierBase &other);
};

class BufferVerifier
{
public:
    BufferVerifier(glu::RenderContext &renderCtx, tcu::TestLog &log, VerifyType verifyType);
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
    bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes, uint32_t targetHint);

private:
    BufferVerifier(const BufferVerifier &other);
    BufferVerifier &operator=(const BufferVerifier &other);

    BufferVerifierBase *m_verifier;
};

class BufferMapVerifier : public BufferVerifierBase
{
public:
    BufferMapVerifier(glu::RenderContext &renderCtx, tcu::TestLog &log) : BufferVerifierBase(renderCtx, log)
    {
    }
    ~BufferMapVerifier(void)
    {
    }

    int getMinSize(void) const
    {
        return 1;
    }
    int getAlignment(void) const
    {
        return 1;
    }
    bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes);
    bool verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes, uint32_t target);
};

class VertexArrayVerifier : public BufferVerifierBase
{
public:
    VertexArrayVerifier(glu::RenderContext &renderCtx, tcu::TestLog &log);
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

    uint32_t m_vao;
    uint32_t m_positionBuf;
    uint32_t m_indexBuf;
};

class IndexArrayVerifier : public BufferVerifierBase
{
public:
    IndexArrayVerifier(glu::RenderContext &renderCtx, tcu::TestLog &log);
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

    uint32_t m_vao;
    uint32_t m_positionBuf;
    uint32_t m_colorBuf;
};

} // namespace BufferTestUtil
} // namespace gls
} // namespace deqp

#endif // _GLSBUFFERTESTUTIL_HPP
