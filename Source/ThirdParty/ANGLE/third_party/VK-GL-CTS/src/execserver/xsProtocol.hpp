#ifndef _XSPROTOCOL_HPP
#define _XSPROTOCOL_HPP
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
 * \brief Execution Server Protocol.
 *//*--------------------------------------------------------------------*/

#include "xsDefs.hpp"
#include "deMemory.h"

#include <string>
#include <vector>

namespace xs
{

enum
{
    PROTOCOL_VERSION    = 18,
    MESSAGE_HEADER_SIZE = 8,

    // Times are in milliseconds.
    KEEPALIVE_SEND_INTERVAL = 5000,
    KEEPALIVE_TIMEOUT       = 30000,
};

enum MessageType
{
    MESSAGETYPE_NONE = 0, //!< Not valid.

    // Commands (from Client to ExecServer).
    MESSAGETYPE_HELLO          = 100, //!< First message from client, specifies the protocol version
    MESSAGETYPE_TEST           = 101, //!< Debug only
    MESSAGETYPE_EXECUTE_BINARY = 111, //!< Request execution of a test package binary.
    MESSAGETYPE_STOP_EXECUTION = 112, //!< Request cancellation of the currently executing binary.

    // Responses (from ExecServer to Client)
    MESSAGETYPE_PROCESS_STARTED       = 200, //!< Requested process has started.
    MESSAGETYPE_PROCESS_LAUNCH_FAILED = 201, //!< Requested process failed to launch.
    MESSAGETYPE_PROCESS_FINISHED      = 202, //!< Requested process has finished (for any reason).
    MESSAGETYPE_PROCESS_LOG_DATA      = 203, //!< Unprocessed log data from TestResults.qpa.
    MESSAGETYPE_INFO                  = 204, //!< Generic info message from ExecServer (for debugging purposes).

    MESSAGETYPE_KEEPALIVE = 102 //!< Keep-alive packet
};

class MessageWriter;

class Message
{
public:
    MessageType type;

    Message(MessageType type_) : type(type_)
    {
    }
    virtual ~Message(void)
    {
    }

    virtual void write(std::vector<uint8_t> &buf) const = 0;

    static void parseHeader(const uint8_t *data, size_t dataSize, MessageType &type, size_t &messageSize);
    static void writeHeader(MessageType type, size_t messageSize, uint8_t *dst, size_t bufSize);

protected:
    void writeNoData(std::vector<uint8_t> &buf) const;

    Message(const Message &other);
    Message &operator=(const Message &other);
};

// Simple messages without any data.
template <int MsgType>
class SimpleMessage : public Message
{
public:
    SimpleMessage(const uint8_t *data, size_t dataSize) : Message((MessageType)MsgType)
    {
        DE_UNREF(data);
        XS_CHECK_MSG(dataSize == 0, "No payload expected");
    }
    SimpleMessage(void) : Message((MessageType)MsgType)
    {
    }
    ~SimpleMessage(void)
    {
    }

    void write(std::vector<uint8_t> &buf) const
    {
        writeNoData(buf);
    }
};

typedef SimpleMessage<MESSAGETYPE_STOP_EXECUTION> StopExecutionMessage;
typedef SimpleMessage<MESSAGETYPE_PROCESS_STARTED> ProcessStartedMessage;
typedef SimpleMessage<MESSAGETYPE_KEEPALIVE> KeepAliveMessage;

class HelloMessage : public Message
{
public:
    int version;

    HelloMessage(const uint8_t *data, size_t dataSize);
    HelloMessage(void) : Message(MESSAGETYPE_HELLO), version(PROTOCOL_VERSION)
    {
    }
    ~HelloMessage(void)
    {
    }

    void write(std::vector<uint8_t> &buf) const;
};

class ExecuteBinaryMessage : public Message
{
public:
    std::string name;
    std::string params;
    std::string workDir;
    std::string caseList;

    ExecuteBinaryMessage(const uint8_t *data, size_t dataSize);
    ExecuteBinaryMessage(void) : Message(MESSAGETYPE_EXECUTE_BINARY)
    {
    }
    ~ExecuteBinaryMessage(void)
    {
    }

    void write(std::vector<uint8_t> &buf) const;
};

class ProcessLogDataMessage : public Message
{
public:
    std::string logData;

    ProcessLogDataMessage(const uint8_t *data, size_t dataSize);
    ~ProcessLogDataMessage(void)
    {
    }

    void write(std::vector<uint8_t> &buf) const;
};

class ProcessLaunchFailedMessage : public Message
{
public:
    std::string reason;

    ProcessLaunchFailedMessage(const uint8_t *data, size_t dataSize);
    ProcessLaunchFailedMessage(const char *reason_) : Message(MESSAGETYPE_PROCESS_LAUNCH_FAILED), reason(reason_)
    {
    }
    ~ProcessLaunchFailedMessage(void)
    {
    }

    void write(std::vector<uint8_t> &buf) const;
};

class ProcessFinishedMessage : public Message
{
public:
    int exitCode;

    ProcessFinishedMessage(const uint8_t *data, size_t dataSize);
    ProcessFinishedMessage(int exitCode_) : Message(MESSAGETYPE_PROCESS_FINISHED), exitCode(exitCode_)
    {
    }
    ~ProcessFinishedMessage(void)
    {
    }

    void write(std::vector<uint8_t> &buf) const;
};

class InfoMessage : public Message
{
public:
    std::string info;

    InfoMessage(const uint8_t *data, size_t dataSize);
    ~InfoMessage(void)
    {
    }

    void write(std::vector<uint8_t> &buf) const;
};

// For debug purposes only.
class TestMessage : public Message
{
public:
    std::string test;

    TestMessage(const uint8_t *data, size_t dataSize);
    ~TestMessage(void)
    {
    }

    void write(std::vector<uint8_t> &buf) const;
};

} // namespace xs

#endif // _XSPROTOCOL_HPP
