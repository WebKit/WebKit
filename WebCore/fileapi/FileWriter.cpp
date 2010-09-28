/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(FILE_SYSTEM)

#include "FileWriter.h"

#include "AsyncFileWriter.h"
#include "Blob.h"
#include "ExceptionCode.h"
#include "FileError.h"
#include "ProgressEvent.h"

namespace WebCore {

FileWriter::FileWriter(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
    , m_readyState(INIT)
    , m_position(0)
    , m_bytesWritten(0)
    , m_bytesToWrite(0)
    , m_truncateLength(-1)
{
}

void FileWriter::initialize(PassOwnPtr<AsyncFileWriter> writer, long long length)
{
    ASSERT(!m_writer);
    ASSERT(length >= 0);
    m_writer = writer;
    m_length = length;
}

FileWriter::~FileWriter()
{
    if (m_readyState == WRITING)
        stop();
}

bool FileWriter::hasPendingActivity() const
{
    return m_readyState == WRITING || ActiveDOMObject::hasPendingActivity();
}

bool FileWriter::canSuspend() const
{
    // FIXME: It is not currently possible to suspend a FileWriter, so pages with FileWriter can not go into page cache.
    return false;
}

void FileWriter::stop()
{
    if (m_writer && m_readyState == WRITING)
        m_writer->abort();
    m_readyState = DONE;
}

void FileWriter::write(Blob* data, ExceptionCode& ec)
{
    ASSERT(m_writer);
    if (m_readyState == WRITING) {
        ec = INVALID_STATE_ERR;
        m_error = FileError::create(ec);
        return;
    }

    m_readyState = WRITING;
    m_bytesWritten = 0;
    m_bytesToWrite = data->size();
    m_writer->write(m_position, data);
}

void FileWriter::seek(long long position, ExceptionCode& ec)
{
    ASSERT(m_writer);
    if (m_readyState == WRITING) {
        ec = INVALID_STATE_ERR;
        m_error = FileError::create(ec);
        return;
    }

    m_bytesWritten = 0;
    m_bytesToWrite = 0;
    if (position > m_length)
        position = m_length;
    else if (position < 0)
        position = m_length + position;
    if (position < 0)
        position = 0;
    m_position = position;
}

void FileWriter::truncate(long long position, ExceptionCode& ec)
{
    ASSERT(m_writer);
    if (m_readyState == WRITING || position < 0) {
        ec = INVALID_STATE_ERR;
        m_error = FileError::create(ec);
        return;
    }
    m_readyState = WRITING;
    m_bytesWritten = 0;
    m_bytesToWrite = 0;
    m_truncateLength = position;
    m_writer->truncate(position);
}

void FileWriter::abort(ExceptionCode& ec)
{
    ASSERT(m_writer);
    if (m_readyState != WRITING) {
        ec = INVALID_STATE_ERR;
        m_error = FileError::create(ec);
        return;
    }
    
    m_error = FileError::create(ABORT_ERR);
    m_writer->abort();
}

void FileWriter::didWrite(long long bytes, bool complete)
{
    ASSERT(bytes > 0);
    ASSERT(bytes + m_bytesWritten > 0);
    ASSERT(bytes + m_bytesWritten <= m_bytesToWrite);
    if (!m_bytesWritten)
        fireEvent(eventNames().writestartEvent);
    m_bytesWritten += bytes;
    ASSERT((m_bytesWritten == m_bytesToWrite) == complete);
    m_position += bytes;
    if (m_position > m_length)
        m_length = m_position;
    fireEvent(eventNames().writeEvent);
    if (complete) {
        m_readyState = DONE;
        fireEvent(eventNames().writeendEvent);
    }
}

void FileWriter::didTruncate()
{
    ASSERT(m_truncateLength >= 0);
    fireEvent(eventNames().writestartEvent);
    m_length = m_truncateLength;
    if (m_position > m_length)
        m_position = m_length;
    m_truncateLength = -1;
    fireEvent(eventNames().writeEvent);
    m_readyState = DONE;
    fireEvent(eventNames().writeendEvent);
}

void FileWriter::didFail(ExceptionCode ec)
{
    m_error = FileError::create(ec);
    fireEvent(eventNames().errorEvent);
    if (ABORT_ERR == ec)
        fireEvent(eventNames().abortEvent);
    fireEvent(eventNames().errorEvent);
    m_readyState = DONE;
    fireEvent(eventNames().writeendEvent);
}

void FileWriter::fireEvent(const AtomicString& type)
{
    // FIXME: the current ProgressEvent uses "unsigned long" for total and loaded attributes. Need to talk with the spec writer to resolve the issue.
    dispatchEvent(ProgressEvent::create(type, true, static_cast<unsigned>(m_bytesWritten), static_cast<unsigned>(m_bytesToWrite)));
}

} // namespace WebCore
 
#endif // ENABLE(FILE_SYSTEM)
