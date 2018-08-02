/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "Chrome.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "InspectorController.h"
#include "InspectorFrontendHost.h"
#include "InspectorPageAgent.h"
#include "Page.h"
#include "ScriptController.h"
#include "ScriptState.h"
#include "Settings.h"
#include "Timer.h"
#include "UserGestureIndicator.h"
#include "WindowFeatures.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <wtf/Deque.h>
#include <wtf/text/CString.h>


namespace WebCore {

using namespace Inspector;

static const char* inspectorAttachedHeightSetting = "inspectorAttachedHeight";
static const unsigned defaultAttachedHeight = 300;
static const float minimumAttachedHeight = 250.0f;
static const float maximumAttachedHeightRatio = 0.75f;
static const float minimumAttachedWidth = 500.0f;
static const float minimumAttachedInspectedWidth = 320.0f;

class InspectorBackendDispatchTask : public RefCounted<InspectorBackendDispatchTask> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<InspectorBackendDispatchTask> create(InspectorController* inspectedPageController)
    {
        return adoptRef(*new InspectorBackendDispatchTask(inspectedPageController));
    }

    void dispatch(const String& message)
    {
        ASSERT_ARG(message, !message.isEmpty());

        m_messages.append(message);
        if (!m_timer.isActive())
            m_timer.startOneShot(0_s);
    }

    void reset()
    {
        m_messages.clear();
        m_timer.stop();
        m_inspectedPageController = nullptr;
    }

    void timerFired()
    {
        ASSERT(m_inspectedPageController);

        // Dispatching a message can possibly close the frontend and destroy
        // the owning frontend client, so keep a protector reference here.
        Ref<InspectorBackendDispatchTask> protectedThis(*this);

        if (!m_messages.isEmpty())
            m_inspectedPageController->dispatchMessageFromFrontend(m_messages.takeFirst());

        if (!m_messages.isEmpty() && m_inspectedPageController)
            m_timer.startOneShot(0_s);
    }

private:
    InspectorBackendDispatchTask(InspectorController* inspectedPageController)
        : m_inspectedPageController(inspectedPageController)
        , m_timer(*this, &InspectorBackendDispatchTask::timerFired)
    {
        ASSERT_ARG(inspectedPageController, inspectedPageController);
    }

    InspectorController* m_inspectedPageController { nullptr };
    Timer m_timer;
    Deque<String> m_messages;
};

String InspectorFrontendClientLocal::Settings::getProperty(const String&)
{
    return String();
}

void InspectorFrontendClientLocal::Settings::setProperty(const String&, const String&)
{
}

InspectorFrontendClientLocal::InspectorFrontendClientLocal(InspectorController* inspectedPageController, Page* frontendPage, std::unique_ptr<Settings> settings)
    : m_inspectedPageController(inspectedPageController)
    , m_frontendPage(frontendPage)
    , m_settings(WTFMove(settings))
    , m_dockSide(DockSide::Undocked)
    , m_dispatchTask(InspectorBackendDispatchTask::create(inspectedPageController))
{
    m_frontendPage->settings().setAllowFileAccessFromFileURLs(true);
}

InspectorFrontendClientLocal::~InspectorFrontendClientLocal()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();
    m_frontendPage = nullptr;
    m_inspectedPageController = nullptr;
    m_dispatchTask->reset();
}

void InspectorFrontendClientLocal::windowObjectCleared()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();
    
    m_frontendHost = InspectorFrontendHost::create(this, m_frontendPage);
    m_frontendHost->addSelfToGlobalObjectInWorld(debuggerWorld());
}

void InspectorFrontendClientLocal::frontendLoaded()
{
    // Call setDockingUnavailable before bringToFront. If we display the inspector window via bringToFront first it causes
    // the call to canAttachWindow to return the wrong result on Windows.
    // Calling bringToFront first causes the visibleHeight of the inspected page to always return 0 immediately after.
    // Thus if we call canAttachWindow first we can avoid this problem. This change does not cause any regressions on Mac.
    setDockingUnavailable(!canAttachWindow());
    bringToFront();
    m_frontendLoaded = true;
    for (auto& evaluate : m_evaluateOnLoad)
        evaluateOnLoad(evaluate);
    m_evaluateOnLoad.clear();
}

UserInterfaceLayoutDirection InspectorFrontendClientLocal::userInterfaceLayoutDirection() const
{
    return m_frontendPage->userInterfaceLayoutDirection();
}

void InspectorFrontendClientLocal::requestSetDockSide(DockSide dockSide)
{
    if (dockSide == DockSide::Undocked) {
        detachWindow();
        setAttachedWindow(dockSide);
    } else if (canAttachWindow()) {
        attachWindow(dockSide);
        setAttachedWindow(dockSide);
    }
}

bool InspectorFrontendClientLocal::canAttachWindow()
{
    // Don't allow attaching to another inspector -- two inspectors in one window is too much!
    bool isInspectorPage = m_inspectedPageController->inspectionLevel() > 0;
    if (isInspectorPage)
        return false;

    // If we are already attached, allow attaching again to allow switching sides.
    if (m_dockSide != DockSide::Undocked)
        return true;

    // Don't allow the attach if the window would be too small to accommodate the minimum inspector size.
    unsigned inspectedPageHeight = m_inspectedPageController->inspectedPage().mainFrame().view()->visibleHeight();
    unsigned inspectedPageWidth = m_inspectedPageController->inspectedPage().mainFrame().view()->visibleWidth();
    unsigned maximumAttachedHeight = inspectedPageHeight * maximumAttachedHeightRatio;
    return minimumAttachedHeight <= maximumAttachedHeight && minimumAttachedWidth <= inspectedPageWidth;
}

void InspectorFrontendClientLocal::setDockingUnavailable(bool unavailable)
{
    evaluateOnLoad(String::format("[\"setDockingUnavailable\", %s]", unavailable ? "true" : "false"));
}

void InspectorFrontendClientLocal::changeAttachedWindowHeight(unsigned height)
{
    unsigned totalHeight = m_frontendPage->mainFrame().view()->visibleHeight() + m_inspectedPageController->inspectedPage().mainFrame().view()->visibleHeight();
    unsigned attachedHeight = constrainedAttachedWindowHeight(height, totalHeight);
    m_settings->setProperty(inspectorAttachedHeightSetting, String::number(attachedHeight));
    setAttachedWindowHeight(attachedHeight);
}

void InspectorFrontendClientLocal::changeAttachedWindowWidth(unsigned width)
{
    unsigned totalWidth = m_frontendPage->mainFrame().view()->visibleWidth() + m_inspectedPageController->inspectedPage().mainFrame().view()->visibleWidth();
    unsigned attachedWidth = constrainedAttachedWindowWidth(width, totalWidth);
    setAttachedWindowWidth(attachedWidth);
}

void InspectorFrontendClientLocal::openInNewTab(const String& url)
{
    UserGestureIndicator indicator { ProcessingUserGesture };
    Frame& mainFrame = m_inspectedPageController->inspectedPage().mainFrame();
    FrameLoadRequest frameLoadRequest { *mainFrame.document(), mainFrame.document()->securityOrigin(), { }, "_blank"_s, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, ShouldOpenExternalURLsPolicy::ShouldNotAllow, InitiatedByMainFrame::Unknown };

    bool created;
    RefPtr<Frame> frame = WebCore::createWindow(mainFrame, mainFrame, WTFMove(frameLoadRequest), { }, created);
    if (!frame)
        return;

    frame->loader().setOpener(&mainFrame);
    frame->page()->setOpenedByDOM();

    // FIXME: Why do we compute the absolute URL with respect to |frame| instead of |mainFrame|?
    ResourceRequest resourceRequest { frame->document()->completeURL(url) };
    FrameLoadRequest frameLoadRequest2 { *mainFrame.document(), mainFrame.document()->securityOrigin(), resourceRequest, "_self"_s, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, ShouldOpenExternalURLsPolicy::ShouldNotAllow, InitiatedByMainFrame::Unknown };
    frame->loader().changeLocation(WTFMove(frameLoadRequest2));
}

void InspectorFrontendClientLocal::moveWindowBy(float x, float y)
{
    FloatRect frameRect = m_frontendPage->chrome().windowRect();
    frameRect.move(x, y);
    m_frontendPage->chrome().setWindowRect(frameRect);
}

void InspectorFrontendClientLocal::setAttachedWindow(DockSide dockSide)
{
    const char* side = "undocked";
    switch (dockSide) {
    case DockSide::Undocked:
        side = "undocked";
        break;
    case DockSide::Right:
        side = "right";
        break;
    case DockSide::Left:
        side = "left";
        break;
    case DockSide::Bottom:
        side = "bottom";
        break;
    }

    m_dockSide = dockSide;

    evaluateOnLoad(String::format("[\"setDockSide\", \"%s\"]", side));
}

void InspectorFrontendClientLocal::restoreAttachedWindowHeight()
{
    unsigned inspectedPageHeight = m_inspectedPageController->inspectedPage().mainFrame().view()->visibleHeight();
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
        return evaluateAsBoolean("[\"isDebuggingEnabled\"]");
    return false;
}

void InspectorFrontendClientLocal::setDebuggingEnabled(bool enabled)
{
    evaluateOnLoad(String::format("[\"setDebuggingEnabled\", %s]", enabled ? "true" : "false"));
}

bool InspectorFrontendClientLocal::isTimelineProfilingEnabled()
{
    if (m_frontendLoaded)
        return evaluateAsBoolean("[\"isTimelineProfilingEnabled\"]");
    return false;
}

void InspectorFrontendClientLocal::setTimelineProfilingEnabled(bool enabled)
{
    evaluateOnLoad(String::format("[\"setTimelineProfilingEnabled\", %s]", enabled ? "true" : "false"));
}

bool InspectorFrontendClientLocal::isProfilingJavaScript()
{
    if (m_frontendLoaded)
        return evaluateAsBoolean("[\"isProfilingJavaScript\"]");
    return false;
}

void InspectorFrontendClientLocal::startProfilingJavaScript()
{
    evaluateOnLoad("[\"startProfilingJavaScript\"]");
}

void InspectorFrontendClientLocal::stopProfilingJavaScript()
{
    evaluateOnLoad("[\"stopProfilingJavaScript\"]");
}

void InspectorFrontendClientLocal::showConsole()
{
    evaluateOnLoad("[\"showConsole\"]");
}

void InspectorFrontendClientLocal::showResources()
{
    evaluateOnLoad("[\"showResources\"]");
}

void InspectorFrontendClientLocal::showMainResourceForFrame(Frame* frame)
{
    String frameId = m_inspectedPageController->pageAgent()->frameId(frame);
    evaluateOnLoad(String::format("[\"showMainResourceForFrame\", \"%s\"]", frameId.ascii().data()));
}

unsigned InspectorFrontendClientLocal::constrainedAttachedWindowHeight(unsigned preferredHeight, unsigned totalWindowHeight)
{
    return roundf(std::max(minimumAttachedHeight, std::min<float>(preferredHeight, totalWindowHeight * maximumAttachedHeightRatio)));
}

unsigned InspectorFrontendClientLocal::constrainedAttachedWindowWidth(unsigned preferredWidth, unsigned totalWindowWidth)
{
    return roundf(std::max(minimumAttachedWidth, std::min<float>(preferredWidth, totalWindowWidth - minimumAttachedInspectedWidth)));
}

void InspectorFrontendClientLocal::sendMessageToBackend(const String& message)
{
    m_dispatchTask->dispatch(message);
}

bool InspectorFrontendClientLocal::isUnderTest()
{
    return m_inspectedPageController->isUnderTest();
}

unsigned InspectorFrontendClientLocal::inspectionLevel() const
{
    return m_inspectedPageController->inspectionLevel() + 1;
}

bool InspectorFrontendClientLocal::evaluateAsBoolean(const String& expression)
{
    auto& state = *mainWorldExecState(&m_frontendPage->mainFrame());
    return m_frontendPage->mainFrame().script().executeScript(expression).toWTFString(&state) == "true";
}

void InspectorFrontendClientLocal::evaluateOnLoad(const String& expression)
{
    if (m_frontendLoaded)
        m_frontendPage->mainFrame().script().executeScript("if (InspectorFrontendAPI) InspectorFrontendAPI.dispatch(" + expression + ")");
    else
        m_evaluateOnLoad.append(expression);
}

Page* InspectorFrontendClientLocal::inspectedPage() const
{
    if (!m_inspectedPageController)
        return nullptr;

    return &m_inspectedPageController->inspectedPage();
}

} // namespace WebCore
