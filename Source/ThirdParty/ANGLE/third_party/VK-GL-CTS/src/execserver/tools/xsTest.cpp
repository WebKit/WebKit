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
 * \brief ExecServer Tests.
 *//*--------------------------------------------------------------------*/

#include "xsDefs.hpp"

#include "xsProtocol.hpp"
#include "deSocket.hpp"
#include "deRingBuffer.hpp"
#include "deFilePath.hpp"
#include "deBlockBuffer.hpp"
#include "deThread.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include "deClock.h"
#include "deProcess.h"
#include "deString.h"
#include "deRandom.h"

#include <memory>
#include <algorithm>

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

class TestClock
{
public:
    inline TestClock(void)
    {
        reset();
    }

    inline void reset(void)
    {
        m_initTime = deGetMicroseconds();
    }

    inline int getMilliseconds(void)
    {
        return (int)((deGetMicroseconds() - m_initTime) / 1000);
    }

private:
    uint64_t m_initTime;
};

class TestContext
{
public:
    TestContext(void) : startServer(false)
    {
    }

    std::string serverPath;
    std::string testerPath;
    de::SocketAddress address;
    bool startServer;

    // Passed from execserver.
    std::string logFileName;
    std::string caseList;

private:
    TestContext(const TestContext &other);
    TestContext &operator=(const TestContext &other);
};

class TestCase
{
public:
    TestCase(TestContext &testCtx, const char *name) : m_testCtx(testCtx), m_name(name)
    {
    }
    virtual ~TestCase(void)
    {
    }

    const char *getName(void) const
    {
        return m_name.c_str();
    }

    virtual void runClient(de::Socket &socket) = 0;
    virtual void runProgram(void)              = 0;

protected:
    TestContext &m_testCtx;
    std::string m_name;
};

class TestExecutor
{
public:
    TestExecutor(TestContext &testCtx);
    ~TestExecutor(void);

    void runCases(const std::vector<TestCase *> &testCases);
    bool runCase(TestCase *testCase);

private:
    TestContext &m_testCtx;
};

TestExecutor::TestExecutor(TestContext &testCtx) : m_testCtx(testCtx)
{
}

TestExecutor::~TestExecutor(void)
{
}

void TestExecutor::runCases(const std::vector<TestCase *> &testCases)
{
    int numPassed = 0;
    int numCases  = (int)testCases.size();

    for (std::vector<TestCase *>::const_iterator i = testCases.begin(); i != testCases.end(); i++)
    {
        if (runCase(*i))
            numPassed += 1;
    }

    printf("\n  %d/%d passed!\n", numPassed, numCases);
}

class FilePrinter : public de::Thread
{
public:
    FilePrinter(void) : m_curFile(nullptr)
    {
    }

    void start(deFile *file)
    {
        DE_ASSERT(!m_curFile);
        m_curFile = file;
        de::Thread::start();
    }

    void run(void)
    {
        char buf[256];
        int64_t numRead = 0;

        while (deFile_read(m_curFile, &buf[0], (int64_t)sizeof(buf), &numRead) == DE_FILERESULT_SUCCESS)
            fwrite(&buf[0], 1, (size_t)numRead, stdout);

        m_curFile = nullptr;
    }

private:
    deFile *m_curFile;
};

bool TestExecutor::runCase(TestCase *testCase)
{
    printf("%s\n", testCase->getName());

    bool success          = false;
    deProcess *serverProc = nullptr;
    FilePrinter stdoutPrinter;
    FilePrinter stderrPrinter;

    try
    {
        if (m_testCtx.startServer)
        {
            string cmdLine = m_testCtx.serverPath + " --port=" + de::toString(m_testCtx.address.getPort());
            serverProc     = deProcess_create();
            XS_CHECK(serverProc);

            if (!deProcess_start(serverProc, cmdLine.c_str(), nullptr))
            {
                string errMsg = deProcess_getLastError(serverProc);
                deProcess_destroy(serverProc);
                XS_FAIL(errMsg.c_str());
            }

            deSleep(200); /* Give 200ms for server to start. */
            XS_CHECK(deProcess_isRunning(serverProc));

            // Start stdout/stderr printers.
            stdoutPrinter.start(deProcess_getStdOut(serverProc));
            stderrPrinter.start(deProcess_getStdErr(serverProc));
        }

        // Connect.
        de::Socket socket;
        socket.connect(m_testCtx.address);

        // Flags.
        socket.setFlags(DE_SOCKET_CLOSE_ON_EXEC);

        // Run case.
        testCase->runClient(socket);

        // Disconnect.
        if (socket.isConnected())
            socket.shutdown();

        // Kill server.
        if (serverProc && deProcess_isRunning(serverProc))
        {
            XS_CHECK(deProcess_terminate(serverProc));
            deSleep(100);
            XS_CHECK(deProcess_waitForFinish(serverProc));

            stdoutPrinter.join();
            stderrPrinter.join();
        }

        success = true;
    }
    catch (const std::exception &e)
    {
        printf("FAIL: %s\n\n", e.what());
    }

    if (serverProc)
        deProcess_destroy(serverProc);

    return success;
}

class ConnectTest : public TestCase
{
public:
    ConnectTest(TestContext &testCtx) : TestCase(testCtx, "connect")
    {
    }

    void runClient(de::Socket &socket)
    {
        DE_UNREF(socket);
    }

    void runProgram(void)
    { /* nothing */
    }
};

class HelloTest : public TestCase
{
public:
    HelloTest(TestContext &testCtx) : TestCase(testCtx, "hello")
    {
    }

    void runClient(de::Socket &socket)
    {
        xs::HelloMessage msg;
        sendMessage(socket, (const xs::Message &)msg);
    }

    void runProgram(void)
    { /* nothing */
    }
};

class ExecFailTest : public TestCase
{
public:
    ExecFailTest(TestContext &testCtx) : TestCase(testCtx, "exec-fail")
    {
    }

    void runClient(de::Socket &socket)
    {
        xs::ExecuteBinaryMessage execMsg;
        execMsg.name     = "foobar-notfound";
        execMsg.params   = "";
        execMsg.caseList = "";
        execMsg.workDir  = "";

        sendMessage(socket, execMsg);

        const int timeout = 100; // 100ms.
        TestClock clock;

        for (;;)
        {
            if (clock.getMilliseconds() > timeout)
                XS_FAIL("Didn't receive PROCESS_LAUNCH_FAILED");

            ScopedMsgPtr msg(readMessage(socket));

            if (msg->type == MESSAGETYPE_PROCESS_LAUNCH_FAILED)
                break;
            else if (msg->type == MESSAGETYPE_KEEPALIVE)
                continue;
            else
                XS_FAIL("Invalid message");
        }
    }

    void runProgram(void)
    { /* nothing */
    }
};

class SimpleExecTest : public TestCase
{
public:
    SimpleExecTest(TestContext &testCtx) : TestCase(testCtx, "simple-exec")
    {
    }

    void runClient(de::Socket &socket)
    {
        xs::ExecuteBinaryMessage execMsg;
        execMsg.name     = m_testCtx.testerPath;
        execMsg.params   = "--program=simple-exec";
        execMsg.caseList = "";
        execMsg.workDir  = "";

        sendMessage(socket, execMsg);

        const int timeout = 5000; // 5s.
        TestClock clock;

        bool gotProcessStarted  = false;
        bool gotProcessFinished = false;

        for (;;)
        {
            if (clock.getMilliseconds() > timeout)
                break;

            ScopedMsgPtr msg(readMessage(socket));

            if (msg->type == MESSAGETYPE_PROCESS_STARTED)
                gotProcessStarted = true;
            else if (msg->type == MESSAGETYPE_PROCESS_LAUNCH_FAILED)
                XS_FAIL("Got PROCESS_LAUNCH_FAILED");
            else if (gotProcessStarted && msg->type == MESSAGETYPE_PROCESS_FINISHED)
            {
                gotProcessFinished = true;
                break;
            }
            else if (msg->type == MESSAGETYPE_KEEPALIVE || msg->type == MESSAGETYPE_INFO)
                continue;
            else
                XS_FAIL((string("Invalid message: ") + de::toString(msg->type)).c_str());
        }

        if (!gotProcessStarted)
            XS_FAIL("Did't get PROCESS_STARTED message");

        if (!gotProcessFinished)
            XS_FAIL("Did't get PROCESS_FINISHED message");
    }

    void runProgram(void)
    { /* print nothing. */
    }
};

class InfoTest : public TestCase
{
public:
    std::string infoStr;

    InfoTest(TestContext &testCtx) : TestCase(testCtx, "info"), infoStr("Hello, World")
    {
    }

    void runClient(de::Socket &socket)
    {
        xs::ExecuteBinaryMessage execMsg;
        execMsg.name     = m_testCtx.testerPath;
        execMsg.params   = "--program=info";
        execMsg.caseList = "";
        execMsg.workDir  = "";

        sendMessage(socket, execMsg);

        const int timeout = 10000; // 10s.
        TestClock clock;

        bool gotProcessStarted   = false;
        bool gotProcessFinished  = false;
        std::string receivedInfo = "";

        for (;;)
        {
            if (clock.getMilliseconds() > timeout)
                break;

            ScopedMsgPtr msg(readMessage(socket));

            if (msg->type == MESSAGETYPE_PROCESS_STARTED)
                gotProcessStarted = true;
            else if (msg->type == MESSAGETYPE_PROCESS_LAUNCH_FAILED)
                XS_FAIL("Got PROCESS_LAUNCH_FAILED");
            else if (gotProcessStarted && msg->type == MESSAGETYPE_INFO)
                receivedInfo += static_cast<const InfoMessage *>(msg.get())->info;
            else if (gotProcessStarted && msg->type == MESSAGETYPE_PROCESS_FINISHED)
            {
                gotProcessFinished = true;
                break;
            }
            else if (msg->type == MESSAGETYPE_KEEPALIVE)
                continue;
            else
                XS_FAIL("Invalid message");
        }

        if (!gotProcessStarted)
            XS_FAIL("Did't get PROCESS_STARTED message");

        if (!gotProcessFinished)
            XS_FAIL("Did't get PROCESS_FINISHED message");

        if (receivedInfo != infoStr)
            XS_FAIL("Info data doesn't match");
    }

    void runProgram(void)
    {
        printf("%s", infoStr.c_str());
    }
};

class LogDataTest : public TestCase
{
public:
    LogDataTest(TestContext &testCtx) : TestCase(testCtx, "logdata")
    {
    }

    void runClient(de::Socket &socket)
    {
        xs::ExecuteBinaryMessage execMsg;
        execMsg.name     = m_testCtx.testerPath;
        execMsg.params   = "--program=logdata";
        execMsg.caseList = "";
        execMsg.workDir  = "";

        sendMessage(socket, execMsg);

        const int timeout = 10000; // 10s.
        TestClock clock;

        bool gotProcessStarted   = false;
        bool gotProcessFinished  = false;
        std::string receivedData = "";

        for (;;)
        {
            if (clock.getMilliseconds() > timeout)
                break;

            ScopedMsgPtr msg(readMessage(socket));

            if (msg->type == MESSAGETYPE_PROCESS_STARTED)
                gotProcessStarted = true;
            else if (msg->type == MESSAGETYPE_PROCESS_LAUNCH_FAILED)
                XS_FAIL("Got PROCESS_LAUNCH_FAILED");
            else if (gotProcessStarted && msg->type == MESSAGETYPE_PROCESS_LOG_DATA)
                receivedData += static_cast<const ProcessLogDataMessage *>(msg.get())->logData;
            else if (gotProcessStarted && msg->type == MESSAGETYPE_PROCESS_FINISHED)
            {
                gotProcessFinished = true;
                break;
            }
            else if (msg->type == MESSAGETYPE_KEEPALIVE)
                continue;
            else if (msg->type == MESSAGETYPE_INFO)
                XS_FAIL(static_cast<const InfoMessage *>(msg.get())->info.c_str());
            else
                XS_FAIL("Invalid message");
        }

        if (!gotProcessStarted)
            XS_FAIL("Did't get PROCESS_STARTED message");

        if (!gotProcessFinished)
            XS_FAIL("Did't get PROCESS_FINISHED message");

        const char *expected = "Foo\nBar\n";
        if (receivedData != expected)
        {
            printf("  received: '%s'\n  expected: '%s'\n", receivedData.c_str(), expected);
            XS_FAIL("Log data doesn't match");
        }
    }

    void runProgram(void)
    {
        deFile *file = deFile_create(m_testCtx.logFileName.c_str(),
                                     DE_FILEMODE_OPEN | DE_FILEMODE_CREATE | DE_FILEMODE_TRUNCATE | DE_FILEMODE_WRITE);
        XS_CHECK(file);

        const char line0[] = "Foo\n";
        const char line1[] = "Bar\n";
        int64_t numWritten = 0;

        // Write first line.
        XS_CHECK(deFile_write(file, line0, sizeof(line0) - 1, &numWritten) == DE_FILERESULT_SUCCESS);
        XS_CHECK(numWritten == sizeof(line0) - 1);

        // Sleep for 0.5s and write line 2.
        deSleep(500);
        XS_CHECK(deFile_write(file, line1, sizeof(line1) - 1, &numWritten) == DE_FILERESULT_SUCCESS);
        XS_CHECK(numWritten == sizeof(line1) - 1);

        deFile_destroy(file);
    }
};

class BigLogDataTest : public TestCase
{
public:
    enum
    {
        DATA_SIZE = 100 * 1024 * 1024
    };

    BigLogDataTest(TestContext &testCtx) : TestCase(testCtx, "biglogdata")
    {
    }

    void runClient(de::Socket &socket)
    {
        xs::ExecuteBinaryMessage execMsg;
        execMsg.name     = m_testCtx.testerPath;
        execMsg.params   = "--program=biglogdata";
        execMsg.caseList = "";
        execMsg.workDir  = "";

        sendMessage(socket, execMsg);

        const int timeout = 30000; // 30s.
        TestClock clock;

        bool gotProcessStarted  = false;
        bool gotProcessFinished = false;
        int receivedBytes       = 0;

        for (;;)
        {
            if (clock.getMilliseconds() > timeout)
                break;

            ScopedMsgPtr msg(readMessage(socket));

            if (msg->type == MESSAGETYPE_PROCESS_STARTED)
                gotProcessStarted = true;
            else if (msg->type == MESSAGETYPE_PROCESS_LAUNCH_FAILED)
                XS_FAIL("Got PROCESS_LAUNCH_FAILED");
            else if (gotProcessStarted && msg->type == MESSAGETYPE_PROCESS_LOG_DATA)
                receivedBytes += (int)static_cast<const ProcessLogDataMessage *>(msg.get())->logData.length();
            else if (gotProcessStarted && msg->type == MESSAGETYPE_PROCESS_FINISHED)
            {
                gotProcessFinished = true;
                break;
            }
            else if (msg->type == MESSAGETYPE_KEEPALIVE)
            {
                // Reply with keepalive.
                sendMessage(socket, KeepAliveMessage());
                continue;
            }
            else if (msg->type == MESSAGETYPE_INFO)
                printf("%s", static_cast<const InfoMessage *>(msg.get())->info.c_str());
            else
                XS_FAIL("Invalid message");
        }

        if (!gotProcessStarted)
            XS_FAIL("Did't get PROCESS_STARTED message");

        if (!gotProcessFinished)
            XS_FAIL("Did't get PROCESS_FINISHED message");

        if (receivedBytes != DATA_SIZE)
        {
            printf("  received: %d bytes\n  expected: %d bytes\n", receivedBytes, DATA_SIZE);
            XS_FAIL("Log data size doesn't match");
        }

        int timeMs = clock.getMilliseconds();
        printf("  Streamed %d bytes in %d ms: %.2f MiB/s\n", DATA_SIZE, timeMs,
               ((float)DATA_SIZE / (float)(1024 * 1024)) / ((float)timeMs / 1000.0f));
    }

    void runProgram(void)
    {
        deFile *file = deFile_create(m_testCtx.logFileName.c_str(),
                                     DE_FILEMODE_OPEN | DE_FILEMODE_CREATE | DE_FILEMODE_TRUNCATE | DE_FILEMODE_WRITE);
        XS_CHECK(file);

        uint8_t tmpBuf[1024 * 16];
        int numWritten = 0;

        deMemset(&tmpBuf, 'a', sizeof(tmpBuf));

        while (numWritten < DATA_SIZE)
        {
            int64_t numWrittenInBatch = 0;
            XS_CHECK(deFile_write(file, &tmpBuf[0], de::min((int)sizeof(tmpBuf), DATA_SIZE - numWritten),
                                  &numWrittenInBatch) == DE_FILERESULT_SUCCESS);
            numWritten += (int)numWrittenInBatch;
        }

        deFile_destroy(file);
    }
};

class KeepAliveTest : public TestCase
{
public:
    KeepAliveTest(TestContext &testCtx) : TestCase(testCtx, "keepalive")
    {
    }

    void runClient(de::Socket &socket)
    {
        // In milliseconds.
        const int sendInterval       = 5000;
        const int minReceiveInterval = 10000;
        const int testTime           = 30000;
        const int sleepTime          = 200;
        const int expectedTimeout    = 40000;
        int curTime                  = 0;
        int lastSendTime             = 0;
        int lastReceiveTime          = 0;
        TestClock clock;

        DE_ASSERT(sendInterval < minReceiveInterval);

        curTime = clock.getMilliseconds();

        while (curTime < testTime)
        {
            bool tryGetKeepalive = false;

            if (curTime - lastSendTime > sendInterval)
            {
                printf("  %d ms: sending keepalive\n", curTime);
                sendMessage(socket, KeepAliveMessage());
                curTime         = clock.getMilliseconds();
                lastSendTime    = curTime;
                tryGetKeepalive = true;
            }

            if (tryGetKeepalive)
            {
                // Try to acquire keepalive.
                printf("  %d ms: waiting for keepalive\n", curTime);
                ScopedMsgPtr msg(readMessage(socket));
                int recvTime = clock.getMilliseconds();

                if (msg->type != MESSAGETYPE_KEEPALIVE)
                    XS_FAIL("Got invalid message");

                printf("  %d ms: got keepalive\n", curTime);

                if (recvTime - lastReceiveTime > minReceiveInterval)
                    XS_FAIL("Server doesn't send keepalives");

                lastReceiveTime = recvTime;
            }

            deSleep(sleepTime);
            curTime = clock.getMilliseconds();
        }

        // Verify that server actually kills the connection upon timeout.
        sendMessage(socket, KeepAliveMessage());
        printf("  waiting %d ms for keepalive timeout...\n", expectedTimeout);
        bool isClosed = false;

        try
        {
            // Reset timer.
            clock.reset();
            curTime = clock.getMilliseconds();

            while (curTime < expectedTimeout)
            {
                // Try to get keepalive message.
                ScopedMsgPtr msg(readMessage(socket));
                if (msg->type != MESSAGETYPE_KEEPALIVE)
                    XS_FAIL("Got invalid message");

                curTime = clock.getMilliseconds();
                printf("  %d ms: got keepalive\n", curTime);
            }
        }
        catch (const SocketError &e)
        {
            if (e.getResult() == DE_SOCKETRESULT_CONNECTION_CLOSED)
            {
                printf("  %d ms: server closed connection", clock.getMilliseconds());
                isClosed = true;
            }
            else
                throw;
        }

        if (isClosed)
            printf("  ok!\n");
        else
            XS_FAIL("Server didn't close connection");
    }

    void runProgram(void)
    { /* nothing */
    }
};

void printHelp(const char *binName)
{
    printf("%s:\n", binName);
    printf("  --client=[name]       Run test [name]\n");
    printf("  --program=[name]      Run program for test [name]\n");
    printf("  --host=[host]         Connect to host [host]\n");
    printf("  --port=[name]         Use port [port]\n");
    printf("  --tester-cmd=[cmd]    Launch tester with [cmd]\n");
    printf("  --server-cmd=[cmd]    Launch server with [cmd]\n");
    printf("  --start-server        Start server for test execution\n");
}

struct CompareCaseName
{
    std::string name;

    CompareCaseName(const string &name_) : name(name_)
    {
    }

    bool operator()(const TestCase *testCase) const
    {
        return name == testCase->getName();
    }
};

void runExecServerTests(int argc, const char *const *argv)
{
    // Construct test context.
    TestContext testCtx;

    testCtx.serverPath  = "execserver";
    testCtx.testerPath  = argv[0];
    testCtx.startServer = false;
    testCtx.address.setHost("127.0.0.1");
    testCtx.address.setPort(50016);

    std::string runClient  = "";
    std::string runProgram = "";

    // Parse command line.
    for (int argNdx = 1; argNdx < argc; argNdx++)
    {
        const char *arg = argv[argNdx];

        if (deStringBeginsWith(arg, "--client="))
            runClient = arg + 9;
        else if (deStringBeginsWith(arg, "--program="))
            runProgram = arg + 10;
        else if (deStringBeginsWith(arg, "--port="))
            testCtx.address.setPort(atoi(arg + 7));
        else if (deStringBeginsWith(arg, "--host="))
            testCtx.address.setHost(arg + 7);
        else if (deStringBeginsWith(arg, "--server-cmd="))
            testCtx.serverPath = arg + 13;
        else if (deStringBeginsWith(arg, "--tester-cmd="))
            testCtx.testerPath = arg + 13;
        else if (deStringBeginsWith(arg, "--deqp-log-filename="))
            testCtx.logFileName = arg + 20;
        else if (deStringBeginsWith(arg, "--deqp-caselist="))
            testCtx.caseList = arg + 16;
        else if (strcmp(arg, "--deqp-stdin-caselist") == 0)
        {
            // \todo [pyry] This is rather brute-force solution...
            char c;
            while (fread(&c, 1, 1, stdin) == 1 && c != 0)
                testCtx.caseList += c;
        }
        else if (strcmp(arg, "--start-server") == 0)
            testCtx.startServer = true;
        else
        {
            printHelp(argv[0]);
            return;
        }
    }

    // Test case list.
    std::vector<TestCase *> testCases;
    testCases.push_back(new ConnectTest(testCtx));
    testCases.push_back(new HelloTest(testCtx));
    testCases.push_back(new ExecFailTest(testCtx));
    testCases.push_back(new SimpleExecTest(testCtx));
    testCases.push_back(new InfoTest(testCtx));
    testCases.push_back(new LogDataTest(testCtx));
    testCases.push_back(new KeepAliveTest(testCtx));
    testCases.push_back(new BigLogDataTest(testCtx));

    try
    {
        if (!runClient.empty())
        {
            // Run single case.
            vector<TestCase *>::iterator casePos =
                std::find_if(testCases.begin(), testCases.end(), CompareCaseName(runClient));
            XS_CHECK(casePos != testCases.end());
            TestExecutor executor(testCtx);
            executor.runCase(*casePos);
        }
        else if (!runProgram.empty())
        {
            // Run program part.
            vector<TestCase *>::iterator casePos =
                std::find_if(testCases.begin(), testCases.end(), CompareCaseName(runProgram));
            XS_CHECK(casePos != testCases.end());
            (*casePos)->runProgram();
            fflush(stdout); // Make sure handles are flushed.
            fflush(stderr);
        }
        else
        {
            // Run all tests.
            TestExecutor executor(testCtx);
            executor.runCases(testCases);
        }
    }
    catch (const std::exception &e)
    {
        printf("ERROR: %s\n", e.what());
    }

    // Destroy cases.
    for (std::vector<TestCase *>::const_iterator i = testCases.begin(); i != testCases.end(); i++)
        delete *i;
}

} // namespace xs

#if 0
void testProcFile (void)
{
    /* Test file api. */
    if (deFileExists("test.txt"))
        deDeleteFile("test.txt");
    deFile* file = deFile_create("test.txt", DE_FILEMODE_CREATE|DE_FILEMODE_WRITE);
    const char test[] = "Hello";
    XS_CHECK(deFile_write(file, test, sizeof(test), nullptr) == DE_FILERESULT_SUCCESS);
    deFile_destroy(file);

    /* Read. */
    char buf[10] = { 0 };
    file = deFile_create("test.txt", DE_FILEMODE_OPEN|DE_FILEMODE_READ);
    XS_CHECK(deFile_read(file, buf, sizeof(test), nullptr) == DE_FILERESULT_SUCCESS);
    printf("buf: %s\n", buf);
    deFile_destroy(file);

    /* Process test. */
    deProcess* proc = deProcess_create("ls -lah /Users/pyry", nullptr);
    deFile* out = deProcess_getStdOut(proc);

    int64_t numRead = 0;
    printf("ls:\n");
    while (deFile_read(out, buf, sizeof(buf)-1, &numRead) == DE_FILERESULT_SUCCESS)
    {
        buf[numRead] = 0;
        printf("%s", buf);
    }
    deProcess_destroy(proc);
}
#endif

#if 0
void testBlockingFile (const char* filename)
{
    deRandom    rnd;
    int            dataSize = 1024*1024;
    uint8_t*    data = (uint8_t*)deCalloc(dataSize);
    deFile*        file;

    deRandom_init(&rnd, 0);

    if (deFileExists(filename))
        DE_VERIFY(deDeleteFile(filename));

    /* Fill in with random data. */
    DE_ASSERT(dataSize % sizeof(int) == 0);
    for (int ndx = 0; ndx < (int)(dataSize/sizeof(int)); ndx++)
        ((uint32_t*)data)[ndx] = deRandom_getUint32(&rnd);

    /* Write with random-sized blocks. */
    file = deFile_create(filename, DE_FILEMODE_CREATE|DE_FILEMODE_WRITE);
    DE_VERIFY(file);

    int curPos = 0;
    while (curPos < dataSize)
    {
        int                blockSize = 1 + deRandom_getUint32(&rnd) % (dataSize-curPos);
        int64_t            numWritten = 0;
        deFileResult    result = deFile_write(file, &data[curPos], blockSize, &numWritten);

        DE_VERIFY(result == DE_FILERESULT_SUCCESS);
        DE_VERIFY(numWritten == blockSize);

        curPos += blockSize;
    }

    deFile_destroy(file);

    /* Read and verify file. */
    file = deFile_create(filename, DE_FILEMODE_OPEN|DE_FILEMODE_READ);
    curPos = 0;
    while (curPos < dataSize)
    {
        uint8_t            block[1024];
        int                numToRead = 1 + deRandom_getUint32(&rnd) % deMin(dataSize-curPos, DE_LENGTH_OF_ARRAY(block));
        int64_t            numRead = 0;
        deFileResult    result = deFile_read(file, block, numToRead, &numRead);

        DE_VERIFY(result == DE_FILERESULT_SUCCESS);
        DE_VERIFY((int)numRead == numToRead);
        DE_VERIFY(deMemCmp(block, &data[curPos], numToRead) == 0);

        curPos += numToRead;
    }
    deFile_destroy(file);
}
#endif

int main(int argc, const char *const *argv)
{
    xs::runExecServerTests(argc, argv);
    return 0;
}
