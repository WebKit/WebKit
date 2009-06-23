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

#include "BooleanObject.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "LiteralParser.h"
#include "PropertyNameArray.h"
#include <wtf/MathExtras.h>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSONObject);

static JSValue JSC_HOST_CALL JSONProtoFuncParse(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL JSONProtoFuncStringify(ExecState*, JSObject*, JSValue, const ArgList&);

}

#include "JSONObject.lut.h"

namespace JSC {

// PropertyNameForFunctionCall objects must be on the stack, since the JSValue that they create is not marked.
class PropertyNameForFunctionCall {
public:
    PropertyNameForFunctionCall(const Identifier&);
    PropertyNameForFunctionCall(unsigned);

    JSValue value(ExecState*) const;

private:
    const Identifier* m_identifier;
    unsigned m_number;
    mutable JSValue m_value;
};

class Stringifier : Noncopyable {
public:
    Stringifier(ExecState*, JSValue replacer, JSValue space);
    ~Stringifier();
    JSValue stringify(JSValue);

    void mark();

private:
    typedef UString StringBuilder;

    class Holder {
    public:
        Holder(JSObject*);

        JSObject* object() const { return m_object; }

        bool appendNextProperty(Stringifier&, StringBuilder&);

    private:
        JSObject* const m_object;
        const bool m_isArray;
        bool m_isJSArray;
        unsigned m_index;
        unsigned m_size;
        RefPtr<PropertyNameArrayData> m_propertyNames;
    };

    friend class Holder;

    static void appendQuotedString(StringBuilder&, const UString&);

    JSValue toJSON(JSValue, const PropertyNameForFunctionCall&);

    enum StringifyResult { StringifyFailed, StringifySucceeded, StringifyFailedDueToUndefinedValue };
    StringifyResult appendStringifiedValue(StringBuilder&, JSValue, JSObject* holder, const PropertyNameForFunctionCall&);

    bool willIndent() const;
    void indent();
    void unindent();
    void startNewLine(StringBuilder&) const;

    Stringifier* const m_nextStringifierToMark;
    ExecState* const m_exec;
    const JSValue m_replacer;
    bool m_usingArrayReplacer;
    PropertyNameArray m_arrayReplacerPropertyNames;
    CallType m_replacerCallType;
    CallData m_replacerCallData;
    const UString m_gap;

    HashSet<JSObject*> m_holderCycleDetector;
    Vector<Holder, 16> m_holderStack;
    UString m_repeatedGap;
    UString m_indent;
};

// ------------------------------ helper functions --------------------------------

static inline JSValue unwrapBoxedPrimitive(JSValue value)
{
    if (!value.isObject())
        return value;
    if (!asObject(value)->inherits(&NumberObject::info) && !asObject(value)->inherits(&StringObject::info) && !asObject(value)->inherits(&BooleanObject::info))
        return value;
    return static_cast<JSWrapperObject*>(asObject(value))->internalValue();
}

static inline UString gap(JSValue space)
{
    space = unwrapBoxedPrimitive(space);

    // If the space value is a number, create a gap string with that number of spaces.
    double spaceCount;
    if (space.getNumber(spaceCount)) {
        const int maxSpaceCount = 100;
        int count;
        if (spaceCount > maxSpaceCount)
            count = maxSpaceCount;
        else if (!(spaceCount > 0))
            count = 0;
        else
            count = static_cast<int>(spaceCount);
        UChar spaces[maxSpaceCount];
        for (int i = 0; i < count; ++i)
            spaces[i] = ' ';
        return UString(spaces, count);
    }

    // If the space value is a string, use it as the gap string, otherwise use no gap string.
    return space.getString();
}

// ------------------------------ PropertyNameForFunctionCall --------------------------------

inline PropertyNameForFunctionCall::PropertyNameForFunctionCall(const Identifier& identifier)
    : m_identifier(&identifier)
{
}

inline PropertyNameForFunctionCall::PropertyNameForFunctionCall(unsigned number)
    : m_identifier(0)
    , m_number(number)
{
}

JSValue PropertyNameForFunctionCall::value(ExecState* exec) const
{
    if (!m_value) {
        if (m_identifier)
            m_value = jsString(exec, m_identifier->ustring());
        else
            m_value = jsNumber(exec, m_number);
    }
    return m_value;
}

// ------------------------------ Stringifier --------------------------------

Stringifier::Stringifier(ExecState* exec, JSValue replacer, JSValue space)
    : m_nextStringifierToMark(exec->globalData().firstStringifierToMark)
    , m_exec(exec)
    , m_replacer(replacer)
    , m_usingArrayReplacer(false)
    , m_arrayReplacerPropertyNames(exec)
    , m_replacerCallType(CallTypeNone)
    , m_gap(gap(space))
{
    exec->globalData().firstStringifierToMark = this;

    if (!m_replacer.isObject())
        return;

    if (asObject(m_replacer)->inherits(&JSArray::info)) {
        m_usingArrayReplacer = true;
        JSObject* array = asObject(m_replacer);
        unsigned length = array->get(exec, exec->globalData().propertyNames->length).toUInt32(exec);
        for (unsigned i = 0; i < length; ++i) {
            JSValue name = array->get(exec, i);
            if (exec->hadException())
                break;
            UString propertyName;
            if (!name.getString(propertyName))
                continue;
            if (exec->hadException())
                return;
            m_arrayReplacerPropertyNames.add(Identifier(exec, propertyName));
        }
        return;
    }

    m_replacerCallType = asObject(m_replacer)->getCallData(m_replacerCallData);
}

Stringifier::~Stringifier()
{
    ASSERT(m_exec->globalData().firstStringifierToMark == this);
    m_exec->globalData().firstStringifierToMark = m_nextStringifierToMark;
}

void Stringifier::mark()
{
    for (Stringifier* stringifier = this; stringifier; stringifier = stringifier->m_nextStringifierToMark) {
        size_t size = m_holderStack.size();
        for (size_t i = 0; i < size; ++i) {
            JSObject* object = m_holderStack[i].object();
            if (!object->marked())
                object->mark();
        }
    }
}

JSValue Stringifier::stringify(JSValue value)
{
    JSObject* object = constructEmptyObject(m_exec);
    if (m_exec->hadException())
        return jsNull();

    PropertyNameForFunctionCall emptyPropertyName(m_exec->globalData().propertyNames->emptyIdentifier);
    object->putDirect(m_exec->globalData().propertyNames->emptyIdentifier, value);

    StringBuilder result;
    if (appendStringifiedValue(result, value, object, emptyPropertyName) != StringifySucceeded)
        return jsUndefined();
    if (m_exec->hadException())
        return jsNull();

    return jsString(m_exec, result);
}

void Stringifier::appendQuotedString(StringBuilder& builder, const UString& value)
{
    int length = value.size();

    // String length plus 2 for quote marks plus 8 so we can accomodate a few escaped characters.
    builder.reserveCapacity(builder.size() + length + 2 + 8);

    builder.append('"');

    const UChar* data = value.data();
    for (int i = 0; i < length; ++i) {
        int start = i;
        while (i < length && (data[i] > 0x1F && data[i] != '"' && data[i] != '\\'))
            ++i;
        builder.append(data + start, i - start);
        if (i >= length)
            break;
        switch (data[i]) {
            case '\t':
                builder.append('\\');
                builder.append('t');
                break;
            case '\r':
                builder.append('\\');
                builder.append('r');
                break;
            case '\n':
                builder.append('\\');
                builder.append('n');
                break;
            case '\f':
                builder.append('\\');
                builder.append('f');
                break;
            case '\b':
                builder.append('\\');
                builder.append('b');
                break;
            case '"':
                builder.append('\\');
                builder.append('"');
                break;
            case '\\':
                builder.append('\\');
                builder.append('\\');
                break;
            default:
                static const char hexDigits[] = "0123456789abcdef";
                UChar ch = data[i];
                UChar hex[] = { '\\', 'u', hexDigits[(ch >> 12) & 0xF], hexDigits[(ch >> 8) & 0xF], hexDigits[(ch >> 4) & 0xF], hexDigits[ch & 0xF] };
                builder.append(hex, sizeof(hex) / sizeof(UChar));
                break;
        }
    }

    builder.append('"');
}

inline JSValue Stringifier::toJSON(JSValue value, const PropertyNameForFunctionCall& propertyName)
{
    ASSERT(!m_exec->hadException());
    if (!value.isObject() || !asObject(value)->hasProperty(m_exec, m_exec->globalData().propertyNames->toJSON))
        return value;

    JSValue toJSONFunction = asObject(value)->get(m_exec, m_exec->globalData().propertyNames->toJSON);
    if (m_exec->hadException())
        return jsNull();

    if (!toJSONFunction.isObject())
        return value;

    JSObject* object = asObject(toJSONFunction);
    CallData callData;
    CallType callType = object->getCallData(callData);
    if (callType == CallTypeNone)
        return value;

    JSValue list[] = { propertyName.value(m_exec) };
    ArgList args(list, sizeof(list) / sizeof(JSValue));
    return call(m_exec, object, callType, callData, value, args);
}

Stringifier::StringifyResult Stringifier::appendStringifiedValue(StringBuilder& builder, JSValue value, JSObject* holder, const PropertyNameForFunctionCall& propertyName)
{
    // Call the toJSON function.
    value = toJSON(value, propertyName);
    if (m_exec->hadException())
        return StringifyFailed;
    if (value.isUndefined() && !holder->inherits(&JSArray::info))
        return StringifyFailedDueToUndefinedValue;

    // Call the replacer function.
    if (m_replacerCallType != CallTypeNone) {
        JSValue list[] = { propertyName.value(m_exec), value };
        ArgList args(list, sizeof(list) / sizeof(JSValue));
        value = call(m_exec, m_replacer, m_replacerCallType, m_replacerCallData, holder, args);
        if (m_exec->hadException())
            return StringifyFailed;
    }

    if (value.isNull()) {
        builder.append("null");
        return StringifySucceeded;
    }

    value = unwrapBoxedPrimitive(value);

    if (value.isBoolean()) {
        builder.append(value.getBoolean() ? "true" : "false");
        return StringifySucceeded;
    }

    UString stringValue;
    if (value.getString(stringValue)) {
        appendQuotedString(builder, stringValue);
        return StringifySucceeded;
    }

    double numericValue;
    if (value.getNumber(numericValue)) {
        if (!isfinite(numericValue))
            builder.append("null");
        else
            builder.append(UString::from(numericValue));
        return StringifySucceeded;
    }

    if (!value.isObject())
        return StringifyFailed;

    JSObject* object = asObject(value);

    // Handle cycle detection, and put the holder on the stack.
    if (!m_holderCycleDetector.add(object).second) {
        throwError(m_exec, TypeError);
        return StringifyFailed;
    }
    bool holderStackWasEmpty = m_holderStack.isEmpty();
    m_holderStack.append(object);
    if (!holderStackWasEmpty)
        return StringifySucceeded;

    // If this is the outermost call, then loop to handle everything on the holder stack.
    TimeoutChecker localTimeoutChecker(m_exec->globalData().timeoutChecker);
    localTimeoutChecker.reset();
    unsigned tickCount = localTimeoutChecker.ticksUntilNextCheck();
    do {
        while (m_holderStack.last().appendNextProperty(*this, builder)) {
            if (m_exec->hadException())
                return StringifyFailed;
            if (!--tickCount) {
                if (localTimeoutChecker.didTimeOut(m_exec)) {
                    m_exec->setException(createInterruptedExecutionException(&m_exec->globalData()));
                    return StringifyFailed;
                }
                tickCount = localTimeoutChecker.ticksUntilNextCheck();
            }
        }
        m_holderCycleDetector.remove(m_holderStack.last().object());
        m_holderStack.removeLast();
    } while (!m_holderStack.isEmpty());
    return StringifySucceeded;
}

inline bool Stringifier::willIndent() const
{
    return !m_gap.isEmpty();
}

inline void Stringifier::indent()
{
    // Use a single shared string, m_repeatedGap, so we don't keep allocating new ones as we indent and unindent.
    int newSize = m_indent.size() + m_gap.size();
    if (newSize > m_repeatedGap.size())
        m_repeatedGap.append(m_gap);
    ASSERT(newSize <= m_repeatedGap.size());
    m_indent = m_repeatedGap.substr(0, newSize);
}

inline void Stringifier::unindent()
{
    ASSERT(m_indent.size() >= m_gap.size());
    m_indent = m_repeatedGap.substr(0, m_indent.size() - m_gap.size());
}

inline void Stringifier::startNewLine(StringBuilder& builder) const
{
    if (m_gap.isEmpty())
        return;
    builder.append('\n');
    builder.append(m_indent);
}

inline Stringifier::Holder::Holder(JSObject* object)
    : m_object(object)
    , m_isArray(object->inherits(&JSArray::info))
    , m_index(0)
{
}

bool Stringifier::Holder::appendNextProperty(Stringifier& stringifier, StringBuilder& builder)
{
    ASSERT(m_index <= m_size);

    ExecState* exec = stringifier.m_exec;

    // First time through, initialize.
    if (!m_index) {
        if (m_isArray) {
            m_isJSArray = isJSArray(&exec->globalData(), m_object);
            m_size = m_object->get(exec, exec->globalData().propertyNames->length).toUInt32(exec);
            builder.append('[');
        } else {
            if (stringifier.m_usingArrayReplacer)
                m_propertyNames = stringifier.m_arrayReplacerPropertyNames.data();
            else {
                PropertyNameArray objectPropertyNames(exec);
                m_object->getPropertyNames(exec, objectPropertyNames);
                m_propertyNames = objectPropertyNames.releaseData();
            }
            m_size = m_propertyNames->propertyNameVector().size();
            builder.append('{');
        }
        stringifier.indent();
    }

    // Last time through, finish up and return false.
    if (m_index == m_size) {
        stringifier.unindent();
        if (m_size && builder[builder.size() - 1] != '{')
            stringifier.startNewLine(builder);
        builder.append(m_isArray ? ']' : '}');
        return false;
    }

    // Handle a single element of the array or object.
    unsigned index = m_index++;
    unsigned rollBackPoint = 0;
    StringifyResult stringifyResult;
    if (m_isArray) {
        // Get the value.
        JSValue value;
        if (m_isJSArray && asArray(m_object)->canGetIndex(index))
            value = asArray(m_object)->getIndex(index);
        else {
            PropertySlot slot(m_object);
            if (!m_object->getOwnPropertySlot(exec, index, slot))
                slot.setUndefined();
            if (exec->hadException())
                return false;
            value = slot.getValue(exec, index);
        }

        // Append the separator string.
        if (index)
            builder.append(',');
        stringifier.startNewLine(builder);

        // Append the stringified value.
        stringifyResult = stringifier.appendStringifiedValue(builder, value, m_object, index);
    } else {
        // Get the value.
        PropertySlot slot(m_object);
        Identifier& propertyName = m_propertyNames->propertyNameVector()[index];
        if (!m_object->getOwnPropertySlot(exec, propertyName, slot))
            return true;
        JSValue value = slot.getValue(exec, propertyName);
        if (exec->hadException())
            return false;

        rollBackPoint = builder.size();

        // Append the separator string.
        if (builder[rollBackPoint - 1] != '{')
            builder.append(',');
        stringifier.startNewLine(builder);

        // Append the property name.
        appendQuotedString(builder, propertyName.ustring());
        builder.append(':');
        if (stringifier.willIndent())
            builder.append(' ');

        // Append the stringified value.
        stringifyResult = stringifier.appendStringifiedValue(builder, value, m_object, propertyName);
    }

    // From this point on, no access to the this pointer or to any members, because the
    // Holder object may have moved if the call to stringify pushed a new Holder onto
    // m_holderStack.

    switch (stringifyResult) {
        case StringifyFailed:
            builder.append("null");
            break;
        case StringifySucceeded:
            break;
        case StringifyFailedDueToUndefinedValue:
            // This only occurs when get an undefined value for an object property.
            // In this case we don't want the separator and property name that we
            // already appended, so roll back.
            builder = builder.substr(0, rollBackPoint);
            break;
    }

    return true;
}

// ------------------------------ JSONObject --------------------------------

const ClassInfo JSONObject::info = { "JSON", 0, 0, ExecState::jsonTable };

/* Source for JSONObject.lut.h
@begin jsonTable
  parse         JSONProtoFuncParse             DontEnum|Function 1
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

void JSONObject::markStringifiers(Stringifier* stringifier)
{
    stringifier->mark();
}

class Walker {
public:
    Walker(ExecState* exec, JSObject* function, CallType callType, CallData callData)
        : m_exec(exec)
        , m_function(function)
        , m_callType(callType)
        , m_callData(callData)
    {
    }
    JSValue walk(JSValue unfiltered);
private:
    JSValue callReviver(JSValue property, JSValue unfiltered)
    {
        JSValue args[] = { property, unfiltered };
        ArgList argList(args, 2);
        return call(m_exec, m_function, m_callType, m_callData, jsNull(), argList);
    }

    friend class Holder;

    ExecState* m_exec;
    JSObject* m_function;
    CallType m_callType;
    CallData m_callData;
};
    
enum WalkerState { StateUnknown, ArrayStartState, ArrayStartVisitMember, ArrayEndVisitMember, 
                                 ObjectStartState, ObjectStartVisitMember, ObjectEndVisitMember };
NEVER_INLINE JSValue Walker::walk(JSValue unfiltered)
{
    Vector<PropertyNameArray, 16> propertyStack;
    Vector<uint32_t, 16> indexStack;
    Vector<JSObject*, 16> objectStack;
    Vector<JSArray*, 16> arrayStack;
    
    Vector<WalkerState, 16> stateStack;
    WalkerState state = StateUnknown;
    JSValue inValue = unfiltered;
    JSValue outValue = jsNull();
    while (1) {
        switch (state) {
            arrayStartState:
            case ArrayStartState: {
                ASSERT(inValue.isObject());
                ASSERT(isJSArray(&m_exec->globalData(), asObject(inValue)));
                JSArray* array = asArray(inValue);
                arrayStack.append(array);
                indexStack.append(0);
                // fallthrough
            }
            arrayStartVisitMember:
            case ArrayStartVisitMember: {
                JSArray* array = arrayStack.last();
                uint32_t index = indexStack.last();
                if (index == array->length()) {
                    outValue = array;
                    arrayStack.removeLast();
                    indexStack.removeLast();
                    break;
                }
                inValue = array->getIndex(index);
                if (inValue.isObject()) {
                    stateStack.append(ArrayEndVisitMember);
                    goto stateUnknown;
                } else
                    outValue = inValue;
                // fallthrough
            }
            case ArrayEndVisitMember: {
                JSArray* array = arrayStack.last();
                array->setIndex(indexStack.last(), callReviver(jsString(m_exec, UString::from(indexStack.last())), outValue));
                if (m_exec->hadException())
                    return jsNull();
                indexStack.last()++;
                goto arrayStartVisitMember;
            }
            objectStartState:
            case ObjectStartState: {
                ASSERT(inValue.isObject());
                ASSERT(!isJSArray(&m_exec->globalData(), asObject(inValue)));
                JSObject* object = asObject(inValue);
                objectStack.append(object);
                indexStack.append(0);
                propertyStack.append(PropertyNameArray(m_exec));
                object->getPropertyNames(m_exec, propertyStack.last());
                // fallthrough
            }
            objectStartVisitMember:
            case ObjectStartVisitMember: {
                JSObject* object = objectStack.last();
                uint32_t index = indexStack.last();
                PropertyNameArray& properties = propertyStack.last();
                if (index == properties.size()) {
                    outValue = object;
                    objectStack.removeLast();
                    indexStack.removeLast();
                    propertyStack.removeLast();
                    break;
                }
                PropertySlot slot;
                object->getOwnPropertySlot(m_exec, properties[index], slot);
                inValue = slot.getValue(m_exec, properties[index]);
                ASSERT(!m_exec->hadException());
                if (inValue.isObject()) {
                    stateStack.append(ObjectEndVisitMember);
                    goto stateUnknown;
                } else
                    outValue = inValue;
                // fallthrough
            }
            case ObjectEndVisitMember: {
                JSObject* object = objectStack.last();
                Identifier prop = propertyStack.last()[indexStack.last()];
                PutPropertySlot slot;
                object->put(m_exec, prop, callReviver(jsString(m_exec, prop.ustring()), outValue), slot);
                if (m_exec->hadException())
                    return jsNull();
                indexStack.last()++;
                goto objectStartVisitMember;
            }
            stateUnknown:
            case StateUnknown:
                if (!inValue.isObject()) {
                    outValue = inValue;
                    break;
                }
                if (isJSArray(&m_exec->globalData(), asObject(inValue)))
                    goto arrayStartState;
                goto objectStartState;
        }
        if (stateStack.isEmpty())
            break;
        state = stateStack.last();
        stateStack.removeLast();
    }
    return callReviver(jsEmptyString(m_exec), outValue);
}

// ECMA-262 v5 15.12.2
JSValue JSC_HOST_CALL JSONProtoFuncParse(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (args.isEmpty())
        return throwError(exec, GeneralError, "JSON.parse requires at least one parameter");
    JSValue value = args.at(0);
    UString source = value.toString(exec);
    if (exec->hadException())
        return jsNull();
    
    LiteralParser jsonParser(exec, source, LiteralParser::StrictJSON);
    JSValue unfiltered = jsonParser.tryLiteralParse();
    if (!unfiltered)
        return throwError(exec, SyntaxError, "Unable to parse JSON string");
    
    if (args.size() < 2)
        return unfiltered;
    
    JSValue function = args.at(1);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return unfiltered;
    return Walker(exec, asObject(function), callType, callData).walk(unfiltered);
}

// ECMA-262 v5 15.12.3
JSValue JSC_HOST_CALL JSONProtoFuncStringify(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (args.isEmpty())
        return throwError(exec, GeneralError, "No input to stringify");
    JSValue value = args.at(0);
    JSValue replacer = args.at(1);
    JSValue space = args.at(2);
    return Stringifier(exec, replacer, space).stringify(value);
}

} // namespace JSC
