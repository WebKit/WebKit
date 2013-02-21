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

#include "Arguments.h"
#include "ArrayConstructor.h"
#include "ArrayPrototype.h"
#include "BooleanConstructor.h"
#include "BooleanPrototype.h"
#include "CodeBlock.h"
#include "CodeCache.h"
#include "DateConstructor.h"
#include "DatePrototype.h"
#include "Debugger.h"
#include "Error.h"
#include "ErrorConstructor.h"
#include "ErrorPrototype.h"
#include "FunctionConstructor.h"
#include "FunctionPrototype.h"
#include "GetterSetter.h"
#include "Interpreter.h"
#include "JSActivation.h"
#include "JSBoundFunction.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSLock.h"
#include "JSNameScope.h"
#include "JSONObject.h"
#include "JSWithScope.h"
#include "LegacyProfiler.h"
#include "Lookup.h"
#include "MathObject.h"
#include "NameConstructor.h"
#include "NameInstance.h"
#include "NamePrototype.h"
#include "NativeErrorConstructor.h"
#include "NativeErrorPrototype.h"
#include "NumberConstructor.h"
#include "NumberPrototype.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "Operations.h"
#include "ParserError.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "StrictEvalActivation.h"
#include "StringConstructor.h"
#include "StringPrototype.h"

#include "JSGlobalObject.lut.h"

namespace JSC {

const ClassInfo JSGlobalObject::s_info = { "GlobalObject", &Base::s_info, 0, ExecState::globalObjectTable, CREATE_METHOD_TABLE(JSGlobalObject) };

const GlobalObjectMethodTable JSGlobalObject::s_globalObjectMethodTable = { &allowsAccessFrom, &supportsProfiling, &supportsRichSourceInfo, &shouldInterruptScript, &javaScriptExperimentsEnabled };

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

// Default number of ticks before a timeout check should be done.
static const int initialTickCountThreshold = 255;

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

JSGlobalObject::JSGlobalObject(JSGlobalData& globalData, Structure* structure, const GlobalObjectMethodTable* globalObjectMethodTable)
    : Base(globalData, structure, 0)
    , m_masqueradesAsUndefinedWatchpoint(adoptRef(new WatchpointSet(InitializedWatching)))
    , m_havingABadTimeWatchpoint(adoptRef(new WatchpointSet(InitializedWatching)))
    , m_weakRandom(Options::forceWeakRandomSeed() ? Options::forcedWeakRandomSeed() : static_cast<unsigned>(randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0)))
    , m_evalEnabled(true)
    , m_globalObjectMethodTable(globalObjectMethodTable ? globalObjectMethodTable : &s_globalObjectMethodTable)
{
}

JSGlobalObject::~JSGlobalObject()
{
    if (m_debugger)
        m_debugger->detach(this);

    if (LegacyProfiler* profiler = globalData().enabledProfiler())
        profiler->stopProfiling(this);
}

void JSGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSGlobalObject*>(cell)->JSGlobalObject::~JSGlobalObject();
}

void JSGlobalObject::setGlobalThis(JSGlobalData& globalData, JSObject* globalThis)
{ 
    m_globalThis.set(globalData, this, globalThis);
}

void JSGlobalObject::init(JSObject* thisValue)
{
    ASSERT(globalData().apiLock().currentThreadIsHoldingLock());

    setGlobalThis(globalData(), thisValue);
    JSGlobalObject::globalExec()->init(0, 0, this, CallFrame::noCaller(), 0, 0);

    m_debugger = 0;

    reset(prototype());
}

void JSGlobalObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (symbolTablePut(thisObject, exec, propertyName, value, slot.isStrictMode()))
        return;
    Base::put(thisObject, exec, propertyName, value, slot);
}

void JSGlobalObject::putDirectVirtual(JSObject* object, ExecState* exec, PropertyName propertyName, JSValue value, unsigned attributes)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (symbolTablePutWithAttributes(thisObject, exec->globalData(), propertyName, value, attributes))
        return;

    JSValue valueBefore = thisObject->getDirect(exec->globalData(), propertyName);
    PutPropertySlot slot;
    Base::put(thisObject, exec, propertyName, value, slot);
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
    if (descriptor.isAccessorDescriptor() && symbolTableGet(thisObject, propertyName, slot))
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
    protoAccessor->setGetter(exec->globalData(), JSFunction::create(exec, this, 0, String(), globalFuncProtoGetter));
    protoAccessor->setSetter(exec->globalData(), JSFunction::create(exec, this, 0, String(), globalFuncProtoSetter));
    m_objectPrototype->putDirectAccessor(exec, exec->propertyNames().underscoreProto, protoAccessor, Accessor | DontEnum);
    m_functionPrototype->structure()->setPrototypeWithoutTransition(exec->globalData(), m_objectPrototype.get());

    m_nameScopeStructure.set(exec->globalData(), this, JSNameScope::createStructure(exec->globalData(), this, jsNull()));
    m_activationStructure.set(exec->globalData(), this, JSActivation::createStructure(exec->globalData(), this, jsNull()));
    m_strictEvalActivationStructure.set(exec->globalData(), this, StrictEvalActivation::createStructure(exec->globalData(), this, jsNull()));
    m_withScopeStructure.set(exec->globalData(), this, JSWithScope::createStructure(exec->globalData(), this, jsNull()));

    m_nullPrototypeObjectStructure.set(exec->globalData(), this, JSFinalObject::createStructure(globalData(), this, jsNull(), JSFinalObject::defaultInlineCapacity()));

    m_callbackFunctionStructure.set(exec->globalData(), this, JSCallbackFunction::createStructure(exec->globalData(), this, m_functionPrototype.get()));
    m_argumentsStructure.set(exec->globalData(), this, Arguments::createStructure(exec->globalData(), this, m_objectPrototype.get()));
    m_callbackConstructorStructure.set(exec->globalData(), this, JSCallbackConstructor::createStructure(exec->globalData(), this, m_objectPrototype.get()));
    m_callbackObjectStructure.set(exec->globalData(), this, JSCallbackObject<JSDestructibleObject>::createStructure(exec->globalData(), this, m_objectPrototype.get()));

    m_arrayPrototype.set(exec->globalData(), this, ArrayPrototype::create(exec, this, ArrayPrototype::createStructure(exec->globalData(), this, m_objectPrototype.get())));
    
    m_originalArrayStructureForIndexingShape[UndecidedShape >> IndexingShapeShift].set(exec->globalData(), this, JSArray::createStructure(exec->globalData(), this, m_arrayPrototype.get(), ArrayWithUndecided));
    m_originalArrayStructureForIndexingShape[Int32Shape >> IndexingShapeShift].set(exec->globalData(), this, JSArray::createStructure(exec->globalData(), this, m_arrayPrototype.get(), ArrayWithInt32));
    m_originalArrayStructureForIndexingShape[DoubleShape >> IndexingShapeShift].set(exec->globalData(), this, JSArray::createStructure(exec->globalData(), this, m_arrayPrototype.get(), ArrayWithDouble));
    m_originalArrayStructureForIndexingShape[ContiguousShape >> IndexingShapeShift].set(exec->globalData(), this, JSArray::createStructure(exec->globalData(), this, m_arrayPrototype.get(), ArrayWithContiguous));
    m_originalArrayStructureForIndexingShape[ArrayStorageShape >> IndexingShapeShift].set(exec->globalData(), this, JSArray::createStructure(exec->globalData(), this, m_arrayPrototype.get(), ArrayWithArrayStorage));
    m_originalArrayStructureForIndexingShape[SlowPutArrayStorageShape >> IndexingShapeShift].set(exec->globalData(), this, JSArray::createStructure(exec->globalData(), this, m_arrayPrototype.get(), ArrayWithSlowPutArrayStorage));
    for (unsigned i = 0; i < NumberOfIndexingShapes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i] = m_originalArrayStructureForIndexingShape[i];
    
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

    m_errorPrototype.set(exec->globalData(), this, ErrorPrototype::create(exec, this, ErrorPrototype::createStructure(exec->globalData(), this, m_objectPrototype.get())));
    m_errorStructure.set(exec->globalData(), this, ErrorInstance::createStructure(exec->globalData(), this, m_errorPrototype.get()));

    // Constructors

    JSCell* objectConstructor = ObjectConstructor::create(exec, this, ObjectConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_objectPrototype.get());
    JSCell* functionConstructor = FunctionConstructor::create(exec, this, FunctionConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_functionPrototype.get());
    JSCell* arrayConstructor = ArrayConstructor::create(exec, this, ArrayConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_arrayPrototype.get());
    JSCell* stringConstructor = StringConstructor::create(exec, this, StringConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_stringPrototype.get());
    JSCell* booleanConstructor = BooleanConstructor::create(exec, this, BooleanConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_booleanPrototype.get());
    JSCell* numberConstructor = NumberConstructor::create(exec, this, NumberConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_numberPrototype.get());
    JSCell* dateConstructor = DateConstructor::create(exec, this, DateConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_datePrototype.get());

    m_regExpConstructor.set(exec->globalData(), this, RegExpConstructor::create(exec, this, RegExpConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_regExpPrototype.get()));

    m_errorConstructor.set(exec->globalData(), this, ErrorConstructor::create(exec, this, ErrorConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), m_errorPrototype.get()));

    Structure* nativeErrorPrototypeStructure = NativeErrorPrototype::createStructure(exec->globalData(), this, m_errorPrototype.get());
    Structure* nativeErrorStructure = NativeErrorConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get());
    m_evalErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("EvalError")));
    m_rangeErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("RangeError")));
    m_referenceErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("ReferenceError")));
    m_syntaxErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("SyntaxError")));
    m_typeErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("TypeError")));
    m_URIErrorConstructor.set(exec->globalData(), this, NativeErrorConstructor::create(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("URIError")));

    m_objectPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, objectConstructor, DontEnum);
    m_functionPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, functionConstructor, DontEnum);
    m_arrayPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, arrayConstructor, DontEnum);
    m_booleanPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, booleanConstructor, DontEnum);
    m_stringPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, stringConstructor, DontEnum);
    m_numberPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, numberConstructor, DontEnum);
    m_datePrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, dateConstructor, DontEnum);
    m_regExpPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, m_regExpConstructor.get(), DontEnum);
    m_errorPrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, m_errorConstructor.get(), DontEnum);

    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().Object, objectConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Function"), functionConstructor, DontEnum);
    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().Array, arrayConstructor, DontEnum);
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

    m_evalFunction.set(exec->globalData(), this, JSFunction::create(exec, this, 1, exec->propertyNames().eval.string(), globalFuncEval));
    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().eval, m_evalFunction.get(), DontEnum);

    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "JSON"), JSONObject::create(exec, this, JSONObject::createStructure(exec->globalData(), this, m_objectPrototype.get())), DontEnum);
    putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Math"), MathObject::create(exec, this, MathObject::createStructure(exec->globalData(), this, m_objectPrototype.get())), DontEnum);

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(exec, "NaN"), jsNaN(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(exec, "Infinity"), jsNumber(std::numeric_limits<double>::infinity()), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(exec, "undefined"), jsUndefined(), DontEnum | DontDelete | ReadOnly)
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
    
    m_specialPointers[Special::CallFunction] = m_callFunction.get();
    m_specialPointers[Special::ApplyFunction] = m_applyFunction.get();
    m_specialPointers[Special::ObjectConstructor] = objectConstructor;
    m_specialPointers[Special::ArrayConstructor] = arrayConstructor;

    if (m_experimentsEnabled) {
        NamePrototype* privateNamePrototype = NamePrototype::create(exec, NamePrototype::createStructure(exec->globalData(), this, m_objectPrototype.get()));
        m_privateNameStructure.set(exec->globalData(), this, NameInstance::createStructure(exec->globalData(), this, privateNamePrototype));

        JSCell* privateNameConstructor = NameConstructor::create(exec, this, NameConstructor::createStructure(exec->globalData(), this, m_functionPrototype.get()), privateNamePrototype);
        privateNamePrototype->putDirectWithoutTransition(exec->globalData(), exec->propertyNames().constructor, privateNameConstructor, DontEnum);
        putDirectWithoutTransition(exec->globalData(), Identifier(exec, "Name"), privateNameConstructor, DontEnum);
    }

    resetPrototype(exec->globalData(), prototype);
}

// Private namespace for helpers for JSGlobalObject::haveABadTime()
namespace {

class ObjectsWithBrokenIndexingFinder : public MarkedBlock::VoidFunctor {
public:
    ObjectsWithBrokenIndexingFinder(MarkedArgumentBuffer&, JSGlobalObject*);
    void operator()(JSCell*);

private:
    MarkedArgumentBuffer& m_foundObjects;
    JSGlobalObject* m_globalObject;
};

ObjectsWithBrokenIndexingFinder::ObjectsWithBrokenIndexingFinder(
    MarkedArgumentBuffer& foundObjects, JSGlobalObject* globalObject)
    : m_foundObjects(foundObjects)
    , m_globalObject(globalObject)
{
}

inline bool hasBrokenIndexing(JSObject* object)
{
    // This will change if we have more indexing types.
    IndexingType type = object->structure()->indexingType();
    // This could be made obviously more efficient, but isn't made so right now, because
    // we expect this to be an unlikely slow path anyway.
    return hasUndecided(type) || hasInt32(type) || hasDouble(type) || hasContiguous(type) || hasFastArrayStorage(type);
}

void ObjectsWithBrokenIndexingFinder::operator()(JSCell* cell)
{
    if (!cell->isObject())
        return;
    
    JSObject* object = asObject(cell);

    // Run this filter first, since it's cheap, and ought to filter out a lot of objects.
    if (!hasBrokenIndexing(object))
        return;
    
    // We only want to have a bad time in the affected global object, not in the entire
    // VM. But we have to be careful, since there may be objects that claim to belong to
    // a different global object that have prototypes from our global object.
    bool foundGlobalObject = false;
    for (JSObject* current = object; ;) {
        if (current->globalObject() == m_globalObject) {
            foundGlobalObject = true;
            break;
        }
        
        JSValue prototypeValue = current->prototype();
        if (prototypeValue.isNull())
            break;
        current = asObject(prototypeValue);
    }
    if (!foundGlobalObject)
        return;
    
    m_foundObjects.append(object);
}

} // end private namespace for helpers for JSGlobalObject::haveABadTime()

void JSGlobalObject::haveABadTime(JSGlobalData& globalData)
{
    ASSERT(&globalData == &this->globalData());
    
    if (isHavingABadTime())
        return;
    
    // Make sure that all allocations or indexed storage transitions that are inlining
    // the assumption that it's safe to transition to a non-SlowPut array storage don't
    // do so anymore.
    m_havingABadTimeWatchpoint->notifyWrite();
    ASSERT(isHavingABadTime()); // The watchpoint is what tells us that we're having a bad time.
    
    // Make sure that all JSArray allocations that load the appropriate structure from
    // this object now load a structure that uses SlowPut.
    for (unsigned i = 0; i < NumberOfIndexingShapes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i].set(globalData, this, originalArrayStructureForIndexingType(ArrayWithSlowPutArrayStorage));
    
    // Make sure that all objects that have indexed storage switch to the slow kind of
    // indexed storage.
    MarkedArgumentBuffer foundObjects; // Use MarkedArgumentBuffer because switchToSlowPutArrayStorage() may GC.
    ObjectsWithBrokenIndexingFinder finder(foundObjects, this);
    globalData.heap.objectSpace().forEachLiveCell(finder);
    while (!foundObjects.isEmpty()) {
        JSObject* object = asObject(foundObjects.last());
        foundObjects.removeLast();
        ASSERT(hasBrokenIndexing(object));
        object->switchToSlowPutArrayStorage(globalData);
    }
}

bool JSGlobalObject::arrayPrototypeChainIsSane()
{
    return !hasIndexedProperties(m_arrayPrototype->structure()->indexingType())
        && m_arrayPrototype->prototype() == m_objectPrototype.get()
        && !hasIndexedProperties(m_objectPrototype->structure()->indexingType())
        && m_objectPrototype->prototype().isNull();
}

void JSGlobalObject::createThrowTypeError(ExecState* exec)
{
    JSFunction* thrower = JSFunction::create(exec, this, 0, String(), globalFuncThrowTypeError);
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
    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_globalThis);

    visitor.append(&thisObject->m_regExpConstructor);
    visitor.append(&thisObject->m_errorConstructor);
    visitor.append(&thisObject->m_evalErrorConstructor);
    visitor.append(&thisObject->m_rangeErrorConstructor);
    visitor.append(&thisObject->m_referenceErrorConstructor);
    visitor.append(&thisObject->m_syntaxErrorConstructor);
    visitor.append(&thisObject->m_typeErrorConstructor);
    visitor.append(&thisObject->m_URIErrorConstructor);

    visitor.append(&thisObject->m_evalFunction);
    visitor.append(&thisObject->m_callFunction);
    visitor.append(&thisObject->m_applyFunction);
    visitor.append(&thisObject->m_throwTypeErrorGetterSetter);

    visitor.append(&thisObject->m_objectPrototype);
    visitor.append(&thisObject->m_functionPrototype);
    visitor.append(&thisObject->m_arrayPrototype);
    visitor.append(&thisObject->m_booleanPrototype);
    visitor.append(&thisObject->m_stringPrototype);
    visitor.append(&thisObject->m_numberPrototype);
    visitor.append(&thisObject->m_datePrototype);
    visitor.append(&thisObject->m_regExpPrototype);
    visitor.append(&thisObject->m_errorPrototype);

    visitor.append(&thisObject->m_withScopeStructure);
    visitor.append(&thisObject->m_strictEvalActivationStructure);
    visitor.append(&thisObject->m_activationStructure);
    visitor.append(&thisObject->m_nameScopeStructure);
    visitor.append(&thisObject->m_argumentsStructure);
    for (unsigned i = 0; i < NumberOfIndexingShapes; ++i)
        visitor.append(&thisObject->m_originalArrayStructureForIndexingShape[i]);
    for (unsigned i = 0; i < NumberOfIndexingShapes; ++i)
        visitor.append(&thisObject->m_arrayStructureForIndexingShapeDuringAllocation[i]);
    visitor.append(&thisObject->m_booleanObjectStructure);
    visitor.append(&thisObject->m_callbackConstructorStructure);
    visitor.append(&thisObject->m_callbackFunctionStructure);
    visitor.append(&thisObject->m_callbackObjectStructure);
    visitor.append(&thisObject->m_dateStructure);
    visitor.append(&thisObject->m_nullPrototypeObjectStructure);
    visitor.append(&thisObject->m_errorStructure);
    visitor.append(&thisObject->m_functionStructure);
    visitor.append(&thisObject->m_boundFunctionStructure);
    visitor.append(&thisObject->m_namedFunctionStructure);
    visitor.append(&thisObject->m_numberObjectStructure);
    visitor.append(&thisObject->m_privateNameStructure);
    visitor.append(&thisObject->m_regExpMatchesArrayStructure);
    visitor.append(&thisObject->m_regExpStructure);
    visitor.append(&thisObject->m_stringObjectStructure);
    visitor.append(&thisObject->m_internalFunctionStructure);
}

JSObject* JSGlobalObject::toThisObject(JSCell* cell, ExecState*)
{
    return jsCast<JSGlobalObject*>(cell)->globalThis();
}

ExecState* JSGlobalObject::globalExec()
{
    return CallFrame::create(m_globalCallFrame + JSStack::CallFrameHeaderSize);
}

void JSGlobalObject::addStaticGlobals(GlobalPropertyInfo* globals, int count)
{
    addRegisters(count);

    for (int i = 0; i < count; ++i) {
        GlobalPropertyInfo& global = globals[i];
        ASSERT(global.attributes & DontDelete);
        
        int index = symbolTable()->size();
        SymbolTableEntry newEntry(index, global.attributes);
        symbolTable()->add(global.identifier.impl(), newEntry);
        registerAt(index).set(globalData(), this, global.value);
    }
}

bool JSGlobalObject::getOwnPropertySlot(JSCell* cell, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    if (getStaticFunctionSlot<Base>(exec, ExecState::globalObjectTable(exec), thisObject, propertyName, slot))
        return true;
    return symbolTableGet(thisObject, propertyName, slot);
}

bool JSGlobalObject::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    if (getStaticFunctionDescriptor<Base>(exec, ExecState::globalObjectTable(exec), thisObject, propertyName, descriptor))
        return true;
    return symbolTableGet(thisObject, propertyName, descriptor);
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
            globalData.heap.deleteAllCompiledCode();
#endif

        m_dynamicGlobalObjectSlot = dynamicGlobalObject;

        // Reset the date cache between JS invocations to force the VM
        // to observe time zone changes.
        globalData.resetDateCache();
    }
}

void slowValidateCell(JSGlobalObject* globalObject)
{
    RELEASE_ASSERT(globalObject->isGlobalObject());
    ASSERT_GC_OBJECT_INHERITS(globalObject, &JSGlobalObject::s_info);
}

UnlinkedProgramCodeBlock* JSGlobalObject::createProgramCodeBlock(CallFrame* callFrame, ProgramExecutable* executable, JSObject** exception)
{
    ParserError error;
    JSParserStrictness strictness = executable->isStrictMode() ? JSParseStrict : JSParseNormal;
    DebuggerMode debuggerMode = hasDebugger() ? DebuggerOn : DebuggerOff;
    ProfilerMode profilerMode = hasProfiler() ? ProfilerOn : ProfilerOff;
    UnlinkedProgramCodeBlock* unlinkedCode = globalData().codeCache()->getProgramCodeBlock(globalData(), executable, executable->source(), strictness, debuggerMode, profilerMode, error);

    if (hasDebugger())
        debugger()->sourceParsed(callFrame, executable->source().provider(), error.m_line, error.m_message);

    if (error.m_type != ParserError::ErrorNone) {
        *exception = error.toErrorObject(this, executable->source());
        return 0;
    }
    
    return unlinkedCode;
}

UnlinkedEvalCodeBlock* JSGlobalObject::createEvalCodeBlock(CallFrame* callFrame, EvalExecutable* executable, JSObject** exception)
{
    ParserError error;
    JSParserStrictness strictness = executable->isStrictMode() ? JSParseStrict : JSParseNormal;
    DebuggerMode debuggerMode = hasDebugger() ? DebuggerOn : DebuggerOff;
    ProfilerMode profilerMode = hasProfiler() ? ProfilerOn : ProfilerOff;
    UnlinkedEvalCodeBlock* unlinkedCode = globalData().codeCache()->getEvalCodeBlock(globalData(), executable, executable->source(), strictness, debuggerMode, profilerMode, error);

    if (hasDebugger())
        debugger()->sourceParsed(callFrame, executable->source().provider(), error.m_line, error.m_message);

    if (error.m_type != ParserError::ErrorNone) {
        *exception = error.toErrorObject(this, executable->source());
        return 0;
    }

    return unlinkedCode;
}

} // namespace JSC
