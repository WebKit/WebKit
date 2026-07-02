#ifndef _TCUANDROIDEXECSERVICE_HPP
#define _TCUANDROIDEXECSERVICE_HPP
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
 * \brief Android ExecService.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "deThread.hpp"
#include "xsExecutionServer.hpp"
#include "xsPosixFileReader.hpp"

#include <jni.h>

namespace tcu
{
namespace Android
{

enum
{
    DEFAULT_PORT         = 50016,
    DEFAULT_SOCKETFAMILY = DE_SOCKETFAMILY_INET4
};

class TestProcess : public xs::TestProcess
{
public:
    TestProcess(JavaVM *vm, jobject context);
    ~TestProcess(void);

    virtual void start(const char *name, const char *params, const char *workingDir, const char *caseList);
    virtual void terminate(void);
    virtual void cleanup(void);

    virtual bool isRunning(void);
    virtual int getExitCode(void) const;

    virtual int readTestLog(uint8_t *dst, int numBytes);
    virtual int readInfoLog(uint8_t *dst, int numBytes);

private:
    JNIEnv *getCurrentThreadEnv(void);

    JavaVM *m_vm;
    jclass m_remoteCls;
    jobject m_remote;
    jmethodID m_start;
    jmethodID m_kill;
    jmethodID m_isRunning;

    uint64_t m_launchTime;
    uint64_t m_lastQueryTime;
    bool m_lastRunningStatus;
    xs::posix::FileReader m_logReader;
};

class ExecutionServer : public xs::ExecutionServer
{
public:
    ExecutionServer(JavaVM *vm, xs::TestProcess *testProcess, deSocketFamily family, int port, RunMode runMode);
    xs::ConnectionHandler *createHandler(de::Socket *socket, const de::SocketAddress &clientAddress);

private:
    JavaVM *m_vm;
};

class ConnectionHandler : public xs::ExecutionRequestHandler
{
public:
    ConnectionHandler(JavaVM *vm, xs::ExecutionServer *server, de::Socket *socket);
    void run(void);

private:
    JavaVM *m_vm;
};

class ServerThread : public de::Thread
{
public:
    ServerThread(JavaVM *vm, xs::TestProcess *testProcess, deSocketFamily family, int port);

    void run(void);
    void stop(void);

private:
    ExecutionServer m_server;
};

class ExecService
{
public:
    ExecService(JavaVM *vm, jobject context, int port, deSocketFamily family = (deSocketFamily)DEFAULT_SOCKETFAMILY);
    ~ExecService(void);

    void start(void);
    void stop(void);

private:
    ExecService(const ExecService &other);
    ExecService &operator=(const ExecService &other);

    TestProcess m_process;
    ServerThread m_thread;
};

} // namespace Android
} // namespace tcu

#endif // _TCUANDROIDEXECSERVICE_HPP
