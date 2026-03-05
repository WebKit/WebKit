/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#pragma once

#include <variant>
#include <wtf/HashMap.h>
#include <wtf/NoVirtualDestructorBase.h>
#include <wtf/Variant.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class PrintStream;

// Make sure compiled symbols contain the WTF namespace prefix, but
// use a different inner namespace name so that JSON::Value is not ambigious.
// Otherwise, the compiler would have both WTF::JSON::Value and JSON::Value
// in scope and client code would have to use WTF::JSON::Value, which is tedious.
namespace JSONImpl {

class Array;
class ArrayBase;
class Object;
class ObjectBase;

// FIXME: unify this JSON parser with JSONParse in JavaScriptCore.
class WTF_EXPORT_PRIVATE Value : public RefCounted<Value>, public NoVirtualDestructorBase {
public:
    static constexpr int maxDepth = 1000;

    void operator delete(Value*, std::destroying_delete_t);
    bool operator!() const;

    static Ref<Value> null();
    static Ref<Value> create(bool);
    static Ref<Value> create(int);
    static Ref<Value> create(double);
    static Ref<Value> create(const String&);
    template<class T>
    static Ref<Value> create(T) = delete;

    enum class Type : uint8_t {
        Null = 0,
        Boolean,
        Double,
        Integer,
        String,
        Object,
        Array,
    };

    enum class ArrayTypeTag { Array };
    enum class ObjectTypeTag { Object };

    Type type() const
    {
        static_assert(std::is_same_v<std::monostate, WTF::variant_alternative_t<static_cast<uint8_t>(Type::Null), decltype(m_value)>>);
        static_assert(std::is_same_v<bool, WTF::variant_alternative_t<static_cast<uint8_t>(Type::Boolean), decltype(m_value)>>);
        static_assert(std::is_same_v<double, WTF::variant_alternative_t<static_cast<uint8_t>(Type::Double), decltype(m_value)>>);
        static_assert(std::is_same_v<int, WTF::variant_alternative_t<static_cast<uint8_t>(Type::Integer), decltype(m_value)>>);
        static_assert(std::is_same_v<String, WTF::variant_alternative_t<static_cast<uint8_t>(Type::String), decltype(m_value)>>);
        static_assert(std::is_same_v<ObjectTypeTag, WTF::variant_alternative_t<static_cast<uint8_t>(Type::Object), decltype(m_value)>>);
        static_assert(std::is_same_v<ArrayTypeTag, WTF::variant_alternative_t<static_cast<uint8_t>(Type::Array), decltype(m_value)>>);
        return static_cast<Type>(m_value.index());
    }
    bool isNull() const { return std::holds_alternative<std::monostate>(m_value); }

    std::optional<bool> asBoolean() const;
    std::optional<int> asInteger() const;
    std::optional<double> asDouble() const;
    const String& asString() const;
    RefPtr<Value> asValue();
    RefPtr<Object> asObject();
    RefPtr<const Object> asObject() const;
    RefPtr<Array> asArray();

    static RefPtr<Value> parseJSON(StringView);
    static std::optional<Ref<Value>> optionalParseJSON(StringView);

    String toJSONString() const;

    void dump(PrintStream&) const;

    // FIXME: <http://webkit.org/b/179847> remove these functions when legacy InspectorObject symbols are no longer needed.
    bool asDouble(double&) const;
    bool asInteger(int&) const;
    bool asString(String&) const;

    size_t memoryCost() const;
    void writeJSON(StringBuilder& output) const;

protected:
    Value() = default;

    explicit Value(ArrayTypeTag type)
        : m_value(type)
    {
    }

    explicit Value(ObjectTypeTag type)
        : m_value(type)
    {
    }

    explicit Value(bool value)
        : m_value(value)
    {
    }

    explicit Value(int value)
        : m_value(value)
    {
    }

    explicit Value(double value)
        : m_value(value)
    {
    }

    explicit Value(const String& value)
        : m_value(value)
    {
    }

    template<typename Visitor> constexpr decltype(auto) visitDerived(Visitor&&);
    template<typename Visitor> constexpr decltype(auto) visitDerived(Visitor&&) const;
    size_t memoryCostImpl() const;
    void writeJSONImpl(StringBuilder& output) const;

private:
    WTF::Variant<std::monostate, bool, double, int, String, ObjectTypeTag, ArrayTypeTag> m_value;
};

class ObjectBase : public Value {
private:
    friend class Value;
    using DataStorage = HashMap<String, Ref<Value>>;
    using OrderStorage = Vector<String>;

public:
    using iterator = DataStorage::iterator;
    using const_iterator = DataStorage::const_iterator;

protected:
    ~ObjectBase();

    // FIXME: use templates to reduce the amount of duplicated set*() methods.
    void setBoolean(const String& name, bool);
    void setInteger(const String& name, int);
    void setDouble(const String& name, double);
    void setString(const String& name, const String&);
    void setValue(const String& name, Ref<Value>&&);
    void setObject(const String& name, Ref<ObjectBase>&&);
    void setArray(const String& name, Ref<ArrayBase>&&);

    iterator find(const String& name) LIFETIME_BOUND;
    const_iterator find(const String& name) const LIFETIME_BOUND;

    WTF_EXPORT_PRIVATE std::optional<bool> getBoolean(const String& name) const;
    WTF_EXPORT_PRIVATE std::optional<double> getDouble(const String& name) const;
    WTF_EXPORT_PRIVATE std::optional<int> getInteger(const String& name) const;
    WTF_EXPORT_PRIVATE String getString(const String& name) const;
    WTF_EXPORT_PRIVATE RefPtr<Object> getObject(const String& name) const;
    WTF_EXPORT_PRIVATE RefPtr<Array> getArray(const String& name) const;
    WTF_EXPORT_PRIVATE RefPtr<Value> getValue(const String& name) const;

    WTF_EXPORT_PRIVATE void remove(const String& name);

    iterator begin() LIFETIME_BOUND { return m_map.begin(); }
    iterator end() LIFETIME_BOUND { return m_map.end(); }
    const_iterator begin() const LIFETIME_BOUND { return m_map.begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_map.end(); }

    OrderStorage keys() const { return m_order; }

    unsigned size() const { return m_map.size(); }

    // FIXME: <http://webkit.org/b/179847> remove these functions when legacy InspectorObject symbols are no longer needed.
    bool getBoolean(const String& name, bool& output) const;
    WTF_EXPORT_PRIVATE bool getString(const String& name, String& output) const;
    bool getObject(const String& name, RefPtr<Object>& output) const;
    bool getArray(const String& name, RefPtr<Array>& output) const;
    WTF_EXPORT_PRIVATE bool getValue(const String& name, RefPtr<Value>& output) const;

protected:
    WTF_EXPORT_PRIVATE ObjectBase();

private:
    size_t memoryCostImpl() const;
    void writeJSONImpl(StringBuilder& output) const;

    DataStorage m_map;
    OrderStorage m_order;
};

class Object : public ObjectBase {
public:
    static WTF_EXPORT_PRIVATE Ref<Object> create();

    // This class expected non-cyclic values, as we cannot serialize cycles in JSON.
    using ObjectBase::setBoolean;
    using ObjectBase::setInteger;
    using ObjectBase::setDouble;
    using ObjectBase::setString;
    using ObjectBase::setValue;
    using ObjectBase::setObject;
    using ObjectBase::setArray;

    using ObjectBase::find;
    using ObjectBase::getBoolean;
    using ObjectBase::getInteger;
    using ObjectBase::getDouble;
    using ObjectBase::getString;
    using ObjectBase::getObject;
    using ObjectBase::getArray;
    using ObjectBase::getValue;

    using ObjectBase::remove;

    using ObjectBase::begin;
    using ObjectBase::end;

    using ObjectBase::keys;

    using ObjectBase::size;
};


class WTF_EXPORT_PRIVATE ArrayBase : public Value {
private:
    friend class Value;
    using DataStorage = Vector<Ref<Value>>;

public:
    using iterator = DataStorage::iterator;
    using const_iterator = DataStorage::const_iterator;

    size_t length() const { return m_map.size(); }

    Ref<Value> NODELETE get(size_t index) const;

protected:
    ~ArrayBase();

    void pushBoolean(bool);
    void pushInteger(int);
    void pushDouble(double);
    void pushString(const String&);
    void pushValue(Ref<Value>&&);
    void pushObject(Ref<ObjectBase>&&);
    void pushArray(Ref<ArrayBase>&&);

    iterator begin() LIFETIME_BOUND { return m_map.begin(); }
    iterator end() LIFETIME_BOUND { return m_map.end(); }
    const_iterator begin() const LIFETIME_BOUND { return m_map.begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_map.end(); }

protected:
    ArrayBase();

private:
    size_t memoryCostImpl() const;
    void writeJSONImpl(StringBuilder& output) const;

    DataStorage m_map;
};

class Array final : public ArrayBase {
public:
    static WTF_EXPORT_PRIVATE Ref<Array> create();

    // This class expected non-cyclic values, as we cannot serialize cycles in JSON.
    using ArrayBase::pushBoolean;
    using ArrayBase::pushInteger;
    using ArrayBase::pushDouble;
    using ArrayBase::pushString;
    using ArrayBase::pushValue;
    using ArrayBase::pushObject;
    using ArrayBase::pushArray;

    using ArrayBase::get;

    using ArrayBase::begin;
    using ArrayBase::end;
};

inline ObjectBase::iterator ObjectBase::find(const String& name) LIFETIME_BOUND
{
    return m_map.find(name);
}

inline ObjectBase::const_iterator ObjectBase::find(const String& name) const LIFETIME_BOUND
{
    return m_map.find(name);
}

inline void ObjectBase::setBoolean(const String& name, bool value)
{
    setValue(name, Value::create(value));
}

inline void ObjectBase::setInteger(const String& name, int value)
{
    setValue(name, Value::create(value));
}

inline void ObjectBase::setDouble(const String& name, double value)
{
    setValue(name, Value::create(value));
}

inline void ObjectBase::setString(const String& name, const String& value)
{
    setValue(name, Value::create(value));
}

inline void ObjectBase::setValue(const String& name, Ref<Value>&& value)
{
    if (m_map.set(name, WTF::move(value)).isNewEntry)
        m_order.append(name);
}

inline void ObjectBase::setObject(const String& name, Ref<ObjectBase>&& value)
{
    if (m_map.set(name, WTF::move(value)).isNewEntry)
        m_order.append(name);
}

inline void ObjectBase::setArray(const String& name, Ref<ArrayBase>&& value)
{
    if (m_map.set(name, WTF::move(value)).isNewEntry)
        m_order.append(name);
}

inline void ArrayBase::pushBoolean(bool value)
{
    m_map.append(Value::create(value));
}

inline void ArrayBase::pushInteger(int value)
{
    m_map.append(Value::create(value));
}

inline void ArrayBase::pushDouble(double value)
{
    m_map.append(Value::create(value));
}

inline void ArrayBase::pushString(const String& value)
{
    m_map.append(Value::create(value));
}

inline void ArrayBase::pushValue(Ref<Value>&& value)
{
    m_map.append(WTF::move(value));
}

inline void ArrayBase::pushObject(Ref<ObjectBase>&& value)
{
    m_map.append(WTF::move(value));
}

inline void ArrayBase::pushArray(Ref<ArrayBase>&& value)
{
    m_map.append(WTF::move(value));
}

template<typename T>
class ArrayOf final : public ArrayBase {
private:
    ArrayOf() { }

    Array& castedArray()
    {
        static_assert(sizeof(Array) == sizeof(ArrayOf<T>), "cannot cast");
        return *static_cast<Array*>(static_cast<ArrayBase*>(this));
    }

public:
    template<typename V = T>
        requires (std::same_as<bool, V> || std::same_as<Value, V>)
    void addItem(bool value)
    {
        castedArray().pushBoolean(value);
    }

    template<typename V = T>
        requires (std::same_as<int, V> || std::same_as<Value, V>)
    void addItem(int value)
    {
        castedArray().pushInteger(value);
    }

    template<typename V = T>
        requires (std::same_as<double, V> || std::same_as<Value, V>)
    void addItem(double value)
    {
        castedArray().pushDouble(value);
    }

    template<typename V = T>
        requires (std::same_as<String, V> || std::same_as<Value, V>)
    void addItem(const String& value)
    {
        castedArray().pushString(value);
    }

    template<typename V = T>
        requires (std::is_base_of_v<Value, V> && !std::is_base_of_v<ObjectBase, V> && !std::is_base_of_v<ArrayBase, V>)
    void addItem(Ref<Value>&& value)
    {
        castedArray().pushValue(WTF::move(value));
    }

    template<typename V = T>
        requires (std::is_base_of_v<ObjectBase, V>)
    void addItem(Ref<ObjectBase>&& value)
    {
        castedArray().pushObject(WTF::move(value));
    }

    template<typename V = T>
        requires (std::is_base_of_v<ArrayBase, V>)
    void addItem(Ref<ArrayBase>&& value)
    {
        castedArray().pushArray(WTF::move(value));
    }

    static Ref<ArrayOf<T>> create()
    {
        return adoptRef(*new ArrayOf<T>());
    }

    using ArrayBase::get;
    using ArrayBase::begin;
    using ArrayBase::end;
};


inline bool Value::operator!() const
{
    return isNull();
}

inline RefPtr<Value> Value::asValue()
{
    return this;
}

inline RefPtr<Object> Value::asObject()
{
    return WTF::visit(WTF::makeVisitor([&](ObjectTypeTag) -> RefPtr<Object> {
        static_assert(sizeof(Object) == sizeof(ObjectBase));
        return static_cast<Object*>(this);
    }, [&](auto&) -> RefPtr<Object> {
        return nullptr;
    }), m_value);
}

inline RefPtr<const Object> Value::asObject() const
{
    return WTF::visit(WTF::makeVisitor([&](ObjectTypeTag) -> RefPtr<const Object> {
        static_assert(sizeof(Object) == sizeof(ObjectBase));
        return static_cast<const Object*>(this);
    }, [&](auto&) -> RefPtr<const Object> {
        return nullptr;
    }), m_value);
}

inline RefPtr<Array> Value::asArray()
{
    return WTF::visit(WTF::makeVisitor([&](ArrayTypeTag) -> RefPtr<Array> {
        static_assert(sizeof(Array) == sizeof(ArrayBase));
        return static_cast<Array*>(this);
    }, [&](auto&) -> RefPtr<Array> {
        return nullptr;
    }), m_value);
}

} // namespace JSONImpl

inline size_t containerSize(const JSONImpl::Array& array) { return array.length(); }

} // namespace WTF

namespace JSON {
using namespace WTF::JSONImpl;
}

