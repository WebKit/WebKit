/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "JavaScriptDebuggerBlackBerry.h"

#include "JavaScriptCallFrame.h"
#include "PageScriptDebugServer.h"
#include "PlatformString.h"
#include "ScriptBreakpoint.h"
#include "SourceCode.h"
#include "WebPage_p.h"

namespace WebCore {

JavaScriptDebuggerBlackBerry::JavaScriptDebuggerBlackBerry(BlackBerry::WebKit::WebPagePrivate* webPagePrivate)
    : m_webPagePrivate(webPagePrivate)
    , m_debugServer(PageScriptDebugServer::shared())
{
    start();
}

JavaScriptDebuggerBlackBerry::~JavaScriptDebuggerBlackBerry()
{
    stop();
}

void JavaScriptDebuggerBlackBerry::start()
{
    m_debugServer.addListener(this, m_webPagePrivate->m_page);
}

void JavaScriptDebuggerBlackBerry::stop()
{
    m_debugServer.removeListener(this, m_webPagePrivate->m_page);
}

void JavaScriptDebuggerBlackBerry::addBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber, const unsigned short* condition, unsigned conditionLength)
{
    if (!url || !urlLength)
        return;
    if (!m_currentCallFrame)
        return;

    String sourceString(url, urlLength);
    String conditionString(condition, conditionLength);
    int actualLineNumber;
    m_debugServer.setBreakpoint(sourceString, ScriptBreakpoint(lineNumber, 0, conditionString), &lineNumber, &actualLineNumber);
}

void JavaScriptDebuggerBlackBerry::updateBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber, const unsigned short* condition, unsigned conditionLength)
{
    if (!url || !urlLength)
        return;
    if (!m_currentCallFrame)
        return;

    String sourceString(url, urlLength);
    String conditionString(condition, conditionLength);
    int actualLineNumber;
    m_debugServer.setBreakpoint(sourceString, ScriptBreakpoint(lineNumber, 0, conditionString), &lineNumber, &actualLineNumber);
}


void JavaScriptDebuggerBlackBerry::removeBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber)
{
    if (!url || !urlLength)
        return;
    if (!m_currentCallFrame)
        return;

    String sourceString(url, urlLength);
    sourceString += ":" + lineNumber;
    m_debugServer.removeBreakpoint(sourceString);
}


bool JavaScriptDebuggerBlackBerry::pauseOnExceptions()
{
    return m_debugServer.pauseOnExceptionsState() == ScriptDebugServer::PauseOnAllExceptions;
}

void JavaScriptDebuggerBlackBerry::setPauseOnExceptions(bool pause)
{
    m_debugServer.setPauseOnExceptionsState(pause ? ScriptDebugServer::PauseOnAllExceptions : ScriptDebugServer::DontPauseOnExceptions);
}

void JavaScriptDebuggerBlackBerry::pauseInDebugger()
{
    m_debugServer.setPauseOnNextStatement(true);
}

void JavaScriptDebuggerBlackBerry::resumeDebugger()
{
    m_debugServer.continueProgram();
}

void JavaScriptDebuggerBlackBerry::stepOverStatementInDebugger()
{
    m_debugServer.stepOverStatement();
}

void JavaScriptDebuggerBlackBerry::stepIntoStatementInDebugger()
{
    m_debugServer.stepIntoStatement();
}

void JavaScriptDebuggerBlackBerry::stepOutOfFunctionInDebugger()
{
    m_debugServer.stepOutOfFunction();
}

void JavaScriptDebuggerBlackBerry::didParseSource(const String& sourceID, const Script& script)
{
    m_webPagePrivate->m_client->javascriptSourceParsed(script.url.characters(), script.url.length(), script.source.characters(), script.source.length());
}

void JavaScriptDebuggerBlackBerry::failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage)
{
    m_webPagePrivate->m_client->javascriptParsingFailed(url.impl()->characters(), url.length(), errorMessage.impl()->characters(), errorMessage.length(), errorLine);
}

void JavaScriptDebuggerBlackBerry::didPause(ScriptState*, const ScriptValue& callFrames, const ScriptValue& exception)
{
    String stacks;

    m_currentCallFrame = m_debugServer.currentCallFrame();
    JavaScriptCallFrame* frame = m_currentCallFrame;

    while (frame && frame->isValid()) {
        JSC::SourceProvider* provider = reinterpret_cast<JSC::SourceProvider*>(frame->sourceID());
        String url(provider->url().characters(), provider->url().length());
        if (url.length())
            stacks += url;
        stacks += ": ";

        if (frame->type() == JSC::DebuggerCallFrame::FunctionType) {
            String name = frame->functionName();
            if (name.length())
                stacks += name;
        }
        stacks += "(): ";

        String line = String::number(frame->line());
        stacks += line + "\n";

        frame = frame->caller();
    }

    m_webPagePrivate->m_client->javascriptPaused(reinterpret_cast<const unsigned short*>(stacks.characters()), stacks.length());
}

void JavaScriptDebuggerBlackBerry::didContinue()
{
    m_currentCallFrame = 0;
    m_webPagePrivate->m_client->javascriptContinued();
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
