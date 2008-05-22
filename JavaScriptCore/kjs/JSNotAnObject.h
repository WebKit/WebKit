/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef JSNotAnObject_h
#define JSNotAnObject_h

#include "object.h"

namespace KJS {
    
    // This unholy class is used to allow us to avoid multiple exception checks
    // in certain SquirrelFish opcodes -- effectively it just silently consumes
    // any operations performed on the result of a failed toObject call.
    class JSNotAnObject : public JSObject {
    public:
        JSNotAnObject(JSObject* exception)
            : m_exception(exception)
        {
        }

        // JSValue methods
        virtual JSValue *toPrimitive(ExecState* exec, JSType preferredType = UnspecifiedType) const;
        virtual bool getPrimitiveNumber(ExecState* exec, double& number, JSValue*& value);
        virtual bool toBoolean(ExecState* exec) const;
        virtual double toNumber(ExecState* exec) const;
        virtual UString toString(ExecState* exec) const;
        virtual JSObject *toObject(ExecState* exec) const;
        
        // marking
        virtual void mark();

        virtual bool getOwnPropertySlot(ExecState* , const Identifier&, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState* , unsigned index, PropertySlot&);

        virtual void put(ExecState*, const Identifier& propertyName, JSValue* value);
        virtual void put(ExecState*, unsigned propertyName, JSValue* value);

        virtual bool deleteProperty(ExecState* exec, const Identifier &propertyName);
        virtual bool deleteProperty(ExecState* exec, unsigned propertyName);

        virtual JSValue *defaultValue(ExecState* exec, JSType hint) const;

        virtual JSObject* construct(ExecState* exec, const List& args);
        virtual JSObject* construct(ExecState* exec, const List& args, const Identifier& functionName, const UString& sourceURL, int lineNumber);
        
        virtual JSValue *callAsFunction(ExecState* exec, JSObject *thisObj, const List &args);

        virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    private:
        JSObject* m_exception;
    };
}

#endif
