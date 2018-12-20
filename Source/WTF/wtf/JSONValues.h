/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
    static const int maxDepth = 1000;

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
    static Ref<Value> create(const char*);

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

    bool asBoolean(bool&) const;
    bool asInteger(int&) const;
    bool asInteger(unsigned&) const;
    bool asInteger(long&) const;
    bool asInteger(long long&) const;
    bool asInteger(unsigned long&) const;
    bool asInteger(unsigned long long&) const;
    bool asDouble(double&) const;
    bool asDouble(float&) const;
    bool asString(String&) const;
    bool asValue(RefPtr<Value>&);

    virtual bool asObject(RefPtr<Object>&);
    virtual bool asArray(RefPtr<Array>&);

    static bool parseJSON(const String& jsonInput, RefPtr<Value>& output);

    String toJSONString() const;
    virtual void writeJSON(StringBuilder& output) const;

    virtual size_t memoryCost() const;

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

    explicit Value(const char* value)
        : m_type { Type::String }
    {
        String wrapper(value);
        m_value.string = wrapper.impl();
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
    typedef HashMap<String, RefPtr<Value>> Dictionary;

public:
    typedef Dictionary::iterator iterator;
    typedef Dictionary::const_iterator const_iterator;

    Object* openAccessors();

    size_t memoryCost() const final;

protected:
    virtual ~ObjectBase();

    bool asObject(RefPtr<Object>& output) override;

    // FIXME: use templates to reduce the amount of duplicated set*() methods.
    void setBoolean(const String& name, bool);
    void setInteger(const String& name, int);
    void setDouble(const String& name, double);
    void setString(const String& name, const String&);
    void setValue(const String& name, RefPtr<Value>&&);
    void setObject(const String& name, RefPtr<ObjectBase>&&);
    void setArray(const String& name, RefPtr<ArrayBase>&&);

    iterator find(const String& name);
    const_iterator find(const String& name) const;

    // FIXME: use templates to reduce the amount of duplicated get*() methods.
    bool getBoolean(const String& name, bool& output) const;
    template<class T> bool getDouble(const String& name, T& output) const
    {
        RefPtr<Value> value;
        if (!getValue(name, value))
            return false;

        return value->asDouble(output);
    }
    template<class T> bool getInteger(const String& name, T& output) const
    {
        RefPtr<Value> value;
        if (!getValue(name, value))
            return false;

        return value->asInteger(output);
    }

    template<class T> Optional<T> getNumber(const String& name) const
    {
        RefPtr<Value> value;
        if (!getValue(name, value))
            return WTF::nullopt;

        T result;
        if (!value->asDouble(result))
            return WTF::nullopt;

        return result;
    }

    bool getString(const String& name, String& output) const;
    bool getObject(const String& name, RefPtr<Object>&) const;
    bool getArray(const String& name, RefPtr<Array>&) const;
    bool getValue(const String& name, RefPtr<Value>&) const;

    void remove(const String& name);

    void writeJSON(StringBuilder& output) const override;

    iterator begin() { return m_map.begin(); }
    iterator end() { return m_map.end(); }
    const_iterator begin() const { return m_map.begin(); }
    const_iterator end() const { return m_map.end(); }

    int size() const { return m_map.size(); }

protected:
    ObjectBase();

private:
    Dictionary m_map;
    Vector<String> m_order;
};

class Object : public ObjectBase {
public:
    static WTF_EXPORT_PRIVATE Ref<Object> create();

    using ObjectBase::asObject;

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
    using ObjectBase::getNumber;
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
public:
    typedef Vector<RefPtr<Value>>::iterator iterator;
    typedef Vector<RefPtr<Value>>::const_iterator const_iterator;

    unsigned length() const { return static_cast<unsigned>(m_map.size()); }

    RefPtr<Value> get(size_t index) const;

    size_t memoryCost() const final;

protected:
    virtual ~ArrayBase();

    bool asArray(RefPtr<Array>&) override;

    void pushBoolean(bool);
    void pushInteger(int);
    void pushDouble(double);
    void pushString(const String&);
    void pushValue(RefPtr<Value>&&);
    void pushObject(RefPtr<ObjectBase>&&);
    void pushArray(RefPtr<ArrayBase>&&);

    void writeJSON(StringBuilder& output) const override;

    iterator begin() { return m_map.begin(); }
    iterator end() { return m_map.end(); }
    const_iterator begin() const { return m_map.begin(); }
    const_iterator end() const { return m_map.end(); }

protected:
    ArrayBase();

private:
    Vector<RefPtr<Value>> m_map;
};

class Array : public ArrayBase {
public:
    static WTF_EXPORT_PRIVATE Ref<Array> create();

    using ArrayBase::asArray;

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

inline void ObjectBase::setValue(const String& name, RefPtr<Value>&& value)
{
    ASSERT(value);
    if (m_map.set(name, WTFMove(value)).isNewEntry)
        m_order.append(name);
}

inline void ObjectBase::setObject(const String& name, RefPtr<ObjectBase>&& value)
{
    ASSERT(value);
    if (m_map.set(name, WTFMove(value)).isNewEntry)
        m_order.append(name);
}

inline void ObjectBase::setArray(const String& name, RefPtr<ArrayBase>&& value)
{
    ASSERT(value);
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

inline void ArrayBase::pushValue(RefPtr<Value>&& value)
{
    ASSERT(value);
    m_map.append(WTFMove(value));
}

inline void ArrayBase::pushObject(RefPtr<ObjectBase>&& value)
{
    ASSERT(value);
    m_map.append(WTFMove(value));
}

inline void ArrayBase::pushArray(RefPtr<ArrayBase>&& value)
{
    ASSERT(value);
    m_map.append(WTFMove(value));
}

template<typename T>
class ArrayOf : public ArrayBase {
private:
    ArrayOf() { }

    Array& castedArray()
    {
        COMPILE_ASSERT(sizeof(Array) == sizeof(ArrayOf<T>), cannot_cast);
        return *static_cast<Array*>(static_cast<ArrayBase*>(this));
    }

public:
    void addItem(RefPtr<T>&& value)
    {
        castedArray().pushValue(WTFMove(value));
    }
    
    void addItem(const String& value)
    {
        castedArray().pushString(value);
    }

    void addItem(int value)
    {
        castedArray().pushInteger(value);
    }

    void addItem(double value)
    {
        castedArray().pushDouble(value);
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

