#ifndef _XSDEFS_HPP
#define _XSDEFS_HPP
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
 * \brief ExecServer defines.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deRingBuffer.hpp"
#include "deBlockBuffer.hpp"

#include <stdexcept>

namespace xs
{

// Configuration.
enum
{
    // Times are in milliseconds.
    LOG_FILE_TIMEOUT  = 5000,
    READ_DATA_TIMEOUT = 500,

    SERVER_IDLE_THRESHOLD = 10,
    SERVER_IDLE_SLEEP     = 50,
    FILEREADER_IDLE_SLEEP = 100,

    LOG_BUFFER_BLOCK_SIZE = 1024,
    LOG_BUFFER_NUM_BLOCKS = 512,

    INFO_BUFFER_BLOCK_SIZE = 64,
    INFO_BUFFER_NUM_BLOCKS = 128,

    SEND_BUFFER_SIZE = 16 * 1024,
    RECV_BUFFER_SIZE = 4 * 1024,

    FILEREADER_TMP_BUFFER_SIZE = 1024,
    SEND_RECV_TMP_BUFFER_SIZE  = 4 * 1024,

    MIN_MSG_PAYLOAD_SIZE = 32
};

typedef de::RingBuffer<uint8_t> ByteBuffer;
typedef de::BlockBuffer<uint8_t> ThreadedByteBuffer;

class Error : public std::runtime_error
{
public:
    Error(const std::string &message) : std::runtime_error(message)
    {
    }
    Error(const char *message, const char *expr, const char *file, int line);
};

class ConnectionError : public Error
{
public:
    ConnectionError(const std::string &message) : Error(message)
    {
    }
};

class ProtocolError : public ConnectionError
{
public:
    ProtocolError(const std::string &message) : ConnectionError(message)
    {
    }
};

} // namespace xs

#define XS_FAIL(MSG) throw xs::Error(MSG, "", __FILE__, __LINE__)
#define XS_CHECK(X)                                        \
    do                                                     \
    {                                                      \
        if ((!false && (X)) ? false : true)                \
            throw xs::Error(NULL, #X, __FILE__, __LINE__); \
    } while (false)
#define XS_CHECK_MSG(X, MSG)                              \
    do                                                    \
    {                                                     \
        if ((!false && (X)) ? false : true)               \
            throw xs::Error(MSG, #X, __FILE__, __LINE__); \
    } while (false)

#endif // _XSDEFS_HPP
