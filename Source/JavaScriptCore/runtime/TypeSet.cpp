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

#include "InspectorJSProtocolTypes.h"
#include "JSCJSValue.h"
#include "JSCJSValueInlines.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/Vector.h>

namespace JSC {

TypeSet::TypeSet()
    : m_seenTypes(TypeNothing)
    , m_isOverflown(false)
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
        ret = TypeNothing;

    return ret;
}

void TypeSet::addTypeInformation(RuntimeType type, PassRefPtr<StructureShape> prpNewShape, StructureID id) 
{
    RefPtr<StructureShape> newShape = prpNewShape;
    m_seenTypes = m_seenTypes | type;

    if (id && newShape && type != TypeString) {
        ASSERT(m_structureIDCache.isValidValue(id));
        auto addResult = m_structureIDCache.add(id);
        if (addResult.isNewEntry) {
            // Make one more pass making sure that: 
            // - We don't have two instances of the same shape. (Same shapes may have different StructureIDs).
            // - We don't have two shapes that share the same prototype chain. If these shapes share the same 
            //   prototype chain, they will be merged into one shape.
            bool found = false;
            String hash = newShape->propertyHash();
            for (size_t i = 0; i < m_structureHistory.size(); i++) {
                RefPtr<StructureShape>& seenShape = m_structureHistory.at(i);
                if (seenShape->propertyHash() == hash) {
                    found = true;
                    break;
                } 
                if (seenShape->hasSamePrototypeChain(newShape)) {
                    seenShape = StructureShape::merge(seenShape, newShape);
                    found = true;
                    break;
                }
            }

            if (!found) {
                if (m_structureHistory.size() < 100)
                    m_structureHistory.append(newShape);
                else if (!m_isOverflown)
                    m_isOverflown = true;
            }
        }
    }
}

void TypeSet::invalidateCache()
{
    m_structureIDCache.clear();
}

String TypeSet::seenTypes() const
{
    if (m_seenTypes == TypeNothing)
        return ASCIILiteral("(Unreached Statement)");

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

    for (size_t i = 0; i < m_structureHistory.size(); i++) {
        RefPtr<StructureShape> shape = m_structureHistory.at(i);
        seen.append(shape->m_constructorName);
        seen.append(' ');
    }

    if (m_structureHistory.size()) 
        seen.append("\nStructures:[ ");
    for (size_t i = 0; i < m_structureHistory.size(); i++) {
        seen.append(m_structureHistory.at(i)->stringRepresentation());
        seen.append(' ');
    }
    if (m_structureHistory.size())
        seen.append(']');

    if (m_structureHistory.size()) {
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
        return emptyString();

    if (m_structureHistory.size() && doesTypeConformTo(TypeObject | TypeNull | TypeUndefined)) {
        String ctorName = leastCommonAncestor(); 

        if (doesTypeConformTo(TypeObject))
            return ctorName;
        if (doesTypeConformTo(TypeObject | TypeNull | TypeUndefined))
            return ctorName + '?';
    }

    // The order of these checks are important. For example, if a value is only a function, it conforms to TypeFunction, but it also conforms to TypeFunction | TypeNull.
    // Therefore, more specific types must be checked first.

    if (doesTypeConformTo(TypeFunction))
        return ASCIILiteral("Function");
    if (doesTypeConformTo(TypeUndefined))
        return ASCIILiteral("Undefined");
    if (doesTypeConformTo(TypeNull))
        return ASCIILiteral("Null");
    if (doesTypeConformTo(TypeBoolean))
        return ASCIILiteral("Boolean");
    if (doesTypeConformTo(TypeMachineInt))
        return ASCIILiteral("Integer");
    if (doesTypeConformTo(TypeNumber | TypeMachineInt))
        return ASCIILiteral("Number");
    if (doesTypeConformTo(TypeString))
        return ASCIILiteral("String");

    if (doesTypeConformTo(TypeNull | TypeUndefined))
        return ASCIILiteral("(?)");

    if (doesTypeConformTo(TypeFunction | TypeNull | TypeUndefined))
        return ASCIILiteral("Function?");
    if (doesTypeConformTo(TypeBoolean | TypeNull | TypeUndefined))
        return ASCIILiteral("Boolean?");
    if (doesTypeConformTo(TypeMachineInt | TypeNull | TypeUndefined))
        return ASCIILiteral("Integer?");
    if (doesTypeConformTo(TypeNumber | TypeMachineInt | TypeNull | TypeUndefined))
        return ASCIILiteral("Number?");
    if (doesTypeConformTo(TypeString | TypeNull | TypeUndefined))
        return ASCIILiteral("String?");
   
    if (doesTypeConformTo(TypeObject | TypeFunction | TypeString))
        return ASCIILiteral("Object");
    if (doesTypeConformTo(TypeObject | TypeFunction | TypeString | TypeNull | TypeUndefined))
        return ASCIILiteral("Object?");

    return ASCIILiteral("(many)");
}

String TypeSet::leastCommonAncestor() const
{
    return StructureShape::leastCommonAncestor(m_structureHistory);
}

#if ENABLE(INSPECTOR)
PassRefPtr<Inspector::Protocol::Array<String>> TypeSet::allPrimitiveTypeNames() const
{
    RefPtr<Inspector::Protocol::Array<String>> seen = Inspector::Protocol::Array<String>::create();
    if (m_seenTypes & TypeUndefined)
        seen->addItem(ASCIILiteral("Undefined"));
    if (m_seenTypes & TypeNull)
        seen->addItem(ASCIILiteral("Null"));
    if (m_seenTypes & TypeBoolean)
        seen->addItem(ASCIILiteral("Boolean"));
    if (m_seenTypes & TypeMachineInt)
        seen->addItem(ASCIILiteral("Integer"));
    if (m_seenTypes & TypeNumber)
        seen->addItem(ASCIILiteral("Number"));
    if (m_seenTypes & TypeString)
        seen->addItem(ASCIILiteral("String"));

    return seen.release();
}

PassRefPtr<Inspector::Protocol::Array<Inspector::Protocol::Runtime::StructureDescription>> TypeSet::allStructureRepresentations() const
{
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Runtime::StructureDescription>> description = Inspector::Protocol::Array<Inspector::Protocol::Runtime::StructureDescription>::create();

    for (size_t i = 0; i < m_structureHistory.size(); i++)
        description->addItem(m_structureHistory.at(i)->inspectorRepresentation());

    return description.release();
}

PassRefPtr<Inspector::Protocol::Runtime::TypeSet> TypeSet::inspectorTypeSet() const
{
    return Inspector::Protocol::Runtime::TypeSet::create()
        .setIsFunction((m_seenTypes & TypeFunction) != TypeNothing)
        .setIsUndefined((m_seenTypes & TypeUndefined) != TypeNothing)
        .setIsNull((m_seenTypes & TypeNull) != TypeNothing)
        .setIsBoolean((m_seenTypes & TypeBoolean) != TypeNothing)
        .setIsInteger((m_seenTypes & TypeMachineInt) != TypeNothing)
        .setIsNumber((m_seenTypes & TypeNumber) != TypeNothing)
        .setIsString((m_seenTypes & TypeString) != TypeNothing)
        .setIsObject((m_seenTypes & TypeObject) != TypeNothing)
        .release();
}
#endif

String TypeSet::toJSONString() const
{
    // This returns a JSON string representing an Object with the following properties:
    //     displayTypeName: 'String'
    //     primitiveTypeNames: 'Array<String>'
    //     structures: 'Array<JSON<StructureShape>>'

    StringBuilder json;
    json.append("{");

    json.append("\"displayTypeName\":");
    json.append("\"");
    json.append(displayName());
    json.append("\"");
    json.append(",");

    json.append("\"primitiveTypeNames\":");
    json.append("[");
    bool hasAnItem = false;
    if (m_seenTypes & TypeUndefined) {
        hasAnItem = true;
        json.append("\"Undefined\"");
    }
    if (m_seenTypes & TypeNull) {
        if (hasAnItem)
            json.append(",");
        hasAnItem = true;
        json.append("\"Null\"");
    }
    if (m_seenTypes & TypeBoolean) {
        if (hasAnItem)
            json.append(",");
        hasAnItem = true;
        json.append("\"Boolean\"");
    }
    if (m_seenTypes & TypeMachineInt) {
        if (hasAnItem)
            json.append(",");
        hasAnItem = true;
        json.append("\"Integer\"");
    }
    if (m_seenTypes & TypeNumber) {
        if (hasAnItem)
            json.append(",");
        hasAnItem = true;
        json.append("\"Number\"");
    }
    if (m_seenTypes & TypeString) {
        if (hasAnItem)
            json.append(",");
        hasAnItem = true;
        json.append("\"String\"");
    }
    json.append("]");

    json.append(",");

    json.append("\"structures\":");
    json.append("[");
    hasAnItem = false;
    for (size_t i = 0; i < m_structureHistory.size(); i++) {
        if (hasAnItem)
            json.append(",");
        hasAnItem = true;
        json.append(m_structureHistory[i]->toJSONString());
    }
    json.append("]");

    json.append("}");
    return json.toString();
}

void TypeSet::dumpSeenTypes()
{
    dataLog(seenTypes(), "\n");
}

StructureShape::StructureShape()
    : m_proto(nullptr)
    , m_propertyHash(nullptr)
    , m_final(false)
    , m_isInDictionaryMode(false)
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
    m_fields.add(impl);
}

String StructureShape::propertyHash() 
{
    ASSERT(m_final);
    if (m_propertyHash)
        return *m_propertyHash;

    StringBuilder builder;
    builder.append(':');
    builder.append(m_constructorName);
    builder.append(':');
    
    for (auto iter = m_fields.begin(), end = m_fields.end(); iter != end; ++iter) {
        String property = String((*iter));
        property.replace(":", "\\:"); // Ensure that hash({"foo:", "bar"}) != hash({"foo", ":bar"}) because we're using colons as a separator and colons are legal characters in field names in JS.
        builder.append(property);
    }

    if (m_proto) {
        builder.append(':');
        builder.append("__proto__");
        builder.append(m_proto->propertyHash());
    }

    m_propertyHash = std::make_unique<String>(builder.toString());
    return *m_propertyHash;
}

String StructureShape::leastCommonAncestor(const Vector<RefPtr<StructureShape>> shapes)
{
    if (!shapes.size())
        return emptyString();

    RefPtr<StructureShape> origin = shapes.at(0);
    for (size_t i = 1; i < shapes.size(); i++) {
        bool foundLUB = false;
        while (!foundLUB) {
            RefPtr<StructureShape> check = shapes.at(i);
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

    representation.append('{');
    while (curShape) {
        for (auto it = curShape->m_fields.begin(), end = curShape->m_fields.end(); it != end; ++it) {
            String prop((*it).get());
            representation.append(prop);
            representation.append(", ");
        }

        if (curShape->m_proto) {
            String prot = makeString("__proto__ [", curShape->m_proto->m_constructorName, ']');
            representation.append(prot);
            representation.append(", ");
        }

        curShape = curShape->m_proto;
    }

    if (representation.length() >= 3)
        representation.resize(representation.length() - 2);

    representation.append('}');

    return representation.toString();
}

String StructureShape::toJSONString() const
{
    // This returns a JSON string representing an Object with the following properties:
    //     constructorName: 'String'
    //     fields: 'Array<String>'
    //     optionalFields: 'Array<String>'
    //     proto: 'JSON<StructureShape> | null'

    StringBuilder json;
    json.append("{");

    json.append("\"constructorName\":");
    json.append("\"");
    json.append(m_constructorName);
    json.append("\"");
    json.append(",");

    json.append("\"isInDictionaryMode\":");
    if (m_isInDictionaryMode)
        json.append("true");
    else
        json.append("false");
    json.append(",");

    json.append("\"fields\":");
    json.append("[");
    bool hasAnItem = false;
    for (auto it = m_fields.begin(), end = m_fields.end(); it != end; ++it) {
        if (hasAnItem)
            json.append(",");
        hasAnItem = true;

        String fieldName((*it).get());
        json.append("\"");
        json.append(fieldName);
        json.append("\"");
    }
    json.append("]");
    json.append(",");

    json.append("\"optionalFields\":");
    json.append("[");
    hasAnItem = false;
    for (auto it = m_optionalFields.begin(), end = m_optionalFields.end(); it != end; ++it) {
        if (hasAnItem)
            json.append(",");
        hasAnItem = true;

        String fieldName((*it).get());
        json.append("\"");
        json.append(fieldName);
        json.append("\"");
    }
    json.append("]");
    json.append(",");

    json.append("\"proto\":");
    if (m_proto)
        json.append(m_proto->toJSONString());
    else
        json.append("null");

    json.append("}");

    return json.toString();
}

#if ENABLE(INSPECTOR)
PassRefPtr<Inspector::Protocol::Runtime::StructureDescription> StructureShape::inspectorRepresentation()
{
    RefPtr<Inspector::Protocol::Runtime::StructureDescription> base = Inspector::Protocol::Runtime::StructureDescription::create();
    RefPtr<Inspector::Protocol::Runtime::StructureDescription> currentObject = base;
    RefPtr<StructureShape> currentShape = this;

    while (currentShape) {
        auto fields = Inspector::Protocol::Array<String>::create();
        auto optionalFields = Inspector::Protocol::Array<String>::create();
        for (auto field : currentShape->m_fields)
            fields->addItem(field.get());
        for (auto field : currentShape->m_optionalFields)
            optionalFields->addItem(field.get());

        currentObject->setFields(fields);
        currentObject->setOptionalFields(optionalFields);
        currentObject->setConstructorName(currentShape->m_constructorName);
        currentObject->setIsImprecise(currentShape->m_isInDictionaryMode);

        if (currentShape->m_proto) {
            RefPtr<Inspector::Protocol::Runtime::StructureDescription> nextObject = Inspector::Protocol::Runtime::StructureDescription::create();
            currentObject->setPrototypeStructure(nextObject);
            currentObject = nextObject;
        }

        currentShape = currentShape->m_proto;
    }

    return base.release();
}
#endif

bool StructureShape::hasSamePrototypeChain(PassRefPtr<StructureShape> prpOther)
{
    RefPtr<StructureShape> self = this;
    RefPtr<StructureShape> other = prpOther;
    while (self && other) {
        if (self->m_constructorName != other->m_constructorName)
            return false;
        self = self->m_proto;
        other = other->m_proto;
    }

    return !self && !other;
}

PassRefPtr<StructureShape> StructureShape::merge(const PassRefPtr<StructureShape> prpA, const PassRefPtr<StructureShape> prpB)
{
    RefPtr<StructureShape> a = prpA;
    RefPtr<StructureShape> b = prpB;
    ASSERT(a->hasSamePrototypeChain(b));

    RefPtr<StructureShape> merged = StructureShape::create();
    for (auto field : a->m_fields) {
        if (b->m_fields.contains(field))
            merged->m_fields.add(field);
        else
            merged->m_optionalFields.add(field);
    }

    for (auto field : b->m_fields) {
        if (!merged->m_fields.contains(field)) {
            auto addResult = merged->m_optionalFields.add(field);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }
    }

    for (auto field : a->m_optionalFields)
        merged->m_optionalFields.add(field);
    for (auto field : b->m_optionalFields)
        merged->m_optionalFields.add(field);

    ASSERT(a->m_constructorName == b->m_constructorName);
    merged->setConstructorName(a->m_constructorName);

    if (a->m_proto) {
        RELEASE_ASSERT(b->m_proto);
        merged->setProto(StructureShape::merge(a->m_proto, b->m_proto));
    }

    merged->markAsFinal();

    return merged.release();
}

void StructureShape::enterDictionaryMode()
{
    m_isInDictionaryMode = true;
}

} //namespace JSC
