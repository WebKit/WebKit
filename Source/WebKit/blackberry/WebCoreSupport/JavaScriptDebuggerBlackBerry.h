/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
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

#ifndef JavaScriptDebuggerBlackBerry_h
#define JavaScriptDebuggerBlackBerry_h

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "ScriptDebugListener.h"

namespace BlackBerry {
namespace WebKit {
class WebPagePrivate;
}
}

namespace WebCore {

class JavaScriptCallFrame;
class PageScriptDebugServer;

class JavaScriptDebuggerBlackBerry : public ScriptDebugListener {
public:
    JavaScriptDebuggerBlackBerry(BlackBerry::WebKit::WebPagePrivate*);
    ~JavaScriptDebuggerBlackBerry();

    void addBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber, const unsigned short* condition, unsigned conditionLength);
    void updateBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber, const unsigned short* condition, unsigned conditionLength);
    void removeBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber);

    bool pauseOnExceptions();
    void setPauseOnExceptions(bool);

    void pauseInDebugger();
    void resumeDebugger();

    void stepOverStatementInDebugger();
    void stepIntoStatementInDebugger();
    void stepOutOfFunctionInDebugger();

    // From ScriptDebugListener
    virtual void didParseSource(const String&  sourceID, const Script&);
    virtual void failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage);
    virtual void didPause(ScriptState*, const ScriptValue& callFrames, const ScriptValue& exception);
    virtual void didContinue();

protected:
    void start();
    void stop();

private:
    BlackBerry::WebKit::WebPagePrivate* m_webPagePrivate;
    PageScriptDebugServer& m_debugServer;

    JavaScriptCallFrame* m_currentCallFrame;
};

} // WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
#endif // JavaScriptDebuggerBlackBerry_h
