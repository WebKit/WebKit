/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include "JSGlobalData.h"
#include "JSVariableObject.h"
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
    class GlobalEvalFunction;
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

        struct JSGlobalObjectData : public JSVariableObjectData {
            JSGlobalObjectData(JSGlobalObject* globalObject, JSObject* thisValue)
                : JSVariableObjectData(&symbolTable, 0)
                , globalScopeChain(globalObject, thisValue)
                , regExpConstructor(0)
                , errorConstructor(0)
                , evalErrorConstructor(0)
                , rangeErrorConstructor(0)
                , referenceErrorConstructor(0)
                , syntaxErrorConstructor(0)
                , typeErrorConstructor(0)
                , URIErrorConstructor(0)
                , evalFunction(0)
                , objectPrototype(0)
                , functionPrototype(0)
                , arrayPrototype(0)
                , booleanPrototype(0)
                , stringPrototype(0)
                , numberPrototype(0)
                , datePrototype(0)
                , regExpPrototype(0)
            {
            }
            
            virtual ~JSGlobalObjectData()
            {
            }

            JSGlobalObject* next;
            JSGlobalObject* prev;

            Debugger* debugger;
            
            ScopeChain globalScopeChain;
            OwnPtr<ExecState> globalExec;

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

            ObjectPrototype* objectPrototype;
            FunctionPrototype* functionPrototype;
            ArrayPrototype* arrayPrototype;
            BooleanPrototype* booleanPrototype;
            StringPrototype* stringPrototype;
            NumberPrototype* numberPrototype;
            DatePrototype* datePrototype;
            RegExpPrototype* regExpPrototype;

            RefPtr<StructureID> argumentsStructure;
            RefPtr<StructureID> arrayStructure;
            RefPtr<StructureID> booleanObjectStructure;
            RefPtr<StructureID> callbackConstructorStructure;
            RefPtr<StructureID> callbackFunctionStructure;
            RefPtr<StructureID> callbackObjectStructure;
            RefPtr<StructureID> dateStructure;
            RefPtr<StructureID> emptyObjectStructure;
            RefPtr<StructureID> errorStructure;
            RefPtr<StructureID> functionStructure;
            RefPtr<StructureID> numberObjectStructure;
            RefPtr<StructureID> prototypeFunctionStructure;
            RefPtr<StructureID> regExpMatchesArrayStructure;
            RefPtr<StructureID> regExpStructure;
            RefPtr<StructureID> stringObjectStructure;

            SymbolTable symbolTable;
            unsigned profileGroup;

            RefPtr<JSGlobalData> globalData;

            HashSet<ProgramCodeBlock*> codeBlocks;

            OwnPtr<HashSet<JSObject*> > arrayVisitedElements; // Global data shared by array prototype functions.
        };

    public:
        void* operator new(size_t, JSGlobalData*);

        JSGlobalObject(JSGlobalData* globalData)
            : JSVariableObject(globalData->nullProtoStructureID, new JSGlobalObjectData(this, this))
        {
            init(this);
        }

    protected:
        JSGlobalObject(PassRefPtr<StructureID> structure, JSGlobalObjectData* data, JSObject* globalThisValue)
            : JSVariableObject(structure, data)
        {
            init(globalThisValue);
        }

    public:
        virtual ~JSGlobalObject();

        virtual void mark();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&, bool& slotIsWriteable);
        virtual void put(ExecState*, const Identifier&, JSValue*, PutPropertySlot&);
        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValue* value, unsigned attributes);

        virtual void defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunc);
        virtual void defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunc);

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

        StructureID* argumentsStructure() const { return d()->argumentsStructure.get(); }
        StructureID* arrayStructure() const { return d()->arrayStructure.get(); }
        StructureID* booleanObjectStructure() const { return d()->booleanObjectStructure.get(); }
        StructureID* callbackConstructorStructure() const { return d()->callbackConstructorStructure.get(); }
        StructureID* callbackFunctionStructure() const { return d()->callbackFunctionStructure.get(); }
        StructureID* callbackObjectStructure() const { return d()->callbackObjectStructure.get(); }
        StructureID* dateStructure() const { return d()->dateStructure.get(); }
        StructureID* emptyObjectStructure() const { return d()->emptyObjectStructure.get(); }
        StructureID* errorStructure() const { return d()->errorStructure.get(); }
        StructureID* functionStructure() const { return d()->functionStructure.get(); }
        StructureID* numberObjectStructure() const { return d()->numberObjectStructure.get(); }
        StructureID* prototypeFunctionStructure() const { return d()->prototypeFunctionStructure.get(); }
        StructureID* regExpMatchesArrayStructure() const { return d()->regExpMatchesArrayStructure.get(); }
        StructureID* regExpStructure() const { return d()->regExpStructure.get(); }
        StructureID* stringObjectStructure() const { return d()->stringObjectStructure.get(); }

        void setProfileGroup(unsigned value) { d()->profileGroup = value; }
        unsigned profileGroup() const { return d()->profileGroup; }

        void setTimeoutTime(unsigned timeoutTime);
        void startTimeoutCheck();
        void stopTimeoutCheck();

        Debugger* debugger() const { return d()->debugger; }
        void setDebugger(Debugger* debugger) { d()->debugger = debugger; }
        
        int recursion() { return d()->recursion; }
        void incRecursion() { ++d()->recursion; }
        void decRecursion() { --d()->recursion; }
        
        ScopeChain& globalScopeChain() { return d()->globalScopeChain; }

        virtual bool isGlobalObject() const { return true; }
        virtual JSGlobalObject* toGlobalObject(ExecState*) const;

        virtual ExecState* globalExec();

        virtual bool shouldInterruptScript() const { return true; }

        virtual bool allowsAccessFrom(const JSGlobalObject*) const { return true; }

        virtual bool isDynamicScope() const;

        HashSet<JSObject*>& arrayVisitedElements() { if (!d()->arrayVisitedElements) d()->arrayVisitedElements.set(new HashSet<JSObject*>); return *d()->arrayVisitedElements; }

        HashSet<ProgramCodeBlock*>& codeBlocks() { return d()->codeBlocks; }

        void copyGlobalsFrom(RegisterFile&);
        void copyGlobalsTo(RegisterFile&);
        
        void resetPrototype(JSValue* prototype);

        JSGlobalData* globalData() { return d()->globalData.get(); }
        JSGlobalObjectData* d() const { return static_cast<JSGlobalObjectData*>(JSVariableObject::d); }

    protected:
        struct GlobalPropertyInfo {
            GlobalPropertyInfo(const Identifier& i, JSValue* v, unsigned a)
                : identifier(i)
                , value(v)
                , attributes(a)
            {
            }

            const Identifier identifier;
            JSValue* value;
            unsigned attributes;
        };
        void addStaticGlobals(GlobalPropertyInfo*, int count);

    private:
        // FIXME: Fold these functions into the constructor.
        void init(JSObject* thisValue);
        void reset(JSValue* prototype);

        void* operator new(size_t); // can only be allocated with JSGlobalData
    };

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

    inline bool JSGlobalObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, bool& slotIsWriteable)
    {
        if (JSVariableObject::getOwnPropertySlotForWrite(exec, propertyName, slot, slotIsWriteable))
            return true;
        return symbolTableGet(propertyName, slot, slotIsWriteable);
    }

    inline JSGlobalObject* ScopeChainNode::globalObject() const
    {
        JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(bottom());
        ASSERT(globalObject->isGlobalObject());
        return globalObject;
    }

    inline JSValue* StructureID::prototypeForLookup(ExecState* exec)
    {
        if (typeInfo().type() == ObjectType)
            return m_prototype;

        if (typeInfo().type() == StringType)
            return exec->lexicalGlobalObject()->stringPrototype();

        ASSERT(typeInfo().type() == NumberType);
        return exec->lexicalGlobalObject()->numberPrototype();
    }

} // namespace JSC

#endif // JSGlobalObject_h
