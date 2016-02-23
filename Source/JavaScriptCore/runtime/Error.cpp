/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel (eric@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Error.h"

#include "ConstructData.h"
#include "ErrorConstructor.h"
#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "NativeErrorConstructor.h"
#include "JSCInlines.h"
#include "SourceCode.h"

#include <wtf/text/StringBuilder.h>

namespace JSC {

static const char* linePropertyName = "line";
static const char* sourceURLPropertyName = "sourceURL";

JSObject* createError(ExecState* exec, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return ErrorInstance::create(exec, globalObject->vm(), globalObject->errorStructure(), message, appender, TypeNothing, true);
}

JSObject* createEvalError(ExecState* exec, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return ErrorInstance::create(exec, globalObject->vm(), globalObject->evalErrorConstructor()->errorStructure(), message, appender, TypeNothing, true);
}

JSObject* createRangeError(ExecState* exec, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return ErrorInstance::create(exec, globalObject->vm(), globalObject->rangeErrorConstructor()->errorStructure(), message, appender, TypeNothing, true);
}

JSObject* createReferenceError(ExecState* exec, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return ErrorInstance::create(exec, globalObject->vm(), globalObject->referenceErrorConstructor()->errorStructure(), message, appender, TypeNothing, true);
}

JSObject* createSyntaxError(ExecState* exec, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return ErrorInstance::create(exec, globalObject->vm(), globalObject->syntaxErrorConstructor()->errorStructure(), message, appender, TypeNothing, true);
}

JSObject* createTypeError(ExecState* exec, const String& message, ErrorInstance::SourceAppender appender, RuntimeType type)
{
    ASSERT(!message.isEmpty());
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return ErrorInstance::create(exec, globalObject->vm(), globalObject->typeErrorConstructor()->errorStructure(), message, appender, type, true);
}

JSObject* createNotEnoughArgumentsError(ExecState* exec, ErrorInstance::SourceAppender appender)
{
    return createTypeError(exec, ASCIILiteral("Not enough arguments"), appender, TypeNothing);
}

JSObject* createURIError(ExecState* exec, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return ErrorInstance::create(exec, globalObject->vm(), globalObject->URIErrorConstructor()->errorStructure(), message, appender, TypeNothing, true);
}

JSObject* createOutOfMemoryError(ExecState* exec, ErrorInstance::SourceAppender appender) 
{
    return createError(exec, ASCIILiteral("Out of memory"), appender);
}


class FindFirstCallerFrameWithCodeblockFunctor {
public:
    FindFirstCallerFrameWithCodeblockFunctor(CallFrame* startCallFrame)
        : m_startCallFrame(startCallFrame)
        , m_foundCallFrame(nullptr)
        , m_foundStartCallFrame(false)
        , m_index(0)
    { }

    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        if (!m_foundStartCallFrame && (visitor->callFrame() == m_startCallFrame))
            m_foundStartCallFrame = true;

        if (m_foundStartCallFrame) {
            if (visitor->callFrame()->codeBlock()) {
                m_foundCallFrame = visitor->callFrame();
                return StackVisitor::Done;
            }
            m_index++;
        }

        return StackVisitor::Continue;
    }

    CallFrame* foundCallFrame() const { return m_foundCallFrame; }
    unsigned index() const { return m_index; }

private:
    CallFrame* m_startCallFrame;
    CallFrame* m_foundCallFrame;
    bool m_foundStartCallFrame;
    unsigned m_index;
};

bool addErrorInfoAndGetBytecodeOffset(ExecState* exec, VM& vm, JSObject* obj, bool useCurrentFrame, CallFrame*& callFrame, unsigned &bytecodeOffset) 
{
    Vector<StackFrame> stackTrace = Vector<StackFrame>();

    if (exec && stackTrace.isEmpty())
        vm.interpreter->getStackTrace(stackTrace);

    if (!stackTrace.isEmpty()) {

        ASSERT(exec == vm.topCallFrame || exec == exec->lexicalGlobalObject()->globalExec() || exec == exec->vmEntryGlobalObject()->globalExec());

        StackFrame* stackFrame;
        for (unsigned i = 0 ; i < stackTrace.size(); ++i) {
            stackFrame = &stackTrace.at(i);
            if (stackFrame->bytecodeOffset)
                break;
        }

        if (bytecodeOffset) {
            FindFirstCallerFrameWithCodeblockFunctor functor(exec);
            vm.topCallFrame->iterate(functor);
            callFrame = functor.foundCallFrame();
            unsigned stackIndex = functor.index();
            bytecodeOffset = stackTrace.at(stackIndex).bytecodeOffset;
        }
        
        unsigned line;
        unsigned column;
        stackFrame->computeLineAndColumn(line, column);
        obj->putDirect(vm, vm.propertyNames->line, jsNumber(line), ReadOnly | DontDelete);
        obj->putDirect(vm, vm.propertyNames->column, jsNumber(column), ReadOnly | DontDelete);

        if (!stackFrame->sourceURL.isEmpty())
            obj->putDirect(vm, vm.propertyNames->sourceURL, jsString(&vm, stackFrame->sourceURL), ReadOnly | DontDelete);
    
        if (!useCurrentFrame)
            stackTrace.remove(0);
        obj->putDirect(vm, vm.propertyNames->stack, vm.interpreter->stackTraceAsString(vm.topCallFrame, stackTrace), DontEnum);

        return true;
    }
    return false;
}

void addErrorInfo(ExecState* exec, JSObject* obj, bool useCurrentFrame)
{
    CallFrame* callFrame = nullptr;
    unsigned bytecodeOffset = 0;
    addErrorInfoAndGetBytecodeOffset(exec, exec->vm(), obj, useCurrentFrame, callFrame, bytecodeOffset);
}

JSObject* addErrorInfo(CallFrame* callFrame, JSObject* error, int line, const SourceCode& source)
{
    VM* vm = &callFrame->vm();
    const String& sourceURL = source.provider()->url();

    if (line != -1)
        error->putDirect(*vm, Identifier::fromString(vm, linePropertyName), jsNumber(line), ReadOnly | DontDelete);
    if (!sourceURL.isNull())
        error->putDirect(*vm, Identifier::fromString(vm, sourceURLPropertyName), jsString(vm, sourceURL), ReadOnly | DontDelete);
    return error;
}


bool hasErrorInfo(ExecState* exec, JSObject* error)
{
    return error->hasProperty(exec, Identifier::fromString(exec, linePropertyName))
        || error->hasProperty(exec, Identifier::fromString(exec, sourceURLPropertyName));
}

JSObject* throwConstructorCannotBeCalledAsFunctionTypeError(ExecState* exec, const char* constructorName)
{
    return exec->vm().throwException(exec, createTypeError(exec, makeString("calling ", constructorName, " constructor without new is invalid")));
}

JSObject* throwTypeError(ExecState* exec)
{
    return exec->vm().throwException(exec, createTypeError(exec));
}

JSObject* throwSyntaxError(ExecState* exec)
{
    return exec->vm().throwException(exec, createSyntaxError(exec, ASCIILiteral("Syntax error")));
}

JSObject* throwSyntaxError(ExecState* exec, const String& message)
{
    return exec->vm().throwException(exec, createSyntaxError(exec, message));
}

JSObject* createError(ExecState* exec, const String& message)
{
    return createError(exec, message, nullptr);
}

JSObject* createEvalError(ExecState* exec, const String& message)
{
    return createEvalError(exec, message, nullptr);
}

JSObject* createRangeError(ExecState* exec, const String& message)
{
    return createRangeError(exec, message, nullptr);
}

JSObject* createReferenceError(ExecState* exec, const String& message)
{
    return createReferenceError(exec, message, nullptr);
}

JSObject* createSyntaxError(ExecState* exec, const String& message)
{
    return createSyntaxError(exec, message, nullptr);
}

JSObject* createTypeError(ExecState* exec)
{
    return createTypeError(exec, ASCIILiteral("Type error"));
}

JSObject* createTypeError(ExecState* exec, const String& message)
{
    return createTypeError(exec, message, nullptr, TypeNothing);
}

JSObject* createNotEnoughArgumentsError(ExecState* exec)
{
    return createNotEnoughArgumentsError(exec, nullptr);
}

JSObject* createURIError(ExecState* exec, const String& message)
{
    return createURIError(exec, message, nullptr);
}

JSObject* createOutOfMemoryError(ExecState* exec)
{
    return createOutOfMemoryError(exec, nullptr);
}


const ClassInfo StrictModeTypeErrorFunction::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(StrictModeTypeErrorFunction) };

void StrictModeTypeErrorFunction::destroy(JSCell* cell)
{
    static_cast<StrictModeTypeErrorFunction*>(cell)->StrictModeTypeErrorFunction::~StrictModeTypeErrorFunction();
}

} // namespace JSC
