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

#include "CollectorHeapIterator.h"
#include "Error.h"
#include "Interpreter.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "Parser.h"
#include "Protect.h"

namespace JSC {

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

void Debugger::recompileAllJSFunctions(JSGlobalData* globalData)
{
    // If JavaScript is running, it's not safe to recompile, since we'll end
    // up throwing away code that is live on the stack.
    ASSERT(!globalData->dynamicGlobalObject);
    if (globalData->dynamicGlobalObject)
        return;

    typedef HashSet<FunctionExecutable*> FunctionExecutableSet;
    typedef HashMap<SourceProvider*, ExecState*> SourceProviderMap;

    FunctionExecutableSet functionExecutables;
    SourceProviderMap sourceProviders;

    LiveObjectIterator it = globalData->heap.primaryHeapBegin();
    LiveObjectIterator heapEnd = globalData->heap.primaryHeapEnd();
    for ( ; it != heapEnd; ++it) {
        if (!(*it)->inherits(&JSFunction::info))
            continue;

        JSFunction* function = asFunction(*it);
        if (function->executable()->isHostFunction())
            continue;

        FunctionExecutable* executable = function->jsExecutable();

        // Check if the function is already in the set - if so,
        // we've already retranslated it, nothing to do here.
        if (!functionExecutables.add(executable).second)
            continue;

        ExecState* exec = function->scope().globalObject()->JSGlobalObject::globalExec();
        executable->recompile(exec);
        if (function->scope().globalObject()->debugger() == this)
            sourceProviders.add(executable->source().provider(), exec);
    }

    // Call sourceParsed() after reparsing all functions because it will execute
    // JavaScript in the inspector.
    SourceProviderMap::const_iterator end = sourceProviders.end();
    for (SourceProviderMap::const_iterator iter = sourceProviders.begin(); iter != end; ++iter)
        sourceParsed(iter->second, SourceCode(iter->first), -1, UString());
}

JSValue evaluateInGlobalCallFrame(const UString& script, JSValue& exception, JSGlobalObject* globalObject)
{
    CallFrame* globalCallFrame = globalObject->globalExec();

    RefPtr<EvalExecutable> eval = EvalExecutable::create(globalCallFrame, makeSource(script));
    JSObject* error = eval->compile(globalCallFrame, globalCallFrame->scopeChain());
    if (error)
        return error;

    return globalObject->globalData()->interpreter->execute(eval.get(), globalCallFrame, globalObject, globalCallFrame->scopeChain(), &exception);
}

} // namespace JSC
