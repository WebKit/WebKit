#ifndef _XSPOSIXFILEREADER_HPP
#define _XSPOSIXFILEREADER_HPP
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

#include "xsDefs.hpp"
#include "deFile.h"
#include "deThread.hpp"

namespace xs
{
namespace posix
{

class FileReader : public de::Thread
{
public:
    FileReader(int blockSize, int numBlocks);
    ~FileReader(void);

    void start(const char *filename);
    void stop(void);

    bool isRunning(void) const
    {
        return m_isRunning;
    }
    int read(uint8_t *dst, int numBytes)
    {
        return m_buf.tryRead(numBytes, dst);
    }

    void run(void);

private:
    deFile *m_file;
    ThreadedByteBuffer m_buf;
    bool m_isRunning;
};

} // namespace posix
} // namespace xs

#endif // _XSPOSIXFILEREADER_HPP
