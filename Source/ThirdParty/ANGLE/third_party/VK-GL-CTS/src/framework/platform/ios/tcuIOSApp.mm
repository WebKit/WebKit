/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief iOS App Wrapper.
 *//*--------------------------------------------------------------------*/

#include "tcuIOSApp.h"
#include "deClock.h"
#include "deFilePath.hpp"
#include "deMemory.h"
#include "deMutex.hpp"
#include "deThread.hpp"
#include "tcuApp.hpp"
#include "tcuCommandLine.hpp"
#include "tcuIOSPlatform.hh"
#include "tcuRenderTarget.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "xsExecutionServer.hpp"
#include "xsPosixFileReader.hpp"
#include "xsTestProcess.hpp"

#include <string>

#import <Foundation/NSBundle.h>
#import <Foundation/NSObject.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSString.h>

using std::string;

namespace
{

class TestThreadState
{
  public:
    enum State
    {
        STATE_NOT_RUNNING = 0,
        STATE_RUNNING,
        STATE_STOP_REQUESTED,

        STATE_LAST
    };

    TestThreadState(void);
    ~TestThreadState(void);

    void requestStart(const char *cmdLine);
    void requestStop(void);
    State getState(void);

    void testExecFinished(void);

    const char *getCommandLine(void) const { return m_cmdLine.c_str(); }

  private:
    de::Mutex m_lock;

    State m_state;
    std::string m_cmdLine;
};

TestThreadState::TestThreadState(void) : m_state(STATE_NOT_RUNNING) {}

TestThreadState::~TestThreadState(void) {}

void TestThreadState::requestStart(const char *cmdLine)
{
    de::ScopedLock stateLock(m_lock);

    TCU_CHECK(m_state == STATE_NOT_RUNNING);

    m_cmdLine = cmdLine;
    m_state = STATE_RUNNING;
}

void TestThreadState::requestStop(void)
{
    de::ScopedLock stateLock(m_lock);

    if (m_state != STATE_NOT_RUNNING)
        m_state = STATE_STOP_REQUESTED;
}

void TestThreadState::testExecFinished(void)
{
    de::ScopedLock stateLock(m_lock);
    m_state = STATE_NOT_RUNNING;
}

TestThreadState::State TestThreadState::getState(void)
{
    de::ScopedLock stateLock(m_lock);
    return m_state;
}

class LocalTestProcess : public xs::TestProcess
{
  public:
    LocalTestProcess(TestThreadState &state, const char *logFileName);
    ~LocalTestProcess(void);

    void start(const char *name, const char *params, const char *workingDir,
               const char *caseList);
    void terminate(void);
    void cleanup(void);

    bool isRunning(void);
    int getExitCode(void) const { return 0; /* not available */ }

    int readInfoLog(uint8_t *dst, int numBytes)
    {
        DE_UNREF(dst && numBytes);
        return 0; /* not supported */
    }
    int readTestLog(uint8_t *dst, int numBytes);

    const char *getLogFileName(void) const { return m_logFileName.c_str(); }

  private:
    TestThreadState &m_state;
    string m_logFileName;
    xs::posix::FileReader m_logReader;
    uint64_t m_processStartTime;
};

LocalTestProcess::LocalTestProcess(TestThreadState &state,
                                   const char *logFileName)
    : m_state(state), m_logFileName(logFileName),
      m_logReader(xs::LOG_BUFFER_BLOCK_SIZE, xs::LOG_BUFFER_NUM_BLOCKS),
      m_processStartTime(0)
{
}

LocalTestProcess::~LocalTestProcess(void) {}

void LocalTestProcess::start(const char *name, const char *params,
                             const char *workingDir, const char *caseList)
{
    DE_UNREF(name && workingDir);

    // Delete old log file.
    if (deFileExists(m_logFileName.c_str()))
        TCU_CHECK(deDeleteFile(m_logFileName.c_str()));

    string cmdLine = string("deqp");
    if (caseList && strlen(caseList) > 0)
        cmdLine += string(" --deqp-caselist=") + caseList;

    if (params && strlen(params) > 0)
        cmdLine += string(" ") + params;

    m_state.requestStart(cmdLine.c_str());
    m_processStartTime = deGetMicroseconds();
}

void LocalTestProcess::terminate(void) { m_state.requestStop(); }

void LocalTestProcess::cleanup(void)
{
    if (isRunning())
    {
        m_state.requestStop();

        // Wait until stopped.
        while (isRunning())
            deSleep(50);
    }

    m_logReader.stop();
}

bool LocalTestProcess::isRunning(void)
{
    return m_state.getState() != TestThreadState::STATE_NOT_RUNNING;
}

int LocalTestProcess::readTestLog(uint8_t *dst, int numBytes)
{
    if (!m_logReader.isRunning())
    {
        if (deGetMicroseconds() - m_processStartTime >
            xs::LOG_FILE_TIMEOUT * 1000)
        {
            // Timeout, kill execution.
            terminate();
            return 0; // \todo [2013-08-13 pyry] Throw exception?
        }

        if (!deFileExists(m_logFileName.c_str()))
            return 0;

        // Start reader.
        m_logReader.start(m_logFileName.c_str());
    }

    DE_ASSERT(m_logReader.isRunning());
    return m_logReader.read(dst, numBytes);
}

class ServerThread : public de::Thread
{
  public:
    ServerThread(xs::TestProcess *testProcess, int port);
    ~ServerThread(void);

    void run(void);
    void stop(void);

  private:
    xs::ExecutionServer m_server;
    bool m_isRunning;
};

ServerThread::ServerThread(xs::TestProcess *testProcess, int port)
    : m_server(testProcess, DE_SOCKETFAMILY_INET4, port,
               xs::ExecutionServer::RUNMODE_FOREVER),
      m_isRunning(false)
{
}

ServerThread::~ServerThread(void) { stop(); }

void ServerThread::run(void)
{
    m_isRunning = true;
    m_server.runServer();
}

void ServerThread::stop(void)
{
    if (m_isRunning)
    {
        m_server.stopServer();
        join();
        m_isRunning = false;
    }
}

string getAppBundleDir(void)
{
    NSString *dataPath = [[NSBundle mainBundle] bundlePath];
    const char *utf8Str = [dataPath UTF8String];

    return string(utf8Str);
}

string getAppDocumentsDir(void)
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                         NSUserDomainMask, YES);
    NSString *docPath = [paths objectAtIndex:0];
    const char *utf8Str = [docPath UTF8String];

    return string(utf8Str);
}

} // namespace

struct tcuIOSApp_s
{
  public:
    tcuIOSApp_s(void *view);
    ~tcuIOSApp_s(void);

    void iterate(void);

  protected:
    void createTestApp(void);
    void destroyTestApp(void);

    TestThreadState m_state;
    LocalTestProcess m_testProcess;
    ServerThread m_server;

    tcu::DirArchive m_archive;
    tcu::ios::ScreenManager m_screenManager;
    tcu::ios::Platform m_platform;

    tcu::TestLog *m_log;
    tcu::CommandLine *m_cmdLine;
    tcu::App *m_app;
};

tcuIOSApp_s::tcuIOSApp_s(void *view)
    : m_testProcess(m_state,
                    de::FilePath::join(getAppDocumentsDir(), "TestResults.qpa")
                        .getPath()),
      m_server(&m_testProcess, 50016), m_archive(getAppBundleDir().c_str()),
      m_screenManager((tcuEAGLView *)view), m_platform(&m_screenManager),
      m_log(nullptr), m_cmdLine(nullptr), m_app(nullptr)
{
    // Start server.
    m_server.start();
}

tcuIOSApp_s::~tcuIOSApp_s(void)
{
    m_server.stop();
    destroyTestApp();
}

void tcuIOSApp::createTestApp(void)
{
    DE_ASSERT(!m_app && !m_log && !m_cmdLine && !m_platform);

    try
    {
        m_log = new tcu::TestLog(m_testProcess.getLogFileName());
        m_cmdLine = new tcu::CommandLine(m_state.getCommandLine());
        m_app = new tcu::App(m_platform, m_archive, *m_log, *m_cmdLine);
    }
    catch (const std::exception &e)
    {
        destroyTestApp();
        tcu::die("%s", e.what());
    }
}

void tcuIOSApp::destroyTestApp(void)
{
    delete m_app;
    delete m_cmdLine;
    delete m_log;
    m_app = nullptr;
    m_cmdLine = nullptr;
    m_log = nullptr;
}

void tcuIOSApp::iterate(void)
{
    TestThreadState::State curState = m_state.getState();

    if (curState == TestThreadState::STATE_RUNNING)
    {
        if (!m_app)
            createTestApp();

        TCU_CHECK(m_app);

        if (!m_app->iterate())
        {
            destroyTestApp();
            m_state.testExecFinished();
        }
    }
    else if (curState == TestThreadState::STATE_STOP_REQUESTED)
    {
        destroyTestApp();
        m_state.testExecFinished();
    }
    // else wait until state has changed?
}

tcuIOSApp *tcuIOSApp_create(void *view)
{
    try
    {
        return new tcuIOSApp(view);
    }
    catch (const std::exception &e)
    {
        tcu::die("FATAL ERROR: %s", e.what());
        return nullptr;
    }
}

void tcuIOSApp_destroy(tcuIOSApp *app) { delete app; }

bool tcuIOSApp_iterate(tcuIOSApp *app)
{
    try
    {
        app->iterate();
        return true;
    }
    catch (const std::exception &e)
    {
        tcu::print("FATAL ERROR: %s\n", e.what());
        return false;
    }
}
