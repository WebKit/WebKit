/*-------------------------------------------------------------------------
 * drawElements Quality Program Execution Server
 * ---------------------------------------------
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
 * \brief File Reader.
 *//*--------------------------------------------------------------------*/

#include "xsPosixFileReader.hpp"

#include <vector>

namespace xs
{
namespace posix
{

FileReader::FileReader(int blockSize, int numBlocks) : m_file(nullptr), m_buf(blockSize, numBlocks), m_isRunning(false)
{
}

FileReader::~FileReader(void)
{
}

void FileReader::start(const char *filename)
{
    DE_ASSERT(!m_isRunning);

    m_file = deFile_create(filename, DE_FILEMODE_OPEN | DE_FILEMODE_READ);
    XS_CHECK(m_file);

#if (DE_OS != DE_OS_IOS)
    // Set to non-blocking mode.
    if (!deFile_setFlags(m_file, DE_FILE_NONBLOCKING))
    {
        deFile_destroy(m_file);
        m_file = nullptr;
        XS_FAIL("Failed to set non-blocking mode");
    }
#endif

    m_isRunning = true;

    de::Thread::start();
}

void FileReader::run(void)
{
    std::vector<uint8_t> tmpBuf(FILEREADER_TMP_BUFFER_SIZE);
    int64_t numRead = 0;

    while (!m_buf.isCanceled())
    {
        deFileResult result = deFile_read(m_file, &tmpBuf[0], (int64_t)tmpBuf.size(), &numRead);

        if (result == DE_FILERESULT_SUCCESS)
        {
            // Write to buffer.
            try
            {
                m_buf.write((int)numRead, &tmpBuf[0]);
                m_buf.flush();
            }
            catch (const ThreadedByteBuffer::CanceledException &)
            {
                // Canceled.
                break;
            }
        }
        else if (result == DE_FILERESULT_END_OF_FILE || result == DE_FILERESULT_WOULD_BLOCK)
        {
            // Wait for more data.
            deSleep(FILEREADER_IDLE_SLEEP);
        }
        else
            break; // Error.
    }
}

void FileReader::stop(void)
{
    if (!m_isRunning)
        return; // Nothing to do.

    m_buf.cancel();

    // Join thread.
    join();

    // Destroy file.
    deFile_destroy(m_file);
    m_file = nullptr;

    // Reset buffer.
    m_buf.clear();

    m_isRunning = false;
}

} // namespace posix
} // namespace xs
