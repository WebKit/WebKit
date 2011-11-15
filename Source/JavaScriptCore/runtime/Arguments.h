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

        unsigned numParameters;
        ptrdiff_t firstParameterIndex;
        unsigned numArguments;

        WriteBarrier<Unknown>* registers;
        OwnArrayPtr<WriteBarrier<Unknown> > registerArray;

        WriteBarrier<Unknown>* extraArguments;
        OwnArrayPtr<bool> deletedArguments;
        WriteBarrier<Unknown> extraArgumentsFixedBuffer[4];

        WriteBarrier<JSFunction> callee;
        bool overrodeLength : 1;
        bool overrodeCallee : 1;
        bool overrodeCaller : 1;
        bool isStrictMode : 1;
        bool isInlineFrame : 1; // If true, all arguments are in the extraArguments buffer.
    };


    class Arguments : public JSNonFinalObject {
    public:
        typedef JSNonFinalObject Base;

        static Arguments* create(JSGlobalData& globalData, CallFrame* callFrame)
        {
            Arguments* arguments = new (allocateCell<Arguments>(globalData.heap)) Arguments(callFrame);
            arguments->finishCreation(callFrame);
            return arguments;
        }
        
        static Arguments* createAndTearOff(JSGlobalData& globalData, CallFrame* callFrame)
        {
            Arguments* arguments = new (allocateCell<Arguments>(globalData.heap)) Arguments(callFrame);
            arguments->finishCreationAndTearOff(callFrame);
            return arguments;
        }
        
        static Arguments* createNoParameters(JSGlobalData& globalData, CallFrame* callFrame)
        {
            Arguments* arguments = new (allocateCell<Arguments>(globalData.heap)) Arguments(callFrame, NoParameters);
            arguments->finishCreation(callFrame, NoParameters);
            return arguments;
        }

        // Use an enum because otherwise gcc insists on doing a memory
        // read.
        enum { MaxArguments = 0x10000 };

    private:
        enum NoParametersType { NoParameters };
        
        Arguments(CallFrame*);
        Arguments(CallFrame*, NoParametersType);

    public:
        virtual ~Arguments();

        static const ClassInfo s_info;

        static void visitChildren(JSCell*, SlotVisitor&);

        void fillArgList(ExecState*, MarkedArgumentBuffer&);

        uint32_t length(ExecState* exec) const 
        {
            if (UNLIKELY(d->overrodeLength))
                return get(exec, exec->propertyNames().length).toUInt32(exec);
            return d->numArguments; 
        }
        
        void copyToRegisters(ExecState* exec, Register* buffer, uint32_t maxSize);
        void tearOff(JSGlobalData&);
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

        void finishCreationButDontTearOff(CallFrame*);
        void finishCreation(CallFrame*);
        void finishCreationAndTearOff(CallFrame*);
        void finishCreation(CallFrame*, NoParametersType);

    private:
        void getArgumentsData(CallFrame*, JSFunction*&, ptrdiff_t& firstParameterIndex, Register*& argv, int& argc);
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

        void init(CallFrame*);

        OwnPtr<ArgumentsData> d;
    };

    Arguments* asArguments(JSValue);

    inline Arguments* asArguments(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&Arguments::s_info));
        return static_cast<Arguments*>(asObject(value));
    }

    ALWAYS_INLINE void Arguments::getArgumentsData(CallFrame* callFrame, JSFunction*& function, ptrdiff_t& firstParameterIndex, Register*& argv, int& argc)
    {
        function = asFunction(callFrame->callee());

        int numParameters = function->jsExecutable()->parameterCount();
        argc = callFrame->argumentCountIncludingThis();
        
        if (callFrame->isInlineCallFrame())
            ASSERT(argc == numParameters + 1);

        if (argc <= numParameters)
            argv = callFrame->registers() - RegisterFile::CallFrameHeaderSize - numParameters;
        else
            argv = callFrame->registers() - RegisterFile::CallFrameHeaderSize - numParameters - argc;

        argc -= 1; // - 1 to skip "this"
        firstParameterIndex = -RegisterFile::CallFrameHeaderSize - numParameters;
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
    
    inline void Arguments::finishCreationButDontTearOff(CallFrame* callFrame)
    {
        Base::finishCreation(callFrame->globalData());
        ASSERT(inherits(&s_info));

        JSFunction* callee;
        ptrdiff_t firstParameterIndex;
        Register* argv;
        int numArguments;
        getArgumentsData(callFrame, callee, firstParameterIndex, argv, numArguments);

        d->numParameters = callee->jsExecutable()->parameterCount();
        d->firstParameterIndex = firstParameterIndex;
        d->numArguments = numArguments;
        d->isInlineFrame = false;

        d->registers = reinterpret_cast<WriteBarrier<Unknown>*>(callFrame->registers());

        WriteBarrier<Unknown>* extraArguments;
        if (d->numArguments <= d->numParameters)
            extraArguments = 0;
        else {
            unsigned numExtraArguments = d->numArguments - d->numParameters;
            if (numExtraArguments > sizeof(d->extraArgumentsFixedBuffer) / sizeof(WriteBarrier<Unknown>))
                extraArguments = new WriteBarrier<Unknown>[numExtraArguments];
            else
                extraArguments = d->extraArgumentsFixedBuffer;
            for (unsigned i = 0; i < numExtraArguments; ++i)
                extraArguments[i].set(callFrame->globalData(), this, argv[d->numParameters + i].jsValue());
        }

        d->extraArguments = extraArguments;

        d->callee.set(callFrame->globalData(), this, callee);
        d->overrodeLength = false;
        d->overrodeCallee = false;
        d->overrodeCaller = false;
        d->isStrictMode = callFrame->codeBlock()->isStrictMode();
    }

    inline void Arguments::finishCreation(CallFrame* callFrame)
    {
        ASSERT(!callFrame->isInlineCallFrame());
        finishCreationButDontTearOff(callFrame);
        if (d->isStrictMode)
            tearOff(callFrame->globalData());
    }

    inline void Arguments::finishCreationAndTearOff(CallFrame* callFrame)
    {
        Base::finishCreation(callFrame->globalData());
        ASSERT(inherits(&s_info));
        
        JSFunction* callee;

        ptrdiff_t firstParameterIndex;
        Register* argv;
        int numArguments;
        getArgumentsData(callFrame, callee, firstParameterIndex, argv, numArguments);
        
        d->numParameters = callee->jsExecutable()->parameterCount();
        d->firstParameterIndex = firstParameterIndex;
        d->numArguments = numArguments;
        
        if (d->numParameters) {
            int registerOffset = d->numParameters + RegisterFile::CallFrameHeaderSize;
            size_t registerArraySize = d->numParameters;
            
            OwnArrayPtr<WriteBarrier<Unknown> > registerArray = adoptArrayPtr(new WriteBarrier<Unknown>[registerArraySize]);
            if (callFrame->isInlineCallFrame()) {
                InlineCallFrame* inlineCallFrame = callFrame->inlineCallFrame();
                for (size_t i = 0; i < registerArraySize; ++i) {
                    ValueRecovery& recovery = inlineCallFrame->arguments[i + 1];
                    // In the future we'll support displaced recoveries (indicating that the
                    // argument was flushed to a different location), but for now we don't do
                    // that so this code will fail if that were to happen. On the other hand,
                    // it's much less likely that we'll support in-register recoveries since
                    // this code does not (easily) have access to registers.
                    JSValue value;
                    Register* location = callFrame->registers() + i - registerOffset;
                    switch (recovery.technique()) {
                    case AlreadyInRegisterFile:
                        value = location->jsValue();
                        break;
                    case AlreadyInRegisterFileAsUnboxedInt32:
                        value = jsNumber(location->unboxedInt32());
                        break;
                    case AlreadyInRegisterFileAsUnboxedCell:
                        value = location->unboxedCell();
                        break;
                    case AlreadyInRegisterFileAsUnboxedBoolean:
                        value = jsBoolean(location->unboxedBoolean());
                        break;
                    case Constant:
                        value = recovery.constant();
                        break;
                    default:
                        ASSERT_NOT_REACHED();
                        break;
                    }
                    registerArray[i].set(callFrame->globalData(), this, value);
                }
            } else {
                for (size_t i = 0; i < registerArraySize; ++i)
                    registerArray[i].set(callFrame->globalData(), this, callFrame->registers()[i - registerOffset].jsValue());
            }
            d->registers = registerArray.get() + d->numParameters + RegisterFile::CallFrameHeaderSize;
            d->registerArray = registerArray.release();
        }
        
        WriteBarrier<Unknown>* extraArguments;
        if (callFrame->isInlineCallFrame())
            ASSERT(d->numArguments == d->numParameters);
        if (d->numArguments <= d->numParameters)
            extraArguments = 0;
        else {
            unsigned numExtraArguments = d->numArguments - d->numParameters;
            if (numExtraArguments > sizeof(d->extraArgumentsFixedBuffer) / sizeof(WriteBarrier<Unknown>))
                extraArguments = new WriteBarrier<Unknown>[numExtraArguments];
            else
                extraArguments = d->extraArgumentsFixedBuffer;
            for (unsigned i = 0; i < numExtraArguments; ++i)
                extraArguments[i].set(callFrame->globalData(), this, argv[d->numParameters + i].jsValue());
        }
        
        d->extraArguments = extraArguments;

        d->callee.set(callFrame->globalData(), this, callee);
        d->overrodeLength = false;
        d->overrodeCallee = false;
        d->overrodeCaller = false;
        d->isInlineFrame = callFrame->isInlineCallFrame();
        d->isStrictMode = callFrame->codeBlock()->isStrictMode();
    }

    inline void Arguments::finishCreation(CallFrame* callFrame, NoParametersType)
    {
        ASSERT(!callFrame->isInlineCallFrame());
        Base::finishCreation(callFrame->globalData());
        ASSERT(inherits(&s_info));
        ASSERT(!asFunction(callFrame->callee())->jsExecutable()->parameterCount());

        unsigned numArguments = callFrame->argumentCount();

        d->numParameters = 0;
        d->numArguments = numArguments;
        d->isInlineFrame = false;

        WriteBarrier<Unknown>* extraArguments;
        if (numArguments > sizeof(d->extraArgumentsFixedBuffer) / sizeof(Register))
            extraArguments = new WriteBarrier<Unknown>[numArguments];
        else
            extraArguments = d->extraArgumentsFixedBuffer;

        Register* argv = callFrame->registers() - RegisterFile::CallFrameHeaderSize - numArguments - 1;
        for (unsigned i = 0; i < numArguments; ++i)
            extraArguments[i].set(callFrame->globalData(), this, argv[i].jsValue());

        d->extraArguments = extraArguments;

        d->callee.set(callFrame->globalData(), this, asFunction(callFrame->callee()));
        d->overrodeLength = false;
        d->overrodeCallee = false;
        d->overrodeCaller = false;
        d->isStrictMode = callFrame->codeBlock()->isStrictMode();
        if (d->isStrictMode)
            tearOff(callFrame->globalData());
    }

    inline void Arguments::tearOff(JSGlobalData& globalData)
    {
        ASSERT(!isTornOff());

        if (!d->numParameters)
            return;

        int registerOffset = d->numParameters + RegisterFile::CallFrameHeaderSize;
        size_t registerArraySize = d->numParameters;

        OwnArrayPtr<WriteBarrier<Unknown> > registerArray = adoptArrayPtr(new WriteBarrier<Unknown>[registerArraySize]);
        for (size_t i = 0; i < registerArraySize; i++)
            registerArray[i].set(globalData, this, d->registers[i - registerOffset].get());
        d->registers = registerArray.get() + registerOffset;
        d->registerArray = registerArray.release();
    }

} // namespace JSC

#endif // Arguments_h
