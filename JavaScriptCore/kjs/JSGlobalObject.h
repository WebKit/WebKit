// -*- c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef KJS_GlobalObject_h
#define KJS_GlobalObject_h

#include "JSVariableObject.h"
#include "Activation.h"

namespace KJS {

    class ActivationImp;
    class ArrayObjectImp;
    class ArrayPrototype;
    class BooleanObjectImp;
    class BooleanPrototype;
    class DateObjectImp;
    class DatePrototype;
    class Debugger;
    class ErrorObjectImp;
    class ErrorPrototype;
    class EvalError;
    class EvalErrorPrototype;
    class FunctionObjectImp;
    class FunctionPrototype;
    struct HashTable;
    class JSGlobalObject;
    class NativeErrorImp;
    class NativeErrorPrototype;
    class NumberObjectImp;
    class NumberPrototype;
    class ObjectObjectImp;
    class ObjectPrototype;
    class PrototypeReflexiveFunction;
    class RangeError;
    class RangeErrorPrototype;
    class ReferenceError;
    class ReferenceError;
    class ReferenceErrorPrototype;
    class RegExpObjectImp;
    class RegExpPrototype;
    class RuntimeMethod;
    class ScopeChain;
    class StringObjectImp;
    class StringPrototype;
    class SyntaxErrorPrototype;
    class TypeError;
    class TypeErrorPrototype;
    class UriError;
    class UriErrorPrototype;
    struct ActivationStackNode;
    struct ThreadClassInfoHashTables;

    typedef Vector<ExecState*, 16> ExecStateStack;

    class JSGlobalObject : public JSVariableObject {
    protected:
        using JSVariableObject::JSVariableObjectData;

        struct JSGlobalObjectData : public JSVariableObjectData {
            JSGlobalObjectData()
                : JSVariableObjectData(&inlineSymbolTable)
            {
            }

            JSGlobalObject* next;
            JSGlobalObject* prev;

            Debugger* debugger;
            
            OwnPtr<GlobalExecState> globalExec;
            int recursion;

            unsigned timeoutTime;
            unsigned timeAtLastCheckTimeout;
            unsigned timeExecuting;
            unsigned timeoutCheckCount;
            unsigned tickCount;
            unsigned ticksUntilNextTimeoutCheck;

            ObjectObjectImp* objectConstructor;
            FunctionObjectImp* functionConstructor;
            ArrayObjectImp* arrayConstructor;
            BooleanObjectImp* booleanConstructor;
            StringObjectImp* stringConstructor;
            NumberObjectImp* numberConstructor;
            DateObjectImp* dateConstructor;
            RegExpObjectImp* regExpConstructor;
            ErrorObjectImp* errorConstructor;
            NativeErrorImp* evalErrorConstructor;
            NativeErrorImp* rangeErrorConstructor;
            NativeErrorImp* referenceErrorConstructor;
            NativeErrorImp* syntaxErrorConstructor;
            NativeErrorImp* typeErrorConstructor;
            NativeErrorImp* URIErrorConstructor;

            PrototypeReflexiveFunction* evalFunction;

            ObjectPrototype* objectPrototype;
            FunctionPrototype* functionPrototype;
            ArrayPrototype* arrayPrototype;
            BooleanPrototype* booleanPrototype;
            StringPrototype* stringPrototype;
            NumberPrototype* numberPrototype;
            DatePrototype* datePrototype;
            RegExpPrototype* regExpPrototype;
            ErrorPrototype* errorPrototype;
            NativeErrorPrototype* evalErrorPrototype;
            NativeErrorPrototype* rangeErrorPrototype;
            NativeErrorPrototype* referenceErrorPrototype;
            NativeErrorPrototype* syntaxErrorPrototype;
            NativeErrorPrototype* typeErrorPrototype;
            NativeErrorPrototype* URIErrorPrototype;
            
            SymbolTable inlineSymbolTable;

            ExecStateStack activeExecStates;

            ActivationStackNode* activations;
            size_t activationCount;
            
            unsigned pageGroupIdentifier;

            OwnPtr<HashSet<JSObject*> > arrayVisitedElements; // Global data shared by array prototype functions.

            PerThreadData perThreadData;
        };

    public:
        JSGlobalObject()
            : JSVariableObject(new JSGlobalObjectData)
        {
            init(this);
        }

    protected:
        JSGlobalObject(JSValue* proto, JSObject* globalThisValue)
            : JSVariableObject(proto, new JSGlobalObjectData)
        {
            init(globalThisValue);
        }

    public:
        virtual ~JSGlobalObject();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual void put(ExecState*, const Identifier&, JSValue*);
        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValue* value, unsigned attributes);

        // Linked list of all global objects.
        static JSGlobalObject* head() { return s_head; }
        JSGlobalObject* next() { return d()->next; }

        // Resets the global object to contain only built-in properties, sets
        // the global object's prototype to "prototype," then adds the 
        // default object prototype to the tail of the global object's 
        // prototype chain.
        void reset(JSValue* prototype);

        // The following accessors return pristine values, even if a script 
        // replaces the global object's associated property.

        ObjectObjectImp* objectConstructor() const { return d()->objectConstructor; }
        FunctionObjectImp* functionConstructor() const { return d()->functionConstructor; }
        ArrayObjectImp* arrayConstructor() const { return d()->arrayConstructor; }
        BooleanObjectImp* booleanConstructor() const { return d()->booleanConstructor; }
        StringObjectImp* stringConstructor() const{ return d()->stringConstructor; }
        NumberObjectImp* numberConstructor() const{ return d()->numberConstructor; }
        DateObjectImp* dateConstructor() const{ return d()->dateConstructor; }
        RegExpObjectImp* regExpConstructor() const { return d()->regExpConstructor; }
        ErrorObjectImp* errorConstructor() const { return d()->errorConstructor; }
        NativeErrorImp* evalErrorConstructor() const { return d()->evalErrorConstructor; }
        NativeErrorImp* rangeErrorConstructor() const { return d()->rangeErrorConstructor; }
        NativeErrorImp* referenceErrorConstructor() const { return d()->referenceErrorConstructor; }
        NativeErrorImp* syntaxErrorConstructor() const { return d()->syntaxErrorConstructor; }
        NativeErrorImp* typeErrorConstructor() const { return d()->typeErrorConstructor; }
        NativeErrorImp* URIErrorConstructor() const { return d()->URIErrorConstructor; }

        PrototypeReflexiveFunction* evalFunction() const { return d()->evalFunction; }

        ObjectPrototype* objectPrototype() const { return d()->objectPrototype; }
        FunctionPrototype* functionPrototype() const { return d()->functionPrototype; }
        ArrayPrototype* arrayPrototype() const { return d()->arrayPrototype; }
        BooleanPrototype* booleanPrototype() const { return d()->booleanPrototype; }
        StringPrototype* stringPrototype() const { return d()->stringPrototype; }
        NumberPrototype* numberPrototype() const { return d()->numberPrototype; }
        DatePrototype* datePrototype() const { return d()->datePrototype; }
        RegExpPrototype* regExpPrototype() const { return d()->regExpPrototype; }
        ErrorPrototype* errorPrototype() const { return d()->errorPrototype; }
        NativeErrorPrototype* evalErrorPrototype() const { return d()->evalErrorPrototype; }
        NativeErrorPrototype* rangeErrorPrototype() const { return d()->rangeErrorPrototype; }
        NativeErrorPrototype* referenceErrorPrototype() const { return d()->referenceErrorPrototype; }
        NativeErrorPrototype* syntaxErrorPrototype() const { return d()->syntaxErrorPrototype; }
        NativeErrorPrototype* typeErrorPrototype() const { return d()->typeErrorPrototype; }
        NativeErrorPrototype* URIErrorPrototype() const { return d()->URIErrorPrototype; }

        void setPageGroupIdentifier(unsigned value) { d()->pageGroupIdentifier = value; }
        unsigned pageGroupIdentifier() const { return d()->pageGroupIdentifier; }

        void setTimeoutTime(unsigned timeoutTime) { d()->timeoutTime = timeoutTime; }
        void startTimeoutCheck();
        void stopTimeoutCheck();
        bool timedOut();

        Debugger* debugger() const { return d()->debugger; }
        void setDebugger(Debugger* debugger) { d()->debugger = debugger; }
        
        int recursion() { return d()->recursion; }
        void incRecursion() { ++d()->recursion; }
        void decRecursion() { --d()->recursion; }

        virtual void mark();

        virtual bool isGlobalObject() const { return true; }
        virtual JSGlobalObject* toGlobalObject(ExecState*) const;

        virtual ExecState* globalExec();

        virtual bool shouldInterruptScript() const { return true; }

        virtual bool allowsAccessFrom(const JSGlobalObject*) const { return true; }

        ActivationImp* pushActivation(ExecState*);
        void popActivation();
        void tearOffActivation(ExecState*, bool markAsRelic = false);

        virtual bool isDynamicScope() const;

        ExecStateStack& activeExecStates() const { return d()->activeExecStates; }

        HashSet<JSObject*>& arrayVisitedElements() { if (!d()->arrayVisitedElements) d()->arrayVisitedElements.set(new HashSet<JSObject*>); return *d()->arrayVisitedElements; }

        // Per-thread hash tables, cached on the global object for faster access.
        const PerThreadData* perThreadData() const { return &d()->perThreadData; }

        // Initialize and/or retrieve per-thread hash tables - use perThreadData() for faster access instead.
        static ThreadClassInfoHashTables* threadClassInfoHashTables();

    private:
        void init(JSObject* thisValue);
        
        JSGlobalObjectData* d() const { return static_cast<JSGlobalObjectData*>(JSVariableObject::d); }

        bool checkTimeout();
        void resetTimeoutCheck();

        void deleteActivationStack();
        void checkActivationCount();

        static JSGlobalObject* s_head;
    };

    inline bool JSGlobalObject::timedOut()
    {
        d()->tickCount++;

        if (d()->tickCount != d()->ticksUntilNextTimeoutCheck)
            return false;

        return checkTimeout();
    }

    inline ActivationImp* JSGlobalObject::pushActivation(ExecState* exec)
    {
        if (d()->activationCount == activationStackNodeSize) {
            ActivationStackNode* newNode = new ActivationStackNode;
            newNode->prev = d()->activations;
            d()->activations = newNode;
            d()->activationCount = 0;
        }
        
        StackActivation* stackEntry = &d()->activations->data[d()->activationCount++];
        stackEntry->activationStorage.init(exec);
        return &stackEntry->activationStorage;
    }

    inline void JSGlobalObject::checkActivationCount()
    {
        if (!d()->activationCount) {
            ActivationStackNode* prev = d()->activations->prev;
            ASSERT(prev);
            delete d()->activations;
            d()->activations = prev;
            d()->activationCount = activationStackNodeSize;
        }
    }

    inline void JSGlobalObject::popActivation()
    {
        checkActivationCount();
        d()->activations->data[--d()->activationCount].activationDataStorage.localStorage.shrink(0);    
    }

} // namespace KJS

#endif // KJS_GlobalObject_h
