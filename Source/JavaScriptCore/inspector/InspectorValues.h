/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
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

#ifndef InspectorValues_h
#define InspectorValues_h

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

class InspectorArray;
class InspectorArrayBase;
class InspectorObject;
class InspectorObjectBase;

class JS_EXPORT_PRIVATE InspectorValue : public RefCounted<InspectorValue> {
public:
    static const int maxDepth = 1000;

    InspectorValue()
        : m_type(Type::Null) { }
    virtual ~InspectorValue() { }

    static PassRefPtr<InspectorValue> null();

    enum class Type {
        Null = 0,
        Boolean,
        Double,
        Integer,
        String,
        Object,
        Array
    };

    Type type() const { return m_type; }

    bool isNull() const { return m_type == Type::Null; }

    virtual bool asBoolean(bool&) const;
    virtual bool asInteger(int&) const;
    virtual bool asInteger(unsigned&) const;
    virtual bool asInteger(long&) const;
    virtual bool asInteger(long long&) const;
    virtual bool asInteger(unsigned long&) const;
    virtual bool asInteger(unsigned long long&) const;
    virtual bool asDouble(double&) const;
    virtual bool asDouble(float&) const;
    virtual bool asString(String&) const;
    virtual bool asValue(RefPtr<InspectorValue>&);
    virtual bool asObject(RefPtr<InspectorObject>&);
    virtual bool asArray(RefPtr<InspectorArray>&);

    static bool parseJSON(const String& jsonInput, RefPtr<InspectorValue>& output);

    String toJSONString() const;
    virtual void writeJSON(StringBuilder& output) const;

protected:
    explicit InspectorValue(Type type) : m_type(type) { }

private:
    Type m_type;
};

class JS_EXPORT_PRIVATE InspectorBasicValue : public InspectorValue {
public:

    static PassRefPtr<InspectorBasicValue> create(bool);
    static PassRefPtr<InspectorBasicValue> create(int);
    static PassRefPtr<InspectorBasicValue> create(double);

    virtual bool asBoolean(bool&) const override;
    // Numbers from the frontend are always parsed as doubles, so we allow
    // clients to convert to integral values with this function.
    virtual bool asInteger(int&) const override;
    virtual bool asInteger(unsigned&) const override;
    virtual bool asInteger(long&) const override;
    virtual bool asInteger(long long&) const override;
    virtual bool asInteger(unsigned long&) const override;
    virtual bool asInteger(unsigned long long&) const override;
    virtual bool asDouble(double&) const override;
    virtual bool asDouble(float&) const override;

    virtual void writeJSON(StringBuilder& output) const override;

private:
    explicit InspectorBasicValue(bool value)
        : InspectorValue(Type::Boolean)
        , m_booleanValue(value) { }

    explicit InspectorBasicValue(int value)
        : InspectorValue(Type::Integer)
        , m_doubleValue(static_cast<double>(value)) { }

    explicit InspectorBasicValue(double value)
        : InspectorValue(Type::Double)
        , m_doubleValue(value) { }

    union {
        bool m_booleanValue;
        double m_doubleValue;
    };
};

class JS_EXPORT_PRIVATE InspectorString : public InspectorValue {
public:
    static PassRefPtr<InspectorString> create(const String&);
    static PassRefPtr<InspectorString> create(const char*);

    virtual bool asString(String& output) const override;

    virtual void writeJSON(StringBuilder& output) const override;

private:
    explicit InspectorString(const String& value)
        : InspectorValue(Type::String)
        , m_stringValue(value) { }

    explicit InspectorString(const char* value)
        : InspectorValue(Type::String)
        , m_stringValue(value) { }

    String m_stringValue;
};

class JS_EXPORT_PRIVATE InspectorObjectBase : public InspectorValue {
private:
    typedef HashMap<String, RefPtr<InspectorValue>> Dictionary;

public:
    typedef Dictionary::iterator iterator;
    typedef Dictionary::const_iterator const_iterator;

    InspectorObject* openAccessors();

protected:
    virtual ~InspectorObjectBase();

    virtual bool asObject(RefPtr<InspectorObject>& output) override;

    // FIXME: use templates to reduce the amount of duplicated set*() methods.
    void setBoolean(const String& name, bool);
    void setInteger(const String& name, int);
    void setDouble(const String& name, double);
    void setString(const String& name, const String&);
    void setValue(const String& name, PassRefPtr<InspectorValue>);
    void setObject(const String& name, PassRefPtr<InspectorObjectBase>);
    void setArray(const String& name, PassRefPtr<InspectorArrayBase>);

    iterator find(const String& name);
    const_iterator find(const String& name) const;

    // FIXME: use templates to reduce the amount of duplicated get*() methods.
    bool getBoolean(const String& name, bool& output) const;
    template<class T> bool getDouble(const String& name, T& output) const
    {
        RefPtr<InspectorValue> value = get(name);
        if (!value)
            return false;
        return value->asDouble(output);
    }
    template<class T> bool getInteger(const String& name, T& output) const
    {
        RefPtr<InspectorValue> value = get(name);
        if (!value)
            return false;
        return value->asInteger(output);
    }

    bool getString(const String& name, String& output) const;
    PassRefPtr<InspectorObject> getObject(const String& name) const;
    PassRefPtr<InspectorArray> getArray(const String& name) const;
    PassRefPtr<InspectorValue> get(const String& name) const;

    void remove(const String& name);

    virtual void writeJSON(StringBuilder& output) const override;

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }

    int size() const { return m_data.size(); }

protected:
    InspectorObjectBase();

private:
    Dictionary m_data;
    Vector<String> m_order;
};

class InspectorObject : public InspectorObjectBase {
public:
    static JS_EXPORT_PRIVATE PassRefPtr<InspectorObject> create();

    using InspectorObjectBase::asObject;

    using InspectorObjectBase::setBoolean;
    using InspectorObjectBase::setInteger;
    using InspectorObjectBase::setDouble;
    using InspectorObjectBase::setString;
    using InspectorObjectBase::setValue;
    using InspectorObjectBase::setObject;
    using InspectorObjectBase::setArray;

    using InspectorObjectBase::find;
    using InspectorObjectBase::getBoolean;
    using InspectorObjectBase::getInteger;
    using InspectorObjectBase::getDouble;
    using InspectorObjectBase::getString;
    using InspectorObjectBase::getObject;
    using InspectorObjectBase::getArray;
    using InspectorObjectBase::get;

    using InspectorObjectBase::remove;

    using InspectorObjectBase::begin;
    using InspectorObjectBase::end;

    using InspectorObjectBase::size;
};


class JS_EXPORT_PRIVATE InspectorArrayBase : public InspectorValue {
public:
    typedef Vector<RefPtr<InspectorValue>>::iterator iterator;
    typedef Vector<RefPtr<InspectorValue>>::const_iterator const_iterator;

    unsigned length() const { return m_data.size(); }

protected:
    virtual ~InspectorArrayBase();

    virtual bool asArray(RefPtr<InspectorArray>&) override;

    void pushBoolean(bool);
    void pushInteger(int);
    void pushDouble(double);
    void pushString(const String&);
    void pushValue(PassRefPtr<InspectorValue>);
    void pushObject(PassRefPtr<InspectorObject>);
    void pushArray(PassRefPtr<InspectorArray>);

    PassRefPtr<InspectorValue> get(size_t index);

    virtual void writeJSON(StringBuilder& output) const override;

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }

protected:
    InspectorArrayBase();

private:
    Vector<RefPtr<InspectorValue>> m_data;
};

class InspectorArray : public InspectorArrayBase {
public:
    static JS_EXPORT_PRIVATE PassRefPtr<InspectorArray> create();

    using InspectorArrayBase::asArray;

    using InspectorArrayBase::pushBoolean;
    using InspectorArrayBase::pushInteger;
    using InspectorArrayBase::pushDouble;
    using InspectorArrayBase::pushString;
    using InspectorArrayBase::pushValue;
    using InspectorArrayBase::pushObject;
    using InspectorArrayBase::pushArray;

    using InspectorArrayBase::get;

    using InspectorArrayBase::begin;
    using InspectorArrayBase::end;
};


inline InspectorObjectBase::iterator InspectorObjectBase::find(const String& name)
{
    return m_data.find(name);
}

inline InspectorObjectBase::const_iterator InspectorObjectBase::find(const String& name) const
{
    return m_data.find(name);
}

inline void InspectorObjectBase::setBoolean(const String& name, bool value)
{
    setValue(name, InspectorBasicValue::create(value));
}

inline void InspectorObjectBase::setInteger(const String& name, int value)
{
    setValue(name, InspectorBasicValue::create(value));
}

inline void InspectorObjectBase::setDouble(const String& name, double value)
{
    setValue(name, InspectorBasicValue::create(value));
}

inline void InspectorObjectBase::setString(const String& name, const String& value)
{
    setValue(name, InspectorString::create(value));
}

inline void InspectorObjectBase::setValue(const String& name, PassRefPtr<InspectorValue> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

inline void InspectorObjectBase::setObject(const String& name, PassRefPtr<InspectorObjectBase> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

inline void InspectorObjectBase::setArray(const String& name, PassRefPtr<InspectorArrayBase> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

inline void InspectorArrayBase::pushBoolean(bool value)
{
    m_data.append(InspectorBasicValue::create(value));
}

inline void InspectorArrayBase::pushInteger(int value)
{
    m_data.append(InspectorBasicValue::create(value));
}

inline void InspectorArrayBase::pushDouble(double value)
{
    m_data.append(InspectorBasicValue::create(value));
}

inline void InspectorArrayBase::pushString(const String& value)
{
    m_data.append(InspectorString::create(value));
}

inline void InspectorArrayBase::pushValue(PassRefPtr<InspectorValue> value)
{
    ASSERT(value);
    m_data.append(value);
}

inline void InspectorArrayBase::pushObject(PassRefPtr<InspectorObject> value)
{
    ASSERT(value);
    m_data.append(value);
}

inline void InspectorArrayBase::pushArray(PassRefPtr<InspectorArray> value)
{
    ASSERT(value);
    m_data.append(value);
}

} // namespace Inspector

#endif // !defined(InspectorValues_h)
