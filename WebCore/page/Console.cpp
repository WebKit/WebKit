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
#include "FrameTree.h"
#include "InspectorController.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include <kjs/list.h>
#include <profiler/Profiler.h>

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
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, exec, arguments, 0, url.string());
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
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, arguments, 0, url.string());
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
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, arguments, 0, url.string());
}

void Console::profile(ExecState* exec, const List& arguments) const
{
    Page* page = m_frame->page();
    if (!page)
        return;

    const UString title = arguments[0]->toString(exec);
    Profiler::profiler()->startProfiling(exec, page->group().identifier(), title);
}

void Console::profileEnd() const
{
    Profiler::profiler()->stopProfiling();
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
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, WarningMessageLevel, exec, arguments, 0, url.string());
}

} // namespace WebCore
