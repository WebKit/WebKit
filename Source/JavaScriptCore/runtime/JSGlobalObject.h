/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef JSGlobalObject_h
#define JSGlobalObject_h

#include "JSArray.h"
#include "JSGlobalData.h"
#include "JSVariableObject.h"
#include "JSWeakObjectMapRefInternal.h"
#include "NumberPrototype.h"
#include "StringPrototype.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/RandomNumber.h>

namespace JSC {

    class ArrayPrototype;
    class BooleanPrototype;
    class DatePrototype;
    class Debugger;
    class ErrorConstructor;
    class FunctionPrototype;
    class GlobalCodeBlock;
    class NativeErrorConstructor;
    class ProgramCodeBlock;
    class RegExpConstructor;
    class RegExpPrototype;
    class RegisterFile;

    struct ActivationStackNode;
    struct HashTable;

    typedef Vector<ExecState*, 16> ExecStateStack;
    
    class JSGlobalObject : public JSVariableObject {
    protected:
        using JSVariableObject::JSVariableObjectData;
        typedef HashSet<RefPtr<OpaqueJSWeakObjectMap> > WeakMapSet;

        struct JSGlobalObjectData : public JSVariableObjectData {
            // We use an explicit destructor function pointer instead of a
            // virtual destructor because we want to avoid adding a vtable
            // pointer to this struct. Adding a vtable pointer would force the
            // compiler to emit costly pointer fixup code when casting from
            // JSVariableObjectData* to JSGlobalObjectData*.
            typedef void (*Destructor)(void*);

            JSGlobalObjectData(Destructor destructor)
                : JSVariableObjectData(&symbolTable, 0)
                , destructor(destructor)
                , registerArraySize(0)
                , globalScopeChain()
                , weakRandom(static_cast<unsigned>(randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0)))
            {
            }
            
            Destructor destructor;
            
            size_t registerArraySize;

            JSGlobalObject* next;
            JSGlobalObject* prev;

            Debugger* debugger;
            
            WriteBarrier<ScopeChainNode> globalScopeChain;
            Register globalCallFrame[RegisterFile::CallFrameHeaderSize];

            WriteBarrier<RegExpConstructor> regExpConstructor;
            WriteBarrier<ErrorConstructor> errorConstructor;
            WriteBarrier<NativeErrorConstructor> evalErrorConstructor;
            WriteBarrier<NativeErrorConstructor> rangeErrorConstructor;
            WriteBarrier<NativeErrorConstructor> referenceErrorConstructor;
            WriteBarrier<NativeErrorConstructor> syntaxErrorConstructor;
            WriteBarrier<NativeErrorConstructor> typeErrorConstructor;
            WriteBarrier<NativeErrorConstructor> URIErrorConstructor;

            WriteBarrier<JSFunction> evalFunction;
            WriteBarrier<JSFunction> callFunction;
            WriteBarrier<JSFunction> applyFunction;

            WriteBarrier<ObjectPrototype> objectPrototype;
            WriteBarrier<FunctionPrototype> functionPrototype;
            WriteBarrier<ArrayPrototype> arrayPrototype;
            WriteBarrier<BooleanPrototype> booleanPrototype;
            WriteBarrier<StringPrototype> stringPrototype;
            WriteBarrier<NumberPrototype> numberPrototype;
            WriteBarrier<DatePrototype> datePrototype;
            WriteBarrier<RegExpPrototype> regExpPrototype;

            WriteBarrier<JSObject> methodCallDummy;

            RefPtr<Structure> argumentsStructure;
            RefPtr<Structure> arrayStructure;
            RefPtr<Structure> booleanObjectStructure;
            RefPtr<Structure> callbackConstructorStructure;
            RefPtr<Structure> callbackFunctionStructure;
            RefPtr<Structure> callbackObjectStructure;
            RefPtr<Structure> dateStructure;
            RefPtr<Structure> emptyObjectStructure;
            RefPtr<Structure> errorStructure;
            RefPtr<Structure> functionStructure;
            RefPtr<Structure> numberObjectStructure;
            RefPtr<Structure> regExpMatchesArrayStructure;
            RefPtr<Structure> regExpStructure;
            RefPtr<Structure> stringObjectStructure;
            RefPtr<Structure> internalFunctionStructure;

            SymbolTable symbolTable;
            unsigned profileGroup;

            RefPtr<JSGlobalData> globalData;

            WeakMapSet weakMaps;
            WeakRandom weakRandom;
        };

    public:
        void* operator new(size_t, JSGlobalData*);
        
        explicit JSGlobalObject()
            : JSVariableObject(JSGlobalObject::createStructure(jsNull()), new JSGlobalObjectData(destroyJSGlobalObjectData))
        {
            COMPILE_ASSERT(JSGlobalObject::AnonymousSlotCount == 1, JSGlobalObject_has_only_a_single_slot);
            putThisToAnonymousValue(0);
            init(this);
        }
        
        explicit JSGlobalObject(NonNullPassRefPtr<Structure> structure)
            : JSVariableObject(structure, new JSGlobalObjectData(destroyJSGlobalObjectData))
        {
            COMPILE_ASSERT(JSGlobalObject::AnonymousSlotCount == 1, JSGlobalObject_has_only_a_single_slot);
            putThisToAnonymousValue(0);
            init(this);
        }

    protected:
        JSGlobalObject(NonNullPassRefPtr<Structure> structure, JSGlobalObjectData* data, JSObject* thisValue)
            : JSVariableObject(structure, data)
        {
            COMPILE_ASSERT(JSGlobalObject::AnonymousSlotCount == 1, JSGlobalObject_has_only_a_single_slot);
            putThisToAnonymousValue(0);
            init(thisValue);
        }

    public:
        virtual ~JSGlobalObject();

        virtual void markChildren(MarkStack&);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier&, PropertyDescriptor&);
        virtual bool hasOwnPropertyForWrite(ExecState*, const Identifier&);
        virtual void put(ExecState*, const Identifier&, JSValue, PutPropertySlot&);
        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValue value, unsigned attributes);

        virtual void defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunc, unsigned attributes);
        virtual void defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunc, unsigned attributes);

        // We use this in the code generator as we perform symbol table
        // lookups prior to initializing the properties
        bool symbolTableHasProperty(const Identifier& propertyName);

        // The following accessors return pristine values, even if a script 
        // replaces the global object's associated property.

        RegExpConstructor* regExpConstructor() const { return d()->regExpConstructor.get(); }

        ErrorConstructor* errorConstructor() const { return d()->errorConstructor.get(); }
        NativeErrorConstructor* evalErrorConstructor() const { return d()->evalErrorConstructor.get(); }
        NativeErrorConstructor* rangeErrorConstructor() const { return d()->rangeErrorConstructor.get(); }
        NativeErrorConstructor* referenceErrorConstructor() const { return d()->referenceErrorConstructor.get(); }
        NativeErrorConstructor* syntaxErrorConstructor() const { return d()->syntaxErrorConstructor.get(); }
        NativeErrorConstructor* typeErrorConstructor() const { return d()->typeErrorConstructor.get(); }
        NativeErrorConstructor* URIErrorConstructor() const { return d()->URIErrorConstructor.get(); }

        JSFunction* evalFunction() const { return d()->evalFunction.get(); }

        ObjectPrototype* objectPrototype() const { return d()->objectPrototype.get(); }
        FunctionPrototype* functionPrototype() const { return d()->functionPrototype.get(); }
        ArrayPrototype* arrayPrototype() const { return d()->arrayPrototype.get(); }
        BooleanPrototype* booleanPrototype() const { return d()->booleanPrototype.get(); }
        StringPrototype* stringPrototype() const { return d()->stringPrototype.get(); }
        NumberPrototype* numberPrototype() const { return d()->numberPrototype.get(); }
        DatePrototype* datePrototype() const { return d()->datePrototype.get(); }
        RegExpPrototype* regExpPrototype() const { return d()->regExpPrototype.get(); }

        JSObject* methodCallDummy() const { return d()->methodCallDummy.get(); }

        Structure* argumentsStructure() const { return d()->argumentsStructure.get(); }
        Structure* arrayStructure() const { return d()->arrayStructure.get(); }
        Structure* booleanObjectStructure() const { return d()->booleanObjectStructure.get(); }
        Structure* callbackConstructorStructure() const { return d()->callbackConstructorStructure.get(); }
        Structure* callbackFunctionStructure() const { return d()->callbackFunctionStructure.get(); }
        Structure* callbackObjectStructure() const { return d()->callbackObjectStructure.get(); }
        Structure* dateStructure() const { return d()->dateStructure.get(); }
        Structure* emptyObjectStructure() const { return d()->emptyObjectStructure.get(); }
        Structure* errorStructure() const { return d()->errorStructure.get(); }
        Structure* functionStructure() const { return d()->functionStructure.get(); }
        Structure* numberObjectStructure() const { return d()->numberObjectStructure.get(); }
        Structure* internalFunctionStructure() const { return d()->internalFunctionStructure.get(); }
        Structure* regExpMatchesArrayStructure() const { return d()->regExpMatchesArrayStructure.get(); }
        Structure* regExpStructure() const { return d()->regExpStructure.get(); }
        Structure* stringObjectStructure() const { return d()->stringObjectStructure.get(); }

        void setProfileGroup(unsigned value) { d()->profileGroup = value; }
        unsigned profileGroup() const { return d()->profileGroup; }

        Debugger* debugger() const { return d()->debugger; }
        void setDebugger(Debugger* debugger) { d()->debugger = debugger; }

        virtual bool supportsProfiling() const { return false; }
        virtual bool supportsRichSourceInfo() const { return true; }

        ScopeChainNode* globalScopeChain() { return d()->globalScopeChain.get(); }

        virtual bool isGlobalObject() const { return true; }

        virtual ExecState* globalExec();

        virtual bool shouldInterruptScript() const { return true; }

        virtual bool allowsAccessFrom(const JSGlobalObject*) const { return true; }

        virtual bool isDynamicScope(bool& requiresDynamicChecks) const;

        void copyGlobalsFrom(RegisterFile&);
        void copyGlobalsTo(RegisterFile&);
        void resizeRegisters(int oldSize, int newSize);

        void resetPrototype(JSValue prototype);

        JSGlobalData& globalData() const { return *d()->globalData.get(); }
        JSGlobalObjectData* d() const { return static_cast<JSGlobalObjectData*>(JSVariableObject::d); }

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType, StructureFlags), AnonymousSlotCount, &s_info);
        }

        void registerWeakMap(OpaqueJSWeakObjectMap* map)
        {
            d()->weakMaps.add(map);
        }

        void deregisterWeakMap(OpaqueJSWeakObjectMap* map)
        {
            d()->weakMaps.remove(map);
        }

        double weakRandomNumber() { return d()->weakRandom.get(); }
    protected:

        static const unsigned AnonymousSlotCount = JSVariableObject::AnonymousSlotCount + 1;
        static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesMarkChildren | OverridesGetPropertyNames | JSVariableObject::StructureFlags;

        struct GlobalPropertyInfo {
            GlobalPropertyInfo(const Identifier& i, JSValue v, unsigned a)
                : identifier(i)
                , value(v)
                , attributes(a)
            {
            }

            const Identifier identifier;
            JSValue value;
            unsigned attributes;
        };
        void addStaticGlobals(GlobalPropertyInfo*, int count);

    private:
        static void destroyJSGlobalObjectData(void*);

        // FIXME: Fold reset into init.
        void init(JSObject* thisValue);
        void reset(JSValue prototype);

        void setRegisters(Register* registers, PassOwnArrayPtr<Register> registerArray, size_t count);

        void* operator new(size_t); // can only be allocated with JSGlobalData
    };

    JSGlobalObject* asGlobalObject(JSValue);

    inline JSGlobalObject* asGlobalObject(JSValue value)
    {
        ASSERT(asObject(value)->isGlobalObject());
        return static_cast<JSGlobalObject*>(asObject(value));
    }

    inline void JSGlobalObject::setRegisters(Register* registers, PassOwnArrayPtr<Register> registerArray, size_t count)
    {
        JSVariableObject::setRegisters(registers, registerArray);
        d()->registerArraySize = count;
    }

    inline void JSGlobalObject::addStaticGlobals(GlobalPropertyInfo* globals, int count)
    {
        size_t oldSize = d()->registerArraySize;
        size_t newSize = oldSize + count;
        Register* registerArray = new Register[newSize];
        if (d()->registerArray)
            memcpy(registerArray + count, d()->registerArray.get(), oldSize * sizeof(Register));
        setRegisters(registerArray + newSize, registerArray, newSize);

        for (int i = 0, index = -static_cast<int>(oldSize) - 1; i < count; ++i, --index) {
            GlobalPropertyInfo& global = globals[i];
            ASSERT(global.attributes & DontDelete);
            SymbolTableEntry newEntry(index, global.attributes);
            symbolTable().add(global.identifier.impl(), newEntry);
            registerAt(index) = global.value;
        }
    }

    inline bool JSGlobalObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
    {
        if (JSVariableObject::getOwnPropertySlot(exec, propertyName, slot))
            return true;
        return symbolTableGet(propertyName, slot);
    }

    inline bool JSGlobalObject::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
    {
        if (symbolTableGet(propertyName, descriptor))
            return true;
        return JSVariableObject::getOwnPropertyDescriptor(exec, propertyName, descriptor);
    }

    inline bool JSGlobalObject::hasOwnPropertyForWrite(ExecState* exec, const Identifier& propertyName)
    {
        PropertySlot slot;
        if (JSVariableObject::getOwnPropertySlot(exec, propertyName, slot))
            return true;
        bool slotIsWriteable;
        return symbolTableGet(propertyName, slot, slotIsWriteable);
    }

    inline bool JSGlobalObject::symbolTableHasProperty(const Identifier& propertyName)
    {
        SymbolTableEntry entry = symbolTable().inlineGet(propertyName.impl());
        return !entry.isNull();
    }

    inline JSValue Structure::prototypeForLookup(ExecState* exec) const
    {
        if (typeInfo().type() == ObjectType)
            return m_prototype.get();

        ASSERT(typeInfo().type() == StringType);
        return exec->lexicalGlobalObject()->stringPrototype();
    }

    inline StructureChain* Structure::prototypeChain(ExecState* exec) const
    {
        // We cache our prototype chain so our clients can share it.
        if (!isValid(exec, m_cachedPrototypeChain.get())) {
            JSValue prototype = prototypeForLookup(exec);
            m_cachedPrototypeChain = StructureChain::create(prototype.isNull() ? 0 : asObject(prototype)->structure());
        }
        return m_cachedPrototypeChain.get();
    }

    inline bool Structure::isValid(ExecState* exec, StructureChain* cachedPrototypeChain) const
    {
        if (!cachedPrototypeChain)
            return false;

        JSValue prototype = prototypeForLookup(exec);
        RefPtr<Structure>* cachedStructure = cachedPrototypeChain->head();
        while(*cachedStructure && !prototype.isNull()) {
            if (asObject(prototype)->structure() != *cachedStructure)
                return false;
            ++cachedStructure;
            prototype = asObject(prototype)->prototype();
        }
        return prototype.isNull() && !*cachedStructure;
    }

    inline JSGlobalObject* ExecState::dynamicGlobalObject()
    {
        if (this == lexicalGlobalObject()->globalExec())
            return lexicalGlobalObject();

        // For any ExecState that's not a globalExec, the 
        // dynamic global object must be set since code is running
        ASSERT(globalData().dynamicGlobalObject);
        return globalData().dynamicGlobalObject;
    }

    inline JSObject* constructEmptyObject(ExecState* exec, JSGlobalObject* globalObject)
    {
        return constructEmptyObject(exec, globalObject->emptyObjectStructure());
    }

    inline JSObject* constructEmptyObject(ExecState* exec)
    {
        return constructEmptyObject(exec, exec->lexicalGlobalObject());
    }
    
    inline JSArray* constructEmptyArray(ExecState* exec)
    {
        return new (exec) JSArray(exec->lexicalGlobalObject()->arrayStructure());
    }
    
    inline JSArray* constructEmptyArray(ExecState* exec, JSGlobalObject* globalObject)
    {
        return new (exec) JSArray(globalObject->arrayStructure());
    }

    inline JSArray* constructEmptyArray(ExecState* exec, unsigned initialLength)
    {
        return new (exec) JSArray(exec->lexicalGlobalObject()->arrayStructure(), initialLength, CreateInitialized);
    }

    inline JSArray* constructArray(ExecState* exec, JSValue singleItemValue)
    {
        MarkedArgumentBuffer values;
        values.append(singleItemValue);
        return new (exec) JSArray(exec->globalData(), exec->lexicalGlobalObject()->arrayStructure(), values);
    }

    inline JSArray* constructArray(ExecState* exec, const ArgList& values)
    {
        return new (exec) JSArray(exec->globalData(), exec->lexicalGlobalObject()->arrayStructure(), values);
    }

    class DynamicGlobalObjectScope {
        WTF_MAKE_NONCOPYABLE(DynamicGlobalObjectScope);
    public:
        DynamicGlobalObjectScope(CallFrame* callFrame, JSGlobalObject* dynamicGlobalObject);

        ~DynamicGlobalObjectScope()
        {
            m_dynamicGlobalObjectSlot = m_savedDynamicGlobalObject;
        }

    private:
        JSGlobalObject*& m_dynamicGlobalObjectSlot;
        JSGlobalObject* m_savedDynamicGlobalObject;
    };

} // namespace JSC

#endif // JSGlobalObject_h
