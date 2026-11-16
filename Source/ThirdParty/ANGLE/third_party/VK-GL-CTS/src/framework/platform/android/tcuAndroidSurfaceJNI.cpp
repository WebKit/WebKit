/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2026 The Android Open Source Project
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
 * \brief Android Surface JNI.
 *//*--------------------------------------------------------------------*/

#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <android/native_activity.h>
#include <android/asset_manager_jni.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

#include "tcuApp.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "tcuResource.hpp"

#include "tcuAndroidPlatform.hpp"
#include "tcuAndroidWindow.hpp"
#include "tcuAndroidUtil.hpp"
#include "tcuAndroidAssets.hpp"

//Java_com_drawelements_deqp_parallelrunner_WorkerService_nativeStartDeqp
extern "C" JNIEXPORT void JNICALL Java_com_drawelements_deqp_parallelrunner_WorkerService_nativeStartDeqp(
    JNIEnv *env, jobject thiz, jobject jSurface, jstring jArgs, jobject jAssetManager)
{

    __android_log_print(ANDROID_LOG_INFO, "dEQP", "nativeStartDeqp called for service:%p, surface:%p", thiz, jSurface);

    ANativeWindow *window = ANativeWindow_fromSurface(env, jSurface);
    if (!window)
        return;

    const char *argsCStr = env->GetStringUTFChars(jArgs, nullptr);
    std::string argsStr(argsCStr);
    env->ReleaseStringUTFChars(jArgs, argsCStr);

    std::vector<char *> argv;
    argv.push_back(strdup("deqp")); // dummy argv[0]

    size_t pos = 0;
    while ((pos = argsStr.find(' ')) != std::string::npos)
    {
        if (pos > 0)
            argv.push_back(strdup(argsStr.substr(0, pos).c_str()));
        argsStr.erase(0, pos + 1);
    }
    if (!argsStr.empty())
        argv.push_back(strdup(argsStr.c_str()));
    int argc = (int)argv.size();

    AAssetManager *assetManager = nullptr;
    if (jAssetManager != nullptr)
    {
        assetManager = AAssetManager_fromJava(env, jAssetManager);
    }

    try
    {
        JavaVM *vm = nullptr;
        env->GetJavaVM(&vm);

        tcu::Android::Platform platform(vm, thiz, window);

        tcu::CommandLine cmdLine(argc, &argv[0]);

        tcu::Android::AssetArchive archive(assetManager);

        tcu::TestLog log(cmdLine.getLogFileName(), cmdLine.getLogFlags());

        tcu::App app(platform, archive, log, cmdLine);

        for (;;)
        {
            if (!app.iterate())
                break;
        }
    }
    catch (const std::exception &e)
    {
        __android_log_print(ANDROID_LOG_ERROR, "dEQP", "nativeStartDeqp caught std::exception: %s", e.what());
    }
    catch (...)
    {
        __android_log_print(ANDROID_LOG_ERROR, "dEQP", "nativeStartDeqp caught unknown exception!");
    }

    // Cleanup
    for (char *arg : argv)
        free(arg);
    ANativeWindow_release(window);
}
