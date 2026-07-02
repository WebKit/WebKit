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
 * \brief Android JNI interface.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuApp.hpp"
#include "tcuAndroidExecService.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include <string>
#include <vector>
#include <cstring>

#include <jni.h>
#include <stdlib.h>
#include <android/log.h>

// ExecService entry points.

static jfieldID getExecServiceField(JNIEnv *env, jobject obj)
{
    jclass cls = env->GetObjectClass(obj);
    TCU_CHECK_INTERNAL(cls);

    jfieldID fid = env->GetFieldID(cls, "m_server", "J");
    TCU_CHECK_INTERNAL(fid);

    return fid;
}

static tcu::Android::ExecService *getExecService(JNIEnv *env, jobject obj)
{
    jfieldID field = getExecServiceField(env, obj);
    return (tcu::Android::ExecService *)(intptr_t)env->GetLongField(obj, field);
}

static void setExecService(JNIEnv *env, jobject obj, tcu::Android::ExecService *service)
{
    jfieldID field = getExecServiceField(env, obj);
    env->SetLongField(obj, field, (jlong)(intptr_t)service);
}

static void logException(const std::exception &e)
{
    __android_log_print(ANDROID_LOG_ERROR, "dEQP", "%s", e.what());
}

DE_BEGIN_EXTERN_C

JNIEXPORT void JNICALL Java_com_drawelements_deqp_execserver_ExecService_startServer(JNIEnv *env, jobject obj,
                                                                                     jint port)
{
    tcu::Android::ExecService *service = nullptr;
    JavaVM *vm                         = nullptr;

    try
    {
        DE_ASSERT(!getExecService(env, obj));

        env->GetJavaVM(&vm);
        TCU_CHECK_INTERNAL(vm);

        service = new tcu::Android::ExecService(vm, obj, port);
        service->start();

        setExecService(env, obj, service);
    }
    catch (const std::exception &e)
    {
        logException(e);
        delete service;
        tcu::die("ExecService.startServer() failed");
    }
}

JNIEXPORT void JNICALL Java_com_drawelements_deqp_execserver_ExecService_stopServer(JNIEnv *env, jobject obj)
{
    try
    {
        tcu::Android::ExecService *service = getExecService(env, obj);
        TCU_CHECK_INTERNAL(service);

        service->stop();
        delete service;

        setExecService(env, obj, nullptr);
    }
    catch (const std::exception &e)
    {
        logException(e);
        tcu::die("ExecService.stopServer() failed");
    }
}

DE_END_EXTERN_C
