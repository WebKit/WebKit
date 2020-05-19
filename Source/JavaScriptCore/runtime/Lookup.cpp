/*
 *  Copyright (C) 2008, 2012, 2015-2016 Apple Inc. All rights reserved.
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

#include "GetterSetter.h"
#include "StructureInlines.h"

namespace JSC {

void reifyStaticAccessor(VM& vm, const HashTableValue& value, JSObject& thisObject, PropertyName propertyName)
{
    JSGlobalObject* globalObject = thisObject.globalObject(vm);
    JSObject* getter = nullptr;
    if (value.accessorGetter()) {
        if (value.attributes() & PropertyAttribute::Builtin)
            getter = JSFunction::create(vm, value.builtinAccessorGetterGenerator()(vm), globalObject);
        else {
            String getterName = tryMakeString("get "_s, String(*propertyName.publicName()));
            if (!getterName)
                return;
            getter = JSFunction::create(vm, globalObject, 0, getterName, value.accessorGetter());
        }
    }
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, nullptr);
    thisObject.putDirectNonIndexAccessor(vm, propertyName, accessor, attributesForStructure(value.attributes()));
}

bool setUpStaticFunctionSlot(VM& vm, const ClassInfo* classInfo, const HashTableValue* entry, JSObject* thisObject, PropertyName propertyName, PropertySlot& slot)
{
    ASSERT(thisObject->globalObject(vm));
    ASSERT(entry->attributes() & PropertyAttribute::BuiltinOrFunctionOrAccessorOrLazyProperty);
    unsigned attributes;
    bool isAccessor = entry->attributes() & PropertyAttribute::Accessor;
    PropertyOffset offset = thisObject->getDirectOffset(vm, propertyName, attributes);

    if (!isValidOffset(offset)) {
        // If a property is ever deleted from an object with a static table, then we reify
        // all static functions at that time - after this we shouldn't be re-adding anything.
        if (thisObject->staticPropertiesReified(vm))
            return false;

        reifyStaticProperty(vm, classInfo, propertyName, *entry, *thisObject);

        offset = thisObject->getDirectOffset(vm, propertyName, attributes);
        if (!isValidOffset(offset)) {
            dataLog("Static hashtable initialiation for ", propertyName, " did not produce a property.\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    if (isAccessor)
        slot.setCacheableGetterSlot(thisObject, attributes, jsCast<GetterSetter*>(thisObject->getDirect(offset)), offset);
    else
        slot.setValue(thisObject, attributes, thisObject->getDirect(offset), offset);
    return true;
}

} // namespace JSC
