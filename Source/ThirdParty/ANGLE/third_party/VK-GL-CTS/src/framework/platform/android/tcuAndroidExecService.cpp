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
 * \brief Android ExecServer.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidExecService.hpp"
#include "deFile.h"
#include "deClock.h"

#if 0
#define DBG_PRINT(ARGS) print ARGS
#else
#define DBG_PRINT(ARGS)
#endif

namespace tcu
{
namespace Android
{

static const char *LOG_FILE_NAME = "/sdcard/dEQP-log.qpa";

enum
{
    PROCESS_START_TIMEOUT  = 5000 * 1000, //!< Timeout in usec.
    PROCESS_QUERY_INTERVAL = 1000 * 1000  //!< Running query interval limit in usec.
};

static void checkJniException(JNIEnv *env, const char *file, int line)
{
    if (env->ExceptionOccurred())
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
        throw InternalError("JNI Exception", nullptr, file, line);
    }
}

#define JNI_CHECK(EXPR)                             \
    do                                              \
    {                                               \
        checkJniException(env, __FILE__, __LINE__); \
        TCU_CHECK_INTERNAL(EXPR);                   \
    } while (false)

// TestProcess

TestProcess::TestProcess(JavaVM *vm, jobject context)
    : m_vm(vm)
    , m_remoteCls(0)
    , m_remote(0)
    , m_start(0)
    , m_kill(0)
    , m_isRunning(0)
    , m_launchTime(0)
    , m_lastQueryTime(0)
    , m_lastRunningStatus(false)
    , m_logReader(xs::LOG_BUFFER_BLOCK_SIZE, xs::LOG_BUFFER_NUM_BLOCKS)
{
    DBG_PRINT(("TestProcess::TestProcess(%p, %p)", vm, context));

    JNIEnv *env         = getCurrentThreadEnv();
    jobject remote      = 0;
    jstring logFileName = 0;

    try
    {
        jclass remoteCls = 0;
        jmethodID ctorId = 0;

        remoteCls = env->FindClass("com/drawelements/deqp/testercore/RemoteAPI");
        JNI_CHECK(remoteCls);

        // Acquire global reference to RemoteAPI class.
        m_remoteCls = reinterpret_cast<jclass>(env->NewGlobalRef(remoteCls));
        JNI_CHECK(m_remoteCls);
        env->DeleteLocalRef(remoteCls);
        remoteCls = 0;

        ctorId = env->GetMethodID(m_remoteCls, "<init>", "(Landroid/content/Context;Ljava/lang/String;)V");
        JNI_CHECK(ctorId);

        logFileName = env->NewStringUTF(LOG_FILE_NAME);
        JNI_CHECK(logFileName);

        // Create RemoteAPI instance.
        remote = env->NewObject(m_remoteCls, ctorId, context, logFileName);
        JNI_CHECK(remote);

        env->DeleteLocalRef(logFileName);
        logFileName = 0;

        // Acquire global reference to remote.
        m_remote = env->NewGlobalRef(remote);
        JNI_CHECK(m_remote);
        env->DeleteLocalRef(remote);
        remote = 0;

        m_start = env->GetMethodID(m_remoteCls, "start", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
        JNI_CHECK(m_start);

        m_kill = env->GetMethodID(m_remoteCls, "kill", "()Z");
        JNI_CHECK(m_kill);

        m_isRunning = env->GetMethodID(m_remoteCls, "isRunning", "()Z");
        JNI_CHECK(m_isRunning);
    }
    catch (...)
    {
        if (logFileName)
            env->DeleteLocalRef(logFileName);
        if (remote)
            env->DeleteLocalRef(remote);
        if (m_remoteCls)
            env->DeleteGlobalRef(reinterpret_cast<jobject>(m_remoteCls));
        if (m_remote)
            env->DeleteGlobalRef(m_remote);
        throw;
    }
}

TestProcess::~TestProcess(void)
{
    DBG_PRINT(("TestProcess::~TestProcess()"));

    try
    {
        JNIEnv *env = getCurrentThreadEnv();
        env->DeleteGlobalRef(m_remote);
        env->DeleteGlobalRef(m_remoteCls);
    }
    catch (...)
    {
    }
}

void TestProcess::start(const char *name, const char *params, const char *workingDir, const char *caseList)
{
    DBG_PRINT(("TestProcess::start(%s, %s, %s, ...)", name, params, workingDir));

    JNIEnv *env         = getCurrentThreadEnv();
    jstring nameStr     = 0;
    jstring paramsStr   = 0;
    jstring caseListStr = 0;

    DE_UNREF(workingDir);

    // Remove old log file if such exists.
    if (deFileExists(LOG_FILE_NAME))
    {
        if (!deDeleteFile(LOG_FILE_NAME) || deFileExists(LOG_FILE_NAME))
            throw xs::TestProcessException(std::string("Failed to remove '") + LOG_FILE_NAME + "'");
    }

    try
    {
        nameStr = env->NewStringUTF(name);
        JNI_CHECK(nameStr);

        paramsStr = env->NewStringUTF(params);
        JNI_CHECK(paramsStr);

        caseListStr = env->NewStringUTF(caseList);
        JNI_CHECK(caseListStr);

        jboolean res = env->CallBooleanMethod(m_remote, m_start, nameStr, paramsStr, caseListStr);
        checkJniException(env, __FILE__, __LINE__);

        if (res == JNI_FALSE)
            throw xs::TestProcessException("Failed to launch activity");

        m_launchTime        = deGetMicroseconds();
        m_lastQueryTime     = m_launchTime;
        m_lastRunningStatus = true;
    }
    catch (...)
    {
        if (nameStr)
            env->DeleteLocalRef(nameStr);
        if (paramsStr)
            env->DeleteLocalRef(paramsStr);
        if (caseListStr)
            env->DeleteLocalRef(caseListStr);
        throw;
    }

    env->DeleteLocalRef(nameStr);
    env->DeleteLocalRef(paramsStr);
    env->DeleteLocalRef(caseListStr);
}

void TestProcess::terminate(void)
{
    DBG_PRINT(("TestProcess::terminate()"));

    JNIEnv *env  = getCurrentThreadEnv();
    jboolean res = env->CallBooleanMethod(m_remote, m_kill);
    checkJniException(env, __FILE__, __LINE__);
    DE_UNREF(res); // Failure to kill process is ignored.
}

void TestProcess::cleanup(void)
{
    DBG_PRINT(("TestProcess::cleanup()"));

    terminate();
    m_logReader.stop();
}

bool TestProcess::isRunning(void)
{
    uint64_t curTime = deGetMicroseconds();

    // On Android process launch is asynchronous so we don't want to poll for process until after some time.
    if (curTime - m_launchTime < PROCESS_START_TIMEOUT || curTime - m_lastQueryTime < PROCESS_QUERY_INTERVAL)
        return m_lastRunningStatus;

    JNIEnv *env  = getCurrentThreadEnv();
    jboolean res = env->CallBooleanMethod(m_remote, m_isRunning);
    checkJniException(env, __FILE__, __LINE__);

    DBG_PRINT(("TestProcess::isRunning(): %s", res == JNI_TRUE ? "true" : "false"));
    m_lastQueryTime     = curTime;
    m_lastRunningStatus = res == JNI_TRUE;

    return m_lastRunningStatus;
}

JNIEnv *TestProcess::getCurrentThreadEnv(void)
{
    JNIEnv *env = nullptr;
    jint ret    = m_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

    if (ret == JNI_OK)
        return env;
    else
        throw InternalError("GetEnv() failed");
}

int TestProcess::readTestLog(uint8_t *dst, int numBytes)
{
    if (!m_logReader.isRunning())
    {
        if (deGetMicroseconds() - m_launchTime > xs::LOG_FILE_TIMEOUT * 1000)
        {
            // Timeout, kill process.
            terminate();
            DBG_PRINT(("TestProcess:readTestLog(): Log file timeout occurred!"));
            return 0; // \todo [2013-08-13 pyry] Throw exception?
        }

        if (!deFileExists(LOG_FILE_NAME))
            return 0;

        // Start reader.
        m_logReader.start(LOG_FILE_NAME);
    }

    DE_ASSERT(m_logReader.isRunning());
    return m_logReader.read(dst, numBytes);
}

int TestProcess::getExitCode(void) const
{
    return 0;
}

int TestProcess::readInfoLog(uint8_t *dst, int numBytes)
{
    // \todo [2012-11-12 pyry] Read device log.
    DE_UNREF(dst && numBytes);
    return 0;
}

// ExecutionServer

ExecutionServer::ExecutionServer(JavaVM *vm, xs::TestProcess *testProcess, deSocketFamily family, int port,
                                 RunMode runMode)
    : xs::ExecutionServer(testProcess, family, port, runMode)
    , m_vm(vm)
{
}

xs::ConnectionHandler *ExecutionServer::createHandler(de::Socket *socket, const de::SocketAddress &clientAddress)
{
    DE_UNREF(clientAddress);
    return new ConnectionHandler(m_vm, this, socket);
}

// ConnectionHandler

ConnectionHandler::ConnectionHandler(JavaVM *vm, xs::ExecutionServer *server, de::Socket *socket)
    : xs::ExecutionRequestHandler(server, socket)
    , m_vm(vm)
{
}

void ConnectionHandler::run(void)
{
    JNIEnv *env = nullptr;
    if (m_vm->AttachCurrentThread(&env, nullptr) != 0)
    {
        print("AttachCurrentThread() failed");
        return;
    }

    xs::ExecutionRequestHandler::run();

    if (m_vm->DetachCurrentThread() != 0)
        print("DetachCurrentThread() failed");
}

// ServerThread

ServerThread::ServerThread(JavaVM *vm, xs::TestProcess *process, deSocketFamily family, int port)
    : m_server(vm, process, family, port, xs::ExecutionServer::RUNMODE_FOREVER)
{
}

void ServerThread::run(void)
{
    try
    {
        m_server.runServer();
    }
    catch (const std::exception &e)
    {
        die("ServerThread::run(): %s", e.what());
    }
}

void ServerThread::stop(void)
{
    m_server.stopServer();
    join();
}

// ExecService

ExecService::ExecService(JavaVM *vm, jobject context, int port, deSocketFamily family)
    : m_process(vm, context)
    , m_thread(vm, &m_process, family, port)
{
}

ExecService::~ExecService(void)
{
}

void ExecService::start(void)
{
    m_thread.start();
}

void ExecService::stop(void)
{
    m_thread.stop();
}

} // namespace Android
} // namespace tcu
