/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "JSONObject.h"

#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "LiteralParser.h"
#include "Nodes.h"
#include "PropertyNameArray.h"

#include "ObjectPrototype.h"
#include "Operations.h"
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>


namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSONObject);

static JSValue JSC_HOST_CALL JSONProtoFuncStringify(ExecState*, JSObject*, JSValue, const ArgList&);

}

#include "JSONObject.lut.h"

namespace JSC {

// ------------------------------ JSONObject --------------------------------

const ClassInfo JSONObject::info = { "JSON", 0, 0, ExecState::jsonTable };

/* Source for JSONObject.lut.h
@begin jsonTable
  stringify     JSONProtoFuncStringify         DontEnum|Function 1
@end
*/

// ECMA 15.8

bool JSONObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    const HashEntry* entry = ExecState::jsonTable(exec)->entry(exec, propertyName);

    if (!entry)
        return JSObject::getOwnPropertySlot(exec, propertyName, slot);

    ASSERT(entry->attributes() & Function);
    setUpStaticFunctionSlot(exec, entry, this, propertyName, slot);
    return true;
}

class Stringifier {
    typedef UString StringBuilder;
    // <https://bugs.webkit.org/show_bug.cgi?id=26276> arbitrary limits for recursion
    // are bad
    enum { MaxObjectDepth = 1600 };
public:
    Stringifier(ExecState* exec, JSValue replacer, const UString& gap)
        : m_exec(exec)
        , m_replacer(replacer)
        , m_gap(gap)
        , m_arrayReplacerPropertyNames(exec)
        , m_usingArrayReplacer(false)
        , m_replacerCallType(CallTypeNone)
    {
        if (m_replacer.isObject()) {
            if (asObject(m_replacer)->inherits(&JSArray::info)) {
                JSObject* array = asObject(m_replacer);
                bool isRealJSArray = isJSArray(&m_exec->globalData(), m_replacer);
                unsigned length = array->get(m_exec, m_exec->globalData().propertyNames->length).toUInt32(m_exec);
                for (unsigned i = 0; i < length; i++) {
                    JSValue name;
                    if (isRealJSArray && asArray(array)->canGetIndex(i))
                        name = asArray(array)->getIndex(i);
                    else {
                        name = array->get(m_exec, i);
                        if (exec->hadException())
                            break;
                    }
                    if (!name.isString())
                        continue;

                    UString propertyName = name.toString(m_exec);
                    if (exec->hadException())
                        return;
                    m_arrayReplacerPropertyNames.add(Identifier(m_exec, propertyName));
                }
                m_usingArrayReplacer = true;
            } else
                m_replacerCallType = asObject(m_replacer)->getCallData(m_replacerCallData);
        }
    }

    JSValue stringify(JSValue value)
    {
        JSObject* obj = constructEmptyObject(m_exec);
        if (m_exec->hadException())
            return jsNull();

        Identifier emptyIdent(m_exec, "");
        obj->putDirect(emptyIdent, value);
        StringKeyGenerator key(emptyIdent);
        value = toJSONValue(key, value);
        if (m_exec->hadException())
            return jsNull();
        
        StringBuilder builder;
        if (!stringify(builder, obj, key, value))
            return jsUndefined();

        return (m_exec->hadException())? m_exec->exception() : jsString(m_exec, builder);
    }

private:
    void appendString(StringBuilder& builder, const UString& value)
    {
        int length = value.size();
        const UChar* data = value.data();
        builder.reserveCapacity(builder.size() + length + 2 + 8); // +2 for "'s +8 for a buffer
        builder.append('\"');
        int index = 0;
        while (index < length) {
            int start = index;
            while (index < length && (data[index] > 0x1f && data[index] != '"' && data[index] != '\\'))
                index++;
            builder.append(data + start, index - start);
            if (index < length) {
                switch(data[index]){
                    case '\t': {
                        UChar tab[] = {'\\','t'};
                        builder.append(tab, 2);
                        break;
                    }
                    case '\r': {
                        UChar cr[] = {'\\','r'};
                        builder.append(cr, 2);
                        break;
                    }
                    case '\n': {
                        UChar nl[] = {'\\','n'};
                        builder.append(nl, 2);
                        break;
                    }
                    case '\f': {
                        UChar tab[] = {'\\','f'}; 
                        builder.append(tab, 2);
                        break;
                    }
                    case '\b': {
                        UChar bs[] = {'\\','b'};
                        builder.append(bs, 2);
                        break;
                    }
                    case '\"': {
                        UChar quote[] = {'\\','"'};
                        builder.append(quote, 2);
                        break;
                    }
                    case '\\': {
                        UChar slash[] = {'\\', '\\'};
                        builder.append(slash, 2);
                        break;
                    }
                    default:
                        int ch = (int)data[index];
                        static const char* hexStr = "0123456789abcdef";
                        UChar oct[] = {'\\', 'u', hexStr[(ch >> 12) & 15], hexStr[(ch >> 8) & 15], hexStr[(ch >> 4) & 15], hexStr[(ch >> 0) & 15]};
                        builder.append(oct, sizeof(oct) / sizeof(UChar));
                        break;
                }
                index++;
            }
        }
        builder.append('\"');
    }

    class StringKeyGenerator {
    public:
        StringKeyGenerator(const Identifier& property)
            : m_property(property)
        {
        }
        JSValue getKey(ExecState* exec) { if (!m_key) m_key = jsString(exec, m_property.ustring()); return m_key;  }

    private:
        Identifier m_property;
        JSValue m_key;
    };

    class IntKeyGenerator {
    public:
        IntKeyGenerator(uint32_t property)
            : m_property(property)
        {
        }
        JSValue getKey(ExecState* exec) { if (!m_key) m_key = jsNumber(exec, m_property); return m_key;  }

    private:
        uint32_t m_property;
        JSValue m_key;
    };

    template <typename KeyGenerator> JSValue toJSONValue(KeyGenerator& jsKey, JSValue jsValue)
    {
        ASSERT(!m_exec->hadException());
        if (!jsValue.isObject() || !asObject(jsValue)->hasProperty(m_exec, m_exec->globalData().propertyNames->toJSON))
            return jsValue;

        JSValue jsToJSON = asObject(jsValue)->get(m_exec, m_exec->globalData().propertyNames->toJSON);

        if (!jsToJSON.isObject())
            return jsValue;

        if (m_exec->hadException())
            return jsNull();

        JSObject* object = asObject(jsToJSON);
        CallData callData;
        CallType callType = object->getCallData(callData);

        if (callType == CallTypeNone)
            return jsValue;

        JSValue list[] = { jsKey.getKey(m_exec) };        
        ArgList args(list, sizeof(list) / sizeof(JSValue));
        return call(m_exec, object, callType, callData, jsValue, args);
    }

    bool stringifyArray(StringBuilder& builder, JSArray* array)
    {
        if (m_objectStack.contains(array)) {
            throwError(m_exec, TypeError);
            return false;
        }

        if (m_objectStack.size() > MaxObjectDepth) {
            m_exec->setException(createStackOverflowError(m_exec));
            return false;
        }

        m_objectStack.add(array);
        UString stepBack = m_indent;
        m_indent.append(m_gap);
        Vector<StringBuilder, 16> partial;
        unsigned len = array->get(m_exec, m_exec->globalData().propertyNames->length).toUInt32(m_exec);
        bool isRealJSArray = isJSArray(&m_exec->globalData(), array);
        for (unsigned i = 0; i < len; i++) {
            JSValue value;
            if (isRealJSArray && array->canGetIndex(i))
                value = array->getIndex(i);
            else {
                value = array->get(m_exec, i);
                if (m_exec->hadException())
                    return false;
            }
            StringBuilder result;
            IntKeyGenerator key(i);
            value = toJSONValue(key, value);
            if (partial.size() && m_gap.isEmpty())
                partial.last().append(',');
            if (!stringify(result, array, key, value) && !m_exec->hadException())
                result.append("null");
            else if (m_exec->hadException())
                return false;
            partial.append(result);
        }
        
        if (partial.isEmpty())
            builder.append("[]");
        else {
            if (m_gap.isEmpty()) {
                builder.append('[');
                for (unsigned i = 0; i < partial.size(); i++)
                    builder.append(partial[i]);
                builder.append(']');
            } else {
                UString separator(",\n");
                separator.append(m_indent);
                builder.append("[\n");
                builder.append(m_indent);
                for (unsigned i = 0; i < partial.size(); i++) {
                    builder.append(partial[i]);
                    if (i < partial.size() - 1)
                        builder.append(separator);
                }
                builder.append('\n');
                builder.append(stepBack);
                builder.append(']');
            }
        }

        m_indent = stepBack;
        m_objectStack.remove(array);
        return true;
    }

    bool stringifyObject(StringBuilder& builder, JSObject* object)
    {
        if (m_objectStack.contains(object)) {
            throwError(m_exec, TypeError);
            return false;
        }

        if (m_objectStack.size() > MaxObjectDepth) {
            m_exec->setException(createStackOverflowError(m_exec));
            return false;
        }

        m_objectStack.add(object);
        UString stepBack = m_indent;
        m_indent.append(m_gap);
        Vector<StringBuilder, 16> partial;

        PropertyNameArray objectPropertyNames(m_exec);
        PropertyNameArray& sourcePropertyNames(m_arrayReplacerPropertyNames);
        if (!m_usingArrayReplacer) {
            object->getPropertyNames(m_exec, objectPropertyNames);
            sourcePropertyNames = objectPropertyNames;
        }

        PropertyNameArray::const_iterator propEnd = sourcePropertyNames.end();
        for (PropertyNameArray::const_iterator propIter = sourcePropertyNames.begin(); propIter != propEnd; propIter++) {
            if (!object->hasOwnProperty(m_exec, *propIter))
                continue;

            JSValue value = object->get(m_exec, *propIter);
            if (m_exec->hadException())
                return false;

            if (value.isUndefined())
                continue;

            StringBuilder result;
            appendString(result,propIter->ustring());
            result.append(':');
            if (!m_gap.isEmpty())
                result.append(' ');
            StringKeyGenerator key(*propIter);
            value = toJSONValue(key, value);
            if (m_exec->hadException())
                return false;

            if (value.isUndefined())
                continue;

            stringify(result, object, key, value);
            partial.append(result);
        }

        if (partial.isEmpty())
            builder.append("{}");
        else {
            if (m_gap.isEmpty()) {
                builder.append('{');
                for (unsigned i = 0; i < partial.size(); i++) {
                    if (i > 0)
                        builder.append(',');
                    builder.append(partial[i]);
                }
                builder.append('}');
            } else {
                UString separator(",\n");
                separator.append(m_indent);
                builder.append("{\n");
                builder.append(m_indent);
                for (unsigned i = 0; i < partial.size(); i++) {
                    builder.append(partial[i]);
                    if (i < partial.size() - 1)
                        builder.append(separator);
                }
                builder.append('\n');
                builder.append(stepBack);
                builder.append('}');
            }

        }

        m_indent = stepBack;
        m_objectStack.remove(object);
        return true;
    }
    
    template <typename KeyGenerator> bool stringify(StringBuilder& builder, JSValue holder, KeyGenerator key, JSValue value)
    {
        if (m_replacerCallType != CallTypeNone) {
            JSValue list[] = {key.getKey(m_exec), value};
            ArgList args(list, sizeof(list) / sizeof(JSValue));
            value = call(m_exec, m_replacer, m_replacerCallType, m_replacerCallData, holder, args);
            if (m_exec->hadException())
                return false;
        }

        if (value.isNull()) {
            builder.append("null");
            return true;
        }

        if (value.isBoolean()) {
            builder.append(value.getBoolean() ? "true" : "false");
            return true;
        }

        if (value.isObject()) {
            if (asObject(value)->inherits(&NumberObject::info) || asObject(value)->inherits(&StringObject::info))
                value = static_cast<JSWrapperObject*>(asObject(value))->internalValue();
        }

        if (value.isString()) {
            appendString(builder, value.toString(m_exec));
            return true;
        }

        if (value.isNumber()) {
            double v = value.toNumber(m_exec);
            if (!isfinite(v))
                builder.append("null");
            else
                builder.append(UString::from(v));
            return true;
        }

        if (value.isObject()) {
            if (asObject(value)->inherits(&JSArray::info))
                return stringifyArray(builder, asArray(value));
            return stringifyObject(builder, asObject(value));
        }

        return false;
    }
    ExecState* m_exec;
    JSValue m_replacer;
    UString m_gap;
    UString m_indent;
    HashSet<JSObject*> m_objectStack;
    PropertyNameArray m_arrayReplacerPropertyNames;
    bool m_usingArrayReplacer;
    CallType m_replacerCallType;
    CallData m_replacerCallData;
};

// ECMA-262 v5 15.12.3
JSValue JSC_HOST_CALL JSONProtoFuncStringify(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (args.isEmpty())
        return throwError(exec, GeneralError, "No input to stringify");

    JSValue value = args.at(0);
    JSValue replacer = args.at(1);
    JSValue space = args.at(2);

    UString gap;
    if (space.isObject()) {
        if (asObject(space)->inherits(&NumberObject::info) || asObject(space)->inherits(&StringObject::info))
            space = static_cast<JSWrapperObject*>(asObject(space))->internalValue();
    }
    if (space.isNumber()) {
        double v = space.toNumber(exec);
        if (v > 100)
            v = 100;
        if (!(v > 0))
            v = 0;
        for (int i = 0; i < v; i++)
            gap.append(' ');
    } else if (space.isString())
        gap = space.toString(exec);

    Stringifier stringifier(exec, args.at(1), gap);
    return stringifier.stringify(value);
}

} // namespace JSC
