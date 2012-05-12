/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich (cwzwarich@uwaterloo.ca)
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
#include "JSGlobalObject.h"

#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"

#include "Arguments.h"
#include "ArrayConstructor.h"
#include "ArrayPrototype.h"
#include "BooleanConstructor.h"
#include "BooleanPrototype.h"
#include "CodeBlock.h"
#include "DateConstructor.h"
#include "DatePrototype.h"
#include "Error.h"
#include "ErrorConstructor.h"
#include "ErrorPrototype.h"
#include "FunctionConstructor.h"
#include "FunctionPrototype.h"
#include "GetterSetter.h"
#include "JSBoundFunction.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSLock.h"
#include "JSONObject.h"
#include "Interpreter.h"
#include "Lookup.h"
#include "MathObject.h"
#include "NativeErrorConstructor.h"
#include "NativeErrorPrototype.h"
#include "NumberConstructor.h"
#include "NumberPrototype.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "Profiler.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "ScopeChainMark.h"
#include "StringConstructor.h"
#include "StringPrototype.h"
#include "Debugger.h"

#include "JSGlobalObject.lut.h"

namespace JSC {

const ClassInfo JSGlobalObject::s_info = { "GlobalObject", &JSVariableObject::s_info, 0, ExecState::globalObjectTable, CREATE_METHOD_TABLE(JSGlobalObject) };

const GlobalObjectMethodTable JSGlobalObject::s_globalObjectMethodTable = { &allowsAccessFrom, &supportsProfiling, &supportsRichSourceInfo, &shouldInterruptScript };

/* Source for JSGlobalObject.lut.h
@begin globalObjectTable
  parseInt              globalFuncParseInt              DontEnum|Function 2
  parseFloat            globalFuncParseFloat            DontEnum|Function 1
  isNaN                 globalFuncIsNaN                 DontEnum|Function 1
  isFinite              globalFuncIsFinite              DontEnum|Function 1
  escape                globalFuncEscape                DontEnum|Function 1
  unescape              globalFuncUnescape              DontEnum|Function 1
  decodeURI             globalFuncDecodeURI             DontEnum|Function 1
  decodeURIComponent    globalFuncDecodeURIComponent    DontEnum|Function 1
  encodeURI             globalFuncEncodeURI             DontEnum|Function 1
  encodeURIComponent    globalFuncEncodeURIComponent    DontEnum|Function 1
@end
*/

ASSERT_CLASS_FITS_IN_CELL(JSGlobalObject);

// Default number of ticks before a timeout check should be done.
static const int initialTickCountThreshold = 255;

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

template <typename T> static inline void visitIfNeeded(SlotVisitor& visitor, WriteBarrier<T>* v)
{
    if (*v)
        visitor.append(v);
}

JSGlobalObject::~JSGlobalObject()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (m_debugger)
        m_debugger->detach(this);

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (UNLIKELY(*profiler != 0)) {
        (*profiler)->stopProfiling(this);
    }
}

void JSGlobalObject::destroy(JSCell* cell)
{
    jsCast<JSGlobalObject*>(cell)->JSGlobalObject::~JSGlobalObject();
}

void JSGlobalObject::init(JSObject* thisValue)
{
    ASSERT(JSLock::currentThreadIsHoldingLock());
    
    structure()->disableSpecificFunctionTracking();

    m_globalScopeChain.set(globalData(), this, ScopeChainNode::create(0, this, &globalData(), this, thisValue));

    JSGlobalObject::globalExec()->init(0, 0, m_globalScopeChain.get(), CallFrame::noCaller(), 0, 0);

    m_debugger = 0;

    reset(prototype());
}

void JSGlobalObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (thisObject->symbolTablePut(exec, propertyName, value, slot.isStrictMode()))
        return;
    JSVariableObject::put(thisObject, exec, propertyName, value, slot);
}

void JSGlobalObject::putDirectVirtual(JSObject* object, ExecState* exec, PropertyName propertyName, JSValue value, unsigned attributes)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (thisObject->symbolTablePutWithAttributes(exec->globalData(), propertyName, value, attributes))
        return;

    JSValue valueBefore = thisObject->getDirect(exec->globalData(), propertyName);
    PutPropertySlot slot;
    JSVariableObject::put(thisObject, exec, propertyName, value, slot);
    if (!valueBefore) {
        JSValue valueAfter = thisObject->getDirect(exec->globalData(), propertyName);
        if (valueAfter)
            JSObject::putDirectVirtual(thisObject, exec, propertyName, valueAfter, attributes);
    }
}

bool JSGlobalObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor, bool shouldThrow)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    PropertySlot slot;
    // silently ignore attempts to add accessors aliasing vars.
    if (descriptor.isAccessorDescriptor() && thisObject->symbolTableGet(propertyName, slot))
        return false;
    return Base::defineOwnProperty(thisObject, exec, propertyName, descriptor, shouldThrow);
}


static inline JSObject* lastInPrototypeChain(JSObject* object)
{
    JSObject* o = object;
    while (o->prototype().isObject())
        o = asObject(o->prototype());
    return o;
}

void JSGlobalObject::reset(JSValue prototype)
{
    ExecState* exec = JSGlobalObject::globalExec();

    m_functionPrototype.set(exec->globalData(), this, FunctionPrototype::create(exec, this, FunctionPrototype::createStructure(exec->globalData(), this, jsNull()))); // The real prototype will be set once ObjectPrototype is created.
    m_functionStructure.set(exec->globalData(), this, JSFunction::createStructure(exec->globalData(), this, m_functionPrototype.get()));
    m_boundFunctionStructure.set(exec->globalData(), this, JSBoundFunction::createStructure(exec->globalData(), this, m_functionPrototype.get()));
    m_namedFunctionStructure.set(exec->globalData(), this, Structure::addPropertyTransition(exec->globalData(), m_functionStructure.get(), exec->globalData().propertyNames->name, DontDelete | ReadOnly | DontEnum, 0, m_functionNameOffset));
    m_internalFunctionStructure.set(exec->globalData(), this, InternalFunction::createStructure(exec->globalData(), this, m_functionPrototype.get()));
    JSFunction* callFunction = 0;
    JSFunction* applyFunction = 0;
    m_functionPrototype->addFunctionProperties(exec, this, &callFunction, &applyFunction);
    m_callFunction.set(exec->globalData(), this, callFunction);
    m_applyFunction.set(exec->globalData(), this, applyFunction);
    m_objectPrototype.set(exec->globalData(), this, ObjectPrototype::create(exec, this, ObjectPrototype::createStructure(exec->globalData(), this, jsNull())));
    GetterSetter* protoAccessor = GetterSetter::create(exec);
    protoAccessor->setGetter(exec->globalData(), JSFunction::create(exec, this, 0, "", globalFuncProtoGetter));
    protoAccessor->setSetter(exec->globalData(), JSFunction::create(exec, this, 0, "", globalFuncProtoSetter));
    m_objectPrototype->putDirectAccessor(exec->globalData(), exec->propertyNames().underscoreProto, protoAccessor, Accessor | DontEnum);
    m_functionPrototype->structure()->setPrototypeWithoutTransition(exec->globalData(), m_objectPrototype.get());

    m_emptyObjectStructure.set(exec->globalData(), this, m_objectPrototype->inheritorID(exec->globalData()));
    m_nullPrototypeObjectStructure.set(exec->globalData(), this, createEmptyObjectStructure(exec->globalData(), this, jsNull()));

    m_callbackFunctionStructure.set(exec->globalData(), this, JSCallbackFunction::createStructure(exec->globalData(), this, m_functionPrototype.get()));
    m_argumentsStructure.set(exec->globalData(), this, Arguments::createStructure(exec->globalData(), this, m_objectPrototype.get()));
    m_callbackConstructorStructure.set(exec->globalData(), this, JSCallbackConstructor::createStructure(exec->globalData(), this, m_objectPrototype.get()));
    m_callbackObjectStructure.set(exec->globalData(), this, JSCallbackObject<JSNonFinalObject>::createStructure(exec->globalData(), this, m_objectPrototype.get()));

    m_arrayPrototype.set(exec->globalData(), this, ArrayPrototype::create(exec, this, ArrayPrototype::createStructure(exec->globalData(), this, m_objectPrototype.get())));
    m_arrayStructure.set(exec->globalData(), this, JSArray::createStructure(exec->globalData(), this, m_arrayPrototype.get()));
    m_regExpMatchesArrayStructure.set(exec->globalData(), this, RegExpMatchesArray::createStructure(exec->globalData(), this, m_arrayPrototype.get()));

    m_stringPrototype.set(exec->globalData(), this, StringPrototype::create(exec, this, StringPrototype::createStructure(exec->globalData(), this, m_objectPrototype.get())));
    m_stringObjectStructure.set(exec->globalData(), this, StringObject::createStructure(exec->globalData(), this, m_stringPrototype.get()));

    m_booleanPrototype.set(exec->globalData(), this, BooleanPrototype::create(exec, this, BooleanPrototype::createStructure(exec->globalData(), this, m_objectPrototype.get())));
    m_booleanObjectStructure.set(exec->globalData(), this, BooleanObject::createStructure(exec->globalData(), this, m_booleanPrototype.get()));

    m_numberPrototype.set(exec->globalData(), this, NumberPrototype::create(exec, this, NumberPrototype::createStructure(exec->globalData(), this, m_objectPrototype.get())));
    m_numberObjectStructure.set(exec->globalData(), this, NumberObject::createStructure(exec->globalData(), this, m_numberPrototype.get()));

    m_datePrototype.set(exec->globalData(), this, DatePrototype::create(exec, this, DatePrototype::createStructure(exec->globalData(), this, m_objectPrototype.get())));
    m_dateStructure.set(exec->globalData(), this, DateInstance::createStructure(exec->globalData(), this, m_datePrototype.get()));

    RegExp* emptyRegex = RegExp::create(exec->globalData(), "", NoFlags);
    
    m_regExpPrototype.set(exec->globalData(), this, RegExpPrototype::create(exec, this, RegExpPrototype::createStructure(exec->globalData(), this, m_objectPrototype.get()), emptyRegex));
    m_regExpStructure.set(exec->globalData(), this, RegExpObject::createStructure(exec->globalData(), this, m_regExpPrototype.get()));

    m_methodCallDummy.set(exec->globalData(), this, constructEmptyObject(exec));

    ErrorPrototype* errorPrototype = ErrorPrototype::create(exec, this, ErrorPrototype::createStructure(exec->globalData(), this, m_objectPrototype.get()));
    m_errorStructure.set(exec->globalData(), this, ErrorInstance::createStructure(exec->globalData(), this, errorPrototype));

    // Constructors

    JSCell* objectConstructor = ObjectConstructor::create(exec, this, ObjectConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_objectPrototype.get());
    JSCell* functionConstructor = FunctionConstructor::create(exec, this, FunctionConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_functionPrototype.get());
    JSCell* arrayConstructor = ArrayConstructor::create(exec, this, ArrayConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_arrayPrototype.get());
    JSCell* stringConstructor = StringConstructor::create(exec, this, StringConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_stringPrototype.get());
    JSCell* booleanConstructor = BooleanConstructor::create(exec, this, BooleanConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_booleanPrototype.get());
    JSCell* numberConstructor = NumberConstructor::create(exec, this, NumberConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_numberPrototype.get());
    JSCell* dateConstructor = DateConstructor::create(exec, this, DateConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_datePrototype.get());

    m_regExpConstructor.set(exec->globalData(), this, RegExpConstructor::create(exec, this, RegExpConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_regExpPrototype.get()));

    m_errorConstructor.set(exec->globalData(), this, ErrorConstructor::create(exec, this, ErrorConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), errorPrototype));

    Structure* nativeErrorPrototypeStructure = NativeErrorPrototype::createStructure(exec->globalData(), this, errorPrototype);
    Structure* nativeErrorStructure = NativeErrorConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get());
    m_evalErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "EvalError"));
    m_rangeErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "RangeError"));
    m_referenceErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "ReferenceError"));
    m_syntaxErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "SyntaxError"));
    m_typeErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "TypeError"));
    m_URIErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "URIError"));

    m_objectPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, objectConstructor, DontEnum);
    m_functionPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, functionConstructor, DontEnum);
    m_arrayPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, arrayConstructor, DontEnum);
    m_booleanPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, booleanConstructor, DontEnum);
    m_stringPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, stringConstructor, DontEnum);
    m_numberPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, numberConstructor, DontEnum);
    m_datePrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, dateConstructor, DontEnum);
    m_regExpPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, m_regExpConstructor.get(), DontEnum);
    errorPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, m_errorConstructor.get(), DontEnum);

    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Object"), objectConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Function"), functionConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Array"), arrayConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Boolean"), booleanConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "String"), stringConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Number"), numberConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Date"), dateConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "RegExp"), m_regExpConstructor.get(), DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Error"), m_errorConstructor.get(), DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "EvalError"), m_evalErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "RangeError"), m_rangeErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "ReferenceError"), m_referenceErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "SyntaxError"), m_syntaxErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "TypeError"), m_typeErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "URIError"), m_URIErrorConstructor.get(), DontEnum);

    m_evalFunction.set(exec->globalData(), this, JSFunction::create(exec, this, 1, exec->propertyNames().eval.ustring(), globalFuncEval));
    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().eval, m_evalFunction.get(), DontEnum);

    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "JSON"), JSONObject::create(exec, this, JSONObject::createStructure(exec->globalData(), this, m_objectPrototype.get())), DontEnum);

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(exec, "Math"), MathObject::create(exec, this, MathObject::createStructure(exec->globalData(), this, m_objectPrototype.get())), DontEnum | DontDelete),
        GlobalPropertyInfo(Identifier(exec, "NaN"), jsNaN(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(exec, "Infinity"), jsNumber(std::numeric_limits<double>::infinity()), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(exec, "undefined"), jsUndefined(), DontEnum | DontDelete | ReadOnly)
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));

    resetPrototype(exec->globalData(), prototype);
}

void JSGlobalObject::createThrowTypeError(ExecState* exec)
{
    JSFunction* thrower = JSFunction::create(exec, this, 0, "", globalFuncThrowTypeError);
    GetterSetter* getterSetter = GetterSetter::create(exec);
    getterSetter->setGetter(exec->globalData(), thrower);
    getterSetter->setSetter(exec->globalData(), thrower);
    m_throwTypeErrorGetterSetter.set(exec->globalData(), this, getterSetter);
}

// Set prototype, and also insert the object prototype at the end of the chain.
void JSGlobalObject::resetPrototype(JSGlobalData& globalData, JSValue prototype)
{
    setPrototype(globalData, prototype);

    JSObject* oldLastInPrototypeChain = lastInPrototypeChain(this);
    JSObject* objectPrototype = m_objectPrototype.get();
    if (oldLastInPrototypeChain != objectPrototype)
        oldLastInPrototypeChain->setPrototype(globalData, objectPrototype);
}

void JSGlobalObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{ 
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    JSVariableObject::visitChildren(thisObject, visitor);

    visitIfNeeded(visitor, &thisObject->m_globalScopeChain);
    visitIfNeeded(visitor, &thisObject->m_methodCallDummy);

    visitIfNeeded(visitor, &thisObject->m_regExpConstructor);
    visitIfNeeded(visitor, &thisObject->m_errorConstructor);
    visitIfNeeded(visitor, &thisObject->m_evalErrorConstructor);
    visitIfNeeded(visitor, &thisObject->m_rangeErrorConstructor);
    visitIfNeeded(visitor, &thisObject->m_referenceErrorConstructor);
    visitIfNeeded(visitor, &thisObject->m_syntaxErrorConstructor);
    visitIfNeeded(visitor, &thisObject->m_typeErrorConstructor);
    visitIfNeeded(visitor, &thisObject->m_URIErrorConstructor);

    visitIfNeeded(visitor, &thisObject->m_evalFunction);
    visitIfNeeded(visitor, &thisObject->m_callFunction);
    visitIfNeeded(visitor, &thisObject->m_applyFunction);
    visitIfNeeded(visitor, &thisObject->m_throwTypeErrorGetterSetter);

    visitIfNeeded(visitor, &thisObject->m_objectPrototype);
    visitIfNeeded(visitor, &thisObject->m_functionPrototype);
    visitIfNeeded(visitor, &thisObject->m_arrayPrototype);
    visitIfNeeded(visitor, &thisObject->m_booleanPrototype);
    visitIfNeeded(visitor, &thisObject->m_stringPrototype);
    visitIfNeeded(visitor, &thisObject->m_numberPrototype);
    visitIfNeeded(visitor, &thisObject->m_datePrototype);
    visitIfNeeded(visitor, &thisObject->m_regExpPrototype);

    visitIfNeeded(visitor, &thisObject->m_argumentsStructure);
    visitIfNeeded(visitor, &thisObject->m_arrayStructure);
    visitIfNeeded(visitor, &thisObject->m_booleanObjectStructure);
    visitIfNeeded(visitor, &thisObject->m_callbackConstructorStructure);
    visitIfNeeded(visitor, &thisObject->m_callbackFunctionStructure);
    visitIfNeeded(visitor, &thisObject->m_callbackObjectStructure);
    visitIfNeeded(visitor, &thisObject->m_dateStructure);
    visitIfNeeded(visitor, &thisObject->m_emptyObjectStructure);
    visitIfNeeded(visitor, &thisObject->m_nullPrototypeObjectStructure);
    visitIfNeeded(visitor, &thisObject->m_errorStructure);
    visitIfNeeded(visitor, &thisObject->m_functionStructure);
    visitIfNeeded(visitor, &thisObject->m_boundFunctionStructure);
    visitIfNeeded(visitor, &thisObject->m_namedFunctionStructure);
    visitIfNeeded(visitor, &thisObject->m_numberObjectStructure);
    visitIfNeeded(visitor, &thisObject->m_regExpMatchesArrayStructure);
    visitIfNeeded(visitor, &thisObject->m_regExpStructure);
    visitIfNeeded(visitor, &thisObject->m_stringObjectStructure);
    visitIfNeeded(visitor, &thisObject->m_internalFunctionStructure);

    if (thisObject->m_registerArray) {
        // Outside the execution of global code, when our variables are torn off,
        // we can mark the torn-off array.
        visitor.appendValues(thisObject->m_registerArray.get(), thisObject->m_registerArraySize);
    } else if (thisObject->m_registers) {
        // During execution of global code, when our variables are in the register file,
        // the symbol table tells us how many variables there are, and registers
        // points to where they end, and the registers used for execution begin.
        visitor.appendValues(thisObject->m_registers - thisObject->symbolTable().size(), thisObject->symbolTable().size());
    }
}

ExecState* JSGlobalObject::globalExec()
{
    return CallFrame::create(m_globalCallFrame + RegisterFile::CallFrameHeaderSize);
}

void JSGlobalObject::resizeRegisters(size_t newSize)
{
    // Previous duplicate symbols may have created spare capacity in m_registerArray.
    if (newSize <= m_registerArraySize)
        return;

    size_t oldSize = m_registerArraySize;
    OwnArrayPtr<WriteBarrier<Unknown> > registerArray = adoptArrayPtr(new WriteBarrier<Unknown>[newSize]);
    for (size_t i = 0; i < oldSize; ++i)
        registerArray[i].set(globalData(), this, m_registerArray[i].get());
    for (size_t i = oldSize; i < newSize; ++i)
        registerArray[i].setUndefined();

    WriteBarrier<Unknown>* registers = registerArray.get();
    setRegisters(registers, registerArray.release(), newSize);
}

void JSGlobalObject::addStaticGlobals(GlobalPropertyInfo* globals, int count)
{
    resizeRegisters(symbolTable().size() + count);

    for (int i = 0; i < count; ++i) {
        GlobalPropertyInfo& global = globals[i];
        ASSERT(global.attributes & DontDelete);
        
        int index = symbolTable().size();
        SymbolTableEntry newEntry(index, global.attributes);
        symbolTable().add(global.identifier.impl(), newEntry);
        registerAt(index).set(globalData(), this, global.value);
    }
}

bool JSGlobalObject::getOwnPropertySlot(JSCell* cell, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    if (getStaticFunctionSlot<JSVariableObject>(exec, ExecState::globalObjectTable(exec), thisObject, propertyName, slot))
        return true;
    return thisObject->symbolTableGet(propertyName, slot);
}

bool JSGlobalObject::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    if (getStaticFunctionDescriptor<JSVariableObject>(exec, ExecState::globalObjectTable(exec), thisObject, propertyName, descriptor))
        return true;
    return thisObject->symbolTableGet(propertyName, descriptor);
}

void JSGlobalObject::clearRareData(JSCell* cell)
{
    jsCast<JSGlobalObject*>(cell)->m_rareData.clear();
}

DynamicGlobalObjectScope::DynamicGlobalObjectScope(JSGlobalData& globalData, JSGlobalObject* dynamicGlobalObject)
    : m_dynamicGlobalObjectSlot(globalData.dynamicGlobalObject)
    , m_savedDynamicGlobalObject(m_dynamicGlobalObjectSlot)
{
    if (!m_dynamicGlobalObjectSlot) {
#if ENABLE(ASSEMBLER)
        if (ExecutableAllocator::underMemoryPressure())
            globalData.heap.discardAllCompiledCode();
#endif

        m_dynamicGlobalObjectSlot = dynamicGlobalObject;

        // Reset the date cache between JS invocations to force the VM
        // to observe time zone changes.
        globalData.resetDateCache();
    }
}

void slowValidateCell(JSGlobalObject* globalObject)
{
    if (!globalObject->isGlobalObject())
        CRASH();
    ASSERT_GC_OBJECT_INHERITS(globalObject, &JSGlobalObject::s_info);
}

} // namespace JSC
