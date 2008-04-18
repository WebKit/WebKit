/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Console.h"

#include "ChromeClient.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorController.h"
#include "Page.h"
#include "PlatformString.h"
#include <kjs/Profiler.h>
#include <kjs/list.h>

using namespace KJS;

namespace WebCore {

Console::Console(Frame* frame)
    : m_frame(frame)
{
}

void Console::disconnectFrame()
{
    m_frame = 0;
}

void Console::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (source == JSMessageSource)
        page->chrome()->client()->addMessageToConsole(message, lineNumber, sourceURL);

    page->inspectorController()->addMessageToConsole(source, level, message, lineNumber, sourceURL);
}

void Console::error(ExecState* exec, const List& arguments)
{
    if (arguments.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = arguments[0]->toString(exec);
    String url = m_frame->loader()->url().prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, url);
    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, exec, arguments, 0, url);
}

void Console::info(ExecState* exec, const List& arguments)
{
    if (arguments.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = arguments[0]->toString(exec);
    String url = m_frame->loader()->url().prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, url);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, arguments, 0, url);
}

void Console::log(ExecState* exec, const List& arguments)
{
    if (arguments.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = arguments[0]->toString(exec);
    String url = m_frame->loader()->url().prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, url);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, arguments, 0, url);
}

void Console::profile(const String& /*title*/) const
{
    // FIXME: Figure out something to do with the title passed in so that it can
    // be displayed by the inspector.
    Profiler::profiler()->startProfiling();
}

void Console::profileEnd() const
{
    Profiler::profiler()->stopProfiling();
    // FIXME: We need to hook into the WebInspector here so that it can process over the data
    // that is in the profiler and display it in some new cool way, as opposed to just printing
    // it or dumping it to the console.
    Profiler::profiler()->printDataInspectorStyle();
}

void Console::warn(ExecState* exec, const List& arguments)
{
    if (arguments.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = arguments[0]->toString(exec);
    String url = m_frame->loader()->url().prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, url);
    page->inspectorController()->addMessageToConsole(JSMessageSource, WarningMessageLevel, exec, arguments, 0, url);
}

} // namespace WebCore
