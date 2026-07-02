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

#include "tcuAndroidTestActivity.hpp"
#include "tcuAndroidUtil.hpp"

#include <android/window.h>

#include <string>
#include <stdlib.h>

using std::string;

namespace tcu
{
namespace Android
{

// TestThread

TestThread::TestThread(NativeActivity &activity, const std::string &cmdLineString, const CommandLine &cmdLine)
    : RenderThread(activity)
    , m_cmdLine(cmdLine)
    , m_platform(activity)
    , m_archive(activity.getNativeActivity()->assetManager)
    , m_log(m_cmdLine.getLogFileName(), m_cmdLine.getLogFlags())
    , m_app(m_platform, m_archive, m_log, m_cmdLine)
    , m_finished(false)
{
    const std::string sessionInfo = "#sessionInfo commandLineParameters \"";
    m_log.writeSessionInfo(sessionInfo + cmdLineString + "\"\n");
}

TestThread::~TestThread(void)
{
    // \note m_testApp is managed by thread.
}

void TestThread::run(void)
{
    RenderThread::run();
}

void TestThread::onWindowCreated(ANativeWindow *window)
{
    m_platform.getWindowRegistry().addWindow(window);
}

void TestThread::onWindowDestroyed(ANativeWindow *window)
{
    m_platform.getWindowRegistry().destroyWindow(window);
}

void TestThread::onWindowResized(ANativeWindow *window)
{
    DE_UNREF(window);
    print("Warning: Native window was resized, results may be undefined");
}

bool TestThread::render(void)
{
    if (!m_finished)
        m_finished = !m_app.iterate();
    return !m_finished;
}

// TestActivity

TestActivity::TestActivity(ANativeActivity *activity)
    : RenderActivity(activity)
    , m_cmdLine(getIntentStringExtra(activity, "cmdLine"))
    , m_testThread(*this, getIntentStringExtra(activity, "cmdLine"), m_cmdLine)
    , m_started(false)
{
    // Set initial orientation.
    setRequestedOrientation(getNativeActivity(), mapScreenRotation(m_cmdLine.getScreenRotation()));

    // Set up window flags.
    ANativeActivity_setWindowFlags(activity,
                                   AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_TURN_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN |
                                       AWINDOW_FLAG_SHOW_WHEN_LOCKED,
                                   0);
}

TestActivity::~TestActivity(void)
{
}

void TestActivity::onStart(void)
{
    if (!m_started)
    {
        setThread(&m_testThread);
        m_testThread.start();
        m_started = true;
    }

    RenderActivity::onStart();
}

void TestActivity::onDestroy(void)
{
    if (m_started)
    {
        setThread(nullptr);
        m_testThread.stop();
        m_started = false;
    }

    RenderActivity::onDestroy();

    // Kill this process.
    print("Done, killing process");
    exit(0);
}

void TestActivity::onConfigurationChanged(void)
{
    RenderActivity::onConfigurationChanged();

    // Update rotation if custom orientation is not enabled.
    if (!CustomOrientation::instance().isEnabled())
    {
        setRequestedOrientation(getNativeActivity(), mapScreenRotation(m_cmdLine.getScreenRotation()));
    }
}

} // namespace Android
} // namespace tcu
