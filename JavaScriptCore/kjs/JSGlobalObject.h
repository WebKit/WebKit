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
    class JSGlobalObject;
    class NativeErrorImp;
    class NativeErrorPrototype;
    class NumberObjectImp;
    class NumberPrototype;
    class ObjectObjectImp;
    class ObjectPrototype;
    class RangeError;
    class RangeErrorPrototype;
    class ReferenceError;
    class ReferenceError;
    class ReferenceErrorPrototype;
    class RegExpObjectImp;
    class RegExpPrototype;
    class RuntimeMethod;
    class SavedBuiltins;
    class ScopeChain;
    class StringObjectImp;
    class StringPrototype;
    class SyntaxErrorPrototype;
    class TypeError;
    class TypeErrorPrototype;
    class UriError;
    class UriErrorPrototype;
    struct ActivationStackNode;

    enum CompatMode { NativeMode, IECompat, NetscapeCompat };

    class JSGlobalObject : public JSVariableObject {
    protected:
        using JSVariableObject::JSVariableObjectData;

        struct JSGlobalObjectData : public JSVariableObjectData {
            JSGlobalObjectData(JSGlobalObject* globalObject)
                : JSVariableObjectData(&inlineSymbolTable)
                , globalExec(globalObject)
            {
            }

            JSGlobalObject* next;
            JSGlobalObject* prev;

            Debugger* debugger;
            CompatMode compatMode;
            
            GlobalExecState globalExec;
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

            ActivationStackNode* activations;
            size_t activationCount;
        };

    public:
        JSGlobalObject()
            : JSVariableObject(new JSGlobalObjectData(this))
        {
            init();
        }

    protected:
        JSGlobalObject(JSValue* proto)
            : JSVariableObject(proto, new JSGlobalObjectData(this))
        {
            init();
        }

    public:
        virtual ~JSGlobalObject();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual void put(ExecState*, const Identifier&, JSValue*, int attr = None);

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

        void saveBuiltins(SavedBuiltins&) const;
        void restoreBuiltins(const SavedBuiltins&);

        void setTimeoutTime(unsigned timeoutTime) { d()->timeoutTime = timeoutTime; }
        void startTimeoutCheck();
        void stopTimeoutCheck();
        bool timedOut();

        Debugger* debugger() const { return d()->debugger; }
        void setDebugger(Debugger* debugger) { d()->debugger = debugger; }

        // FIXME: Let's just pick one compatible behavior and go with it.
        void setCompatMode(CompatMode mode) { d()->compatMode = mode; }
        CompatMode compatMode() const { return d()->compatMode; }
        
        int recursion() { return d()->recursion; }
        void incRecursion() { ++d()->recursion; }
        void decRecursion() { --d()->recursion; }

        virtual void mark();

        virtual bool isGlobalObject() const { return true; }

        virtual ExecState* globalExec();

        virtual bool shouldInterruptScript() const { return true; }

        virtual bool allowsAccessFrom(const JSGlobalObject*) const { return true; }

        ActivationImp* pushActivation(ExecState*);
        void popActivation();
        void tearOffActivation(ExecState*, bool markAsRelic = false);

    private:
        void init();
        
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

} // namespace KJS

#endif // KJS_GlobalObject_h
