/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "JSActivation.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "Machine.h"

namespace JSC {

    struct ArgumentsData : Noncopyable {
        JSActivation* activation;

        unsigned numParameters;
        int firstParameterIndex;
        unsigned numArguments;

        Register* registers;
        OwnArrayPtr<Register> registerArray;

        Register* extraArguments;
        OwnArrayPtr<bool> deletedArguments;
        Register extraArgumentsFixedBuffer[4];

        JSFunction* callee;
        bool overrodeLength : 1;
        bool overrodeCallee : 1;
    };


    class Arguments : public JSObject {
    public:
        Arguments(ExecState*, Register* callFrame);
        Arguments(ExecState*, JSActivation*);
        virtual ~Arguments();

        static const ClassInfo info;

        virtual void mark();

        void fillArgList(ExecState*, ArgList&);

        void copyRegisters();
        void setRegisters(Register* registers) { d->registers = registers; }

    private:
        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        virtual void put(ExecState*, const Identifier& propertyName, JSValue*, PutPropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValue*, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);

        virtual const ClassInfo* classInfo() const { return &info; }

        void init(ExecState*, Register* callFrame);

        OwnPtr<ArgumentsData> d;
    };

    inline void Arguments::init(ExecState* exec, Register* callFrame)
    {
        JSFunction* callee;
        int firstParameterIndex;
        Register* argv;
        int numArguments;
        exec->machine()->getArgumentsData(callFrame, callee, firstParameterIndex, argv, numArguments);

        d->numParameters = callee->numParameters();
        d->firstParameterIndex = firstParameterIndex;
        d->numArguments = numArguments;

        d->registers = callFrame;

        Register* extraArguments;
        if (d->numArguments <= d->numParameters)
            extraArguments = 0;
        else {
            unsigned numExtraArguments = d->numArguments - d->numParameters;
            if (numExtraArguments > sizeof(d->extraArgumentsFixedBuffer) / sizeof(Register))
                extraArguments = new Register[numExtraArguments];
            else
                extraArguments = d->extraArgumentsFixedBuffer;
            for (unsigned i = 0; i < numExtraArguments; ++i)
                extraArguments[i] = argv[d->numParameters + i];
        }

        d->extraArguments = extraArguments;

        d->callee = callee;
        d->overrodeLength = false;
        d->overrodeCallee = false;
    }

    inline Arguments::Arguments(ExecState* exec, Register* callFrame)
        : JSObject(exec->lexicalGlobalObject()->argumentsStructure())
        , d(new ArgumentsData)
    {
        d->activation = 0;
        init(exec, callFrame);
    }

    inline Arguments::Arguments(ExecState* exec, JSActivation* activation)
        : JSObject(exec->lexicalGlobalObject()->argumentsStructure())
        , d(new ArgumentsData)
    {
        ASSERT(activation);
        d->activation = activation;
        init(exec, &activation->registerAt(0));
    }

    inline void Arguments::copyRegisters()
    {
        ASSERT(!d->activation);
        ASSERT(!d->registerArray);

        size_t numParametersMinusThis = d->callee->m_body->generatedByteCode().numParameters - 1;

        if (!numParametersMinusThis)
            return;

        int registerOffset = numParametersMinusThis + RegisterFile::CallFrameHeaderSize;
        size_t registerArraySize = numParametersMinusThis;

        Register* registerArray = new Register[registerArraySize];
        memcpy(registerArray, d->registers - registerOffset, registerArraySize * sizeof(Register));
        d->registerArray.set(registerArray);
        d->registers = registerArray + registerOffset;
    }

    // This JSActivation function is defined here so it can get at Arguments::setRegisters.
    inline void JSActivation::copyRegisters(JSValue* arguments)
    {
        ASSERT(!d()->registerArray);

        size_t numParametersMinusThis = d()->functionBody->generatedByteCode().numParameters - 1;
        size_t numVars = d()->functionBody->generatedByteCode().numVars;
        size_t numLocals = numVars + numParametersMinusThis;

        if (!numLocals)
            return;

        int registerOffset = numParametersMinusThis + RegisterFile::CallFrameHeaderSize;
        size_t registerArraySize = numLocals + RegisterFile::CallFrameHeaderSize;

        Register* registerArray = copyRegisterArray(d()->registers - registerOffset, registerArraySize);
        setRegisters(registerArray + registerOffset, registerArray);
        if (arguments) {
            ASSERT(arguments->isObject(&Arguments::info));
            static_cast<Arguments*>(arguments)->setRegisters(registerArray + registerOffset);
        }
    }

} // namespace JSC

#endif // Arguments_h
