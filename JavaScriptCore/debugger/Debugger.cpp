/*
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "Debugger.h"

#include "JSGlobalObject.h"
#include "Interpreter.h"
#include "Parser.h"

namespace JSC {

Debugger::Debugger()
{
}

Debugger::~Debugger()
{
    HashSet<JSGlobalObject*>::iterator end = m_globalObjects.end();
    for (HashSet<JSGlobalObject*>::iterator it = m_globalObjects.begin(); it != end; ++it)
        (*it)->setDebugger(0);
}

void Debugger::attach(JSGlobalObject* globalObject)
{
    ASSERT(!globalObject->debugger());
    globalObject->setDebugger(this);
    m_globalObjects.add(globalObject);
}

void Debugger::detach(JSGlobalObject* globalObject)
{
    ASSERT(m_globalObjects.contains(globalObject));
    m_globalObjects.remove(globalObject);
    globalObject->setDebugger(0);
}

JSValue evaluateInGlobalCallFrame(const UString& script, JSValue& exception, JSGlobalObject* globalObject)
{
    CallFrame* globalCallFrame = globalObject->globalExec();

    int errLine;
    UString errMsg;
    SourceCode source = makeSource(script);
    RefPtr<EvalNode> evalNode = globalObject->globalData()->parser->parse<EvalNode>(globalCallFrame, globalObject->debugger(), source, &errLine, &errMsg);
    if (!evalNode)
        return Error::create(globalCallFrame, SyntaxError, errMsg, errLine, source.provider()->asID(), source.provider()->url());

    return globalObject->globalData()->interpreter->execute(evalNode.get(), globalCallFrame, globalObject, globalCallFrame->scopeChain(), &exception);
}

} // namespace JSC
