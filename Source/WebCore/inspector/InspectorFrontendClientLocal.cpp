/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorFrontendClientLocal.h"

#if ENABLE(INSPECTOR)

#include "Chrome.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorController.h"
#include "InspectorFrontendHost.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include "Settings.h"
#include "Timer.h"
#include <wtf/Deque.h>

namespace WebCore {

static const char* inspectorAttachedHeightSetting = "inspectorAttachedHeight";
static const unsigned defaultAttachedHeight = 300;
static const float minimumAttachedHeight = 250.0f;
static const float maximumAttachedHeightRatio = 0.75f;

class InspectorBackendDispatchTask {
public:
    InspectorBackendDispatchTask(InspectorController* inspectorController)
        : m_inspectorController(inspectorController)
        , m_timer(this, &InspectorBackendDispatchTask::onTimer)
    {
    }

    void dispatch(const String& message)
    {
        m_messages.append(message);
        if (!m_timer.isActive())
            m_timer.startOneShot(0);
    }

    void reset()
    {
        m_messages.clear();
        m_timer.stop();
    }

    void onTimer(Timer<InspectorBackendDispatchTask>*)
    {
        if (!m_messages.isEmpty()) {
            // Dispatch can lead to the timer destruction -> schedule the next shot first.
            m_timer.startOneShot(0);
            m_inspectorController->dispatchMessageFromFrontend(m_messages.takeFirst());
        }
    }

private:
    InspectorController* m_inspectorController;
    Timer<InspectorBackendDispatchTask> m_timer;
    Deque<String> m_messages;
};

String InspectorFrontendClientLocal::Settings::getProperty(const String&)
{
    return String();
}

void InspectorFrontendClientLocal::Settings::setProperty(const String&, const String&)
{
}

InspectorFrontendClientLocal::InspectorFrontendClientLocal(InspectorController* inspectorController, Page* frontendPage, PassOwnPtr<Settings> settings)
    : m_inspectorController(inspectorController)
    , m_frontendPage(frontendPage)
    , m_frontendScriptState(0)
    , m_settings(settings)
    , m_frontendLoaded(false)
{
    m_frontendPage->settings()->setAllowFileAccessFromFileURLs(true);
    m_dispatchTask = adoptPtr(new InspectorBackendDispatchTask(inspectorController));
}

InspectorFrontendClientLocal::~InspectorFrontendClientLocal()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();
    m_frontendPage = 0;
    m_frontendScriptState = 0;
    m_inspectorController = 0;
}

void InspectorFrontendClientLocal::windowObjectCleared()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();
    // FIXME: don't keep reference to the script state
    m_frontendScriptState = scriptStateFromPage(debuggerWorld(), m_frontendPage);
    m_frontendHost = InspectorFrontendHost::create(this, m_frontendPage);
    ScriptGlobalObject::set(m_frontendScriptState, "InspectorFrontendHost", m_frontendHost.get());
}

void InspectorFrontendClientLocal::frontendLoaded()
{
    bringToFront();
    m_frontendLoaded = true;
    if (!m_evaluateOnLoad.isEmpty())
        evaluateOnLoad(m_evaluateOnLoad);
}

void InspectorFrontendClientLocal::requestAttachWindow()
{
    if (!canAttachWindow())
        return;
    attachWindow();
    setAttachedWindow(true);
}

void InspectorFrontendClientLocal::requestDetachWindow()
{
    detachWindow();
    setAttachedWindow(false);
}

bool InspectorFrontendClientLocal::canAttachWindow()
{
    unsigned inspectedPageHeight = m_inspectorController->inspectedPage()->mainFrame()->view()->visibleHeight();

    // Don't allow the attach if the window would be too small to accommodate the minimum inspector height.
    return minimumAttachedHeight <= inspectedPageHeight * maximumAttachedHeightRatio;
}

void InspectorFrontendClientLocal::changeAttachedWindowHeight(unsigned height)
{
    unsigned totalHeight = m_frontendPage->mainFrame()->view()->visibleHeight() + m_inspectorController->inspectedPage()->mainFrame()->view()->visibleHeight();
    unsigned attachedHeight = constrainedAttachedWindowHeight(height, totalHeight);
    m_settings->setProperty(inspectorAttachedHeightSetting, String::number(attachedHeight));
    setAttachedWindowHeight(attachedHeight);
}

void InspectorFrontendClientLocal::moveWindowBy(float x, float y)
{
    FloatRect frameRect = m_frontendPage->chrome()->windowRect();
    frameRect.move(x, y);
    m_frontendPage->chrome()->setWindowRect(frameRect);
}

void InspectorFrontendClientLocal::setAttachedWindow(bool attached)
{
    evaluateAsBoolean(String::format("InspectorFrontendAPI.setAttachedWindow(%s)", attached ? "true" : "false"));
}

void InspectorFrontendClientLocal::restoreAttachedWindowHeight()
{
    unsigned inspectedPageHeight = m_inspectorController->inspectedPage()->mainFrame()->view()->visibleHeight();
    String value = m_settings->getProperty(inspectorAttachedHeightSetting);
    unsigned preferredHeight = value.isEmpty() ? defaultAttachedHeight : value.toUInt();
    
    // This call might not go through (if the window starts out detached), but if the window is initially created attached,
    // InspectorController::attachWindow is never called, so we need to make sure to set the attachedWindowHeight.
    // FIXME: Clean up code so we only have to call setAttachedWindowHeight in InspectorController::attachWindow
    setAttachedWindowHeight(constrainedAttachedWindowHeight(preferredHeight, inspectedPageHeight));
}

bool InspectorFrontendClientLocal::isDebuggingEnabled()
{
    if (m_frontendLoaded)
        return evaluateAsBoolean("InspectorFrontendAPI.isDebuggingEnabled()");
    return false;
}

void InspectorFrontendClientLocal::setDebuggingEnabled(bool enabled)
{
    evaluateOnLoad(String::format("InspectorFrontendAPI.setDebuggingEnabled(%s)", enabled ? "true" : "false"));
}

bool InspectorFrontendClientLocal::isTimelineProfilingEnabled()
{
    if (m_frontendLoaded)
        return evaluateAsBoolean("InspectorFrontendAPI.isTimelineProfilingEnabled()");
    return false;
}

void InspectorFrontendClientLocal::setTimelineProfilingEnabled(bool enabled)
{
    evaluateOnLoad(String::format("InspectorFrontendAPI.setTimelineProfilingEnabled(%s)", enabled ? "true" : "false"));
}

bool InspectorFrontendClientLocal::isProfilingJavaScript()
{
    if (m_frontendLoaded)
        return evaluateAsBoolean("InspectorFrontendAPI.isProfilingJavaScript()");
    return false;
}

void InspectorFrontendClientLocal::startProfilingJavaScript()
{
    evaluateOnLoad("InspectorFrontendAPI.startProfilingJavaScript()");
}

void InspectorFrontendClientLocal::stopProfilingJavaScript()
{
    evaluateOnLoad("InspectorFrontendAPI.stopProfilingJavaScript()");
}

void InspectorFrontendClientLocal::showConsole()
{
    evaluateOnLoad("InspectorFrontendAPI.showConsole()");
}

unsigned InspectorFrontendClientLocal::constrainedAttachedWindowHeight(unsigned preferredHeight, unsigned totalWindowHeight)
{
    using namespace std;
    return roundf(max(minimumAttachedHeight, min<float>(preferredHeight, totalWindowHeight * maximumAttachedHeightRatio)));
}

void InspectorFrontendClientLocal::sendMessageToBackend(const String& message)
{
    m_dispatchTask->dispatch(message);
}

bool InspectorFrontendClientLocal::evaluateAsBoolean(const String& expression)
{
    if (!m_frontendPage->mainFrame())
        return false;
    ScriptValue value = m_frontendPage->mainFrame()->script()->executeScript(expression);
    return value.toString(mainWorldScriptState(m_frontendPage->mainFrame())) == "true";
}

void InspectorFrontendClientLocal::evaluateOnLoad(const String& expression)
{
    if (m_frontendLoaded)
        m_frontendPage->mainFrame()->script()->executeScript(expression);
    else
        m_evaluateOnLoad = "setTimeout(function() { " + expression + "; }, 0)";
}

} // namespace WebCore

#endif
