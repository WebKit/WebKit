/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSVariableObject_h
#define JSVariableObject_h

#include "Register.h"
#include "SymbolTable.h"
#include "UnusedParam.h"
#include "object.h"
#include <wtf/UnusedParam.h>

namespace KJS {

    class Register;

    class JSVariableObject : public JSObject {
    public:
        SymbolTable& symbolTable() const { return *d->symbolTable; }

        virtual void putWithAttributes(ExecState*, const Identifier&, JSValue*, unsigned attributes) = 0;

        virtual bool deleteProperty(ExecState*, const Identifier&);
        virtual void getPropertyNames(ExecState*, PropertyNameArray&);
        
        virtual bool isVariableObject() const;
        virtual bool isDynamicScope() const = 0;

        virtual bool getPropertyAttributes(ExecState*, const Identifier& propertyName, unsigned& attributes) const;

        JSValue*& valueAt(int index) const { return registers()[index].u.jsValue; }
    protected:
        // Subclasses of JSVariableObject can subclass this struct to add data
        // without increasing their own size (since there's a hard limit on the
        // size of a JSCell).
        struct JSVariableObjectData {
            JSVariableObjectData(SymbolTable* symbolTable_, Register** registerBase_, int registerOffset_)
                : symbolTable(symbolTable_)
                , registerBase(registerBase_)
                , registerOffset(registerOffset_)
            {
                ASSERT(symbolTable_);
                ASSERT(registerBase_);
            }

            SymbolTable* symbolTable; // Maps name -> offset from "r" in register file.

            Register** registerBase; // Location where a pointer to the base of the register file is stored.
            int registerOffset; // Offset of "r", the register past the end of local storage.
        };

        JSVariableObject(JSVariableObjectData* data)
            : d(data) // Subclass owns this pointer.
        {
        }

        JSVariableObject(JSValue* proto, JSVariableObjectData* data)
            : JSObject(proto)
            , d(data) // Subclass owns this pointer.
        {
        }

        Register** registerBase() const { return d->registerBase; }
        Register* registers() const { return *registerBase() + d->registerOffset; }

        bool symbolTableGet(const Identifier&, PropertySlot&);
        bool symbolTableGet(const Identifier&, PropertySlot&, bool& slotIsWriteable);
        bool symbolTablePut(const Identifier&, JSValue*);
        bool symbolTablePutWithAttributes(const Identifier&, JSValue*, unsigned attributes);

        JSVariableObjectData* d;
    };

    inline bool JSVariableObject::symbolTableGet(const Identifier& propertyName, PropertySlot& slot)
    {
        SymbolTableEntry entry = symbolTable().inlineGet(propertyName.ustring().rep());
        if (!entry.isEmpty()) {
            slot.setValueSlot(&valueAt(entry.getIndex()));
            return true;
        }
        return false;
    }

    inline bool JSVariableObject::symbolTableGet(const Identifier& propertyName, PropertySlot& slot, bool& slotIsWriteable)
    {
        SymbolTableEntry entry = symbolTable().inlineGet(propertyName.ustring().rep());
        if (!entry.isEmpty()) {
            slot.setValueSlot(&valueAt(entry.getIndex()));
            slotIsWriteable = !entry.isReadOnly();
            return true;
        }
        return false;
    }

    inline bool JSVariableObject::symbolTablePut(const Identifier& propertyName, JSValue* value)
    {
        SymbolTableEntry entry = symbolTable().inlineGet(propertyName.ustring().rep());
        if (entry.isEmpty())
            return false;
        if (entry.isReadOnly())
            return true;
        valueAt(entry.getIndex()) = value;
        return true;
    }

    inline bool JSVariableObject::symbolTablePutWithAttributes(const Identifier& propertyName, JSValue* value, unsigned attributes)
    {
        SymbolTable::iterator iter = symbolTable().find(propertyName.ustring().rep());
        if (iter == symbolTable().end())
            return false;
        SymbolTableEntry& entry = iter->second;
        ASSERT(!entry.isEmpty());
        entry.setAttributes(attributes);
        valueAt(entry.getIndex()) = value;
        return true;
    }

} // namespace KJS

#endif // JSVariableObject_h
