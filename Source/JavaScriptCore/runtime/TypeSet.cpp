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

#include "InspectorJSTypeBuilders.h"
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

    if (id && shape && !v.isString() && !v.isFunction()) {
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

String TypeSet::seenTypes() const
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
    if (m_seenTypes & TypeObject)
         seen.append("Object ");

    for (size_t i = 0; i < m_structureHistory->size(); i++) {
        RefPtr<StructureShape> shape = m_structureHistory->at(i);
        seen.append(shape->m_constructorName);
        seen.append(" ");
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
        seen.append("\nLeast Common Ancestor: ");
        seen.append(leastCommonAncestor());
    }

    return seen.toString();
}

bool TypeSet::doesTypeConformTo(uint32_t test) const
{
    // This function checks if our seen types conform  to the types described by the test bitstring. (i.e we haven't seen more types than test).
    // We are <= to those types if ANDing with the bitstring doesn't zero out any of our bits.

    // For example:

    // 0b0110 (seen)
    // 0b1111 (test)
    // ------ (AND)
    // 0b0110 == seen

    // 0b0110 (seen)
    // 0b0010 (test)
    // ------ (AND)
    // 0b0010 != seen

    return (m_seenTypes & test) == m_seenTypes;
}

String TypeSet::displayName() const
{
    if (m_seenTypes == TypeNothing)
        return "";

    if (m_structureHistory->size() && doesTypeConformTo(TypeObject | TypeNull | TypeUndefined)) {
        String ctorName = leastCommonAncestor(); 

        if (doesTypeConformTo(TypeObject))
            return ctorName;
        else if (doesTypeConformTo(TypeObject | TypeNull | TypeUndefined))
            return ctorName + "?";
    }

    // The order of these checks are important. For example, if a value is only a function, it conforms to TypeFunction, but it also conforms to TypeFunction | TypeNull.
    // Therefore, more specific types must be checked first.

    if (doesTypeConformTo(TypeFunction))
        return "Function";
    if (doesTypeConformTo(TypeUndefined))
        return "Undefined";
    if (doesTypeConformTo(TypeNull))
        return "Null";
    if (doesTypeConformTo(TypeBoolean))
        return "Boolean";
    if (doesTypeConformTo(TypeMachineInt))
        return "Integer";
    if (doesTypeConformTo(TypeNumber | TypeMachineInt))
        return "Number";
    if (doesTypeConformTo(TypeString))
        return "String";

    if (doesTypeConformTo(TypeNull | TypeUndefined))
        return "(?)";

    if (doesTypeConformTo(TypeFunction | TypeNull | TypeUndefined))
        return "Function?";
    if (doesTypeConformTo(TypeBoolean | TypeNull | TypeUndefined))
        return "Boolean?";
    if (doesTypeConformTo(TypeMachineInt | TypeNull | TypeUndefined))
        return "Integer?";
    if (doesTypeConformTo(TypeNumber | TypeMachineInt | TypeNull | TypeUndefined))
        return "Number?";
    if (doesTypeConformTo(TypeString | TypeNull | TypeUndefined))
        return "String?";
   
    if (doesTypeConformTo(TypeObject | TypeFunction | TypeString))
        return "Object";
    if (doesTypeConformTo(TypeObject | TypeFunction | TypeString | TypeNull | TypeUndefined))
        return "Object?";

    return "(many)";
}

PassRefPtr<Inspector::TypeBuilder::Array<String>> TypeSet::allPrimitiveTypeNames() const
{
    RefPtr<Inspector::TypeBuilder::Array<String>> seen = Inspector::TypeBuilder::Array<String>::create();
    if (m_seenTypes & TypeFunction)
         seen->addItem("Function");
    if (m_seenTypes & TypeUndefined)
         seen->addItem("Undefined");
    if (m_seenTypes & TypeNull)
         seen->addItem("Null");
    if (m_seenTypes & TypeBoolean)
         seen->addItem("Boolean");
    if (m_seenTypes & TypeMachineInt)
         seen->addItem("Integer");
    if (m_seenTypes & TypeNumber)
         seen->addItem("Number");
    if (m_seenTypes & TypeString)
         seen->addItem("String");

    return seen.release();
}

PassRefPtr<Inspector::TypeBuilder::Array<Inspector::InspectorObject>> TypeSet::allStructureRepresentations() const
{
    RefPtr<Inspector::TypeBuilder::Array<Inspector::InspectorObject>> ret = Inspector::TypeBuilder::Array<Inspector::InspectorObject>::create();

    for (size_t i = 0; i < m_structureHistory->size(); i++)
        ret->addItem(m_structureHistory->at(i)->inspectorRepresentation());

    return ret.release();
}

String TypeSet::leastCommonAncestor() const
{
    return StructureShape::leastCommonAncestor(m_structureHistory);
}

void TypeSet::dumpSeenTypes()
{
    dataLog(seenTypes(), "\n");
}

StructureShape::StructureShape()
    : m_proto(nullptr)
    , m_propertyHash(nullptr)
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
    m_fields.append(impl);
}

String StructureShape::propertyHash() 
{
    ASSERT(m_final);
    if (m_propertyHash)
        return *m_propertyHash;

    StringBuilder builder;
    builder.append(":");
    builder.append(m_constructorName);
    builder.append(":");
    
    for (auto iter = m_fields.begin(), end = m_fields.end(); iter != end; ++iter) {
        String property = String((*iter));
        property.replace(":", "\\:"); // Ensure that hash({"foo:", "bar"}) != hash({"foo", ":bar"}) because we're using colons as a separator and colons are legal characters in field names in JS.
        builder.append(property);
    }

    if (m_proto) {
        builder.append(":");
        builder.append("__proto__");
        builder.append(m_proto->propertyHash());
    }

    m_propertyHash = std::make_unique<String>(builder.toString());
    return *m_propertyHash;
}

String StructureShape::leastCommonAncestor(const Vector<RefPtr<StructureShape>>* shapes)
{
    if (!shapes->size())
        return "";

    RefPtr<StructureShape> origin = shapes->at(0);
    for (size_t i = 1; i < shapes->size(); i++) {
        bool foundLUB = false;
        while (!foundLUB) {
            RefPtr<StructureShape> check = shapes->at(i);
            String curCtorName = origin->m_constructorName;
            while (check) {
                if (check->m_constructorName == curCtorName) {
                    foundLUB = true;
                    break;
                }
                check = check->m_proto;
            }
            if (!foundLUB) {
                origin = origin->m_proto;
                // All Objects must share the 'Object' Prototype. Therefore, at the very least, we should always converge on 'Object' before reaching a null prototype.
                RELEASE_ASSERT(origin); 
            }
        }

        if (origin->m_constructorName == "Object")
            break;
    }

    return origin->m_constructorName;
}

String StructureShape::stringRepresentation()
{
    StringBuilder representation;
    RefPtr<StructureShape> curShape = this;

    representation.append("{");
    while (curShape) {
        for (auto iter = curShape->m_fields.begin(), end = curShape->m_fields.end(); iter != end; ++iter) {
            String prop((*iter).get());
            representation.append(prop);
            representation.append(", ");
        }

        if (curShape->m_proto) {
            String prot = String("__proto__") + String(" [") + curShape->m_proto->m_constructorName + String("]");
            representation.append(prot);
            representation.append(", ");
        }

        curShape = curShape->m_proto;
    }

    if (representation.length() >= 3)
        representation.resize(representation.length() - 2);

    representation.append("}");

    return representation.toString();
}

PassRefPtr<Inspector::InspectorObject> StructureShape::inspectorRepresentation()
{
    RefPtr<Inspector::InspectorObject> base = Inspector::InspectorObject::create();
    RefPtr<Inspector::InspectorObject> currentObject = base;
    RefPtr<StructureShape> currentShape = this;

    while (currentShape) {
        RefPtr<Inspector::TypeBuilder::Array<String>> fields = Inspector::TypeBuilder::Array<String>::create();
        for (auto iter = currentShape->m_fields.begin(), end = currentShape->m_fields.end(); iter != end; ++iter)
            fields->addItem((*iter).get());

        currentObject->setArray(ASCIILiteral("fields"), fields->asArray());
        currentObject->setString(ASCIILiteral("constructorName"), currentShape->m_constructorName);

        if (currentShape->m_proto) {
            RefPtr<Inspector::InspectorObject> nextObject = Inspector::InspectorObject::create();
            currentObject->setObject(ASCIILiteral("prototypeStructure"), nextObject);
            currentObject = nextObject;
        }

        currentShape = currentShape->m_proto;
    }

    return base.release();
}

} //namespace JSC
