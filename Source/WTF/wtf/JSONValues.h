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

#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WTF {

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
class WTF_EXPORT_PRIVATE Value : public RefCounted<Value> {
public:
    static constexpr int maxDepth = 1000;

    virtual ~Value()
    {
        switch (m_type) {
        case Type::Null:
        case Type::Boolean:
        case Type::Double:
        case Type::Integer:
            break;
        case Type::String:
            if (m_value.string)
                m_value.string->deref();
            break;
        case Type::Object:
        case Type::Array:
            break;
        }
    }

    static Ref<Value> null();
    static Ref<Value> create(bool);
    static Ref<Value> create(int);
    static Ref<Value> create(double);
    static Ref<Value> create(const String&);
    template<class T>
    static Ref<Value> create(T) = delete;

    enum class Type {
        Null = 0,
        Boolean,
        Double,
        Integer,
        String,
        Object,
        Array,
    };

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Type::Null; }

    std::optional<bool> asBoolean() const;
    std::optional<int> asInteger() const;
    std::optional<double> asDouble() const;
    String asString() const;
    RefPtr<Value> asValue();
    virtual RefPtr<Object> asObject();
    virtual RefPtr<Array> asArray();

    static RefPtr<Value> parseJSON(const String&);
    static void escapeString(StringBuilder&, StringView);

    String toJSONString() const;
    virtual void writeJSON(StringBuilder& output) const;

    virtual size_t memoryCost() const;

    // FIXME: <http://webkit.org/b/179847> remove these functions when legacy InspectorObject symbols are no longer needed.
    bool asDouble(double&) const;
    bool asInteger(int&) const;
    bool asString(String&) const;

protected:
    Value()
        : m_type { Type::Null }
    {
    }

    explicit Value(Type type)
        : m_type(type)
    {
    }

    explicit Value(bool value)
        : m_type { Type::Boolean }
    {
        m_value.boolean = value;
    }

    explicit Value(int value)
        : m_type { Type::Integer }
    {
        m_value.number = static_cast<double>(value);
    }

    explicit Value(double value)
        : m_type(Type::Double)
    {
        m_value.number = value;
    }

    explicit Value(const String& value)
        : m_type { Type::String }
    {
        m_value.string = value.impl();
        if (m_value.string)
            m_value.string->ref();
    }

private:
    Type m_type { Type::Null };
    union {
        bool boolean;
        double number;
        StringImpl* string;
    } m_value;
};

class WTF_EXPORT_PRIVATE ObjectBase : public Value {
private:
    using DataStorage = HashMap<String, Ref<Value>>;
    using OrderStorage = Vector<String>;

public:
    using iterator = DataStorage::iterator;
    using const_iterator = DataStorage::const_iterator;

    RefPtr<Object> asObject() final;

    size_t memoryCost() const final;

protected:
    ~ObjectBase() override;

    // FIXME: use templates to reduce the amount of duplicated set*() methods.
    void setBoolean(const String& name, bool);
    void setInteger(const String& name, int);
    void setDouble(const String& name, double);
    void setString(const String& name, const String&);
    void setValue(const String& name, Ref<Value>&&);
    void setObject(const String& name, Ref<ObjectBase>&&);
    void setArray(const String& name, Ref<ArrayBase>&&);

    iterator find(const String& name);
    const_iterator find(const String& name) const;

    std::optional<bool> getBoolean(const String& name) const;
    std::optional<double> getDouble(const String& name) const;
    std::optional<int> getInteger(const String& name) const;
    String getString(const String& name) const;
    RefPtr<Object> getObject(const String& name) const;
    RefPtr<Array> getArray(const String& name) const;
    RefPtr<Value> getValue(const String& name) const;

    void remove(const String& name);

    void writeJSON(StringBuilder& output) const final;

    iterator begin() { return m_map.begin(); }
    iterator end() { return m_map.end(); }
    const_iterator begin() const { return m_map.begin(); }
    const_iterator end() const { return m_map.end(); }

    unsigned size() const { return m_map.size(); }

    // FIXME: <http://webkit.org/b/179847> remove these functions when legacy InspectorObject symbols are no longer needed.
    bool getBoolean(const String& name, bool& output) const;
    bool getString(const String& name, String& output) const;
    bool getObject(const String& name, RefPtr<Object>& output) const;
    bool getArray(const String& name, RefPtr<Array>& output) const;
    bool getValue(const String& name, RefPtr<Value>& output) const;

protected:
    ObjectBase();

private:
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

    using ObjectBase::size;
};


class WTF_EXPORT_PRIVATE ArrayBase : public Value {
private:
    using DataStorage = Vector<Ref<Value>>;

public:
    using iterator = DataStorage::iterator;
    using const_iterator = DataStorage::const_iterator;

    size_t length() const { return m_map.size(); }

    Ref<Value> get(size_t index) const;

    size_t memoryCost() const final;

    RefPtr<Array> asArray() final;

protected:
    ~ArrayBase() override;

    void pushBoolean(bool);
    void pushInteger(int);
    void pushDouble(double);
    void pushString(const String&);
    void pushValue(Ref<Value>&&);
    void pushObject(Ref<ObjectBase>&&);
    void pushArray(Ref<ArrayBase>&&);

    iterator begin() { return m_map.begin(); }
    iterator end() { return m_map.end(); }
    const_iterator begin() const { return m_map.begin(); }
    const_iterator end() const { return m_map.end(); }
    void writeJSON(StringBuilder& output) const final;

protected:
    ArrayBase();

private:
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

inline ObjectBase::iterator ObjectBase::find(const String& name)
{
    return m_map.find(name);
}

inline ObjectBase::const_iterator ObjectBase::find(const String& name) const
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
    if (m_map.set(name, WTFMove(value)).isNewEntry)
        m_order.append(name);
}

inline void ObjectBase::setObject(const String& name, Ref<ObjectBase>&& value)
{
    if (m_map.set(name, WTFMove(value)).isNewEntry)
        m_order.append(name);
}

inline void ObjectBase::setArray(const String& name, Ref<ArrayBase>&& value)
{
    if (m_map.set(name, WTFMove(value)).isNewEntry)
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
    m_map.append(WTFMove(value));
}

inline void ArrayBase::pushObject(Ref<ObjectBase>&& value)
{
    m_map.append(WTFMove(value));
}

inline void ArrayBase::pushArray(Ref<ArrayBase>&& value)
{
    m_map.append(WTFMove(value));
}

template<typename T>
class ArrayOf final : public ArrayBase {
private:
    ArrayOf() { }

    Array& castedArray()
    {
        COMPILE_ASSERT(sizeof(Array) == sizeof(ArrayOf<T>), cannot_cast);
        return *static_cast<Array*>(static_cast<ArrayBase*>(this));
    }

public:

    template <typename V = T>
    std::enable_if_t<std::is_same_v<bool, V> || std::is_same_v<Value, V>> addItem(bool value)
    {
        castedArray().pushBoolean(value);
    }

    template <typename V = T>
    std::enable_if_t<std::is_same_v<int, V> || std::is_same_v<Value, V>> addItem(int value)
    {
        castedArray().pushInteger(value);
    }

    template <typename V = T>
    std::enable_if_t<std::is_same_v<double, V> || std::is_same_v<Value, V>> addItem(double value)
    {
        castedArray().pushDouble(value);
    }

    template <typename V = T>
    std::enable_if_t<std::is_same_v<String, V> || std::is_same_v<Value, V>> addItem(const String& value)
    {
        castedArray().pushString(value);
    }

    template <typename V = T>
    std::enable_if_t<std::is_base_of_v<Value, V> && !std::is_base_of_v<ObjectBase, V> && !std::is_base_of_v<ArrayBase, V>> addItem(Ref<Value>&& value)
    {
        castedArray().pushValue(WTFMove(value));
    }

    template <typename V = T>
    std::enable_if_t<std::is_base_of_v<ObjectBase, V>> addItem(Ref<ObjectBase>&& value)
    {
        castedArray().pushObject(WTFMove(value));
    }

    template <typename V = T>
    std::enable_if_t<std::is_base_of_v<ArrayBase, V>> addItem(Ref<ArrayBase>&& value)
    {
        castedArray().pushArray(WTFMove(value));
    }

    static Ref<ArrayOf<T>> create()
    {
        return adoptRef(*new ArrayOf<T>());
    }

    using ArrayBase::get;
    using ArrayBase::begin;
    using ArrayBase::end;
};

} // namespace JSONImpl

} // namespace WTF

namespace JSON {
using namespace WTF::JSONImpl;
}

