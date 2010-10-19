/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CppVariant.h"

#include "WebBindings.h"
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/StringExtras.h>

using namespace WebKit;
using namespace std;

CppVariant::CppVariant()
{
    type = NPVariantType_Null;
}

// Note that Set() performs a deep copy, which is necessary to safely
// call FreeData() on the value in the destructor.
CppVariant::CppVariant(const CppVariant& original)
{
    type = NPVariantType_Null;
    set(original);
}

// See comment for copy constructor, above.
CppVariant& CppVariant::operator=(const CppVariant& original)
{
    if (&original != this)
        set(original);
    return *this;
}

CppVariant::~CppVariant()
{
    freeData();
}

void CppVariant::freeData()
{
    WebBindings::releaseVariantValue(this);
}

bool CppVariant::isEqual(const CppVariant& other) const
{
    if (type != other.type)
        return false;

    switch (type) {
    case NPVariantType_Bool:
        return (value.boolValue == other.value.boolValue);
    case NPVariantType_Int32:
        return (value.intValue == other.value.intValue);
    case NPVariantType_Double:
        return (value.doubleValue == other.value.doubleValue);
    case NPVariantType_String: {
        const NPString *this_value = &value.stringValue;
        const NPString *other_value = &other.value.stringValue;
        uint32_t len = this_value->UTF8Length;
        return len == other_value->UTF8Length
            && !strncmp(this_value->UTF8Characters,
                        other_value->UTF8Characters, len);
    }
    case NPVariantType_Null:
    case NPVariantType_Void:
        return true;
    case NPVariantType_Object: {
        NPObject* thisValue = value.objectValue;
        NPObject* otherValue = other.value.objectValue;
        return thisValue->_class == otherValue->_class
            && thisValue->referenceCount == otherValue->referenceCount;
    }
    }
    return false;
}

void CppVariant::copyToNPVariant(NPVariant* result) const
{
    result->type = type;
    switch (type) {
    case NPVariantType_Bool:
        result->value.boolValue = value.boolValue;
        break;
    case NPVariantType_Int32:
        result->value.intValue = value.intValue;
        break;
    case NPVariantType_Double:
        result->value.doubleValue = value.doubleValue;
        break;
    case NPVariantType_String:
        WebBindings::initializeVariantWithStringCopy(result, &value.stringValue);
        break;
    case NPVariantType_Null:
    case NPVariantType_Void:
        // Nothing to set.
        break;
    case NPVariantType_Object:
        result->type = NPVariantType_Object;
        result->value.objectValue = WebBindings::retainObject(value.objectValue);
        break;
    }
}

void CppVariant::set(const NPVariant& newValue)
{
    freeData();
    switch (newValue.type) {
    case NPVariantType_Bool:
        set(newValue.value.boolValue);
        break;
    case NPVariantType_Int32:
        set(newValue.value.intValue);
        break;
    case NPVariantType_Double:
        set(newValue.value.doubleValue);
        break;
    case NPVariantType_String:
        set(newValue.value.stringValue);
        break;
    case NPVariantType_Null:
    case NPVariantType_Void:
        type = newValue.type;
        break;
    case NPVariantType_Object:
        set(newValue.value.objectValue);
        break;
    }
}

void CppVariant::setNull()
{
    freeData();
    type = NPVariantType_Null;
}

void CppVariant::set(bool newValue)
{
    freeData();
    type = NPVariantType_Bool;
    value.boolValue = newValue;
}

void CppVariant::set(int32_t newValue)
{
    freeData();
    type = NPVariantType_Int32;
    value.intValue = newValue;
}

void CppVariant::set(double newValue)
{
    freeData();
    type = NPVariantType_Double;
    value.doubleValue = newValue;
}

// The newValue must be a null-terminated string.
void CppVariant::set(const char* newValue)
{
    freeData();
    type = NPVariantType_String;
    NPString newString = {newValue,
                          static_cast<uint32_t>(strlen(newValue))};
    WebBindings::initializeVariantWithStringCopy(this, &newString);
}

void CppVariant::set(const string& newValue)
{
    freeData();
    type = NPVariantType_String;
    NPString newString = {newValue.data(),
                          static_cast<uint32_t>(newValue.size())};
    WebBindings::initializeVariantWithStringCopy(this, &newString);
}

void CppVariant::set(const NPString& newValue)
{
    freeData();
    type = NPVariantType_String;
    WebBindings::initializeVariantWithStringCopy(this, &newValue);
}

void CppVariant::set(NPObject* newValue)
{
    freeData();
    type = NPVariantType_Object;
    value.objectValue = WebBindings::retainObject(newValue);
}

string CppVariant::toString() const
{
    ASSERT(isString());
    return string(value.stringValue.UTF8Characters,
                  value.stringValue.UTF8Length);
}

int32_t CppVariant::toInt32() const
{
    if (isInt32())
        return value.intValue;
    if (isDouble())
        return static_cast<int32_t>(value.doubleValue);
    ASSERT_NOT_REACHED();
    return 0;
}

double CppVariant::toDouble() const
{
    if (isInt32())
        return static_cast<double>(value.intValue);
    if (isDouble())
        return value.doubleValue;
    ASSERT_NOT_REACHED();
    return 0;
}

bool CppVariant::toBoolean() const
{
    ASSERT(isBool());
    return value.boolValue;
}

Vector<string> CppVariant::toStringVector() const
{

    ASSERT(isObject());
    Vector<string> stringVector;
    NPObject* npValue = value.objectValue;
    NPIdentifier lengthId = WebBindings::getStringIdentifier("length");

    if (!WebBindings::hasProperty(0, npValue, lengthId))
        return stringVector;

    NPVariant lengthValue;
    if (!WebBindings::getProperty(0, npValue, lengthId, &lengthValue))
        return stringVector;

    int length = 0;
    // The length is a double in some cases.
    if (NPVARIANT_IS_DOUBLE(lengthValue))
        length = static_cast<int>(NPVARIANT_TO_DOUBLE(lengthValue));
    else if (NPVARIANT_IS_INT32(lengthValue))
        length = NPVARIANT_TO_INT32(lengthValue);
    WebBindings::releaseVariantValue(&lengthValue);

    // For sanity, only allow 100 items.
    length = min(100, length);
    for (int i = 0; i < length; ++i) {
        // Get each of the items.
        char indexInChar[20]; // Enough size to store 32-bit integer
        snprintf(indexInChar, 20, "%d", i);
        string index(indexInChar);
        NPIdentifier indexId = WebBindings::getStringIdentifier(index.c_str());
        if (!WebBindings::hasProperty(0, npValue, indexId))
            continue;
        NPVariant indexValue;
        if (!WebBindings::getProperty(0, npValue, indexId, &indexValue))
            continue;
        if (NPVARIANT_IS_STRING(indexValue)) {
            string item(NPVARIANT_TO_STRING(indexValue).UTF8Characters,
                        NPVARIANT_TO_STRING(indexValue).UTF8Length);
            stringVector.append(item);
        }
        WebBindings::releaseVariantValue(&indexValue);
    }
    return stringVector;
}

bool CppVariant::invoke(const string& method, const CppVariant* arguments,
                        uint32_t argumentCount, CppVariant& result) const
{
    ASSERT(isObject());
    NPIdentifier methodName = WebBindings::getStringIdentifier(method.c_str());
    NPObject* npObject = value.objectValue;
    if (!WebBindings::hasMethod(0, npObject, methodName))
        return false;
    NPVariant r;
    bool status = WebBindings::invoke(0, npObject, methodName, arguments, argumentCount, &r);
    result.set(r);
    return status;
}
