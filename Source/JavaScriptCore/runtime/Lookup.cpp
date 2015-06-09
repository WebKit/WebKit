/*
 *  Copyright (C) 2008, 2012 Apple Inc. All rights reserved.
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
#include "GetterSetter.h"
#include "JSFunction.h"
#include "JSCInlines.h"

namespace JSC {

void HashTable::createTable() const
{
    ASSERT(!keys);
    keys = static_cast<const char**>(fastMalloc(sizeof(char*) * numberOfValues));

    for (int i = 0; i < numberOfValues; ++i) {
        if (values[i].m_key)
            keys[i] = values[i].m_key;
        else
            keys[i] = 0;
    }
}

void HashTable::deleteTable() const
{
    if (keys) {
        fastFree(keys);
        keys = nullptr;
    }
}

void reifyStaticAccessor(VM& vm, const HashTableValue& value, JSObject& thisObj, PropertyName propertyName)
{
    JSGlobalObject* globalObject = thisObj.globalObject();
    GetterSetter* accessor = GetterSetter::create(vm, globalObject);
    if (value.accessorGetter()) {
        RefPtr<StringImpl> getterName = WTF::tryMakeString(ASCIILiteral("get "), String(*propertyName.publicName()));
        if (!getterName)
            return;
        accessor->setGetter(vm, globalObject, JSFunction::create(vm, globalObject, 0, *getterName, value.accessorGetter()));
    }
    thisObj.putDirectNonIndexAccessor(vm, propertyName, accessor, value.attributes());
}

bool setUpStaticFunctionSlot(ExecState* exec, const HashTableValue* entry, JSObject* thisObj, PropertyName propertyName, PropertySlot& slot)
{
    ASSERT(thisObj->globalObject());
    ASSERT(entry->attributes() & BuiltinOrFunctionOrAccessor);
    VM& vm = exec->vm();
    unsigned attributes;
    bool isAccessor = entry->attributes() & Accessor;
    PropertyOffset offset = thisObj->getDirectOffset(vm, propertyName, attributes);

    if (!isValidOffset(offset)) {
        // If a property is ever deleted from an object with a static table, then we reify
        // all static functions at that time - after this we shouldn't be re-adding anything.
        if (thisObj->staticFunctionsReified())
            return false;

        if (entry->attributes() & Builtin)
            thisObj->putDirectBuiltinFunction(vm, thisObj->globalObject(), propertyName, entry->builtinGenerator()(vm), entry->attributes());
        else if (entry->attributes() & Function) {
            thisObj->putDirectNativeFunction(
                vm, thisObj->globalObject(), propertyName, entry->functionLength(),
                entry->function(), entry->intrinsic(), entry->attributes());
        } else {
            ASSERT(isAccessor);
            reifyStaticAccessor(vm, *entry, *thisObj, propertyName);
        }

        offset = thisObj->getDirectOffset(vm, propertyName, attributes);
        ASSERT(isValidOffset(offset));
    }

    if (isAccessor)
        slot.setCacheableGetterSlot(thisObj, attributes, jsCast<GetterSetter*>(thisObj->getDirect(offset)), offset);
    else
        slot.setValue(thisObj, attributes, thisObj->getDirect(offset), offset);
    return true;
}

} // namespace JSC
