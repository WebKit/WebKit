/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ErrorInstance_h
#define ErrorInstance_h

#include "Interpreter.h"
#include "RuntimeType.h"
#include "SourceProvider.h"

namespace JSC {

class ErrorInstance : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ErrorInstanceType, StructureFlags), info());
    }

    static ErrorInstance* create(VM& vm, Structure* structure, const String& message, Vector<StackFrame> stackTrace = Vector<StackFrame>())
    {
        ErrorInstance* instance = new (NotNull, allocateCell<ErrorInstance>(vm.heap)) ErrorInstance(vm, structure);
        instance->finishCreation(vm, message, stackTrace);
        return instance;
    }

    static ErrorInstance* create(ExecState* exec, Structure* structure, JSValue message, Vector<StackFrame> stackTrace = Vector<StackFrame>())
    {
        return create(exec->vm(), structure, message.isUndefined() ? String() : message.toString(exec)->value(exec), stackTrace);
    }

    enum SourceTextWhereErrorOccurred { FoundExactSource, FoundApproximateSource };
    typedef String (*SourceAppender) (const String& originalMessage, const String& sourceText, RuntimeType, SourceTextWhereErrorOccurred);

    bool hasSourceAppender() const { return !!m_sourceAppender; }
    SourceAppender sourceAppender() const { return m_sourceAppender; }
    void setSourceAppender(SourceAppender appender) { m_sourceAppender = appender; }
    void clearSourceAppender() { m_sourceAppender = nullptr; }
    void setRuntimeTypeForCause(RuntimeType type) { m_runtimeTypeForCause = type; }
    RuntimeType runtimeTypeForCause() const { return m_runtimeTypeForCause; }
    void clearRuntimeTypeForCause() { m_runtimeTypeForCause = TypeNothing; }

protected:
    explicit ErrorInstance(VM&, Structure*);

    void finishCreation(VM&, const String&, Vector<StackFrame> = Vector<StackFrame>());

    SourceAppender m_sourceAppender { nullptr };
    RuntimeType m_runtimeTypeForCause { TypeNothing };
};

} // namespace JSC

#endif // ErrorInstance_h
