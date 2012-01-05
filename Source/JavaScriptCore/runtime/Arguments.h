/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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

#ifndef Arguments_h
#define Arguments_h

#include "CodeOrigin.h"
#include "JSActivation.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "Interpreter.h"
#include "ObjectConstructor.h"

namespace JSC {

    struct ArgumentsData {
        WTF_MAKE_NONCOPYABLE(ArgumentsData); WTF_MAKE_FAST_ALLOCATED;
    public:
        ArgumentsData() { }
        WriteBarrier<JSActivation> activation;

        unsigned numArguments;

        WriteBarrier<Unknown>* registers;
        OwnArrayPtr<WriteBarrier<Unknown> > registerArray;

        OwnArrayPtr<bool> deletedArguments;

        WriteBarrier<JSFunction> callee;
        bool overrodeLength : 1;
        bool overrodeCallee : 1;
        bool overrodeCaller : 1;
        bool isStrictMode : 1;
    };

    class Arguments : public JSNonFinalObject {
    public:
        typedef JSNonFinalObject Base;

        static Arguments* create(JSGlobalData& globalData, CallFrame* callFrame)
        {
            Arguments* arguments = new (NotNull, allocateCell<Arguments>(globalData.heap)) Arguments(callFrame);
            arguments->finishCreation(callFrame);
            return arguments;
        }

        enum { MaxArguments = 0x10000 };

    private:
        enum NoParametersType { NoParameters };
        
        Arguments(CallFrame*);
        Arguments(CallFrame*, NoParametersType);

    public:
        static const ClassInfo s_info;

        static void visitChildren(JSCell*, SlotVisitor&);

        void fillArgList(ExecState*, MarkedArgumentBuffer&);

        uint32_t length(ExecState* exec) const 
        {
            if (UNLIKELY(d->overrodeLength))
                return get(exec, exec->propertyNames().length).toUInt32(exec);
            return d->numArguments; 
        }
        
        void copyToArguments(ExecState*, CallFrame*, uint32_t length);
        void tearOff(CallFrame*);
        bool isTornOff() const { return d->registerArray; }
        void didTearOffActivation(JSGlobalData& globalData, JSActivation* activation)
        {
            if (isTornOff())
                return;
            d->activation.set(globalData, this, activation);
            d->registers = &activation->registerAt(0);
        }

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype) 
        { 
            return Structure::create(globalData, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), &s_info); 
        }

    protected:
        static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesVisitChildren | OverridesGetPropertyNames | JSObject::StructureFlags;

        void finishCreation(CallFrame*);

    private:
        static void destroy(JSCell*);
        static bool getOwnPropertySlot(JSCell*, ExecState*, const Identifier& propertyName, PropertySlot&);
        static bool getOwnPropertySlotByIndex(JSCell*, ExecState*, unsigned propertyName, PropertySlot&);
        static bool getOwnPropertyDescriptor(JSObject*, ExecState*, const Identifier&, PropertyDescriptor&);
        static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
        static void put(JSCell*, ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue);
        static bool deleteProperty(JSCell*, ExecState*, const Identifier& propertyName);
        static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);
        void createStrictModeCallerIfNecessary(ExecState*);
        void createStrictModeCalleeIfNecessary(ExecState*);

        WriteBarrier<Unknown>& argument(size_t);

        void init(CallFrame*);

        OwnPtr<ArgumentsData> d;
    };

    Arguments* asArguments(JSValue);

    inline Arguments* asArguments(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&Arguments::s_info));
        return static_cast<Arguments*>(asObject(value));
    }

    inline Arguments::Arguments(CallFrame* callFrame)
        : JSNonFinalObject(callFrame->globalData(), callFrame->lexicalGlobalObject()->argumentsStructure())
        , d(adoptPtr(new ArgumentsData))
    {
    }

    inline Arguments::Arguments(CallFrame* callFrame, NoParametersType)
        : JSNonFinalObject(callFrame->globalData(), callFrame->lexicalGlobalObject()->argumentsStructure())
        , d(adoptPtr(new ArgumentsData))
    {
    }

    inline WriteBarrier<Unknown>& Arguments::argument(size_t i)
    {
        return d->registers[CallFrame::argumentOffset(i)];
    }

    inline void Arguments::finishCreation(CallFrame* callFrame)
    {
        Base::finishCreation(callFrame->globalData());
        ASSERT(inherits(&s_info));

        JSFunction* callee = asFunction(callFrame->callee());
        d->numArguments = callFrame->argumentCount();
        d->registers = reinterpret_cast<WriteBarrier<Unknown>*>(callFrame->registers());
        d->callee.set(callFrame->globalData(), this, callee);
        d->overrodeLength = false;
        d->overrodeCallee = false;
        d->overrodeCaller = false;
        d->isStrictMode = callFrame->codeBlock()->isStrictMode();

        // The bytecode generator omits op_tear_off_activation in cases of no
        // declared parameters, so we need to tear off immediately.
        if (d->isStrictMode || !callee->jsExecutable()->parameterCount())
            tearOff(callFrame);
    }

} // namespace JSC

#endif // Arguments_h
