/*
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "Lookup.h"

#include "Executable.h"
#include "JSFunction.h"

namespace JSC {

void HashTable::createTable(JSGlobalData* globalData) const
{
    ASSERT(!table);
    int linkIndex = compactHashSizeMask + 1;
    HashEntry* entries = new HashEntry[compactSize];
    for (int i = 0; i < compactSize; ++i)
        entries[i].setKey(0);
    for (int i = 0; values[i].key; ++i) {
        StringImpl* identifier = Identifier::add(globalData, values[i].key).leakRef();
        int hashIndex = identifier->existingHash() & compactHashSizeMask;
        HashEntry* entry = &entries[hashIndex];

        if (entry->key()) {
            while (entry->next()) {
                entry = entry->next();
            }
            ASSERT(linkIndex < compactSize);
            entry->setNext(&entries[linkIndex++]);
            entry = entry->next();
        }

        entry->initialize(identifier, values[i].attributes, values[i].value1, values[i].value2
#if ENABLE(JIT)
                          , values[i].generator
#if ENABLE(DFG_JIT)
                          , values[i].intrinsic
#endif
#endif
                          );
    }
    table = entries;
}

void HashTable::deleteTable() const
{
    if (table) {
        int max = compactSize;
        for (int i = 0; i != max; ++i) {
            if (StringImpl* key = table[i].key())
                key->deref();
        }
        delete [] table;
        table = 0;
    }
}

bool setUpStaticFunctionSlot(ExecState* exec, const HashEntry* entry, JSObject* thisObj, const Identifier& propertyName, PropertySlot& slot)
{
    ASSERT(thisObj->globalObject());
    ASSERT(entry->attributes() & Function);
    WriteBarrierBase<Unknown>* location = thisObj->getDirectLocation(exec->globalData(), propertyName);

    if (!location) {
        // If a property is ever deleted from an object with a static table, then we reify
        // all static functions at that time - after this we shouldn't be re-adding anything.
        if (thisObj->staticFunctionsReified())
            return false;
    
        JSFunction* function;
        JSGlobalObject* globalObject = thisObj->globalObject();
#if ENABLE(JIT)
        if (exec->globalData().canUseJIT() && (entry->generator() || entry->intrinsic() != DFG::NoIntrinsic))
            function = JSFunction::create(exec, globalObject, entry->functionLength(), propertyName, exec->globalData().getHostFunction(entry->function(), entry->generator(), entry->intrinsic()));
        else
#endif
            function = JSFunction::create(exec, globalObject, entry->functionLength(), propertyName, entry->function());

        thisObj->putDirect(exec->globalData(), propertyName, function, entry->attributes());
        location = thisObj->getDirectLocation(exec->globalData(), propertyName);
    }

    slot.setValue(thisObj, location->get(), thisObj->offsetForLocation(location));
    return true;
}

} // namespace JSC
