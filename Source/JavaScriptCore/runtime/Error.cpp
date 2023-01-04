/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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

#include "ExecutableBaseInlines.h"
#include "Interpreter.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "SourceCode.h"
#include "StackFrame.h"

namespace JSC {

JSObject* createError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(), message, JSValue(), appender, TypeNothing, ErrorType::Error, true);
}

JSObject* createEvalError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::EvalError), message, JSValue(), appender, TypeNothing, ErrorType::EvalError, true);
}

JSObject* createRangeError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::RangeError), message, JSValue(), appender, TypeNothing, ErrorType::RangeError, true);
}

JSObject* createReferenceError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::ReferenceError), message, JSValue(), appender, TypeNothing, ErrorType::ReferenceError, true);
}

JSObject* createSyntaxError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::SyntaxError), message, JSValue(), appender, TypeNothing, ErrorType::SyntaxError, true);
}

JSObject* createTypeError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender, RuntimeType type)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::TypeError), message, JSValue(), appender, type, ErrorType::TypeError, true);
}

JSObject* createNotEnoughArgumentsError(JSGlobalObject* globalObject, ErrorInstance::SourceAppender appender)
{
    return createTypeError(globalObject, "Not enough arguments"_s, appender, TypeNothing);
}

JSObject* createURIError(JSGlobalObject* globalObject, const String& message, ErrorInstance::SourceAppender appender)
{
    ASSERT(!message.isEmpty());
    return ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::URIError), message, JSValue(), appender, TypeNothing, ErrorType::URIError, true);
}

JSObject* createError(JSGlobalObject* globalObject, ErrorType errorType, const String& message)
{
    return createError(globalObject, static_cast<ErrorTypeWithExtension>(errorType), message);
}

JSObject* createError(JSGlobalObject* globalObject, ErrorTypeWithExtension errorType, const String& message)
{
    switch (errorType) {
    case ErrorTypeWithExtension::Error:
        return createError(globalObject, message);
    case ErrorTypeWithExtension::EvalError:
        return createEvalError(globalObject, message);
    case ErrorTypeWithExtension::RangeError:
        return createRangeError(globalObject, message);
    case ErrorTypeWithExtension::ReferenceError:
        return createReferenceError(globalObject, message);
    case ErrorTypeWithExtension::SyntaxError:
        return createSyntaxError(globalObject, message);
    case ErrorTypeWithExtension::TypeError:
        return createTypeError(globalObject, message);
    case ErrorTypeWithExtension::URIError:
        return createURIError(globalObject, message);
    case ErrorTypeWithExtension::AggregateError:
        break;
    case ErrorTypeWithExtension::OutOfMemoryError:
        return createOutOfMemoryError(globalObject, message);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static JSObject* createGetterTypeError(JSGlobalObject* globalObject, const String& message)
{
    ASSERT(!message.isEmpty());
    auto* error = ErrorInstance::create(globalObject, globalObject->vm(), globalObject->errorStructure(ErrorType::TypeError), message, JSValue(), nullptr, TypeNothing, ErrorType::TypeError);
    error->setNativeGetterTypeError();
    return error;
}

class FindFirstCallerFrameWithCodeblockFunctor {
public:
    FindFirstCallerFrameWithCodeblockFunctor(CallFrame* startCallFrame)
        : m_startCallFrame(startCallFrame)
    { }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        if (!m_foundStartCallFrame && (visitor->callFrame() == m_startCallFrame))
            m_foundStartCallFrame = true;

        if (!m_foundStartCallFrame)
            return IterationStatus::Continue;

        if (visitor->isWasmFrame())
            return IterationStatus::Continue;

        if (visitor->isImplementationVisibilityPrivate())
            return IterationStatus::Continue;

        auto* codeBlock = visitor->codeBlock();
        if (!codeBlock)
            return IterationStatus::Continue;

        m_codeBlock = codeBlock;

        if (!codeBlock->unlinkedCodeBlock()->isBuiltinFunction())
            m_bytecodeIndex = visitor->bytecodeIndex();
        return IterationStatus::Done;
    }

    CodeBlock* codeBlock() const { return m_codeBlock; }
    BytecodeIndex bytecodeIndex() const { return m_bytecodeIndex; }

private:
    CallFrame* m_startCallFrame;
    mutable CodeBlock* m_codeBlock { nullptr };
    mutable bool m_foundStartCallFrame { false };
    mutable BytecodeIndex m_bytecodeIndex { 0 };
};

std::unique_ptr<Vector<StackFrame>> getStackTrace(JSGlobalObject*, VM& vm, JSObject* obj, bool useCurrentFrame)
{
    JSGlobalObject* globalObject = obj->globalObject();
    if (!globalObject->stackTraceLimit())
        return nullptr;

    size_t framesToSkip = useCurrentFrame ? 0 : 1;
    std::unique_ptr<Vector<StackFrame>> stackTrace = makeUnique<Vector<StackFrame>>();
    vm.interpreter.getStackTrace(obj, *stackTrace, framesToSkip, globalObject->stackTraceLimit().value());
    return stackTrace;
}

std::tuple<CodeBlock*, BytecodeIndex> getBytecodeIndex(VM& vm, CallFrame* startCallFrame)
{
    FindFirstCallerFrameWithCodeblockFunctor functor(startCallFrame);
    StackVisitor::visit(vm.topCallFrame, vm, functor);
    return { functor.codeBlock(), functor.bytecodeIndex() };
}

bool getLineColumnAndSource(VM& vm, Vector<StackFrame>* stackTrace, unsigned& line, unsigned& column, String& sourceURL)
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
            sourceURL = frame.sourceURL(vm);
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
        getLineColumnAndSource(vm, stackTrace, line, column, sourceURL);
        obj->putDirect(vm, vm.propertyNames->line, jsNumber(line));
        obj->putDirect(vm, vm.propertyNames->column, jsNumber(column));
        if (!sourceURL.isEmpty())
            obj->putDirect(vm, vm.propertyNames->sourceURL, jsString(vm, WTFMove(sourceURL)));

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
    const String& sourceURL = source.provider()->sourceURL();
    
    // The putDirect() calls below should really be put() so that they trigger materialization of
    // the line/sourceURL properties. Otherwise, what we set here will just be overwritten later.
    // But calling put() would be bad because we'd rather not do effectful things here. Luckily, we
    // know that this will get called on some kind of error - so we can just directly ask the
    // ErrorInstance to materialize whatever it needs to. There's a chance that we get passed some
    // other kind of object, which also has materializable properties. But this code is heuristic-ey
    // enough that if we're wrong in such corner cases, it's not the end of the world.
    if (ErrorInstance* errorInstance = jsDynamicCast<ErrorInstance*>(error))
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

JSObject* createTypeErrorCopy(JSGlobalObject* globalObject, JSValue error)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    String errorString = "Error encountered during evaluation"_s;

    if (error.isPrimitive()) {
        errorString = error.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    } else if (error.isObject()) {
        auto structure = error.asCell()->structure();
        if (!structure->isProxy()) {
            auto slot = PropertySlot(error, PropertySlot::InternalMethodType::GetOwnProperty);
            bool found = error.getOwnPropertySlot(globalObject, vm.propertyNames->message, slot);
            RETURN_IF_EXCEPTION(scope, { });
            if (found) {
                if (slot.isValue()) {
                    JSValue message = slot.getValue(globalObject, vm.propertyNames->message);
                    RETURN_IF_EXCEPTION(scope, { });
                    errorString = message.toWTFString(globalObject);
                    RETURN_IF_EXCEPTION(scope, { });
                }
            }
        }
    }

    return createTypeError(globalObject, errorString);
}

String makeDOMAttributeGetterTypeErrorMessage(const char* interfaceName, const String& attributeName)
{
    return makeString("The ", interfaceName, '.', attributeName, " getter can only be used on instances of ", interfaceName);
}

String makeDOMAttributeSetterTypeErrorMessage(const char* interfaceName, const String& attributeName)
{
    return makeString("The ", interfaceName, '.', attributeName, " setter can only be used on instances of ", interfaceName);
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
    return throwException(globalObject, scope, createSyntaxError(globalObject));
}

Exception* throwSyntaxError(JSGlobalObject* globalObject, ThrowScope& scope, const String& message)
{
    return throwException(globalObject, scope, createSyntaxError(globalObject, message));
}

JSValue throwDOMAttributeGetterTypeError(JSGlobalObject* globalObject, ThrowScope& scope, const ClassInfo* classInfo, PropertyName propertyName)
{
    return throwException(globalObject, scope, createGetterTypeError(globalObject, makeDOMAttributeGetterTypeErrorMessage(classInfo->className, String(propertyName.uid()))));
}

JSValue throwDOMAttributeSetterTypeError(JSGlobalObject* globalObject, ThrowScope& scope, const ClassInfo* classInfo, PropertyName propertyName)
{
    return throwException(globalObject, scope, createTypeError(globalObject, makeDOMAttributeSetterTypeErrorMessage(classInfo->className, String(propertyName.uid()))));
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

JSObject* createSyntaxError(JSGlobalObject* globalObject)
{
    return createSyntaxError(globalObject, "Syntax error"_s, nullptr);
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
    auto* error = createRangeError(globalObject, "Out of memory"_s, nullptr);
    jsCast<ErrorInstance*>(error)->setOutOfMemoryError();
    return error;
}

JSObject* createOutOfMemoryError(JSGlobalObject* globalObject, const String& message)
{
    if (message.isEmpty())
        return createOutOfMemoryError(globalObject);
    auto* error = createRangeError(globalObject, makeString("Out of memory: ", message), nullptr);
    jsCast<ErrorInstance*>(error)->setOutOfMemoryError();
    return error;
}

} // namespace JSC
