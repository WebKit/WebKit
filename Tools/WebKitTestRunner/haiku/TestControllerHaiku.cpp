/*
 * Copyright (C) 2014 Haiku, inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "TestController.h"

#include <Application.h>
#include "NotImplemented.h"
#include "PlatformWebView.h"
#include "WebView.h"

namespace WTR {

void TestController::notifyDone()
{
    notImplemented();
}

void TestController::platformInitialize()
{
    const char* isDebugging = getenv("WEB_PROCESS_CMD_PREFIX");
    if (isDebugging && *isDebugging) {
        m_useWaitToDumpWatchdogTimer = false;
        m_forceNoTimeout = true;
    }

    new BApplication("application/x-vnd.haiku-webkit.testrunner");
}

void TestController::platformDestroy()
{
    delete be_app;
}

void TestController::platformWillRunTest(const TestInvocation&)
{
}

unsigned TestController::imageCountInGeneralPasteboard() const
{
	// FIXME implement (scan BClipboard?)
	return 0;
}

void TestController::platformRunUntil(bool& condition, double timeout)
{
    // FIXME condition (?), timeout (via BMessageRunnner)
    notImplemented();

    be_app->Run();
}

static const char* getEnvironmentVariableOrExit(const char* variableName)
{
    const char* value = getenv(variableName);
    if (!value) {
        fprintf(stderr, "%s environment variable not found\n", variableName);
        exit(0);
    }

    return value;
}

void TestController::initializeInjectedBundlePath()
{
    const char* bundlePath = getEnvironmentVariableOrExit("TEST_RUNNER_INJECTED_BUNDLE_FILENAME");
    m_injectedBundlePath.adopt(WKStringCreateWithUTF8CString(bundlePath));
}

void TestController::initializeTestPluginDirectory()
{
    const char* pluginPath = getEnvironmentVariableOrExit("TEST_RUNNER_PLUGIN_PATH");
    m_testPluginDirectory.adopt(WKStringCreateWithUTF8CString(pluginPath));
}

void TestController::platformInitializeContext()
{
}

void TestController::setHidden(bool hidden)
{
    PlatformWKView view = mainWebView()->platformView();

    if (!view) {
        fprintf(stderr, "ERROR: view is null.\n");
        return;
    }

    if (hidden)
        view->Hide();
    else
        view->Show();
}

void TestController::runModal(PlatformWebView*)
{
    // FIXME: Need to implement this to test showModalDialog.
}

const char* TestController::platformLibraryPathForTesting()
{
    return 0;
}

}
