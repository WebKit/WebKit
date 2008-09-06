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

#ifndef JSPropertyNameIterator_h
#define JSPropertyNameIterator_h

#include "JSObject.h"
#include "JSString.h"
#include "PropertyNameArray.h"

namespace KJS {

    class Identifier;
    class JSObject;

    class JSPropertyNameIterator : public JSCell {
    public:
        static JSPropertyNameIterator* create(ExecState*, JSValue*);

        virtual ~JSPropertyNameIterator();

        virtual JSValue* toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double&, JSValue*&);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual void mark();

        JSValue* next(ExecState*);
        void invalidate();

    private:
        JSPropertyNameIterator(JSObject*, Identifier* propertyNames, size_t numProperties);

        JSObject* m_object;
        Identifier* m_propertyNames;
        Identifier* m_position;
        Identifier* m_end;
    };

inline JSPropertyNameIterator::JSPropertyNameIterator(JSObject* object, Identifier* propertyNames, size_t numProperties)
    : m_object(object)
    , m_propertyNames(propertyNames)
    , m_position(propertyNames)
    , m_end(propertyNames + numProperties)
{
}

inline JSPropertyNameIterator* JSPropertyNameIterator::create(ExecState* exec, JSValue* v)
{
    if (v->isUndefinedOrNull())
        return new (exec) JSPropertyNameIterator(0, 0, 0);

    JSObject* o = v->toObject(exec);
    PropertyNameArray propertyNames(exec);
    o->getPropertyNames(exec, propertyNames);
    size_t numProperties = propertyNames.size();
    return new (exec) JSPropertyNameIterator(o, propertyNames.releaseIdentifiers(), numProperties);
}

inline JSValue* JSPropertyNameIterator::next(ExecState* exec)
{
    while (m_position != m_end) {
        if (m_object->hasProperty(exec, *m_position))
            return jsOwnedString(exec, (*m_position++).ustring());
        m_position++;
    }
    invalidate();
    return 0;
}

} // namespace KJS

#endif // JSPropertyNameIterator_h
