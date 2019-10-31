/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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
#include "Interpreter.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "NativeErrorConstructor.h"
#include "SourceCode.h"
#include "StackFrame.h"
#include "SuperSampler.h"

namespace JSC {

JSObject* createError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(), message, appender, TypeNothing, true);
}

JSObject* createEvalError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::EvalError), message, appender, TypeNothing, true);
}

JSObject* createRangeError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::RangeError), message, appender, TypeNothing, true);
}

JSObject* createReferenceError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::ReferenceError), message, appender, TypeNothing, true);
}

JSObject* createSyntaxError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::SyntaxError), message, appender, TypeNothing, true);
}

JSObject* createTypeError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender, RuntimeType type)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::TypeError), message, appender, type, true);
}

JSObject* createNotEnoughArgumentsError(JSGlobalObject* globalObject, ErrorInstance::SourceAppender appender)
{
    return createTypeError(globalObject, "Not enough arguments"_s, appender, TypeNothing);
}

JSObject* createURIError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::URIError), message, appender, TypeNothing, true);
}

JSObject* createError(JSGlobalObject* globalObject, ErrorType errorType, const String& message)
{
    switch (errorType) {
    case ErrorType::Error:
        return createError(globalObject, message);
    case ErrorType::EvalError:
        return createEvalError(globalObject, message);
    case ErrorType::RangeError:
        return createRangeError(globalObject, message);
    case ErrorType::ReferenceError:
        return createReferenceError(globalObject, message);
    case ErrorType::SyntaxError:
        return createSyntaxError(globalObject, message);
    case ErrorType::TypeError:
        return createTypeError(globalObject, message);
    case ErrorType::URIError:
        return createURIError(globalObject, message);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

JSObject* createGetterTypeError(JSGlobalObject* globalObject, const String& message)
{
    ASSERT(!message.isEmpty());
    auto* error = ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::TypeError), message);
    error->setNativeGetterTypeError();
    return error;
}

class FindFirstCallerFrameWithCodeblockFunctor {
public:
    FindFirstCallerFrameWithCodeblockFunctor(CallFrame* startCallFrame)
        : m_startCallFrame(startCallFrame)
        , m_foundCallFrame(nullptr)
        , m_foundStartCallFrame(false)
        , m_index(0)
    { }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (!m_foundStartCallFrame && (visitor->callFrame() == m_startCallFrame))
            m_foundStartCallFrame = true;

        if (m_foundStartCallFrame) {
            if (!visitor->isWasmFrame() && visitor->callFrame()->codeBlock()) {
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
    mutable CallFrame* m_foundCallFrame;
    mutable bool m_foundStartCallFrame;
    mutable unsigned m_index;
};

std::unique_ptr<Vector<StackFrame>> getStackTrace(JSGlobalObject*, VM& vm, JSObject* obj, bool useCurrentFrame)
{
    JSGlobalObject* globalObject = obj->globalObject(vm);
    if (!globalObject->stackTraceLimit())
        return nullptr;

    size_t framesToSkip = useCurrentFrame ? 0 : 1;
    std::unique_ptr<Vector<StackFrame>> stackTrace = makeUnique<Vector<StackFrame>>();
    vm.interpreter->getStackTrace(obj, *stackTrace, framesToSkip, globalObject->stackTraceLimit().value());
    return stackTrace;
}

void getBytecodeIndex(VM& vm, CallFrame* startCallFrame, Vector<StackFrame>* stackTrace, CallFrame*& callFrame, BytecodeIndex& bytecodeIndex)
{
    FindFirstCallerFrameWithCodeblockFunctor functor(startCallFrame);
    StackVisitor::visit(vm.topCallFrame, vm, functor);
    callFrame = functor.foundCallFrame();
    unsigned stackIndex = functor.index();
    bytecodeIndex = BytecodeIndex(0);
    if (stackTrace && stackIndex < stackTrace->size() && stackTrace->at(stackIndex).hasBytecodeIndex())
        bytecodeIndex = stackTrace->at(stackIndex).bytecodeIndex();
}

bool getLineColumnAndSource(Vector<StackFrame>* stackTrace, unsigned& line, unsigned& column, String& sourceURL)
{
    line = 0;
    column = 0;
    sourceURL = String();
    
    if (!stackTrace)
        return false;
    
    for (unsigned i = 0 ; i < stackTrace->size(); ++i) {
        StackFrame& frame = stackTrace->at(i);
        if (frame.hasLineAndColumnInfo()) {
            frame.computeLineAndColumn(line, column);
            sourceURL = frame.sourceURL();
            return true;
        }
    }
    
    return false;
}

bool addErrorInfo(VM& vm, Vector<StackFrame>* stackTrace, JSObject* obj)
{
    if (!stackTrace)
        return false;

    if (!stackTrace->isEmpty()) {
        unsigned line;
        unsigned column;
        String sourceURL;
        getLineColumnAndSource(stackTrace, line, column, sourceURL);
        obj->putDirect(vm, vm.propertyNames->line, jsNumber(line));
        obj->putDirect(vm, vm.propertyNames->column, jsNumber(column));
        if (!sourceURL.isEmpty())
            obj->putDirect(vm, vm.propertyNames->sourceURL, jsString(vm, sourceURL));

        obj->putDirect(vm, vm.propertyNames->stack, jsString(vm, Interpreter::stackTraceAsString(vm, *stackTrace)), static_cast<unsigned>(PropertyAttribute::DontEnum));

        return true;
    }

    obj->putDirect(vm, vm.propertyNames->stack, vm.smallStrings.emptyString(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    return false;
}

void addErrorInfo(JSGlobalObject* globalObject, JSObject* obj, bool useCurrentFrame)
{
    VM& vm = globalObject->vm();
    std::unique_ptr<Vector<StackFrame>> stackTrace = getStackTrace(globalObject, vm, obj, useCurrentFrame);
    addErrorInfo(vm, stackTrace.get(), obj);
}

JSObject* addErrorInfo(VM& vm, JSObject* error, int line, const SourceCode& source)
{
    const String& sourceURL = source.provider()->url();
    
    // The putDirect() calls below should really be put() so that they trigger materialization of
    // the line/sourceURL properties. Otherwise, what we set here will just be overwritten later.
    // But calling put() would be bad because we'd rather not do effectful things here. Luckily, we
    // know that this will get called on some kind of error - so we can just directly ask the
    // ErrorInstance to materialize whatever it needs to. There's a chance that we get passed some
    // other kind of object, which also has materializable properties. But this code is heuristic-ey
    // enough that if we're wrong in such corner cases, it's not the end of the world.
    if (ErrorInstance* errorInstance = jsDynamicCast<ErrorInstance*>(vm, error))
        errorInstance->materializeErrorInfoIfNeeded(vm);
    
    // FIXME: This does not modify the column property, which confusingly continues to reflect
    // the column at which the exception was thrown.
    // https://bugs.webkit.org/show_bug.cgi?id=176673
    if (line != -1)
        error->putDirect(vm, vm.propertyNames->line, jsNumber(line));
    if (!sourceURL.isNull())
        error->putDirect(vm, vm.propertyNames->sourceURL, jsString(vm, sourceURL));
    return error;
}

Exception* throwConstructorCannotBeCalledAsFunctionTypeError(JSGlobalObject* globalObject, ThrowScope& scope, const char* constructorName)
{
    return throwTypeError(globalObject, scope, makeString("calling ", constructorName, " constructor without new is invalid"));
}

Exception* throwTypeError(JSGlobalObject* globalObject, ThrowScope& scope)
{
    return throwException(globalObject, scope, createTypeError(globalObject));
}

Exception* throwTypeError(JSGlobalObject* globalObject, ThrowScope& scope, ASCIILiteral errorMessage)
{
    return throwTypeError(globalObject, scope, String(errorMessage));
}

Exception* throwTypeError(JSGlobalObject* globalObject, ThrowScope& scope, const String& message)
{
    return throwException(globalObject, scope, createTypeError(globalObject, message));
}

Exception* throwSyntaxError(JSGlobalObject* globalObject, ThrowScope& scope)
{
    return throwException(globalObject, scope, createSyntaxError(globalObject, "Syntax error"_s));
}

Exception* throwSyntaxError(JSGlobalObject* globalObject, ThrowScope& scope, const String& message)
{
    return throwException(globalObject, scope, createSyntaxError(globalObject, message));
}

Exception* throwGetterTypeError(JSGlobalObject* globalObject, ThrowScope& scope, const String& message)
{
    return throwException(globalObject, scope, createGetterTypeError(globalObject, message));
}

JSValue throwDOMAttributeGetterTypeError(JSGlobalObject* globalObject, ThrowScope& scope, const ClassInfo* classInfo, PropertyName propertyName)
{
    return throwGetterTypeError(globalObject, scope, makeString("The ", classInfo->className, '.', String(propertyName.uid()), " getter can only be used on instances of ", classInfo->className));
}

JSObject* createError(JSGlobalObject* globalObject, const String& message)
{
    return createError(globalObject, message, nullptr);
}

JSObject* createEvalError(JSGlobalObject* globalObject, const String& message)
{
    return createEvalError(globalObject, message, nullptr);
}

JSObject* createRangeError(JSGlobalObject* globalObject, const String& message)
{
    return createRangeError(globalObject, message, nullptr);
}

JSObject* createReferenceError(JSGlobalObject* globalObject, const String& message)
{
    return createReferenceError(globalObject, message, nullptr);
}

JSObject* createSyntaxError(JSGlobalObject* globalObject, const String& message)
{
    return createSyntaxError(globalObject, message, nullptr);
}

JSObject* createTypeError(JSGlobalObject* globalObject)
{
    return createTypeError(globalObject, "Type error"_s);
}

JSObject* createTypeError(JSGlobalObject* globalObject, const String& message)
{
    return createTypeError(globalObject, message, nullptr, TypeNothing);
}

JSObject* createNotEnoughArgumentsError(JSGlobalObject* globalObject)
{
    return createNotEnoughArgumentsError(globalObject, nullptr);
}

JSObject* createURIError(JSGlobalObject* globalObject, const String& message)
{
    return createURIError(globalObject, message, nullptr);
}

JSObject* createOutOfMemoryError(JSGlobalObject* globalObject)
{
    auto* error = createError(globalObject, "Out of memory"_s, nullptr);
    jsCast<ErrorInstance*>(error)->setOutOfMemoryError();
    return error;
}

JSObject* createOutOfMemoryError(JSGlobalObject* globalObject, const String& message)
{
    
    auto* error = createError(globalObject, makeString("Out of memory: ", message), nullptr);
    jsCast<ErrorInstance*>(error)->setOutOfMemoryError();
    return error;
}

} // namespace JSC
