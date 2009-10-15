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
#include "NativeFunctionWrapper.h"
#include "NumberPrototype.h"
#include "StringPrototype.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>

namespace JSC {

    class ArrayPrototype;
    class BooleanPrototype;
    class DatePrototype;
    class Debugger;
    class ErrorConstructor;
    class FunctionPrototype;
    class GlobalCodeBlock;
    class GlobalEvalFunction;
    class NativeErrorConstructor;
    class ProgramCodeBlock;
    class PrototypeFunction;
    class RegExpConstructor;
    class RegExpPrototype;
    class RegisterFile;

    struct ActivationStackNode;
    struct HashTable;

    typedef Vector<ExecState*, 16> ExecStateStack;
    
    class JSGlobalObject : public JSVariableObject {
    protected:
        using JSVariableObject::JSVariableObjectData;

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
                , globalScopeChain(NoScopeChain())
                , regExpConstructor(0)
                , errorConstructor(0)
                , evalErrorConstructor(0)
                , rangeErrorConstructor(0)
                , referenceErrorConstructor(0)
                , syntaxErrorConstructor(0)
                , typeErrorConstructor(0)
                , URIErrorConstructor(0)
                , evalFunction(0)
                , callFunction(0)
                , applyFunction(0)
                , objectPrototype(0)
                , functionPrototype(0)
                , arrayPrototype(0)
                , booleanPrototype(0)
                , stringPrototype(0)
                , numberPrototype(0)
                , datePrototype(0)
                , regExpPrototype(0)
                , methodCallDummy(0)
            {
            }
            
            Destructor destructor;
            
            size_t registerArraySize;

            JSGlobalObject* next;
            JSGlobalObject* prev;

            Debugger* debugger;
            
            ScopeChain globalScopeChain;
            Register globalCallFrame[RegisterFile::CallFrameHeaderSize];

            int recursion;

            RegExpConstructor* regExpConstructor;
            ErrorConstructor* errorConstructor;
            NativeErrorConstructor* evalErrorConstructor;
            NativeErrorConstructor* rangeErrorConstructor;
            NativeErrorConstructor* referenceErrorConstructor;
            NativeErrorConstructor* syntaxErrorConstructor;
            NativeErrorConstructor* typeErrorConstructor;
            NativeErrorConstructor* URIErrorConstructor;

            GlobalEvalFunction* evalFunction;
            NativeFunctionWrapper* callFunction;
            NativeFunctionWrapper* applyFunction;

            ObjectPrototype* objectPrototype;
            FunctionPrototype* functionPrototype;
            ArrayPrototype* arrayPrototype;
            BooleanPrototype* booleanPrototype;
            StringPrototype* stringPrototype;
            NumberPrototype* numberPrototype;
            DatePrototype* datePrototype;
            RegExpPrototype* regExpPrototype;

            JSObject* methodCallDummy;

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
            RefPtr<Structure> prototypeFunctionStructure;
            RefPtr<Structure> regExpMatchesArrayStructure;
            RefPtr<Structure> regExpStructure;
            RefPtr<Structure> stringObjectStructure;

            SymbolTable symbolTable;
            unsigned profileGroup;

            RefPtr<JSGlobalData> globalData;

            HashSet<GlobalCodeBlock*> codeBlocks;
        };

    public:
        void* operator new(size_t, JSGlobalData*);

        explicit JSGlobalObject()
            : JSVariableObject(JSGlobalObject::createStructure(jsNull()), new JSGlobalObjectData(destroyJSGlobalObjectData))
        {
            init(this);
        }

    protected:
        JSGlobalObject(NonNullPassRefPtr<Structure> structure, JSGlobalObjectData* data, JSObject* thisValue)
            : JSVariableObject(structure, data)
        {
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

        // Linked list of all global objects that use the same JSGlobalData.
        JSGlobalObject*& head() { return d()->globalData->head; }
        JSGlobalObject* next() { return d()->next; }

        // The following accessors return pristine values, even if a script 
        // replaces the global object's associated property.

        RegExpConstructor* regExpConstructor() const { return d()->regExpConstructor; }

        ErrorConstructor* errorConstructor() const { return d()->errorConstructor; }
        NativeErrorConstructor* evalErrorConstructor() const { return d()->evalErrorConstructor; }
        NativeErrorConstructor* rangeErrorConstructor() const { return d()->rangeErrorConstructor; }
        NativeErrorConstructor* referenceErrorConstructor() const { return d()->referenceErrorConstructor; }
        NativeErrorConstructor* syntaxErrorConstructor() const { return d()->syntaxErrorConstructor; }
        NativeErrorConstructor* typeErrorConstructor() const { return d()->typeErrorConstructor; }
        NativeErrorConstructor* URIErrorConstructor() const { return d()->URIErrorConstructor; }

        GlobalEvalFunction* evalFunction() const { return d()->evalFunction; }

        ObjectPrototype* objectPrototype() const { return d()->objectPrototype; }
        FunctionPrototype* functionPrototype() const { return d()->functionPrototype; }
        ArrayPrototype* arrayPrototype() const { return d()->arrayPrototype; }
        BooleanPrototype* booleanPrototype() const { return d()->booleanPrototype; }
        StringPrototype* stringPrototype() const { return d()->stringPrototype; }
        NumberPrototype* numberPrototype() const { return d()->numberPrototype; }
        DatePrototype* datePrototype() const { return d()->datePrototype; }
        RegExpPrototype* regExpPrototype() const { return d()->regExpPrototype; }

        JSObject* methodCallDummy() const { return d()->methodCallDummy; }

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
        Structure* prototypeFunctionStructure() const { return d()->prototypeFunctionStructure.get(); }
        Structure* regExpMatchesArrayStructure() const { return d()->regExpMatchesArrayStructure.get(); }
        Structure* regExpStructure() const { return d()->regExpStructure.get(); }
        Structure* stringObjectStructure() const { return d()->stringObjectStructure.get(); }

        void setProfileGroup(unsigned value) { d()->profileGroup = value; }
        unsigned profileGroup() const { return d()->profileGroup; }

        Debugger* debugger() const { return d()->debugger; }
        void setDebugger(Debugger* debugger) { d()->debugger = debugger; }
        
        virtual bool supportsProfiling() const { return false; }
        
        int recursion() { return d()->recursion; }
        void incRecursion() { ++d()->recursion; }
        void decRecursion() { --d()->recursion; }
        
        ScopeChain& globalScopeChain() { return d()->globalScopeChain; }

        virtual bool isGlobalObject() const { return true; }

        virtual ExecState* globalExec();

        virtual bool shouldInterruptScript() const { return true; }

        virtual bool allowsAccessFrom(const JSGlobalObject*) const { return true; }

        virtual bool isDynamicScope() const;

        HashSet<GlobalCodeBlock*>& codeBlocks() { return d()->codeBlocks; }

        void copyGlobalsFrom(RegisterFile&);
        void copyGlobalsTo(RegisterFile&);
        
        void resetPrototype(JSValue prototype);

        JSGlobalData* globalData() { return d()->globalData.get(); }
        JSGlobalObjectData* d() const { return static_cast<JSGlobalObjectData*>(JSVariableObject::d); }

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType, OverridesGetOwnPropertySlot));
        }

    protected:
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

        void setRegisters(Register* registers, Register* registerArray, size_t count);

        void* operator new(size_t); // can only be allocated with JSGlobalData
    };

    JSGlobalObject* asGlobalObject(JSValue);

    inline JSGlobalObject* asGlobalObject(JSValue value)
    {
        ASSERT(asObject(value)->isGlobalObject());
        return static_cast<JSGlobalObject*>(asObject(value));
    }

    inline void JSGlobalObject::setRegisters(Register* registers, Register* registerArray, size_t count)
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
            symbolTable().add(global.identifier.ustring().rep(), newEntry);
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

    inline JSValue Structure::prototypeForLookup(ExecState* exec) const
    {
        if (typeInfo().type() == ObjectType)
            return m_prototype;

#if USE(JSVALUE32)
        if (typeInfo().type() == StringType)
            return exec->lexicalGlobalObject()->stringPrototype();

        ASSERT(typeInfo().type() == NumberType);
        return exec->lexicalGlobalObject()->numberPrototype();
#else
        ASSERT(typeInfo().type() == StringType);
        return exec->lexicalGlobalObject()->stringPrototype();
#endif
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

    inline JSObject* constructEmptyObject(ExecState* exec)
    {
        return new (exec) JSObject(exec->lexicalGlobalObject()->emptyObjectStructure());
    }

    inline JSArray* constructEmptyArray(ExecState* exec)
    {
        return new (exec) JSArray(exec->lexicalGlobalObject()->arrayStructure());
    }

    inline JSArray* constructEmptyArray(ExecState* exec, unsigned initialLength)
    {
        return new (exec) JSArray(exec->lexicalGlobalObject()->arrayStructure(), initialLength);
    }

    inline JSArray* constructArray(ExecState* exec, JSValue singleItemValue)
    {
        MarkedArgumentBuffer values;
        values.append(singleItemValue);
        return new (exec) JSArray(exec->lexicalGlobalObject()->arrayStructure(), values);
    }

    inline JSArray* constructArray(ExecState* exec, const ArgList& values)
    {
        return new (exec) JSArray(exec->lexicalGlobalObject()->arrayStructure(), values);
    }

    class DynamicGlobalObjectScope : public Noncopyable {
    public:
        DynamicGlobalObjectScope(CallFrame* callFrame, JSGlobalObject* dynamicGlobalObject) 
            : m_dynamicGlobalObjectSlot(callFrame->globalData().dynamicGlobalObject)
            , m_savedDynamicGlobalObject(m_dynamicGlobalObjectSlot)
        {
            m_dynamicGlobalObjectSlot = dynamicGlobalObject;
        }

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
