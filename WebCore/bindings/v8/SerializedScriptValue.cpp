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

#include "Blob.h"
#include "ByteArray.h"
#include "CanvasPixelArray.h"
#include "File.h"
#include "FileList.h"
#include "ImageData.h"
#include "SharedBuffer.h"
#include "V8Blob.h"
#include "V8File.h"
#include "V8FileList.h"
#include "V8ImageData.h"
#include "V8Proxy.h"

#include <v8.h>
#include <wtf/Assertions.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

// FIXME:
// - catch V8 exceptions
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
    Uint32Tag = 'U',
    DateTag = 'D',
    NumberTag = 'N',
    BlobTag = 'b',
    FileTag = 'f',
    FileListTag = 'l',
    ImageDataTag = '#',
    ArrayTag = '[',
    ObjectTag = '{',
    SparseArrayTag = '@',
};

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
    Writer()
        : m_position(0)
    {
    }

    // Write functions for primitive types.

    void writeUndefined() { append(UndefinedTag); }

    void writeNull() { append(NullTag); }

    void writeTrue() { append(TrueTag); }

    void writeFalse() { append(FalseTag); }

    void writeString(const char* data, int length)
    {
        ASSERT(length >= 0);
        append(StringTag);
        doWriteString(data, length);
    }

    void writeWebCoreString(const String& string)
    {
        // Uses UTF8 encoding so we can read it back as either V8 or
        // WebCore string.
        append(StringTag);
        doWriteWebCoreString(string);
    }

    void writeInt32(int32_t value)
    {
        append(Int32Tag);
        doWriteUint32(ZigZag::encode(static_cast<uint32_t>(value)));
    }

    void writeUint32(uint32_t value)
    {
        append(Uint32Tag);
        doWriteUint32(value);
    }

    void writeDate(double numberValue)
    {
        append(DateTag);
        doWriteNumber(numberValue);
    }

    void writeNumber(double number)
    {
        append(NumberTag);
        doWriteNumber(number);
    }

    void writeBlob(const String& path)
    {
        append(BlobTag);
        doWriteWebCoreString(path);
    }

    void writeFile(const String& path)
    {
        append(FileTag);
        doWriteWebCoreString(path);
    }

    void writeFileList(const FileList& fileList)
    {
        append(FileListTag);
        uint32_t length = fileList.length();
        doWriteUint32(length);
        for (unsigned i = 0; i < length; ++i)
            doWriteWebCoreString(fileList.item(i)->path());
    }

    void writeImageData(uint32_t width, uint32_t height, const uint8_t* pixelData, uint32_t pixelDataLength)
    {
        append(ImageDataTag);
        doWriteUint32(width);
        doWriteUint32(height);
        doWriteUint32(pixelDataLength);
        append(pixelData, pixelDataLength);
    }

    void writeArray(uint32_t length)
    {
        append(ArrayTag);
        doWriteUint32(length);
    }

    void writeObject(uint32_t numProperties)
    {
        append(ObjectTag);
        doWriteUint32(numProperties);
    }

    void writeSparseArray(uint32_t numProperties, uint32_t length)
    {
        append(SparseArrayTag);
        doWriteUint32(numProperties);
        doWriteUint32(length);
    }

    Vector<BufferValueType>& data()
    {
        fillHole();
        return m_buffer;
    }

private:
    void doWriteString(const char* data, int length)
    {
        doWriteUint32(static_cast<uint32_t>(length));
        append(reinterpret_cast<const uint8_t*>(data), length);
    }

    void doWriteWebCoreString(const String& string)
    {
        RefPtr<SharedBuffer> buffer = utf8Buffer(string);
        doWriteString(buffer->data(), buffer->size());
    }

    void doWriteUint32(uint32_t value)
    {
        while (true) {
            uint8_t b = (value & varIntMask);
            value >>= varIntShift;
            if (!value) {
                append(b);
                break;
            }
            append(b | (1 << varIntShift));
        }
    }

    void doWriteNumber(double number)
    {
        append(reinterpret_cast<uint8_t*>(&number), sizeof(number));
    }

    void append(SerializationTag tag)
    {
        append(static_cast<uint8_t>(tag));
    }

    void append(uint8_t b)
    {
        ensureSpace(1);
        *byteAt(m_position++) = b;
    }

    void append(const uint8_t* data, int length)
    {
        ensureSpace(length);
        memcpy(byteAt(m_position), data, length);
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
            *byteAt(m_position) = static_cast<uint8_t>(PaddingTag);
    }

    uint8_t* byteAt(int position) { return reinterpret_cast<uint8_t*>(m_buffer.data()) + position; }

    Vector<BufferValueType> m_buffer;
    unsigned m_position;
};

class Serializer {
    class StateBase;
public:
    explicit Serializer(Writer& writer)
        : m_writer(writer)
        , m_depth(0)
        , m_hasError(false)
    {
    }

    bool serialize(v8::Handle<v8::Value> value)
    {
        v8::HandleScope scope;
        StateBase* state = doSerialize(value, 0);
        while (state)
            state = state->advance(*this);
        return !m_hasError;
    }

    // Functions used by serialization states.

    StateBase* doSerialize(v8::Handle<v8::Value> value, StateBase* next);

    StateBase* writeArray(uint32_t length, StateBase* state)
    {
        m_writer.writeArray(length);
        return pop(state);
    }

    StateBase* writeObject(uint32_t numProperties, StateBase* state)
    {
        m_writer.writeObject(numProperties);
        return pop(state);
    }

    StateBase* writeSparseArray(uint32_t numProperties, uint32_t length, StateBase* state)
    {
        m_writer.writeSparseArray(numProperties, length);
        return pop(state);
    }

private:
    class StateBase : public Noncopyable {
    public:
        virtual ~StateBase() { }

        // Link to the next state to form a stack.
        StateBase* nextState() { return m_next; }

        // Composite object we're processing in this state.
        v8::Handle<v8::Value> composite() { return m_composite; }

        // Serializes (a part of) the current composite and returns
        // the next state to process or null when this is the final
        // state.
        virtual StateBase* advance(Serializer&) = 0;

    protected:
        StateBase(v8::Handle<v8::Value> composite, StateBase* next)
            : m_composite(composite)
            , m_next(next)
        {
        }

    private:
        v8::Handle<v8::Value> m_composite;
        StateBase* m_next;
    };

    // Dummy state that is used to signal serialization errors.
    class ErrorState : public StateBase {
    public:
        ErrorState()
            : StateBase(v8::Handle<v8::Value>(), 0)
        {
        }

        virtual StateBase* advance(Serializer&)
        {
            delete this;
            return 0;
        }
    };

    template <typename T>
    class State : public StateBase {
    public:
        v8::Handle<T> composite() { return v8::Handle<T>::Cast(StateBase::composite()); }

    protected:
        State(v8::Handle<T> composite, StateBase* next)
            : StateBase(composite, next)
        {
        }
    };

#if 0
    // Currently unused, see comment in newArrayState.
    class ArrayState : public State<v8::Array> {
    public:
        ArrayState(v8::Handle<v8::Array> array, StateBase* next)
            : State<v8::Array>(array, next)
            , m_index(-1)
        {
        }

        virtual StateBase* advance(Serializer& serializer)
        {
            ++m_index;
            for (; m_index < composite()->Length(); ++m_index) {
                if (StateBase* newState = serializer.doSerialize(composite()->Get(m_index), this))
                    return newState;
            }
            return serializer.writeArray(composite()->Length(), this);
        }

    private:
        unsigned m_index;
    };
#endif

    class AbstractObjectState : public State<v8::Object> {
    public:
        AbstractObjectState(v8::Handle<v8::Object> object, StateBase* next)
            : State<v8::Object>(object, next)
            , m_propertyNames(object->GetPropertyNames())
            , m_index(-1)
            , m_numSerializedProperties(0)
            , m_nameDone(false)
        {
        }

        virtual StateBase* advance(Serializer& serializer)
        {
            ++m_index;
            for (; m_index < m_propertyNames->Length(); ++m_index) {
                if (m_propertyName.IsEmpty()) {
                    v8::Local<v8::Value> propertyName = m_propertyNames->Get(m_index);
                    if ((propertyName->IsString() && composite()->HasRealNamedProperty(propertyName.As<v8::String>()))
                        || (propertyName->IsUint32() && composite()->HasRealIndexedProperty(propertyName->Uint32Value()))) {
                        m_propertyName = propertyName;
                    } else
                        continue;
                }
                ASSERT(!m_propertyName.IsEmpty());
                if (!m_nameDone) {
                    m_nameDone = true;
                    if (StateBase* newState = serializer.doSerialize(m_propertyName, this))
                        return newState;
                }
                v8::Local<v8::Value> value = composite()->Get(m_propertyName);
                m_nameDone = false;
                m_propertyName.Clear();
                ++m_numSerializedProperties;
                if (StateBase* newState = serializer.doSerialize(value, this))
                    return newState;
            }
            return objectDone(m_numSerializedProperties, serializer);
        }

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer&) = 0;

    private:
        v8::Local<v8::Array> m_propertyNames;
        v8::Local<v8::Value> m_propertyName;
        unsigned m_index;
        unsigned m_numSerializedProperties;
        bool m_nameDone;
    };

    class ObjectState : public AbstractObjectState {
    public:
        ObjectState(v8::Handle<v8::Object> object, StateBase* next)
            : AbstractObjectState(object, next)
        {
        }

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer& serializer)
        {
            return serializer.writeObject(numProperties, this);
        }
    };

    class SparseArrayState : public AbstractObjectState {
    public:
        SparseArrayState(v8::Handle<v8::Array> array, StateBase* next)
            : AbstractObjectState(array, next)
        {
        }

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer& serializer)
        {
            return serializer.writeSparseArray(numProperties, composite().As<v8::Array>()->Length(), this);
        }
    };

    StateBase* push(StateBase* state)
    {
        ASSERT(state);
        ++m_depth;
        return checkComposite(state) ? state : handleError(state);
    }

    StateBase* pop(StateBase* state)
    {
        ASSERT(state);
        --m_depth;
        StateBase* next = state->nextState();
        delete state;
        return next;
    }

    StateBase* handleError(StateBase* state)
    {
        m_hasError = true;
        while (state) {
            StateBase* tmp = state->nextState();
            delete state;
            state = tmp;
        }
        return new ErrorState;
    }

    bool checkComposite(StateBase* top)
    {
        ASSERT(top);
        if (m_depth > maxDepth)
            return false;
        if (!shouldCheckForCycles(m_depth))
            return true;
        v8::Handle<v8::Value> composite = top->composite();
        for (StateBase* state = top->nextState(); state; state = state->nextState()) {
            if (state->composite() == composite)
                return false;
        }
        return true;
    }

    void writeString(v8::Handle<v8::Value> value)
    {
        v8::String::Utf8Value stringValue(value);
        m_writer.writeString(*stringValue, stringValue.length());
    }

    void writeBlob(v8::Handle<v8::Value> value)
    {
        Blob* blob = V8Blob::toNative(value.As<v8::Object>());
        if (!blob)
            return;
        m_writer.writeBlob(blob->path());
    }

    void writeFile(v8::Handle<v8::Value> value)
    {
        File* file = V8File::toNative(value.As<v8::Object>());
        if (!file)
            return;
        m_writer.writeFile(file->path());
    }

    void writeFileList(v8::Handle<v8::Value> value)
    {
        FileList* fileList = V8FileList::toNative(value.As<v8::Object>());
        if (!fileList)
            return;
        m_writer.writeFileList(*fileList);
    }

    void writeImageData(v8::Handle<v8::Value> value)
    {
        ImageData* imageData = V8ImageData::toNative(value.As<v8::Object>());
        if (!imageData)
            return;
        WTF::ByteArray* pixelArray = imageData->data()->data();
        m_writer.writeImageData(imageData->width(), imageData->height(), pixelArray->data(), pixelArray->length());
    }

    static StateBase* newArrayState(v8::Handle<v8::Array> array, StateBase* next)
    {
        // FIXME: use plain Array state when we can quickly check that
        // an array is not sparse and has only indexed properties.
        return new SparseArrayState(array, next);
    }

    static StateBase* newObjectState(v8::Handle<v8::Object> object, StateBase* next)
    {
        // FIXME:
        // - check not a wrapper
        // - support File, etc.
        return new ObjectState(object, next);
    }

    Writer& m_writer;
    int m_depth;
    bool m_hasError;
};

Serializer::StateBase* Serializer::doSerialize(v8::Handle<v8::Value> value, StateBase* next)
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
    else if (value->IsUint32())
        m_writer.writeUint32(value->Uint32Value());
    else if (value->IsDate())
        m_writer.writeDate(value->NumberValue());
    else if (value->IsNumber())
        m_writer.writeNumber(value.As<v8::Number>()->Value());
    else if (value->IsString())
        writeString(value);
    else if (value->IsArray())
        return push(newArrayState(value.As<v8::Array>(), next));
    else if (V8File::HasInstance(value))
        writeFile(value);
    else if (V8Blob::HasInstance(value))
        writeBlob(value);
    else if (V8FileList::HasInstance(value))
        writeFileList(value);
    else if (V8ImageData::HasInstance(value))
        writeImageData(value);
    else if (value->IsObject())
        return push(newObjectState(value.As<v8::Object>(), next));
    return 0;
}

// Interface used by Reader to create objects of composite types.
class CompositeCreator {
public:
    virtual ~CompositeCreator() { }

    virtual bool createArray(uint32_t length, v8::Handle<v8::Value>* value) = 0;
    virtual bool createObject(uint32_t numProperties, v8::Handle<v8::Value>* value) = 0;
    virtual bool createSparseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>* value) = 0;
};

// Reader is responsible for deserializing primitive types and
// restoring information about saved objects of composite types.
class Reader {
public:
    Reader(const uint8_t* buffer, int length)
        : m_buffer(buffer)
        , m_length(length)
        , m_position(0)
    {
        ASSERT(length >= 0);
    }

    bool isEof() const { return m_position >= m_length; }

    bool read(v8::Handle<v8::Value>* value, CompositeCreator& creator)
    {
        SerializationTag tag;
        if (!readTag(&tag))
            return false;
        switch (tag) {
        case InvalidTag:
            return false;
        case PaddingTag:
            return true;
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
        case Uint32Tag:
            if (!readUint32(value))
                return false;
            break;
        case DateTag:
            if (!readDate(value))
                return false;
            break;
        case NumberTag:
            if (!readNumber(value))
                return false;
            break;
        case BlobTag:
            if (!readBlob(value))
                return false;
            break;
        case FileTag:
            if (!readFile(value))
                return false;
            break;
        case FileListTag:
            if (!readFileList(value))
                return false;
            break;
        case ImageDataTag:
            if (!readImageData(value))
                return false;
            break;
        case ArrayTag: {
            uint32_t length;
            if (!doReadUint32(&length))
                return false;
            if (!creator.createArray(length, value))
                return false;
            break;
        }
        case ObjectTag: {
            uint32_t numProperties;
            if (!doReadUint32(&numProperties))
                return false;
            if (!creator.createObject(numProperties, value))
                return false;
            break;
        }
        case SparseArrayTag: {
            uint32_t numProperties;
            uint32_t length;
            if (!doReadUint32(&numProperties))
                return false;
            if (!doReadUint32(&length))
                return false;
            if (!creator.createSparseArray(numProperties, length, value))
                return false;
            break;
        }
        default:
            return false;
        }
        return !value->IsEmpty();
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
        *value = v8::String::New(reinterpret_cast<const char*>(m_buffer + m_position), length);
        m_position += length;
        return true;
    }

    bool readWebCoreString(String* string)
    {
        uint32_t length;
        if (!doReadUint32(&length))
            return false;
        if (m_position + length > m_length)
            return false;
        *string = String::fromUTF8(reinterpret_cast<const char*>(m_buffer + m_position), length);
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

    bool readUint32(v8::Handle<v8::Value>* value)
    {
        uint32_t rawValue;
        if (!doReadUint32(&rawValue))
            return false;
        *value = v8::Integer::New(rawValue);
        return true;
    }

    bool readDate(v8::Handle<v8::Value>* value)
    {
        double numberValue;
        if (!doReadNumber(&numberValue))
            return false;
        *value = v8::Date::New(numberValue);
        return true;
    }

    bool readNumber(v8::Handle<v8::Value>* value)
    {
        double number;
        if (!doReadNumber(&number))
            return false;
        *value = v8::Number::New(number);
        return true;
    }

    bool readImageData(v8::Handle<v8::Value>* value)
    {
        uint32_t width;
        uint32_t height;
        uint32_t pixelDataLength;
        if (!doReadUint32(&width))
            return false;
        if (!doReadUint32(&height))
            return false;
        if (!doReadUint32(&pixelDataLength))
            return false;
        if (m_position + pixelDataLength > m_length)
            return false;
        PassRefPtr<ImageData> imageData = ImageData::create(width, height);
        WTF::ByteArray* pixelArray = imageData->data()->data();
        ASSERT(pixelArray);
        ASSERT(pixelArray->length() >= pixelDataLength);
        memcpy(pixelArray->data(), m_buffer + m_position, pixelDataLength);
        m_position += pixelDataLength;
        *value = toV8(imageData);
        return true;
    }

    bool readBlob(v8::Handle<v8::Value>* value)
    {
        String path;
        if (!readWebCoreString(&path))
            return false;
        PassRefPtr<Blob> blob = Blob::create(path);
        *value = toV8(blob);
        return true;
    }

    bool readFile(v8::Handle<v8::Value>* value)
    {
        String path;
        if (!readWebCoreString(&path))
            return false;
        PassRefPtr<File> file = File::create(path);
        *value = toV8(file);
        return true;
    }

    bool readFileList(v8::Handle<v8::Value>* value)
    {
        uint32_t length;
        if (!doReadUint32(&length))
            return false;
        PassRefPtr<FileList> fileList = FileList::create();
        for (unsigned i = 0; i < length; ++i) {
            String path;
            if (!readWebCoreString(&path))
                return false;
            fileList->append(File::create(path));
        }
        *value = toV8(fileList);
        return true;
    }

    bool doReadUint32(uint32_t* value)
    {
        *value = 0;
        uint8_t currentByte;
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

    bool doReadNumber(double* number)
    {
        if (m_position + sizeof(double) > m_length)
            return false;
        uint8_t* numberAsByteArray = reinterpret_cast<uint8_t*>(number);
        for (unsigned i = 0; i < sizeof(double); ++i)
            numberAsByteArray[i] = m_buffer[m_position++];
        return true;
    }

    const uint8_t* m_buffer;
    const unsigned m_length;
    unsigned m_position;
};

class Deserializer : public CompositeCreator {
public:
    explicit Deserializer(Reader& reader)
        : m_reader(reader)
    {
    }

    v8::Handle<v8::Value> deserialize()
    {
        v8::HandleScope scope;
        while (!m_reader.isEof()) {
            if (!doDeserialize())
                return v8::Null();
        }
        if (stackDepth() != 1)
            return v8::Null();
        return scope.Close(element(0));
    }

    virtual bool createArray(uint32_t length, v8::Handle<v8::Value>* value)
    {
        if (length > stackDepth())
            return false;
        v8::Local<v8::Array> array = v8::Array::New(length);
        if (array.IsEmpty())
            return false;
        const int depth = stackDepth() - length;
        for (unsigned i = 0; i < length; ++i)
            array->Set(i, element(depth + i));
        pop(length);
        *value = array;
        return true;
    }

    virtual bool createObject(uint32_t numProperties, v8::Handle<v8::Value>* value)
    {
        v8::Local<v8::Object> object = v8::Object::New();
        if (object.IsEmpty())
            return false;
        return initializeObject(object, numProperties, value);
    }

    virtual bool createSparseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>* value)
    {
        v8::Local<v8::Array> array = v8::Array::New(length);
        if (array.IsEmpty())
            return false;
        return initializeObject(array, numProperties, value);
    }

private:
    bool initializeObject(v8::Handle<v8::Object> object, uint32_t numProperties, v8::Handle<v8::Value>* value)
    {
        unsigned length = 2 * numProperties;
        if (length > stackDepth())
            return false;
        for (unsigned i = stackDepth() - length; i < stackDepth(); i += 2) {
            v8::Local<v8::Value> propertyName = element(i);
            v8::Local<v8::Value> propertyValue = element(i + 1);
            object->Set(propertyName, propertyValue);
        }
        pop(length);
        *value = object;
        return true;
    }

    bool doDeserialize()
    {
        v8::Local<v8::Value> value;
        if (!m_reader.read(&value, *this))
            return false;
        if (!value.IsEmpty())
            push(value);
        return true;
    }

    void push(v8::Local<v8::Value> value) { m_stack.append(value); }

    void pop(unsigned length)
    {
        ASSERT(length <= m_stack.size());
        m_stack.shrink(m_stack.size() - length);
    }

    unsigned stackDepth() const { return m_stack.size(); }

    v8::Local<v8::Value> element(unsigned index)
    {
        ASSERT(index < m_stack.size());
        return m_stack[index];
    }

    Reader& m_reader;
    Vector<v8::Local<v8::Value> > m_stack;
};

} // namespace

SerializedScriptValue::SerializedScriptValue(v8::Handle<v8::Value> value, bool& didThrow)
{
    didThrow = false;
    Writer writer;
    Serializer serializer(writer);
    if (!serializer.serialize(value)) {
        throwError(NOT_SUPPORTED_ERR);
        didThrow = true;
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
        Writer writer;
        writer.writeWebCoreString(data);
        m_data = StringImpl::adopt(writer.data());
    }
}

v8::Handle<v8::Value> SerializedScriptValue::deserialize()
{
    if (!m_data.impl())
        return v8::Null();
    COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
    Reader reader(reinterpret_cast<const uint8_t*>(m_data.impl()->characters()), 2 * m_data.length());
    Deserializer deserializer(reader);
    return deserializer.deserialize();
}

} // namespace WebCore
