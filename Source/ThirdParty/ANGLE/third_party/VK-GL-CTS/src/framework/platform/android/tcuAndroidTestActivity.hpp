#ifndef _TCUANDROIDTESTACTIVITY_HPP
#define _TCUANDROIDTESTACTIVITY_HPP
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
 * \brief Android test activity.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuAndroidRenderActivity.hpp"
#include "tcuAndroidPlatform.hpp"
#include "tcuCommandLine.hpp"
#include "tcuAndroidAssets.hpp"
#include "tcuTestLog.hpp"
#include "tcuApp.hpp"

namespace tcu
{
namespace Android
{

class TestThread : public RenderThread
{
public:
    TestThread(NativeActivity &activity, const std::string &cmdLineString, const CommandLine &cmdLine);
    ~TestThread(void);

    void run(void);

private:
    virtual void onWindowCreated(ANativeWindow *window);
    virtual void onWindowResized(ANativeWindow *window);
    virtual void onWindowDestroyed(ANativeWindow *window);
    virtual bool render(void);

    const CommandLine &m_cmdLine;
    Platform m_platform;
    AssetArchive m_archive;
    TestLog m_log;
    App m_app;
    bool m_finished; //!< Is execution finished.
};

class TestActivity : public RenderActivity
{
public:
    TestActivity(ANativeActivity *nativeActivity);
    ~TestActivity(void);

    virtual void onStart(void);
    virtual void onDestroy(void);
    virtual void onConfigurationChanged(void);

private:
    CommandLine m_cmdLine;
    TestThread m_testThread;
    bool m_started;
};

} // namespace Android
} // namespace tcu

#endif // _TCUANDROIDTESTACTIVITY_HPP
