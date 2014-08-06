/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TypeSet.h"

#include "JSCJSValue.h"
#include "JSCJSValueInlines.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/Vector.h>

namespace JSC {

TypeSet::TypeSet()
    : m_seenTypes(TypeNothing)
    , m_structureHistory(new Vector<RefPtr<StructureShape>>)
{
}

RuntimeType TypeSet::getRuntimeTypeForValue(JSValue v)
{
    RuntimeType ret;
    if (v.isFunction())
        ret = TypeFunction;
    else if (v.isUndefined())
        ret = TypeUndefined;
    else if (v.isNull())
        ret = TypeNull;
    else if (v.isBoolean())
        ret = TypeBoolean;
    else if (v.isMachineInt())
        ret = TypeMachineInt;
    else if (v.isNumber())
        ret = TypeNumber;
    else if (v.isString())
        ret = TypeString;
    else if (v.isPrimitive())
        ret = TypePrimitive;
    else if (v.isObject())
        ret = TypeObject;
    else
        CRASH();

    return ret;
}

void TypeSet::addTypeForValue(JSValue v, PassRefPtr<StructureShape> shape, StructureID id) 
{
    RuntimeType t = getRuntimeTypeForValue(v);
    m_seenTypes = m_seenTypes | t;

    if (id && shape) {
        ASSERT(m_structureIDHistory.isValidKey(id));
        auto iter = m_structureIDHistory.find(id);
        if (iter == m_structureIDHistory.end()) {
            m_structureIDHistory.set(id, 1);
            // Make one more pass making sure that we don't have the same shape. (Same shapes may have different StructureIDs).
            bool found = false;
            String hash = shape->propertyHash();
            for (size_t i = 0; i < m_structureHistory->size(); i++) {
                RefPtr<StructureShape> obj = m_structureHistory->at(i);
                if (obj->propertyHash() == hash) {
                    found = true;
                    break;
                }
            }

            if (!found)
                m_structureHistory->append(shape);
        }
    }
}

String TypeSet::seenTypes() 
{
    if (m_seenTypes == TypeNothing)
        return "(Unreached Statement)";

    StringBuilder seen;

    if (m_seenTypes & TypeFunction)
         seen.append("Function ");
    if (m_seenTypes & TypeUndefined)
         seen.append("Undefined ");
    if (m_seenTypes & TypeNull)
         seen.append("Null ");
    if (m_seenTypes & TypeBoolean)
         seen.append("Boolean ");
    if (m_seenTypes & TypeMachineInt)
         seen.append("MachineInt ");
    if (m_seenTypes & TypeNumber)
         seen.append("Number ");
    if (m_seenTypes & TypeString)
         seen.append("String ");
    if (m_seenTypes & TypePrimitive)
         seen.append("Primitive ");
    if (m_seenTypes & TypeObject)
         seen.append("Object ");

    for (size_t i = 0; i < m_structureHistory->size(); i++) {
        RefPtr<StructureShape> shape = m_structureHistory->at(i);
        if (!shape->m_constructorName.isEmpty()) {
            seen.append(shape->m_constructorName);
            seen.append(" ");
        }
    }

    if (m_structureHistory->size()) 
        seen.append("\nStructures:[ ");
    for (size_t i = 0; i < m_structureHistory->size(); i++) {
        seen.append(m_structureHistory->at(i)->stringRepresentation());
        seen.append(" ");
    }
    if (m_structureHistory->size())
        seen.append("]");

    if (m_structureHistory->size()) {
        seen.append("\nLUB: ");
        seen.append(StructureShape::leastUpperBound(m_structureHistory));
    }

    return seen.toString();
}

void TypeSet::dumpSeenTypes()
{
    dataLog(seenTypes(), "\n");
}

StructureShape::StructureShape()
    : m_propertyHash(nullptr)
    , m_final(false)
{
}

void StructureShape::markAsFinal()
{
    ASSERT(!m_final);
    m_final = true;
}

void StructureShape::addProperty(RefPtr<StringImpl> impl)
{
    ASSERT(!m_final);
    m_fields.set(impl, true);
}

String StructureShape::propertyHash() 
{
    ASSERT(m_final);
    if (m_propertyHash)
        return *m_propertyHash;

    StringBuilder builder;
    builder.append(":");
    for (auto iter = m_fields.begin(), end = m_fields.end(); iter != end; ++iter) {
        String property = String(iter->key);
        property.replace(":", "\\:"); // Ensure that hash({"foo:", "bar"}) != hash({"foo", ":bar"}) because we're using colons as a separator and colons are legal characters in field names in JS.
        builder.append(property);
    }

    m_propertyHash = std::make_unique<String>(builder.toString());
    return *m_propertyHash;
}

String StructureShape::leastUpperBound(Vector<RefPtr<StructureShape>>* shapes)
{
    if (!shapes->size())
        return "";

    StringBuilder lub;
    RefPtr<StructureShape> origin = shapes->at(0);
    lub.append("{");
    for (auto iter = origin->m_fields.begin(), end = origin->m_fields.end(); iter != end; ++iter) {
        bool shouldAdd = true;
        for (size_t i = 1, size = shapes->size(); i < size; i++) {
            // If all other Shapes have the same field as origin, add it to the least upper bound.
            if (!shapes->at(i)->m_fields.contains(iter->key)) {
                shouldAdd = false;
                break;
            }
        }
        if (shouldAdd)
            lub.append(String(iter->key.get()) + String(", "));
    }

    if (lub.length() >= 3)
        lub.resize(lub.length() - 2); // Remove the trailing ', '

    lub.append("}");
    
    return lub.toString();
}

String StructureShape::stringRepresentation()
{
    StringBuilder representation;
    representation.append("{");
    for (auto iter = m_fields.begin(), end = m_fields.end(); iter != end; ++iter) {
        String prop(iter->key.get());
        representation.append(prop);
        representation.append(", ");
    }

    if (representation.length() >= 3)
        representation.resize(representation.length() - 2);

    representation.append("}");

    return representation.toString();
}

} //namespace JSC
