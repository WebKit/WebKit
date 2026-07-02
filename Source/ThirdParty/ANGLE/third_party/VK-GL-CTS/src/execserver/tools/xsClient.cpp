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
 * \brief ExecServer Client.
 *//*--------------------------------------------------------------------*/

#include "xsDefs.hpp"
#include "xsProtocol.hpp"
#include "deSocket.hpp"
#include "deUniquePtr.hpp"

#include "deString.h"

#include <memory>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstdlib>

using std::string;
using std::vector;

namespace xs
{

typedef de::UniquePtr<Message> ScopedMsgPtr;

class SocketError : public Error
{
public:
    SocketError(deSocketResult result, const char *message, const char *file, int line)
        : Error(message, deGetSocketResultName(result), file, line)
        , m_result(result)
    {
    }

    deSocketResult getResult(void) const
    {
        return m_result;
    }

private:
    deSocketResult m_result;
};

// Helpers.
void sendMessage(de::Socket &socket, const Message &message)
{
    // Format message.
    vector<uint8_t> buf;
    message.write(buf);

    // Write to socket.
    size_t pos = 0;
    while (pos < buf.size())
    {
        size_t numLeft        = buf.size() - pos;
        size_t numSent        = 0;
        deSocketResult result = socket.send(&buf[pos], numLeft, &numSent);

        if (result != DE_SOCKETRESULT_SUCCESS)
            throw SocketError(result, "send() failed", __FILE__, __LINE__);

        pos += numSent;
    }
}

void readBytes(de::Socket &socket, vector<uint8_t> &dst, size_t numBytes)
{
    size_t numRead = 0;
    dst.resize(numBytes);
    while (numRead < numBytes)
    {
        size_t numLeft        = numBytes - numRead;
        size_t curNumRead     = 0;
        deSocketResult result = socket.receive(&dst[numRead], numLeft, &curNumRead);

        if (result != DE_SOCKETRESULT_SUCCESS)
            throw SocketError(result, "receive() failed", __FILE__, __LINE__);

        numRead += curNumRead;
    }
}

Message *readMessage(de::Socket &socket)
{
    // Header.
    vector<uint8_t> header;
    readBytes(socket, header, MESSAGE_HEADER_SIZE);

    MessageType type;
    size_t messageSize;
    Message::parseHeader(&header[0], (int)header.size(), type, messageSize);

    // Simple messages without any data.
    switch (type)
    {
    case MESSAGETYPE_KEEPALIVE:
        return new KeepAliveMessage();
    case MESSAGETYPE_PROCESS_STARTED:
        return new ProcessStartedMessage();
    default:
        break; // Read message with data.
    }

    vector<uint8_t> messageBuf;
    readBytes(socket, messageBuf, messageSize - MESSAGE_HEADER_SIZE);

    switch (type)
    {
    case MESSAGETYPE_HELLO:
        return new HelloMessage(&messageBuf[0], (int)messageBuf.size());
    case MESSAGETYPE_TEST:
        return new TestMessage(&messageBuf[0], (int)messageBuf.size());
    case MESSAGETYPE_PROCESS_LOG_DATA:
        return new ProcessLogDataMessage(&messageBuf[0], (int)messageBuf.size());
    case MESSAGETYPE_INFO:
        return new InfoMessage(&messageBuf[0], (int)messageBuf.size());
    case MESSAGETYPE_PROCESS_LAUNCH_FAILED:
        return new ProcessLaunchFailedMessage(&messageBuf[0], (int)messageBuf.size());
    case MESSAGETYPE_PROCESS_FINISHED:
        return new ProcessFinishedMessage(&messageBuf[0], (int)messageBuf.size());
    default:
        XS_FAIL("Unknown message");
    }
}

class CommandLine
{
public:
    de::SocketAddress address;
    std::string program;
    std::string params;
    std::string workingDir;
    std::string caseList;
    std::string dstFileName;
};

class Client
{
public:
    Client(const CommandLine &cmdLine);
    ~Client(void);

    void run(void);

private:
    const CommandLine &m_cmdLine;
    de::Socket m_socket;
};

Client::Client(const CommandLine &cmdLine) : m_cmdLine(cmdLine)
{
}

Client::~Client(void)
{
}

void Client::run(void)
{
    // Connect to server.
    m_socket.connect(m_cmdLine.address);

    printf("Connected to %s:%d!\n", m_cmdLine.address.getHost(), m_cmdLine.address.getPort());

    // Open result file.
    std::fstream out(m_cmdLine.dstFileName.c_str(), std::fstream::out | std::fstream::binary);

    printf("  writing to %s\n", m_cmdLine.dstFileName.c_str());

    // Send execution request.
    {
        ExecuteBinaryMessage msg;

        msg.name     = m_cmdLine.program;
        msg.params   = m_cmdLine.params;
        msg.workDir  = m_cmdLine.workingDir;
        msg.caseList = m_cmdLine.caseList;

        sendMessage(m_socket, msg);
        printf("  execution request sent.\n");
    }

    // Run client loop.
    bool isRunning = true;
    while (isRunning)
    {
        ScopedMsgPtr msg(readMessage(m_socket));

        switch (msg->type)
        {
        case MESSAGETYPE_HELLO:
            printf("  HelloMessage\n");
            break;

        case MESSAGETYPE_KEEPALIVE:
        {
            printf("  KeepAliveMessage\n");

            // Reply with keepalive.
            sendMessage(m_socket, KeepAliveMessage());
            break;
        }

        case MESSAGETYPE_INFO:
            printf("  InfoMessage: '%s'\n", static_cast<InfoMessage *>(msg.get())->info.c_str());
            break;

        case MESSAGETYPE_PROCESS_STARTED:
            printf("  ProcessStartedMessage\n");
            break;

        case MESSAGETYPE_PROCESS_FINISHED:
            printf("  ProcessFinished: exit code = %d\n", static_cast<ProcessFinishedMessage *>(msg.get())->exitCode);
            isRunning = false;
            break;

        case MESSAGETYPE_PROCESS_LAUNCH_FAILED:
            printf("  ProcessLaunchFailed: '%s'\n",
                   static_cast<ProcessLaunchFailedMessage *>(msg.get())->reason.c_str());
            isRunning = false;
            break;

        case MESSAGETYPE_PROCESS_LOG_DATA:
        {
            ProcessLogDataMessage *logDataMsg = static_cast<ProcessLogDataMessage *>(msg.get());
            printf("  ProcessLogDataMessage: %d bytes\n", (int)logDataMsg->logData.length());
            out << logDataMsg->logData;
            break;
        }

        default:
            XS_FAIL("Unknown message");
        }
    }

    // Close output file.
    out.close();

    // Close connection.
    m_socket.shutdown();
    m_socket.close();

    printf("Done!\n");
}

string parseString(const char *str)
{
    if (str[0] == '\'' || str[0] == '"')
    {
        const char *p = str;
        char endChar  = *p++;
        std::ostringstream o;

        while (*p != endChar && *p)
        {
            if (*p == '\\')
            {
                switch (p[1])
                {
                case 0:
                    DE_ASSERT(false);
                    break;
                case 'n':
                    o << '\n';
                    break;
                case 't':
                    o << '\t';
                    break;
                default:
                    o << p[1];
                    break;
                }

                p += 2;
            }
            else
                o << *p++;
        }

        return o.str();
    }
    else
        return string(str);
}

void printHelp(const char *binName)
{
    printf("%s:\n", binName);
    printf("  --host=[host]          Connect to host [host]\n");
    printf("  --port=[name]          Use port [port]\n");
    printf("  --program=[program]    Test program\n");
    printf("  --params=[params]      Test program params\n");
    printf("  --workdir=[dir]        Working directory\n");
    printf("  --caselist=[caselist]  Test case list\n");
    printf("  --out=filename         Test result file\n");
}

int runClient(int argc, const char *const *argv)
{
    CommandLine cmdLine;

    // Defaults.
    cmdLine.address.setHost("127.0.0.1");
    cmdLine.address.setPort(50016);
    cmdLine.dstFileName = "TestResults.qpa";

    // Parse command line.
    for (int argNdx = 1; argNdx < argc; argNdx++)
    {
        const char *arg = argv[argNdx];

        if (deStringBeginsWith(arg, "--port="))
            cmdLine.address.setPort(atoi(arg + 7));
        else if (deStringBeginsWith(arg, "--host="))
            cmdLine.address.setHost(parseString(arg + 7).c_str());
        else if (deStringBeginsWith(arg, "--program="))
            cmdLine.program = parseString(arg + 10);
        else if (deStringBeginsWith(arg, "--params="))
            cmdLine.params = parseString(arg + 9);
        else if (deStringBeginsWith(arg, "--workdir="))
            cmdLine.workingDir = parseString(arg + 10);
        else if (deStringBeginsWith(arg, "--caselist="))
            cmdLine.caseList = parseString(arg + 11);
        else if (deStringBeginsWith(arg, "--out="))
            cmdLine.dstFileName = parseString(arg + 6);
        else
        {
            printHelp(argv[0]);
            return -1;
        }
    }

    // Run client.
    try
    {
        Client client(cmdLine);
        client.run();
    }
    catch (const std::exception &e)
    {
        printf("%s\n", e.what());
        return -1;
    }

    return 0;
}

} // namespace xs

int main(int argc, const char *const *argv)
{
    return xs::runClient(argc, argv);
}
