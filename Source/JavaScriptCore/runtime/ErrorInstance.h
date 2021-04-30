/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "ErrorType.h"
#include "JSObject.h"
#include "RuntimeType.h"
#include "StackFrame.h"

namespace JSC {

class ErrorInstance : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetOwnSpecialPropertyNames | OverridesPut;
    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<ErrorInstance*>(cell)->ErrorInstance::~ErrorInstance();
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.errorInstanceSpace<mode>();
    }

    enum SourceTextWhereErrorOccurred { FoundExactSource, FoundApproximateSource };
    typedef String (*SourceAppender) (const String& originalMessage, const String& sourceText, RuntimeType, SourceTextWhereErrorOccurred);

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ErrorInstanceType, StructureFlags), info());
    }

    static ErrorInstance* create(JSGlobalObject* globalObject, VM& vm, Structure* structure, const String& message, JSValue cause, SourceAppender appender = nullptr, RuntimeType type = TypeNothing, ErrorType errorType = ErrorType::Error, bool useCurrentFrame = true)
    {
        ErrorInstance* instance = new (NotNull, allocateCell<ErrorInstance>(vm.heap)) ErrorInstance(vm, structure, errorType);
        instance->finishCreation(vm, globalObject, message, cause, appender, type, useCurrentFrame);
        return instance;
    }

    static ErrorInstance* create(JSGlobalObject*, Structure*, JSValue message, JSValue options, SourceAppender = nullptr, RuntimeType = TypeNothing, ErrorType = ErrorType::Error, bool useCurrentFrame = true);

    bool hasSourceAppender() const { return !!m_sourceAppender; }
    SourceAppender sourceAppender() const { return m_sourceAppender; }
    void setSourceAppender(SourceAppender appender) { m_sourceAppender = appender; }
    void clearSourceAppender() { m_sourceAppender = nullptr; }
    void setRuntimeTypeForCause(RuntimeType type) { m_runtimeTypeForCause = type; }
    RuntimeType runtimeTypeForCause() const { return m_runtimeTypeForCause; }
    void clearRuntimeTypeForCause() { m_runtimeTypeForCause = TypeNothing; }

    ErrorType errorType() const { return m_errorType; }
    void setStackOverflowError() { m_stackOverflowError = true; }
    bool isStackOverflowError() const { return m_stackOverflowError; }
    void setOutOfMemoryError() { m_outOfMemoryError = true; }
    bool isOutOfMemoryError() const { return m_outOfMemoryError; }

    void setNativeGetterTypeError() { m_nativeGetterTypeError = true; }
    bool isNativeGetterTypeError() const { return m_nativeGetterTypeError; }

    JS_EXPORT_PRIVATE String sanitizedToString(JSGlobalObject*);
    JS_EXPORT_PRIVATE String sanitizedMessageString(JSGlobalObject*);
    JS_EXPORT_PRIVATE String sanitizedNameString(JSGlobalObject*);
    
    Vector<StackFrame>* stackTrace() { return m_stackTrace.get(); }

    bool materializeErrorInfoIfNeeded(VM&);
    bool materializeErrorInfoIfNeeded(VM&, PropertyName);

    void finalizeUnconditionally(VM&);

protected:
    explicit ErrorInstance(VM&, Structure*, ErrorType);

    void finishCreation(VM&, JSGlobalObject*, const String& message, JSValue cause, SourceAppender = nullptr, RuntimeType = TypeNothing, bool useCurrentFrame = true);

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static void getOwnSpecialPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, DontEnumPropertiesMode);
    static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);

    void computeErrorInfo(VM&);

    SourceAppender m_sourceAppender { nullptr };
    std::unique_ptr<Vector<StackFrame>> m_stackTrace;
    unsigned m_line;
    unsigned m_column;
    String m_sourceURL;
    String m_stackString;
    RuntimeType m_runtimeTypeForCause { TypeNothing };
    ErrorType m_errorType { ErrorType::Error };
    bool m_stackOverflowError : 1;
    bool m_outOfMemoryError : 1;
    bool m_errorInfoMaterialized : 1;
    bool m_nativeGetterTypeError : 1;
};

} // namespace JSC
