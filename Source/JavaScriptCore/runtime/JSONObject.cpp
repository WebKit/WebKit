/*
 * Copyright (C) 2009-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Alexey Shvayka <shvaikalesh@gmail.com>.
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

#include "ArrayConstructor.h"
#include "BigIntObject.h"
#include "BooleanObject.h"
#include "JSArrayInlines.h"
#include "JSCInlines.h"
#include "LiteralParser.h"
#include "ObjectConstructorInlines.h"
#include "PropertyNameArray.h"
#include "VMInlines.h"
#include <wtf/text/EscapedFormsForJSON.h>
#include <wtf/text/StringBuilder.h>

// Turn this on to log information about fastStringify usage, with a focus on why it failed.
#define FAST_STRINGIFY_LOG_USAGE 0

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSONObject);

static JSC_DECLARE_HOST_FUNCTION(jsonProtoFuncParse);
static JSC_DECLARE_HOST_FUNCTION(jsonProtoFuncStringify);

}

#include "JSONObject.lut.h"

namespace JSC {

JSONObject::JSONObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void JSONObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// PropertyNameForFunctionCall objects must be on the stack, since the JSValue that they create is not marked.
class PropertyNameForFunctionCall {
public:
    PropertyNameForFunctionCall(PropertyName);
    PropertyNameForFunctionCall(unsigned);

    JSValue value(JSGlobalObject*) const;

private:
    PropertyName m_propertyName;
    unsigned m_number;
    mutable JSValue m_value;
};

class Stringifier {
    WTF_MAKE_NONCOPYABLE(Stringifier);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    static String stringify(JSGlobalObject&, JSValue, JSValue replacer, JSValue space);

private:
    class Holder {
    public:
        enum RootHolderTag { RootHolder };
        Holder(JSGlobalObject*, JSObject*, Structure*);
        Holder(RootHolderTag, JSObject*);

        JSObject* object() const { return m_object; }
        bool isArray() const { return m_isArray; }
        bool hasFastObjectProperties() const { return m_hasFastObjectProperties; }

        bool appendNextProperty(Stringifier&, StringBuilder&);

    private:
        JSObject* m_object { nullptr };
        Structure* m_structure { nullptr };
        const bool m_isJSArray { false };
        const bool m_isArray { false };
        bool m_hasFastObjectProperties { false };
        unsigned m_index { 0 };
        unsigned m_size { 0 };
        RefPtr<PropertyNameArrayData> m_propertyNames;
        Vector<std::tuple<PropertyName, unsigned>, 8> m_propertiesAndOffsets;
    };

    friend class Holder;

    Stringifier(JSGlobalObject*, JSValue replacer, JSValue space);

    JSValue toJSON(JSValue, const PropertyNameForFunctionCall&);

    enum StringifyResult { StringifyFailed, StringifySucceeded, StringifyFailedDueToUndefinedOrSymbolValue };
    StringifyResult appendStringifiedValue(StringBuilder&, JSValue, const Holder&, const PropertyNameForFunctionCall&);

    bool willIndent() const;
    void indent();
    void unindent();
    void startNewLine(StringBuilder&) const;
    bool isCallableReplacer() const { return m_replacerCallData.type != CallData::Type::None; }

    JSGlobalObject* const m_globalObject;
    JSValue m_replacer;
    bool m_usingArrayReplacer { false };
    PropertyNameArray m_arrayReplacerPropertyNames;
    CallData m_replacerCallData;
    String m_gap;

    MarkedArgumentBufferWithSize<16> m_objectStack;
    Vector<Holder, 16, UnsafeVectorOverflow> m_holderStack;
    String m_repeatedGap;
    StringView m_indent;
};

// ------------------------------ helper functions --------------------------------

static inline JSValue unwrapBoxedPrimitive(JSGlobalObject* globalObject, JSObject* object)
{
    if (object->inherits<NumberObject>())
        return jsNumber(object->toNumber(globalObject));
    if (object->inherits<StringObject>())
        return object->toString(globalObject);
    if (object->inherits<BooleanObject>() || object->inherits<BigIntObject>())
        return jsCast<JSWrapperObject*>(object)->internalValue();

    // Do not unwrap SymbolObject to Symbol. It is not performed in the spec.
    // http://www.ecma-international.org/ecma-262/6.0/#sec-serializejsonproperty

    return object;
}

static inline JSValue unwrapBoxedPrimitive(JSGlobalObject* globalObject, JSValue value)
{
    return value.isObject() ? unwrapBoxedPrimitive(globalObject, asObject(value)) : value;
}

static inline String gap(JSGlobalObject* globalObject, JSValue space)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const unsigned maxGapLength = 10;
    space = unwrapBoxedPrimitive(globalObject, space);
    RETURN_IF_EXCEPTION(scope, { });

    // If the space value is a number, create a gap string with that number of spaces.
    if (space.isNumber()) {
        double spaceCount = space.asNumber();
        int count;
        if (spaceCount > maxGapLength)
            count = maxGapLength;
        else if (!(spaceCount > 0))
            count = 0;
        else
            count = static_cast<int>(spaceCount);
        char spaces[maxGapLength];
        for (int i = 0; i < count; ++i)
            spaces[i] = ' ';
        return String(spaces, count);
    }

    // If the space value is a string, use it as the gap string, otherwise use no gap string.
    String spaces = space.getString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (spaces.length() <= maxGapLength)
        return spaces;
    return spaces.substringSharingImpl(0, maxGapLength);
}

// ------------------------------ PropertyNameForFunctionCall --------------------------------

inline PropertyNameForFunctionCall::PropertyNameForFunctionCall(PropertyName propertyName)
    : m_propertyName(propertyName)
{
}

inline PropertyNameForFunctionCall::PropertyNameForFunctionCall(unsigned number)
    : m_propertyName(nullptr)
    , m_number(number)
{
}

JSValue PropertyNameForFunctionCall::value(JSGlobalObject* globalObject) const
{
    if (!m_value) {
        VM& vm = globalObject->vm();
        if (!m_propertyName.isNull())
            m_value = jsString(vm, String { m_propertyName.uid() });
        else {
            if (m_number <= 9)
                return vm.smallStrings.singleCharacterString(m_number + '0');
            m_value = jsNontrivialString(vm, vm.numericStrings.add(m_number));
        }
    }
    return m_value;
}

// ------------------------------ Stringifier --------------------------------

Stringifier::Stringifier(JSGlobalObject* globalObject, JSValue replacer, JSValue space)
    : m_globalObject(globalObject)
    , m_replacer(replacer)
    , m_arrayReplacerPropertyNames(globalObject->vm(), PropertyNameMode::Strings, PrivateSymbolMode::Exclude)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_replacer.isObject()) {
        JSObject* replacerObject = asObject(m_replacer);

        m_replacerCallData = JSC::getCallData(replacerObject);
        if (m_replacerCallData.type == CallData::Type::None) {
            bool isArrayReplacer = JSC::isArray(globalObject, replacerObject);
            RETURN_IF_EXCEPTION(scope, );
            if (isArrayReplacer) {
                m_usingArrayReplacer = true;
                forEachInArrayLike(globalObject, replacerObject, [&] (JSValue name) -> bool {
                    if (name.isObject()) {
                        auto* nameObject = jsCast<JSObject*>(name);
                        if (!nameObject->inherits<NumberObject>() && !nameObject->inherits<StringObject>())
                            return true;
                    } else if (!name.isNumber() && !name.isString())
                        return true;

                    JSString* propertyNameString = name.toString(globalObject);
                    RETURN_IF_EXCEPTION(scope, false);
                    auto propertyName = propertyNameString->toIdentifier(globalObject);
                    RETURN_IF_EXCEPTION(scope, false);
                    m_arrayReplacerPropertyNames.add(WTFMove(propertyName));
                    return true;
                });
                RETURN_IF_EXCEPTION(scope, );
            }
        }
    }

    scope.release();
    m_gap = gap(globalObject, space);
}

String Stringifier::stringify(JSGlobalObject& globalObject, JSValue value, JSValue replacer, JSValue space)
{
    VM& vm = globalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Stringifier stringifier(&globalObject, replacer, space);
    RETURN_IF_EXCEPTION(scope, { });

    PropertyNameForFunctionCall emptyPropertyName(vm.propertyNames->emptyIdentifier.impl());

    // If the replacer is not callable, root object wrapper is non-user-observable.
    // We can skip creating this wrapper object.
    JSObject* object = nullptr;
    if (stringifier.isCallableReplacer()) {
        object = constructEmptyObject(&globalObject);
        object->putDirect(vm, vm.propertyNames->emptyIdentifier, value);
    }

    StringBuilder result(StringBuilder::OverflowHandler::RecordOverflow);
    Holder root(Holder::RootHolder, object);
    auto stringifyResult = stringifier.appendStringifiedValue(result, value, root, emptyPropertyName);
    RETURN_IF_EXCEPTION(scope, { });
    if (UNLIKELY(result.hasOverflowed())) {
        throwOutOfMemoryError(&globalObject, scope);
        return { };
    }
    if (UNLIKELY(stringifyResult != StringifySucceeded))
        RELEASE_AND_RETURN(scope, { });
    RELEASE_AND_RETURN(scope, result.toString());
}

ALWAYS_INLINE JSValue Stringifier::toJSON(JSValue baseValue, const PropertyNameForFunctionCall& propertyName)
{
    VM& vm = m_globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    scope.assertNoException();

    JSValue toJSONFunction;
    if (baseValue.isObject())
        toJSONFunction = asObject(baseValue)->structure()->cachedSpecialProperty(CachedSpecialPropertyKey::ToJSON);

    if (!toJSONFunction) {
        PropertySlot slot(baseValue, PropertySlot::InternalMethodType::Get);
        bool hasProperty = baseValue.getPropertySlot(m_globalObject, vm.propertyNames->toJSON, slot);
        RETURN_IF_EXCEPTION(scope, { });
        toJSONFunction = hasProperty ? slot.getValue(m_globalObject, vm.propertyNames->toJSON) : jsUndefined();
        RETURN_IF_EXCEPTION(scope, { });

        if (baseValue.isObject())
            asObject(baseValue)->structure()->cacheSpecialProperty(m_globalObject, vm, toJSONFunction, CachedSpecialPropertyKey::ToJSON, slot);
    }

    auto callData = JSC::getCallData(toJSONFunction);
    if (callData.type == CallData::Type::None)
        return baseValue;

    MarkedArgumentBuffer args;
    args.append(propertyName.value(m_globalObject));
    ASSERT(!args.hasOverflowed());
    RELEASE_AND_RETURN(scope, call(m_globalObject, asObject(toJSONFunction), callData, baseValue, args));
}

// We clamp recursion well beyond anything reasonable.
constexpr unsigned maximumSideStackRecursion = 40000;
Stringifier::StringifyResult Stringifier::appendStringifiedValue(StringBuilder& builder, JSValue value, const Holder& holder, const PropertyNameForFunctionCall& propertyName)
{
    VM& vm = m_globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Recursion is avoided by !holderStackWasEmpty check and do/while loop at the end of this method.
    // We're having this recursion check here as a fail safe in case the code
    // below get modified such that recursion is no longer avoided.
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(m_globalObject, scope);
        return StringifyFailed;
    }

    // Call the toJSON function.
    if (value.isObject() || value.isBigInt()) {
        value = toJSON(value, propertyName);
        RETURN_IF_EXCEPTION(scope, StringifyFailed);
    }

    // Call the replacer function.
    if (isCallableReplacer()) {
        MarkedArgumentBuffer args;
        args.append(propertyName.value(m_globalObject));
        args.append(value);
        ASSERT(!args.hasOverflowed());
        ASSERT(holder.object());
        value = call(m_globalObject, m_replacer, m_replacerCallData, holder.object(), args);
        RETURN_IF_EXCEPTION(scope, StringifyFailed);
    }

    if ((value.isUndefined() || value.isSymbol()) && !holder.isArray())
        return StringifyFailedDueToUndefinedOrSymbolValue;

    if (value.isNull()) {
        builder.append("null"_s);
        return StringifySucceeded;
    }

    if (value.isObject()) {
        value = unwrapBoxedPrimitive(m_globalObject, asObject(value));
        RETURN_IF_EXCEPTION(scope, StringifyFailed);
    }

    if (value.isBoolean()) {
        if (value.isTrue())
            builder.append("true"_s);
        else
            builder.append("false"_s);
        return StringifySucceeded;
    }

    if (value.isString()) {
        String string = asString(value)->value(m_globalObject);
        RETURN_IF_EXCEPTION(scope, StringifyFailed);
        builder.appendQuotedJSONString(string);
        return StringifySucceeded;
    }

    if (value.isNumber()) {
        if (value.isInt32())
            builder.append(value.asInt32());
        else {
            double number = value.asNumber();
            if (!std::isfinite(number))
                builder.append("null"_s);
            else
                builder.append(number);
        }
        return StringifySucceeded;
    }

    if (value.isBigInt()) {
        throwTypeError(m_globalObject, scope, "JSON.stringify cannot serialize BigInt."_s);
        return StringifyFailed;
    }

    if (!value.isObject())
        return StringifyFailed;

    JSObject* object = asObject(value);
    if (object->isCallable()) {
        if (holder.isArray()) {
            builder.append("null"_s);
            return StringifySucceeded;
        }
        return StringifyFailedDueToUndefinedOrSymbolValue;
    }

    if (UNLIKELY(builder.hasOverflowed()))
        return StringifyFailed;

    // Handle cycle detection, and put the holder on the stack.
    for (unsigned i = 0; i < m_holderStack.size(); i++) {
        if (m_holderStack[i].object() == object) {
            throwTypeError(m_globalObject, scope, "JSON.stringify cannot serialize cyclic structures."_s);
            return StringifyFailed;
        }
    }

    if (UNLIKELY(m_holderStack.size() >= maximumSideStackRecursion)) {
        throwStackOverflowError(m_globalObject, scope);
        return StringifyFailed;
    }

    bool holderStackWasEmpty = m_holderStack.isEmpty();
    Structure* structure = object->structure();
    m_holderStack.append(Holder(m_globalObject, object, structure));
    m_objectStack.appendWithCrashOnOverflow(object);
    m_objectStack.appendWithCrashOnOverflow(structure);
    RETURN_IF_EXCEPTION(scope, StringifyFailed);
    if (!holderStackWasEmpty)
        return StringifySucceeded;

    do {
        while (m_holderStack.last().appendNextProperty(*this, builder))
            RETURN_IF_EXCEPTION(scope, StringifyFailed);
        RETURN_IF_EXCEPTION(scope, StringifyFailed);
        if (UNLIKELY(builder.hasOverflowed()))
            return StringifyFailed;
        m_holderStack.removeLast();
        m_objectStack.removeLast();
        m_objectStack.removeLast();
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
    unsigned newSize = m_indent.length() + m_gap.length();
    if (newSize > m_repeatedGap.length())
        m_repeatedGap = makeString(m_repeatedGap, m_gap);
    ASSERT(newSize <= m_repeatedGap.length());
    m_indent = StringView { m_repeatedGap }.left(newSize);
}

inline void Stringifier::unindent()
{
    ASSERT(m_indent.length() >= m_gap.length());
    m_indent = StringView { m_repeatedGap }.left(m_indent.length() - m_gap.length());
}

inline void Stringifier::startNewLine(StringBuilder& builder) const
{
    if (willIndent())
        builder.append('\n', m_indent);
}

inline Stringifier::Holder::Holder(JSGlobalObject* globalObject, JSObject* object, Structure* structure)
    : m_object(object)
    , m_structure(structure)
    , m_isJSArray(isJSArray(object))
    , m_isArray(JSC::isArray(globalObject, object))
{
}

inline Stringifier::Holder::Holder(RootHolderTag, JSObject* object)
    : m_object(object)
{
}

bool Stringifier::Holder::appendNextProperty(Stringifier& stringifier, StringBuilder& builder)
{
    ASSERT(m_index <= m_size);

    JSGlobalObject* globalObject = stringifier.m_globalObject;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // First time through, initialize.
    if (!m_index) {
        if (m_isArray) {
            uint64_t length = toLength(globalObject, m_object);
            RETURN_IF_EXCEPTION(scope, false);
            if (UNLIKELY(length > std::numeric_limits<uint32_t>::max())) {
                throwOutOfMemoryError(globalObject, scope);
                return false;
            }
            m_size = static_cast<uint32_t>(length);
            RETURN_IF_EXCEPTION(scope, false);
            builder.append('[');
        } else {
            if (stringifier.m_usingArrayReplacer) {
                m_propertyNames = stringifier.m_arrayReplacerPropertyNames.data();
                m_size = m_propertyNames->propertyNameVector().size();
            } else if (m_structure && m_object->structureID() == m_structure->id() && canPerformFastPropertyEnumerationForJSONStringify(m_structure)) {
                m_structure->forEachProperty(vm, [&](const PropertyTableEntry& entry) -> bool {
                    if (entry.attributes() & PropertyAttribute::DontEnum)
                        return true;

                    PropertyName propertyName(entry.key());
                    if (propertyName.isSymbol())
                        return true;
                    m_propertiesAndOffsets.constructAndAppend(propertyName, entry.offset());
                    return true;
                });
                m_hasFastObjectProperties = true;
                m_size = m_propertiesAndOffsets.size();
            } else {
                PropertyNameArray objectPropertyNames(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
                m_object->methodTable()->getOwnPropertyNames(m_object, globalObject, objectPropertyNames, DontEnumPropertiesMode::Exclude);
                RETURN_IF_EXCEPTION(scope, false);
                m_propertyNames = objectPropertyNames.releaseData();
                m_size = m_propertyNames->propertyNameVector().size();
            }
            builder.append('{');
        }
        stringifier.indent();
    }
    if (UNLIKELY(builder.hasOverflowed()))
        return false;

    // Last time through, finish up and return false.
    if (m_index == m_size) {
        stringifier.unindent();
        if (m_size && builder[builder.length() - 1] != '{')
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
        if (m_isJSArray && m_object->canGetIndexQuickly(index))
            value = m_object->getIndexQuickly(index);
        else {
            value = m_object->get(globalObject, index);
            RETURN_IF_EXCEPTION(scope, false);
        }

        // Append the separator string.
        if (index)
            builder.append(',');
        stringifier.startNewLine(builder);

        // Append the stringified value.
        stringifyResult = stringifier.appendStringifiedValue(builder, value, *this, index);
        ASSERT(stringifyResult != StringifyFailedDueToUndefinedOrSymbolValue);
    } else {
        PropertyName propertyName { nullptr };
        JSValue value;
        if (m_hasFastObjectProperties) {
            propertyName = std::get<0>(m_propertiesAndOffsets[index]);
            if (m_object->structureID() == m_structure->id()) {
                unsigned offset = std::get<1>(m_propertiesAndOffsets[index]);
                value = m_object->getDirect(offset);
            } else {
                value = m_object->get(globalObject, propertyName);
                RETURN_IF_EXCEPTION(scope, false);
            }
        } else {
            propertyName = m_propertyNames->propertyNameVector()[index];
            value = m_object->get(globalObject, propertyName);
            RETURN_IF_EXCEPTION(scope, false);
        }

        rollBackPoint = builder.length();

        // Append the separator string.
        if (builder[rollBackPoint - 1] != '{')
            builder.append(',');
        stringifier.startNewLine(builder);

        // Append the property name, colon, and space.
        builder.appendQuotedJSONString(*propertyName.uid());
        builder.append(':');
        if (stringifier.willIndent())
            builder.append(' ');

        // Append the stringified value.
        stringifyResult = stringifier.appendStringifiedValue(builder, value, *this, propertyName);
    }
    RETURN_IF_EXCEPTION(scope, false);

    // From this point on, no access to the this pointer or to any members, because the
    // Holder object may have moved if the call to stringify pushed a new Holder onto
    // m_holderStack.

    switch (stringifyResult) {
        case StringifyFailed:
            builder.append("null"_s);
            break;
        case StringifySucceeded:
            break;
        case StringifyFailedDueToUndefinedOrSymbolValue:
            // This only occurs when we get an undefined value or a symbol value for
            // an object property. In this case we don't want the separator and
            // property name that we already appended, so roll back.
            builder.shrink(rollBackPoint);
            break;
    }

    return true;
}

// ------------------------------ FastStringifier --------------------------------

// FastStringifier does a no-side-effects stringify of the most common types of
// objects and arrays. It bails out if the serialization is any longer than a
// fixed buffer and handles only the simplest cases, including only 8-bit character
// strings. Instead of explicit checks to prevent excessive recursion and cycles,
// it counts on hitting the buffer size limit to catch those things. If it fails,
// since there is no side effect, the full general purpose Stringifier can be used
// and the only cost of the fast stringifying attempt is the time wasted.

class FastStringifier {
public:
    // Returns null string if the fast case fails.
    static String stringify(JSGlobalObject&, JSValue, JSValue replacer, JSValue space);

private:
    explicit FastStringifier(JSGlobalObject&);
    void append(JSValue);
    String result() const;

    void append(char, char, char, char);
    void append(char, char, char, char, char);
    template<typename T> void recordFailure(T&& reason);
    void recordBufferFull();
    String firstGetterSetterPropertyName() const;
    void recordFastPropertyEnumerationFailure(JSObject&);
    bool haveFailure() const;
    bool hasRemainingCapacity(unsigned size = 1);
    bool hasRemainingCapacitySlow(unsigned size);
    bool mayHaveToJSON(JSObject&) const;

    static void logOutcome(ASCIILiteral);
    static void logOutcome(String&&);

    static unsigned usableBufferSize(unsigned availableBufferSize);

    JSGlobalObject& m_globalObject;
    VM& m_vm;
    unsigned m_length { 0 }; // length of content already filled into m_buffer.
    unsigned m_capacity { 0 };
    bool m_checkedObjectPrototype { false };
    bool m_checkedArrayPrototype { false };

    LChar m_buffer[6000];
};

#if !FAST_STRINGIFY_LOG_USAGE

inline void FastStringifier::logOutcome(ASCIILiteral)
{
}

#else

void FastStringifier::logOutcome(ASCIILiteral outcome)
{
    logOutcome(String { outcome });
}

void FastStringifier::logOutcome(String&& outcome)
{
    static NeverDestroyed<HashCountedSet<String>> set;
    static std::atomic<unsigned> count;
    set->add(outcome);
    if (!(++count % 100)) {
        Vector<KeyValuePair<String, unsigned>> vector;
        for (auto& pair : set.get())
            vector.append(pair);
        std::sort(vector.begin(), vector.end(), [](auto& a, auto &b) {
            return a.value != b.value ? a.value > b.value : codePointCompareLessThan(a.key, b.key);
        });
        dataLogLn("fastStringify outcomes");
        for (auto& pair : vector) {
            dataLogF("%5u", pair.value);
            dataLogLn(": ", pair.key);
        }
    }
}

#endif

inline unsigned FastStringifier::usableBufferSize(unsigned availableBufferSize)
{
    // FastStringifier relies on m_capacity (i.e. the remaining usable capacity) in m_buffer
    // to limit recursion. Hence, we need to compute an appropriate m_capacity value.
    //
    // To do this, we empirically measured the worst case stack usage incurred by 1 recursion
    // of any of the append methods. Assuming each call to append() only consumes 1 LChar in
    // m_buffer, the amount of buffer size that FastStringifier is allowed to run with can be
    // estimated as:
    //
    //      stackCapacityForRecursion = remainingStackCapacity - maxLeafFunctionStackUsage
    //      maxAllowedBufferSize = stackCapacityForRecursion / maxRecursionFrameSize
    //      usableBufferSize = min(maxAllowedBufferSize, sizeof(m_buffer))
    //
    // 1. A leaf function is any function that append() calls which does not recurse.
    //    At peak recursion, there needs to be enough room left on the stack to execute any
    //    of these leaf functions i.e. maxLeafFunctionStackUsage.
    //
    //    We estimate maxLeafFunctionStackUsage to be StackBounds::DefaultReservedZone.
    //    stack.recursionLimit() already adds DefaultReservedZone to the bottom of the stack.
    //    Hence, using stack.recursionLimit() to compute stackCapacityForRecursion will leave
    //    us with the needed stack space for leaf functions to execute.
    //
    // 2. We can compute m_capacity as:
    //
    //      m_capacity = m_length + usableBufferSize
    //
    //    where m_length is the position of the next usable character for emission in m_buffer.
    //
    // 3. This calculation of m_capacity is a best effort estimate. If we're not
    //    conservative enough and get it wrong, the worst that can happen is that we'll
    //    crash when recursion causes us to step on the stack guard page at the bottom of
    //    the stack. The goal of trying to estimate a good m_capacity value is to avoid
    //    this stack overflow crash.
    //
    //    Note that for a Release build, maxRecursionFrameSize is measured to be less than
    //    384 bytes. This is well below stack guard page sizes which are between 4 and 16K
    //    depending on the OS. Hence, recursing too deeply with FastStringifier::append()
    //    is guaranteed to crash in the stack guard page.
    //
    // 4. If we're too conservative, we might fail out of FastStringifier too eagerly.
    //    In this case, we'll just fall back to the slow path Stringifier. The only down
    //    side here is potential loss of some performance opportunity when we encounter
    //    a workload that recurses deeply. We expect such workloads to be rare.

    auto& stack = Thread::current().stack();
    uint8_t* stackPointer = bitwise_cast<uint8_t*>(currentStackPointer());
    uint8_t* stackLimit = bitwise_cast<uint8_t*>(stack.recursionLimit());
    size_t stackCapacityForRecursion = stackPointer - stackLimit;

#if ASAN_ENABLED
    // Measured to be ~4608 for a Debug ASAN build on arm64E, rounding up to 5K for margin.
    constexpr size_t maxRecursionFrameSize = 5 * KB;
#elif !defined(NDEBUG)
    // Measured to be ~912 for a Debug build on arm64E, rounding up to 1280 for margin.
    constexpr size_t maxRecursionFrameSize = 1280;
#else
    // Measured to be ~224 for a Release build on arm64E, rounding up to 384 for margin.
    constexpr size_t maxRecursionFrameSize = 384;
#endif
    ASSERT(static_cast<unsigned>(stackCapacityForRecursion) == stackCapacityForRecursion);
    unsigned allowedBufferSize = stackCapacityForRecursion / maxRecursionFrameSize;
    unsigned usableBufferSize = std::min(allowedBufferSize, availableBufferSize);
    return usableBufferSize;
}

inline FastStringifier::FastStringifier(JSGlobalObject& globalObject)
    : m_globalObject(globalObject)
    , m_vm(globalObject.vm())
{
    m_capacity = m_length + usableBufferSize(sizeof(m_buffer));
}

inline bool FastStringifier::haveFailure() const
{
    return m_length > std::size(m_buffer);
}

inline String FastStringifier::result() const
{
    if (haveFailure())
        return { };
#if FAST_STRINGIFY_LOG_USAGE
    static std::atomic<unsigned> maxSizeSeen;
    if (m_length > maxSizeSeen) {
        maxSizeSeen = m_length;
        dataLogLn("max fastStringify buffer size used: ", m_length);
    }
    logOutcome("success"_s);
#endif
    return { m_buffer, m_length };
}

template<typename T> inline void FastStringifier::recordFailure(T&& reason)
{
    if (!haveFailure())
        logOutcome(std::forward<T>(reason));
    m_length = std::size(m_buffer) + 1;
}

inline void FastStringifier::recordBufferFull()
{
    recordFailure("buffer full"_s);
}

ALWAYS_INLINE bool FastStringifier::hasRemainingCapacity(unsigned size)
{
    ASSERT(!haveFailure());
    ASSERT(size > 0);
    unsigned remainingCapacity = m_capacity - m_length;
    if (size <= remainingCapacity)
        return true;
    return hasRemainingCapacitySlow(size);
}

bool FastStringifier::hasRemainingCapacitySlow(unsigned size)
{
    ASSERT(!haveFailure());

    unsigned unusedBufferSize = sizeof(m_buffer) - m_length;
    unsigned usableSize = usableBufferSize(unusedBufferSize);
    if (usableSize < size)
        return false;

    m_capacity = m_length + usableSize;
    ASSERT(m_capacity - m_length >= size);
    return true;
}

#if !FAST_STRINGIFY_LOG_USAGE

inline void FastStringifier::recordFastPropertyEnumerationFailure(JSObject&)
{
    recordFailure("!canPerformFastPropertyEnumerationForJSONStringify"_s);
}

#else

String FastStringifier::firstGetterSetterPropertyName(JSObject& object) const
{
    auto scope = DECLARE_THROW_SCOPE(m_vm);
    PropertyNameArray names(m_vm, PropertyNameMode::Strings, PrivateSymbolMode::Include);
    JSObject::getOwnPropertyNames(&object, &m_globalObject, names, DontEnumPropertiesMode::Include);
    CLEAR_AND_RETURN_IF_EXCEPTION(scope, "getOwnPropertyNames exception occurred"_s);
    for (auto& name : names) {
        PropertySlot slot(&object, PropertySlot::InternalMethodType::Get);
        JSObject::getOwnPropertySlot(&object, &m_globalObject, name, slot);
        CLEAR_AND_RETURN_IF_EXCEPTION(scope, "getOwnPropertySlot exception occurred"_s);
        if (slot.isAccessor())
            RELEASE_AND_RETURN(scope, name.string());
    }
    RELEASE_AND_RETURN(scope, "not found"_s);
}

void FastStringifier::recordFastPropertyEnumerationFailure(JSObject& object)
{
    auto& structure = *object.structure();
    if (structure.typeInfo().overridesGetOwnPropertySlot())
        recordFailure("overridesGetOwnPropertySlot"_s);
    else if (structure.typeInfo().overridesAnyFormOfGetOwnPropertyNames())
        recordFailure("overridesAnyFormOfGetOwnPropertyNames"_s);
    else if (hasIndexedProperties(structure.indexingType()))
        recordFailure("hasIndexedProperties"_s);
    else if (structure.hasGetterSetterProperties())
        recordFailure("getter/setter: "_s + firstGetterSetterPropertyName(object));
    else if (structure.hasReadOnlyOrGetterSetterPropertiesExcludingProto())
        recordFailure("hasReadOnlyOrGetterSetterPropertiesExcludingProto"_s);
    else if (structure.hasCustomGetterSetterProperties())
        recordFailure("hasCustomGetterSetterProperties"_s);
    else if (structure.isUncacheableDictionary())
        recordFailure("isUncacheableDictionary"_s);
    else if (structure.hasUnderscoreProtoPropertyExcludingOriginalProto())
        recordFailure("hasUnderscoreProtoPropertyExcludingOriginalProto"_s);
    else
        recordFailure("!canPerformFastPropertyEnumerationForJSONStringify mystery"_s);
}

#endif

inline bool FastStringifier::mayHaveToJSON(JSObject& object) const
{
    if (auto function = object.structure()->cachedSpecialProperty(CachedSpecialPropertyKey::ToJSON))
        return !function.isUndefined();
    if (UNLIKELY(object.noSideEffectMayHaveNonIndexProperty(m_vm, m_vm.propertyNames->toJSON))) {
        // Getting the property value so we can cache it could cause side effects; instead return true without caching anything.
        return true;
    }
    // Cache this so we can answer false next time without redoing the noSideEffectMayHaveNonIndexProperty work.
    PropertySlot slot { &object, PropertySlot::InternalMethodType::Get };
    object.structure()->cacheSpecialProperty(&m_globalObject, m_vm, jsUndefined(), CachedSpecialPropertyKey::ToJSON, slot);
    return false;
}

inline void FastStringifier::append(char a, char b, char c, char d)
{
    if (UNLIKELY(!hasRemainingCapacity(4))) {
        recordBufferFull();
        return;
    }
    m_buffer[m_length] = a;
    m_buffer[m_length + 1] = b;
    m_buffer[m_length + 2] = c;
    m_buffer[m_length + 3] = d;
    m_length += 4;
}

inline void FastStringifier::append(char a, char b, char c, char d, char e)
{
    if (UNLIKELY(!hasRemainingCapacity(5))) {
        recordBufferFull();
        return;
    }
    m_buffer[m_length] = a;
    m_buffer[m_length + 1] = b;
    m_buffer[m_length + 2] = c;
    m_buffer[m_length + 3] = d;
    m_buffer[m_length + 4] = e;
    m_length += 5;
}

void FastStringifier::append(JSValue value)
{
    if (value.isNull()) {
        append('n', 'u', 'l', 'l');
        return;
    }

    if (value.isTrue()) {
        append('t', 'r', 'u', 'e');
        return;
    }

    if (value.isFalse()) {
        append('f', 'a', 'l', 's', 'e');
        return;
    }

    if (value.isInt32()) {
        auto number = value.asInt32();
        auto length = lengthOfIntegerAsString(number);
        if (UNLIKELY(!hasRemainingCapacity(length))) {
            recordBufferFull();
            return;
        }
        writeIntegerToBuffer(number, &m_buffer[m_length]);
        m_length += length;
        return;
    }

    if (value.isDouble()) {
        auto number = value.asDouble();
        if (!std::isfinite(number)) {
            append('n', 'u', 'l', 'l');
            return;
        }
        if (UNLIKELY(!hasRemainingCapacity(sizeof(NumberToStringBuffer)))) {
            recordBufferFull();
            return;
        }
        WTF::double_conversion::StringBuilder builder { reinterpret_cast<char*>(&m_buffer[m_length]), sizeof(NumberToStringBuffer) };
        WTF::double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToShortest(number, &builder);
        m_length += builder.position();
        return;
    }

    if (UNLIKELY(!value.isCell())) {
        recordFailure("value type"_s);
        return;
    }
    auto& cell = *value.asCell();

    switch (cell.type()) {
    case StringType: {
        auto& string = asString(&cell)->tryGetValue();
        if (UNLIKELY(string.isNull())) {
            recordFailure("String::tryGetValue"_s);
            return;
        }
        if (UNLIKELY(!string.is8Bit())) {
            recordFailure("16-bit string"_s);
            return;
        }
        auto stringLength = string.length();
        if (UNLIKELY(!hasRemainingCapacity(1 + stringLength + 1))) {
            recordBufferFull();
            return;
        }
        m_buffer[m_length] = '"';
        auto* characters = string.characters8();
        for (unsigned i = 0; i < stringLength; ++i) {
            auto character = characters[i];
            if (UNLIKELY(WTF::escapedFormsForJSON[character])) {
                recordFailure("string character needs escaping"_s);
                return;
            }
            m_buffer[m_length + 1 + i] = character;
        }
        m_buffer[m_length + 1 + stringLength] = '"';
        m_length += 1 + stringLength + 1;
        return;
    }

    case ObjectType:
    case FinalObjectType: {
        auto& object = *asObject(&cell);
        if (UNLIKELY(object.isCallable())) {
            recordFailure("callable object"_s);
            return;
        }
        auto& structure = *object.structure();
        if (UNLIKELY(structure.hasPolyProto())) {
            recordFailure("hasPolyProto"_s);
            return;
        }
        if (UNLIKELY(structure.storedPrototype() != m_globalObject.objectPrototype())) {
            recordFailure("non-standard object prototype"_s);
            return;
        }
        if (!m_checkedObjectPrototype) {
            if (UNLIKELY(mayHaveToJSON(*m_globalObject.objectPrototype()))) {
                recordFailure("object prototype may have toJSON"_s);
                return;
            }
            m_checkedObjectPrototype = true;
        }
        if (UNLIKELY(!hasRemainingCapacity())) {
            recordBufferFull();
            return;
        }
        m_buffer[m_length++] = '{';
        if (UNLIKELY(!canPerformFastPropertyEnumerationForJSONStringify(&structure))) {
            recordFastPropertyEnumerationFailure(object);
            return;
        }
        structure.forEachProperty(m_vm, [&](const PropertyTableEntry& entry) -> bool {
            if (entry.attributes() & PropertyAttribute::DontEnum)
                return true;
            auto& name = *entry.key();
            if (UNLIKELY(name.isSymbol())) {
                recordFailure("symbol"_s);
                return false;
            }
            if (UNLIKELY(!name.is8Bit())) {
                recordFailure("16-bit property name"_s);
                return false;
            }
            bool needComma = m_buffer[m_length - 1] != '{';
            unsigned nameLength = name.length();
            if (UNLIKELY(!hasRemainingCapacity(needComma + 1 + nameLength + 2))) {
                recordBufferFull();
                return false;
            }
            if (needComma)
                m_buffer[m_length++] = ',';
            m_buffer[m_length] = '"';
            auto* characters = name.characters8();
            for (unsigned i = 0; i < nameLength; ++i) {
                auto character = characters[i];
                if (UNLIKELY(WTF::escapedFormsForJSON[character])) {
                    recordFailure("property name character needs escaping"_s);
                    return false;
                }
                m_buffer[m_length + 1 + i] = character;
            }
            m_buffer[m_length + 1 + nameLength] = '"';
            m_buffer[m_length + 1 + nameLength + 1] = ':';
            m_length += 1 + nameLength + 2;
            if (UNLIKELY(object.structure() != &structure)) {
                ASSERT_NOT_REACHED();
                recordFailure("unexpected structure transition"_s);
                return false;
            }
            append(object.getDirect(entry.offset()));
            return !haveFailure();
        });
        if (UNLIKELY(haveFailure()))
            return;
        if (UNLIKELY(!hasRemainingCapacity())) {
            recordBufferFull();
            return;
        }
        m_buffer[m_length++] = '}';
        return;
    }

    case ArrayType: {
        auto& array = *asArray(&cell);
        if (!m_checkedArrayPrototype) {
            if (UNLIKELY(mayHaveToJSON(*m_globalObject.arrayPrototype()))) {
                recordFailure("array prototype may have toJSON"_s);
                return;
            }
            m_checkedArrayPrototype = true;
        }
        auto& structure = *array.structure();
        if (UNLIKELY(!m_globalObject.isOriginalArrayStructure(&structure))) {
            structure.forEachProperty(m_vm, [&](const PropertyTableEntry& entry) -> bool {
                if (UNLIKELY(entry.key() == m_vm.propertyNames->toJSON)) {
                    recordFailure("array has toJSON"_s);
                    return false;
                }
                return true;
            });
            if (haveFailure())
                return;
        }
        if (UNLIKELY(!hasRemainingCapacity())) {
            recordBufferFull();
            return;
        }
        m_buffer[m_length++] = '[';
        for (unsigned i = 0, length = array.length(); i < length; ++i) {
            if (i) {
                if (UNLIKELY(!hasRemainingCapacity())) {
                    recordBufferFull();
                    return;
                }
                m_buffer[m_length++] = ',';
            }
            if (UNLIKELY(!array.canGetIndexQuickly(i))) {
                recordFailure("!canGetIndexQuickly"_s);
                return;
            }
            append(array.getIndexQuickly(i));
            if (UNLIKELY(haveFailure()))
                return;
        }
        if (UNLIKELY(!hasRemainingCapacity())) {
            recordBufferFull();
            return;
        }
        m_buffer[m_length++] = ']';
        return;
    }

    case JSFunctionType:
        recordFailure("function"_s);
        return;

    default:
        recordFailure("object type"_s);
    }
}

inline String FastStringifier::stringify(JSGlobalObject& globalObject, JSValue value, JSValue replacer, JSValue space)
{
    if (replacer.isObject()) {
        logOutcome("replacer"_s);
        return { };
    }
    if (!space.isUndefined()) {
        logOutcome("space"_s);
        return { };
    }
    FastStringifier stringifier(globalObject);
    stringifier.append(value);
    return stringifier.result();
}

static inline String stringify(JSGlobalObject& globalObject, JSValue value, JSValue replacer, JSValue space)
{
    if (String result = FastStringifier::stringify(globalObject, value, replacer, space); !result.isNull())
        return result;
    String result = Stringifier::stringify(globalObject, value, replacer, space);
#if FAST_STRINGIFY_LOG_USAGE
    if (!result.isNull())
        dataLogLn("Not fastStringify: ", result);
#endif
    return result;
}

// ------------------------------ JSONObject --------------------------------

const ClassInfo JSONObject::s_info = { "JSON"_s, &JSNonFinalObject::s_info, &jsonTable, nullptr, CREATE_METHOD_TABLE(JSONObject) };

/* Source for JSONObject.lut.h
@begin jsonTable
  parse         jsonProtoFuncParse             DontEnum|Function 2
  stringify     jsonProtoFuncStringify         DontEnum|Function 3
@end
*/

// ECMA 15.8

class Walker {
    WTF_MAKE_NONCOPYABLE(Walker);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    Walker(JSGlobalObject* globalObject, JSObject* function, CallData callData)
        : m_globalObject(globalObject)
        , m_function(function)
        , m_callData(callData)
    {
    }
    JSValue walk(JSValue unfiltered);
private:
    JSValue callReviver(JSObject* thisObj, JSValue property, JSValue unfiltered)
    {
        MarkedArgumentBuffer args;
        args.append(property);
        args.append(unfiltered);
        ASSERT(!args.hasOverflowed());
        return call(m_globalObject, m_function, m_callData, thisObj, args);
    }

    friend class Holder;

    JSGlobalObject* m_globalObject;
    JSObject* m_function;
    CallData m_callData;
};

enum WalkerState { StateUnknown, ArrayStartState, ArrayStartVisitMember, ArrayEndVisitMember, 
                                 ObjectStartState, ObjectStartVisitMember, ObjectEndVisitMember };
NEVER_INLINE JSValue Walker::walk(JSValue unfiltered)
{
    VM& vm = m_globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<PropertyNameArray, 16, UnsafeVectorOverflow> propertyStack;
    Vector<uint32_t, 16, UnsafeVectorOverflow> indexStack;
    MarkedArgumentBuffer markedStack;
    Vector<unsigned, 16, UnsafeVectorOverflow> arrayLengthStack;
    
    Vector<WalkerState, 16, UnsafeVectorOverflow> stateStack;
    WalkerState state = StateUnknown;
    JSValue inValue = unfiltered;
    JSValue outValue = jsNull();
    
    while (1) {
        switch (state) {
            arrayStartState:
            case ArrayStartState: {
                ASSERT(inValue.isObject());
                ASSERT(isArray(m_globalObject, inValue));
                EXCEPTION_ASSERT(!scope.exception());

                if (UNLIKELY(markedStack.size() >= maximumSideStackRecursion))
                    return throwStackOverflowError(m_globalObject, scope);

                JSObject* array = asObject(inValue);
                markedStack.appendWithCrashOnOverflow(array);
                uint64_t length = toLength(m_globalObject, array);
                RETURN_IF_EXCEPTION(scope, { });
                if (UNLIKELY(length > std::numeric_limits<uint32_t>::max())) {
                    throwOutOfMemoryError(m_globalObject, scope);
                    return { };
                }
                RETURN_IF_EXCEPTION(scope, { });
                arrayLengthStack.append(static_cast<uint32_t>(length));
                indexStack.append(0);
            }
            arrayStartVisitMember:
            FALLTHROUGH;
            case ArrayStartVisitMember: {
                JSObject* array = asObject(markedStack.last());
                uint32_t index = indexStack.last();
                unsigned arrayLength = arrayLengthStack.last();
                if (index == arrayLength) {
                    outValue = array;
                    markedStack.removeLast();
                    arrayLengthStack.removeLast();
                    indexStack.removeLast();
                    break;
                }
                if (isJSArray(array) && array->canGetIndexQuickly(index))
                    inValue = array->getIndexQuickly(index);
                else {
                    inValue = array->get(m_globalObject, index);
                    RETURN_IF_EXCEPTION(scope, { });
                }

                if (inValue.isObject()) {
                    stateStack.append(ArrayEndVisitMember);
                    goto stateUnknown;
                } else
                    outValue = inValue;
                FALLTHROUGH;
            }
            case ArrayEndVisitMember: {
                JSObject* array = asObject(markedStack.last());
                JSValue filteredValue = callReviver(array, jsString(vm, String::number(indexStack.last())), outValue);
                RETURN_IF_EXCEPTION(scope, { });
                if (filteredValue.isUndefined())
                    array->methodTable()->deletePropertyByIndex(array, m_globalObject, indexStack.last());
                else
                    array->putDirectIndex(m_globalObject, indexStack.last(), filteredValue, 0, PutDirectIndexShouldNotThrow);
                RETURN_IF_EXCEPTION(scope, { });
                indexStack.last()++;
                goto arrayStartVisitMember;
            }
            objectStartState:
            case ObjectStartState: {
                ASSERT(inValue.isObject());
                ASSERT(!isJSArray(inValue));
                if (UNLIKELY(markedStack.size() >= maximumSideStackRecursion))
                    return throwStackOverflowError(m_globalObject, scope);

                JSObject* object = asObject(inValue);
                markedStack.appendWithCrashOnOverflow(object);
                indexStack.append(0);
                propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                object->methodTable()->getOwnPropertyNames(object, m_globalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                RETURN_IF_EXCEPTION(scope, { });
            }
            objectStartVisitMember:
            FALLTHROUGH;
            case ObjectStartVisitMember: {
                JSObject* object = jsCast<JSObject*>(markedStack.last());
                uint32_t index = indexStack.last();
                PropertyNameArray& properties = propertyStack.last();
                if (index == properties.size()) {
                    outValue = object;
                    markedStack.removeLast();
                    indexStack.removeLast();
                    propertyStack.removeLast();
                    break;
                }
                inValue = object->get(m_globalObject, properties[index]);
                // The holder may be modified by the reviver function so any lookup may throw
                RETURN_IF_EXCEPTION(scope, { });

                if (inValue.isObject()) {
                    stateStack.append(ObjectEndVisitMember);
                    goto stateUnknown;
                } else
                    outValue = inValue;
                FALLTHROUGH;
            }
            case ObjectEndVisitMember: {
                JSObject* object = jsCast<JSObject*>(markedStack.last());
                Identifier prop = propertyStack.last()[indexStack.last()];
                JSValue filteredValue = callReviver(object, jsString(vm, prop.string()), outValue);
                RETURN_IF_EXCEPTION(scope, { });
                if (filteredValue.isUndefined())
                    JSCell::deleteProperty(object, m_globalObject, prop);
                else {
                    unsigned attributes;
                    PropertyOffset offset = object->getDirectOffset(vm, prop, attributes);
                    if (LIKELY(offset != invalidOffset && attributes == static_cast<unsigned>(PropertyAttribute::None))) {
                        object->putDirectOffset(vm, offset, filteredValue);
                        object->structure()->didReplaceProperty(offset);
                    } else {
                        bool shouldThrow = false;
                        object->createDataProperty(m_globalObject, prop, filteredValue, shouldThrow);
                    }
                }
                RETURN_IF_EXCEPTION(scope, { });
                indexStack.last()++;
                goto objectStartVisitMember;
            }
            stateUnknown:
            case StateUnknown:
                if (!inValue.isObject()) {
                    outValue = inValue;
                    break;
                }
                bool valueIsArray = isArray(m_globalObject, inValue);
                RETURN_IF_EXCEPTION(scope, { });
                if (valueIsArray)
                    goto arrayStartState;
                goto objectStartState;
        }
        if (stateStack.isEmpty())
            break;

        state = stateStack.last();
        stateStack.removeLast();
    }
    JSObject* finalHolder = constructEmptyObject(m_globalObject);
    finalHolder->putDirect(vm, vm.propertyNames->emptyIdentifier, outValue);
    RELEASE_AND_RETURN(scope, callReviver(finalHolder, jsEmptyString(vm), outValue));
}

// ECMA-262 v5 15.12.2
JSC_DEFINE_HOST_FUNCTION(jsonProtoFuncParse, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* string = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto viewWithString = string->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    StringView view = viewWithString.view;

    JSValue unfiltered;
    if (view.is8Bit()) {
        LiteralParser<LChar> jsonParser(globalObject, view.characters8(), view.length(), StrictJSON);
        unfiltered = jsonParser.tryLiteralParse();
        EXCEPTION_ASSERT(!scope.exception() || !unfiltered);
        if (!unfiltered) {
            RETURN_IF_EXCEPTION(scope, { });
            return throwVMError(globalObject, scope, createSyntaxError(globalObject, jsonParser.getErrorMessage()));
        }
    } else {
        LiteralParser<UChar> jsonParser(globalObject, view.characters16(), view.length(), StrictJSON);
        unfiltered = jsonParser.tryLiteralParse();
        EXCEPTION_ASSERT(!scope.exception() || !unfiltered);
        if (!unfiltered) {
            RETURN_IF_EXCEPTION(scope, { });
            return throwVMError(globalObject, scope, createSyntaxError(globalObject, jsonParser.getErrorMessage()));
        }
    }
    
    if (callFrame->argumentCount() < 2)
        return JSValue::encode(unfiltered);
    
    JSValue function = callFrame->uncheckedArgument(1);
    auto callData = JSC::getCallData(function);
    if (callData.type == CallData::Type::None)
        return JSValue::encode(unfiltered);
    scope.release();
    Walker walker(globalObject, asObject(function), callData);
    return JSValue::encode(walker.walk(unfiltered));
}

// ECMA-262 v5 15.12.3
JSC_DEFINE_HOST_FUNCTION(jsonProtoFuncStringify, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    String result = stringify(*globalObject, callFrame->argument(0), callFrame->argument(1), callFrame->argument(2));
    return result.isNull() ? encodedJSUndefined() : JSValue::encode(jsString(globalObject->vm(), WTFMove(result)));
}

JSValue JSONParse(JSGlobalObject* globalObject, StringView json)
{
    if (json.isNull())
        return JSValue();

    if (json.is8Bit()) {
        LiteralParser<LChar> jsonParser(globalObject, json.characters8(), json.length(), StrictJSON);
        return jsonParser.tryLiteralParse();
    }

    LiteralParser<UChar> jsonParser(globalObject, json.characters16(), json.length(), StrictJSON);
    return jsonParser.tryLiteralParse();
}

JSValue JSONParseWithException(JSGlobalObject* globalObject, StringView json)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (json.isNull())
        return JSValue();

    if (json.is8Bit()) {
        LiteralParser<LChar> jsonParser(globalObject, json.characters8(), json.length(), StrictJSON);
        JSValue result = jsonParser.tryLiteralParse();
        RETURN_IF_EXCEPTION(scope, { });
        if (!result)
            throwSyntaxError(globalObject, scope, jsonParser.getErrorMessage());
        return result;
    }

    LiteralParser<UChar> jsonParser(globalObject, json.characters16(), json.length(), StrictJSON);
    JSValue result = jsonParser.tryLiteralParse();
    RETURN_IF_EXCEPTION(scope, { });
    if (!result)
        throwSyntaxError(globalObject, scope, jsonParser.getErrorMessage());
    return result;
}

String JSONStringify(JSGlobalObject* globalObject, JSValue value, JSValue space)
{
    return stringify(*globalObject, value, jsNull(), space);
}

String JSONStringify(JSGlobalObject* globalObject, JSValue value, unsigned indent)
{
    return stringify(*globalObject, value, jsNull(), jsNumber(indent));
}

} // namespace JSC
