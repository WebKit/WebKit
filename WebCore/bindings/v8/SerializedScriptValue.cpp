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
#include "SerializedScriptValue.h"

#include "SharedBuffer.h"

#include <v8.h>
#include <wtf/Assertions.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

// FIXME:
// - catch V8 exceptions
// - be ready to get empty handles
// - consider crashing in debug mode on deserialization errors

namespace WebCore {

namespace {

typedef UChar BufferValueType;

// Serialization format is a sequence of (tag, optional data)
// pairs. Tag always takes exactly one byte.
enum SerializationTag {
    InvalidTag = '!',
    PaddingTag = '\0',
    UndefinedTag = '_',
    NullTag = '0',
    TrueTag = 'T',
    FalseTag = 'F',
    StringTag = 'S',
    Int32Tag = 'I',
    NumberTag = 'N',
    ObjectTag = '{',
    ArrayTag = '[',
};

// Helpers to do verbose handle casts.

template <typename T, typename U>
static v8::Handle<T> handleCast(v8::Handle<U> handle) { return v8::Handle<T>::Cast(handle); }

template <typename T, typename U>
static v8::Local<T> handleCast(v8::Local<U> handle) { return v8::Local<T>::Cast(handle); }

static bool shouldCheckForCycles(int depth)
{
    ASSERT(depth >= 0);
    // Since we are not required to spot the cycle as soon as it
    // happens we can check for cycles only when the current depth
    // is a power of two.
    return !(depth & (depth - 1));
}

static const int maxDepth = 20000;

// VarInt encoding constants.
static const int varIntShift = 7;
static const int varIntMask = (1 << varIntShift) - 1;

// ZigZag encoding helps VarInt encoding stay small for negative
// numbers with small absolute values.
class ZigZag {
public:
    static uint32_t encode(uint32_t value)
    {
        if (value & (1U << 31))
            value = ((~value) << 1) + 1;
        else
            value <<= 1;
        return value;
    }

    static uint32_t decode(uint32_t value)
    {
        if (value & 1)
            value = ~(value >> 1);
        else
            value >>= 1;
        return value;
    }

private:
    ZigZag();
};

// Writer is responsible for serializing primitive types and storing
// information used to reconstruct composite types.
class Writer : Noncopyable {
public:
    Writer() : m_position(0)
    {
    }

    // Write functions for primitive types.

    void writeUndefined() { append(UndefinedTag); }

    void writeNull() { append(NullTag); }

    void writeTrue() { append(TrueTag); }

    void writeFalse() { append(FalseTag); }

    void writeString(const char* data, int length)
    {
        append(StringTag);
        doWriteUint32(static_cast<uint32_t>(length));
        append(data, length);
    }

    void writeInt32(int32_t value)
    {
        append(Int32Tag);
        doWriteUint32(ZigZag::encode(static_cast<uint32_t>(value)));
    }

    void writeNumber(double number)
    {
        append(NumberTag);
        append(reinterpret_cast<char*>(&number), sizeof(number));
    }

    // Records that a composite object can be constructed by using
    // |length| previously stored values.
    void endComposite(SerializationTag tag, int32_t length)
    {
        ASSERT(tag == ObjectTag || tag == ArrayTag);
        append(tag);
        doWriteUint32(static_cast<uint32_t>(length));
    }

    Vector<BufferValueType>& data()
    {
        fillHole();
        return m_buffer;
    }

private:
    void doWriteUint32(uint32_t value)
    {
        while (true) {
            char b = (value & varIntMask);
            value >>= varIntShift;
            if (!value) {
                append(b);
                break;
            }
            append(b | (1 << varIntShift));
        }
    }

    void append(SerializationTag tag)
    {
        append(static_cast<char>(tag));
    }

    void append(char b)
    {
        ensureSpace(1);
        *charAt(m_position++) = b;
    }

    void append(const char* data, int length)
    {
        ensureSpace(length);
        memcpy(charAt(m_position), data, length);
        m_position += length;
    }

    void ensureSpace(int extra)
    {
        COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
        m_buffer.grow((m_position + extra + 1) / 2); // "+ 1" to round up.
    }

    void fillHole()
    {
        COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
        // If the writer is at odd position in the buffer, then one of
        // the bytes in the last UChar is not initialized.
        if (m_position % 2)
            *charAt(m_position) = static_cast<char>(PaddingTag);
    }

    char* charAt(int position) { return reinterpret_cast<char*>(m_buffer.data()) + position; }

    Vector<BufferValueType> m_buffer;
    unsigned m_position;
};

class Serializer {
public:
    explicit Serializer(Writer& writer)
        : m_writer(writer)
        , m_state(0)
        , m_depth(0)
    {
    }

    bool serialize(v8::Handle<v8::Value> value)
    {
        v8::HandleScope scope;
        StackCleaner cleaner(&m_state);
        if (!doSerialize(value))
            return false;
        while (top()) {
            int length;
            while (!top()->isDone(&length)) {
                // Note that doSerialize() can change current top().
                if (!doSerialize(top()->advance()))
                    return false;
            }
            m_writer.endComposite(top()->tag(), length);
            pop();
        }
        return true;
    }

private:
    class StateBase : public Noncopyable {
    public:
        virtual ~StateBase() { }

        // Link to the next state to form a stack.
        StateBase* nextState() { return m_next; }
        void setNextState(StateBase* next) { m_next = next; }

        // Composite object we're processing in this state.
        v8::Handle<v8::Value> composite() { return m_composite; }

        // Serialization tag for the current composite.
        virtual SerializationTag tag() const = 0;

        // Returns whether iteration over subobjects of the current
        // composite object is done. If yes, |*length| is set to the
        // number of subobjects.
        virtual bool isDone(int* length) = 0;

        // Advances to the next subobject.
        // Requires: !this->isDone().
        virtual v8::Local<v8::Value> advance() = 0;

    protected:
        StateBase(v8::Handle<v8::Value> composite)
            : m_next(0)
            , m_composite(composite)
        {
        }

    private:
        StateBase* m_next;
        v8::Handle<v8::Value> m_composite;
    };

    template <typename T, SerializationTag compositeTag>
    class State : public StateBase {
    public:
        v8::Handle<T> composite() { return handleCast<T>(StateBase::composite()); }

        virtual SerializationTag tag() const { return compositeTag; }

    protected:
        explicit State(v8::Handle<T> composite) : StateBase(composite)
        {
        }
    };

    // Helper to clean up the state stack in case of errors.
    class StackCleaner : Noncopyable {
    public:
        explicit StackCleaner(StateBase** stack) : m_stack(stack)
        {
        }

        ~StackCleaner()
        {
            StateBase* state = *m_stack;
            while (state) {
                StateBase* tmp = state->nextState();
                delete state;
                state = tmp;
            }
            *m_stack = 0;
        }

    private:
        StateBase** m_stack;
    };

    class ArrayState : public State<v8::Array, ArrayTag> {
    public:
        ArrayState(v8::Handle<v8::Array> array)
            : State<v8::Array, ArrayTag>(array)
            , m_index(0)
        {
        }

        virtual bool isDone(int* length)
        {
            *length = composite()->Length();
            return static_cast<int>(m_index) >= *length;
        }

        virtual v8::Local<v8::Value> advance()
        {
            ASSERT(m_index < composite()->Length());
            v8::HandleScope scope;
            return scope.Close(composite()->Get(v8::Integer::New(m_index++)));
        }

    private:
        unsigned m_index;
    };

    class ObjectState : public State<v8::Object, ObjectTag> {
    public:
        ObjectState(v8::Handle<v8::Object> object)
            : State<v8::Object, ObjectTag>(object)
            , m_propertyNames(object->GetPropertyNames())
            , m_index(-1)
            , m_length(0)
        {
            nextProperty();
        }

        virtual bool isDone(int* length)
        {
            *length = m_length;
            return m_index >= 2 * m_propertyNames->Length();
        }

        virtual v8::Local<v8::Value> advance()
        {
            ASSERT(m_index < 2 * m_propertyNames->Length());
            if (!(m_index % 2)) {
                ++m_index;
                return m_propertyName;
            }
            v8::Local<v8::Value> result = composite()->Get(m_propertyName);
            nextProperty();
            return result;
        }

    private:
        void nextProperty()
        {
            v8::HandleScope scope;
            ++m_index;
            ASSERT(!(m_index % 2));
            for (; m_index < 2 * m_propertyNames->Length(); m_index += 2) {
                v8::Local<v8::Value> propertyName = m_propertyNames->Get(v8::Integer::New(m_index / 2));
                if ((propertyName->IsString() && composite()->HasRealNamedProperty(handleCast<v8::String>(propertyName)))
                    || (propertyName->IsInt32() && composite()->HasRealIndexedProperty(propertyName->Uint32Value()))) {
                    m_propertyName = scope.Close(propertyName);
                    m_length += 2;
                    return;
                }
            }
        }

        v8::Local<v8::Array> m_propertyNames;
        v8::Local<v8::Value> m_propertyName;
        unsigned m_index;
        unsigned m_length;
    };

    bool doSerialize(v8::Handle<v8::Value> value)
    {
        if (value->IsUndefined())
            m_writer.writeUndefined();
        else if (value->IsNull())
            m_writer.writeNull();
        else if (value->IsTrue())
            m_writer.writeTrue();
        else if (value->IsFalse())
            m_writer.writeFalse();
        else if (value->IsInt32())
            m_writer.writeInt32(value->Int32Value());
        else if (value->IsNumber())
            m_writer.writeNumber(handleCast<v8::Number>(value)->Value());
        else if (value->IsString()) {
            v8::String::Utf8Value stringValue(value);
            m_writer.writeString(*stringValue, stringValue.length());
        } else if (value->IsArray()) {
            if (!checkComposite(value))
                return false;
            push(new ArrayState(handleCast<v8::Array>(value)));
        } else if (value->IsObject()) {
            if (!checkComposite(value))
                return false;
            push(new ObjectState(handleCast<v8::Object>(value)));
            // FIXME:
            // - check not a wrapper
            // - support File, ImageData, etc.
        }
        return true;
    }

    void push(StateBase* state)
    {
        state->setNextState(m_state);
        m_state = state;
        ++m_depth;
    }

    StateBase* top() { return m_state; }

    void pop()
    {
        if (!m_state)
            return;
        StateBase* top = m_state;
        m_state = top->nextState();
        delete top;
        --m_depth;
    }

    bool checkComposite(v8::Handle<v8::Value> composite)
    {
        if (m_depth > maxDepth)
            return false;
        if (!shouldCheckForCycles(m_depth))
            return true;
        for (StateBase* state = top(); state; state = state->nextState()) {
            if (state->composite() == composite)
                return false;
        }
        return true;
    }

    Writer& m_writer;
    StateBase* m_state;
    int m_depth;
};

// Reader is responsible for deserializing primitive types and
// restoring information about saved objects of composite types.
class Reader {
public:
    Reader(const char* buffer, int length)
        : m_buffer(buffer)
        , m_length(length)
        , m_position(0)
    {
        ASSERT(length >= 0);
    }

    bool isEof() const { return m_position >= m_length; }

    bool read(SerializationTag* tag, v8::Handle<v8::Value>* value, int* length)
    {
        uint32_t rawLength;
        if (!readTag(tag))
            return false;
        switch (*tag) {
        case InvalidTag:
            return false;
        case PaddingTag:
            break;
        case UndefinedTag:
            *value = v8::Undefined();
            break;
        case NullTag:
            *value = v8::Null();
            break;
        case TrueTag:
            *value = v8::True();
            break;
        case FalseTag:
            *value = v8::False();
            break;
        case StringTag:
            if (!readString(value))
                return false;
            break;
        case Int32Tag:
            if (!readInt32(value))
                return false;
            break;
        case NumberTag:
            if (!readNumber(value))
                return false;
            break;
        case ObjectTag:
        case ArrayTag:
            if (!doReadUint32(&rawLength))
                return false;
            *length = rawLength;
            break;
        }
        return true;
    }

private:
    bool readTag(SerializationTag* tag)
    {
        if (m_position >= m_length)
            return false;
        *tag = static_cast<SerializationTag>(m_buffer[m_position++]);
        return true;
    }

    bool readString(v8::Handle<v8::Value>* value)
    {
        uint32_t length;
        if (!doReadUint32(&length))
            return false;
        if (m_position + length > m_length)
            return false;
        *value = v8::String::New(m_buffer + m_position, length);
        m_position += length;
        return true;
    }

    bool readInt32(v8::Handle<v8::Value>* value)
    {
        uint32_t rawValue;
        if (!doReadUint32(&rawValue))
            return false;
        *value = v8::Integer::New(static_cast<int32_t>(ZigZag::decode(rawValue)));
        return true;
    }

    bool readNumber(v8::Handle<v8::Value>* value)
    {
        if (m_position + sizeof(double) > m_length)
            return false;
        double number;
        char* numberAsByteArray = reinterpret_cast<char*>(&number);
        for (unsigned i = 0; i < sizeof(double); ++i)
            numberAsByteArray[i] = m_buffer[m_position++];
        *value = v8::Number::New(number);
        return true;
    }

    bool doReadUint32(uint32_t* value)
    {
        *value = 0;
        char currentByte;
        int shift = 0;
        do {
            if (m_position >= m_length)
                return false;
            currentByte = m_buffer[m_position++];
            *value |= ((currentByte & varIntMask) << shift);
            shift += varIntShift;
        } while (currentByte & (1 << varIntShift));
        return true;
    }

    const char* m_buffer;
    const unsigned m_length;
    unsigned m_position;
};

class Deserializer {
public:
    explicit Deserializer(Reader& reader) : m_reader(reader)
    {
    }

    v8::Local<v8::Value> deserialize()
    {
        v8::HandleScope scope;
        while (!m_reader.isEof()) {
            if (!doDeserialize())
                return v8::Local<v8::Value>();
        }
        if (stackDepth() != 1)
            return v8::Local<v8::Value>();
        return scope.Close(element(0));
    }

private:
    bool doDeserialize()
    {
        SerializationTag tag;
        v8::Local<v8::Value> value;
        int length = 0;
        if (!m_reader.read(&tag, &value, &length))
            return false;
        if (!value.IsEmpty()) {
            push(value);
        } else if (tag == ObjectTag) {
            if (length > stackDepth())
                return false;
            v8::Local<v8::Object> object = v8::Object::New();
            for (int i = stackDepth() - length; i < stackDepth(); i += 2) {
                v8::Local<v8::Value> propertyName = element(i);
                v8::Local<v8::Value> propertyValue = element(i + 1);
                object->Set(propertyName, propertyValue);
            }
            pop(length);
            push(object);
        } else if (tag == ArrayTag) {
            if (length > stackDepth())
                return false;
            v8::Local<v8::Array> array = v8::Array::New(length);
            const int depth = stackDepth() - length;
            {
                v8::HandleScope scope;
                for (int i = 0; i < length; ++i)
                    array->Set(v8::Integer::New(i), element(depth + i));
            }
            pop(length);
            push(array);
        } else if (tag != PaddingTag)
            return false;
        return true;
    }

    void push(v8::Local<v8::Value> value) { m_stack.append(value); }

    void pop(unsigned length)
    {
        ASSERT(length <= m_stack.size());
        m_stack.shrink(m_stack.size() - length);
    }

    int stackDepth() const { return m_stack.size(); }

    v8::Local<v8::Value> element(unsigned index)
    {
        ASSERT(index < m_stack.size());
        return m_stack[index];
    }

    Reader& m_reader;
    Vector<v8::Local<v8::Value> > m_stack;
};

} // namespace

SerializedScriptValue::SerializedScriptValue(v8::Handle<v8::Value> value)
{
    Writer writer;
    Serializer serializer(writer);
    if (!serializer.serialize(value)) {
        // FIXME: throw exception
        return;
    }
    m_data = StringImpl::adopt(writer.data());
}

SerializedScriptValue::SerializedScriptValue(String data, StringDataMode mode)
{
    if (mode == WireData)
        m_data = data;
    else {
        ASSERT(mode == StringValue);
        RefPtr<SharedBuffer> buffer = utf8Buffer(data);
        Writer writer;
        writer.writeString(buffer->data(), buffer->size());
        m_data = StringImpl::adopt(writer.data());
    }
}

v8::Local<v8::Value> SerializedScriptValue::deserialize()
{
    if (!m_data.impl())
        return v8::Local<v8::Value>();
    COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
    Reader reader(reinterpret_cast<const char*>(m_data.impl()->characters()), 2 * m_data.length());
    Deserializer deserializer(reader);
    return deserializer.deserialize();
}

} // namespace WebCore
