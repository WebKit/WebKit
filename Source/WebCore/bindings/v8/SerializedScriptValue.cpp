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
#include "ExceptionCode.h"
#include "File.h"
#include "FileList.h"
#include "ImageData.h"
#include "SharedBuffer.h"
#include "V8Binding.h"
#include "V8Blob.h"
#include "V8File.h"
#include "V8FileList.h"
#include "V8ImageData.h"
#include "V8Proxy.h"
#include "V8Utilities.h"

#include <wtf/Assertions.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

// FIXME: consider crashing in debug mode on deserialization errors

namespace WebCore {

namespace {

typedef UChar BufferValueType;

// Serialization format is a sequence of (tag, optional data)
// pairs. Tag always takes exactly one byte.
enum SerializationTag {
    InvalidTag = '!', // Causes deserialization to fail.
    PaddingTag = '\0', // Is ignored (but consumed).
    UndefinedTag = '_', // -> <undefined>
    NullTag = '0', // -> <null>
    TrueTag = 'T', // -> <true>
    FalseTag = 'F', // -> <false>
    StringTag = 'S', // string:RawString -> string
    Int32Tag = 'I', // value:ZigZag-encoded int32 -> Integer
    Uint32Tag = 'U', // value:uint32_t -> Integer
    DateTag = 'D', // value:double -> Date (ref)
    MessagePortTag = 'M', // index:int -> MessagePort. Fills the result with transferred MessagePort.
    NumberTag = 'N', // value:double -> Number
    BlobTag = 'b', // url:WebCoreString, type:WebCoreString, size:uint64_t -> Blob (ref)
    FileTag = 'f', // file:RawFile -> File (ref)
    FileListTag = 'l', // length:uint32_t, files:RawFile[length] -> FileList (ref)
    ImageDataTag = '#', // width:uint32_t, height:uint32_t, pixelDataLength:uint32_t, data:byte[pixelDataLength] -> ImageData (ref)
    ObjectTag = '{', // numProperties:uint32_t -> pops the last object from the open stack;
                     //                           fills it with the last numProperties name,value pairs pushed onto the deserialization stack
    SparseArrayTag = '@', // numProperties:uint32_t, length:uint32_t -> pops the last object from the open stack;
                          //                                            fills it with the last numProperties name,value pairs pushed onto the deserialization stack
    DenseArrayTag = '$', // numProperties:uint32_t, length:uint32_t -> pops the last object from the open stack;
                         //                                            fills it with the last length elements and numProperties name,value pairs pushed onto deserialization stack
    RegExpTag = 'R', // pattern:RawString, flags:uint32_t -> RegExp (ref)
    ArrayBufferTag = 'B', // byteLength:uint32_t, data:byte[byteLength] -> ArrayBuffer (ref)
    ArrayBufferTransferTag = 't', // index:uint32_t -> ArrayBuffer. For ArrayBuffer transfer
    ArrayBufferViewTag = 'V', // subtag:byte, byteOffset:uint32_t, byteLength:uint32_t -> ArrayBufferView (ref). Consumes an ArrayBuffer from the top of the deserialization stack.
    ObjectReferenceTag = '^', // ref:uint32_t -> reference table[ref]
    GenerateFreshObjectTag = 'o', // -> empty object allocated an object ID and pushed onto the open stack (ref)
    GenerateFreshSparseArrayTag = 'a', // length:uint32_t -> empty array[length] allocated an object ID and pushed onto the open stack (ref)
    GenerateFreshDenseArrayTag = 'A', // length:uint32_t -> empty array[length] allocated an object ID and pushed onto the open stack (ref)
    ReferenceCountTag = '?', // refTableSize:uint32_t -> If the reference table is not refTableSize big, fails.
    StringObjectTag = 's', //  string:RawString -> new String(string) (ref)
    NumberObjectTag = 'n', // value:double -> new Number(value) (ref)
    TrueObjectTag = 'y', // new Boolean(true) (ref)
    FalseObjectTag = 'x', // new Boolean(false) (ref)
    VersionTag = 0xFF // version:uint32_t -> Uses this as the file version.
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
class Writer {
    WTF_MAKE_NONCOPYABLE(Writer);
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

    void writeBlob(const String& url, const String& type, unsigned long long size)
    {
        append(BlobTag);
        doWriteWebCoreString(url);
        doWriteWebCoreString(type);
        doWriteUint64(size);
    }

    void writeFile(const String& path, const String& url, const String& type)
    {
        append(FileTag);
        doWriteWebCoreString(path);
        doWriteWebCoreString(url);
        doWriteWebCoreString(type);
    }

    void writeFileList(const FileList& fileList)
    {
        append(FileListTag);
        uint32_t length = fileList.length();
        doWriteUint32(length);
        for (unsigned i = 0; i < length; ++i) {
            doWriteWebCoreString(fileList.item(i)->path());
            doWriteWebCoreString(fileList.item(i)->url().string());
            doWriteWebCoreString(fileList.item(i)->type());
        }
    }

    void writeImageData(uint32_t width, uint32_t height, const uint8_t* pixelData, uint32_t pixelDataLength)
    {
        append(ImageDataTag);
        doWriteUint32(width);
        doWriteUint32(height);
        doWriteUint32(pixelDataLength);
        append(pixelData, pixelDataLength);
    }

    void writeRegExp(v8::Local<v8::String> pattern, v8::RegExp::Flags flags)
    {
        append(RegExpTag);
        v8::String::Utf8Value patternUtf8Value(pattern);
        doWriteString(*patternUtf8Value, patternUtf8Value.length());
        doWriteUint32(static_cast<uint32_t>(flags));
    }

    void writeArray(uint32_t length)
    {
        append(ArrayTag);
        doWriteUint32(length);
    }

    void writeTransferredArrayBuffer(uint32_t index)
    {
        append(ArrayBufferTransferTag);
        doWriteUint32(index);
    }

    void writeObjectReference(uint32_t reference)
    {
        append(ObjectReferenceTag);
        doWriteUint32(reference);
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

    template<class T>
    void doWriteUintHelper(T value)
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

    void doWriteUint32(uint32_t value)
    {
        doWriteUintHelper(value);
    }

    void doWriteUint64(uint64_t value)
    {
        doWriteUintHelper(value);
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
    enum Status {
        Success,
        InputError,
        DataCloneError,
        InvalidStateError,
        JSException,
        JSFailure
    };

    Serializer(Writer& writer, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, v8::TryCatch& tryCatch)
        : m_writer(writer)
        , m_tryCatch(tryCatch)
        , m_depth(0)
        , m_status(Success)
    {
        ASSERT(!tryCatch.HasCaught());
        if (messagePorts) {
            for (size_t i = 0; i < messagePorts->size(); i++)
                m_transferredMessagePorts.set(V8MessagePort::wrap(messagePorts->at(i).get()), i);
        }
        if (arrayBuffers) {
            for (size_t i = 0; i < arrayBuffers->size(); i++)  {
                v8::Handle<v8::Object> v8ArrayBuffer = V8ArrayBuffer::wrap(arrayBuffers->at(i).get());
                // Coalesce multiple occurences of the same buffer to the first index.
                if (!m_transferredArrayBuffers.contains(v8ArrayBuffer))
                    m_transferredArrayBuffers.set(v8ArrayBuffer, i);
            }
        }
    }

    Status serialize(v8::Handle<v8::Value> value)
    {
        v8::HandleScope scope;
        StateBase* state = doSerialize(value, 0);
        while (state)
            state = state->advance(*this);
        return m_status;
    }

    // Functions used by serialization states.

    StateBase* doSerialize(v8::Handle<v8::Value> value, StateBase* next);

    StateBase* checkException(StateBase* state)
    {
        return m_tryCatch.HasCaught() ? handleError(JSException, state) : 0;
    }

    StateBase* reportFailure(StateBase* state)
    {
        return handleError(JSFailure, state);
    }

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
    class StateBase {
        WTF_MAKE_NONCOPYABLE(StateBase);
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
                v8::Handle<v8::Value> value = composite()->Get(m_index);
                if (StateBase* newState = serializer.checkException(this))
                    return newState;
                if (StateBase* newState = serializer.doSerialize(value, this))
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
            , m_index(0)
            , m_numSerializedProperties(0)
            , m_nameDone(false)
        {
        }

        virtual StateBase* advance(Serializer& serializer)
        {
            if (!m_index) {
                m_propertyNames = composite()->GetPropertyNames();
                if (StateBase* newState = serializer.checkException(this))
                    return newState;
                if (m_propertyNames.IsEmpty())
                    return serializer.reportFailure(this);
            }
            while (m_index < m_propertyNames->Length()) {
                if (!m_nameDone) {
                    v8::Local<v8::Value> propertyName = m_propertyNames->Get(m_index);
                    if (StateBase* newState = serializer.checkException(this))
                        return newState;
                    if (propertyName.IsEmpty())
                        return serializer.reportFailure(this);
                    bool hasStringProperty = propertyName->IsString() && composite()->HasRealNamedProperty(propertyName.As<v8::String>());
                    if (StateBase* newState = serializer.checkException(this))
                        return newState;
                    bool hasIndexedProperty = !hasStringProperty && propertyName->IsUint32() && composite()->HasRealIndexedProperty(propertyName->Uint32Value());
                    if (StateBase* newState = serializer.checkException(this))
                        return newState;
                    if (hasStringProperty || hasIndexedProperty)
                        m_propertyName = propertyName;
                    else {
                        ++m_index;
                        continue;
                    }
                }
                ASSERT(!m_propertyName.IsEmpty());
                if (!m_nameDone) {
                    m_nameDone = true;
                    if (StateBase* newState = serializer.doSerialize(m_propertyName, this))
                        return newState;
                }
                v8::Local<v8::Value> value = composite()->Get(m_propertyName);
                if (StateBase* newState = serializer.checkException(this))
                    return newState;
                m_nameDone = false;
                m_propertyName.Clear();
                ++m_index;
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
        return checkComposite(state) ? state : handleError(InputError, state);
    }

    StateBase* pop(StateBase* state)
    {
        ASSERT(state);
        --m_depth;
        StateBase* next = state->nextState();
        delete state;
        return next;
    }

    StateBase* handleError(Status errorStatus, StateBase* state)
    {
        ASSERT(errorStatus != Success);
        m_status = errorStatus;
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
        m_writer.writeBlob(blob->url().string(), blob->type(), blob->size());
    }

    void writeFile(v8::Handle<v8::Value> value)
    {
        File* file = V8File::toNative(value.As<v8::Object>());
        if (!file)
            return;
        m_writer.writeFile(file->path(), file->url().string(), file->type());
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

    void writeRegExp(v8::Handle<v8::Value> value)
    {
        v8::Handle<v8::RegExp> regExp = value.As<v8::RegExp>();
        m_writer.writeRegExp(regExp->GetSource(), regExp->GetFlags());
    }

    static StateBase* newArrayState(v8::Handle<v8::Array> array, StateBase* next)
    {
        ASSERT(!object.IsEmpty());
        ArrayBufferView* arrayBufferView = V8ArrayBufferView::toNative(object);
        if (!arrayBufferView)
            return 0;
        if (!arrayBufferView->buffer())
            return handleError(DataCloneError, next);
        v8::Handle<v8::Value> underlyingBuffer = toV8(arrayBufferView->buffer());
        if (underlyingBuffer.IsEmpty())
            return handleError(DataCloneError, next);
        StateBase* stateOut = doSerialize(underlyingBuffer, 0);
        if (stateOut)
            return handleError(DataCloneError, next);
        m_writer.writeArrayBufferView(*arrayBufferView);
        // This should be safe: we serialize something that we know to be a wrapper (see
        // the toV8 call above), so the call to doSerialize above should neither cause
        // the stack to overflow nor should it have the potential to reach this
        // ArrayBufferView again. We do need to grey the underlying buffer before we grey
        // its view, however; ArrayBuffers may be shared, so they need to be given reference IDs,
        // and an ArrayBufferView cannot be constructed without a corresponding ArrayBuffer
        // (or without an additional tag that would allow us to do two-stage construction
        // like we do for Objects and Arrays).
        greyObject(object);
        return 0;
    }

    StateBase* writeArrayBuffer(v8::Handle<v8::Value> value, StateBase* next)
    {
        ArrayBuffer* arrayBuffer = V8ArrayBuffer::toNative(value.As<v8::Object>());
        if (!arrayBuffer)
            return 0;
        if (arrayBuffer->isNeutered())
            return handleError(InvalidStateError, next);
        ASSERT(!m_transferredArrayBuffers.contains(value.As<v8::Object>()));
        m_writer.writeArrayBuffer(*arrayBuffer);
        return 0;
    }

    StateBase* writeTransferredArrayBuffer(v8::Handle<v8::Value> value, uint32_t index, StateBase* next)
    {
        ArrayBuffer* arrayBuffer = V8ArrayBuffer::toNative(value.As<v8::Object>());
        if (!arrayBuffer)
            return 0;
        if (arrayBuffer->isNeutered())
            return handleError(DataCloneError, next);
        m_writer.writeTransferredArrayBuffer(index);
        return 0;
    }

    static bool shouldSerializeDensely(uint32_t length, uint32_t propertyCount) 
    {
        // Let K be the cost of serializing all property values that are there
        // Cost of serializing sparsely: 5*propertyCount + K (5 bytes per uint32_t key)
        // Cost of serializing densely: K + 1*(length - propertyCount) (1 byte for all properties that are not there)
        // so densely is better than sparsly whenever 6*propertyCount > length
        return 6 * propertyCount >= length;
    }

    StateBase* startArrayState(v8::Handle<v8::Array> array, StateBase* next)
    {
        v8::Handle<v8::Array> propertyNames = array->GetPropertyNames();
        if (StateBase* newState = checkException(next))
            return newState;
        uint32_t length = array->Length();

        if (shouldSerializeDensely(length, propertyNames->Length())) {
            m_writer.writeGenerateFreshDenseArray(length);
            return push(new DenseArrayState(array, propertyNames, next));
        }

        m_writer.writeGenerateFreshSparseArray(length);
        return push(new SparseArrayState(array, propertyNames, next));
    }

    StateBase* startObjectState(v8::Handle<v8::Object> object, StateBase* next)
    {
        m_writer.writeGenerateFreshObject();
        // FIXME: check not a wrapper
        return new ObjectState(object, next);
    }

    Writer& m_writer;
    v8::TryCatch& m_tryCatch;
    int m_depth;
    Status m_status;
    typedef V8ObjectMap<v8::Object, uint32_t> ObjectPool;
    ObjectPool m_objectPool;
    ObjectPool m_transferredMessagePorts;
    ObjectPool m_transferredArrayBuffers;
    uint32_t m_nextObjectReference;
};

Serializer::StateBase* Serializer::doSerialize(v8::Handle<v8::Value> value, StateBase* next)
{
    if (m_execDepth + (next ? next->execDepth() : 0) > 1) {
        m_writer.writeNull();
        return 0;
    }
    m_writer.writeReferenceCount(m_nextObjectReference);
    uint32_t objectReference;
    uint32_t arrayBufferIndex;
    if ((value->IsObject() || value->IsDate() || value->IsRegExp())
        && m_objectPool.tryGet(value.As<v8::Object>(), &objectReference)) {
        // Note that IsObject() also detects wrappers (eg, it will catch the things
        // that we grey and write below).
        ASSERT(!value->IsString());
        m_writer.writeObjectReference(objectReference);
    } else if (value.IsEmpty())
        return reportFailure(next);
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
    else if (V8MessagePort::HasInstance(value)) {
        uint32_t messagePortIndex;
        if (m_transferredMessagePorts.tryGet(value.As<v8::Object>(), &messagePortIndex))
                m_writer.writeTransferredMessagePort(messagePortIndex);
            else
                return handleError(DataCloneError, next);
    } else if (V8ArrayBuffer::HasInstance(value) && m_transferredArrayBuffers.tryGet(value.As<v8::Object>(), &arrayBufferIndex))
        return writeTransferredArrayBuffer(value, arrayBufferIndex, next);
    else {
        v8::Handle<v8::Object> jsObject = value.As<v8::Object>();
        if (jsObject.IsEmpty())
            return handleError(DataCloneError, next);
        greyObject(jsObject);
        if (value->IsDate())
            m_writer.writeDate(value->NumberValue());
        else if (value->IsStringObject())
            writeStringObject(value);
        else if (value->IsNumberObject())
            writeNumberObject(value);
        else if (value->IsBooleanObject())
            writeBooleanObject(value);
        else if (value->IsArray()) {
            return startArrayState(value.As<v8::Array>(), next);
        } else if (V8File::HasInstance(value))
            writeFile(value);
        else if (V8Blob::HasInstance(value))
            writeBlob(value);
        else if (V8FileList::HasInstance(value))
            writeFileList(value);
        else if (V8ImageData::HasInstance(value))
            writeImageData(value);
        else if (value->IsRegExp())
            writeRegExp(value);
        else if (V8ArrayBuffer::HasInstance(value))
            return writeArrayBuffer(value, next);
        else if (value->IsObject()) {
            if (isHostObject(jsObject) || jsObject->IsCallable() || value->IsNativeError())
                return handleError(DataCloneError, next);
            return startObjectState(jsObject, next);
        } else
            return handleError(DataCloneError, next);
    }
    return 0;
}

// Interface used by Reader to create objects of composite types.
class CompositeCreator {
public:
    virtual ~CompositeCreator() { }

    virtual bool consumeTopOfStack(v8::Handle<v8::Value>*) = 0;
    virtual uint32_t objectReferenceCount() = 0;
    virtual void pushObjectReference(const v8::Handle<v8::Value>&) = 0;
    virtual bool tryGetObjectFromObjectReference(uint32_t reference, v8::Handle<v8::Value>*) = 0;
    virtual bool tryGetTransferredMessagePort(uint32_t index, v8::Handle<v8::Value>*) = 0;
    virtual bool tryGetTransferredArrayBuffer(uint32_t index, v8::Handle<v8::Value>*) = 0;
    virtual bool newSparseArray(uint32_t length) = 0;
    virtual bool newDenseArray(uint32_t length) = 0;
    virtual bool newObject() = 0;
    virtual bool completeObject(uint32_t numProperties, v8::Handle<v8::Value>*) = 0;
    virtual bool completeSparseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>*) = 0;
    virtual bool completeDenseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>*) = 0;
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
        case RegExpTag:
            if (!readRegExp(value))
                return false;
            break;
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
        case DenseArrayTag: {
            uint32_t numProperties;
            uint32_t length;
            if (!doReadUint32(&numProperties))
                return false;
            if (!doReadUint32(&length))
                return false;
            if (!creator.completeDenseArray(numProperties, length, value))
                return false;
            break;
        }
        case ArrayBufferViewTag: {
            if (m_version <= 0)
                return false;
            if (!readArrayBufferView(value, creator))
                return false;
            creator.pushObjectReference(*value);
            break;
        }
        case ArrayBufferTag: {
            if (m_version <= 0)
                return false;
            if (!readArrayBuffer(value))
                return false;
            creator.pushObjectReference(*value);
            break;
        }
        case GenerateFreshObjectTag: {
            if (m_version <= 0)
                return false;
            if (!creator.newObject())
                return false;
            return true;
        }
        case GenerateFreshSparseArrayTag: {
            if (m_version <= 0)
                return false;
            uint32_t length;
            if (!doReadUint32(&length))
                return false;
            if (!creator.newSparseArray(length))
                return false;
            return true;
        }
        case GenerateFreshDenseArrayTag: {
            if (m_version <= 0)
                return false;
            uint32_t length;
            if (!doReadUint32(&length))
                return false;
            if (!creator.newDenseArray(length))
                return false;
            return true;
        }
        case MessagePortTag: {
            if (m_version <= 0)
                return false;
            uint32_t index;
            if (!doReadUint32(&index))
                return false;
            if (!creator.tryGetTransferredMessagePort(index, value))
                return false;
            break;
        }
        case ArrayBufferTransferTag: {
            if (m_version <= 0)
                return false;
            uint32_t index;
            if (!doReadUint32(&index))
                return false;
            if (!creator.tryGetTransferredArrayBuffer(index, value))
                return false;
            break;
        }
        case ObjectReferenceTag: {
            if (m_version <= 0)
                return false;
            uint32_t reference;
            if (!doReadUint32(&reference))
                return false;
            if (!creator.tryGetObjectFromObjectReference(reference, value))
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
        *value = v8::Integer::NewFromUnsigned(rawValue);
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
        RefPtr<ImageData> imageData = ImageData::create(IntSize(width, height));
        WTF::ByteArray* pixelArray = imageData->data()->data();
        ASSERT(pixelArray);
        ASSERT(pixelArray->length() >= pixelDataLength);
        memcpy(pixelArray->data(), m_buffer + m_position, pixelDataLength);
        m_position += pixelDataLength;
        *value = toV8(imageData.release());
        return true;
    }

    PassRefPtr<ArrayBuffer> doReadArrayBuffer()
    {
        uint32_t byteLength;
        if (!doReadUint32(&byteLength))
            return 0;
        if (m_position + byteLength > m_length)
            return 0;
        const void* bufferStart = m_buffer + m_position;
        RefPtr<ArrayBuffer> arrayBuffer = ArrayBuffer::create(bufferStart, byteLength);
        m_position += byteLength;
        return arrayBuffer.release();
    }

    bool readArrayBuffer(v8::Handle<v8::Value>* value)
    {
        RefPtr<ArrayBuffer> arrayBuffer = doReadArrayBuffer();
        if (!arrayBuffer)
            return false;
        *value = toV8(arrayBuffer.release());
        return true;
    }

    bool readArrayBufferView(v8::Handle<v8::Value>* value, CompositeCreator& creator)
    {
        ArrayBufferViewSubTag subTag;
        uint32_t byteOffset;
        uint32_t byteLength;
        RefPtr<ArrayBuffer> arrayBuffer;
        v8::Handle<v8::Value> arrayBufferV8Value;
        if (!readArrayBufferViewSubTag(&subTag))
            return false;
        if (!doReadUint32(&byteOffset))
            return false;
        if (!doReadUint32(&byteLength))
            return false;
        if (!creator.consumeTopOfStack(&arrayBufferV8Value))
            return false;
        if (arrayBufferV8Value.IsEmpty()) 
            return false;
        arrayBuffer = V8ArrayBuffer::toNative(arrayBufferV8Value.As<v8::Object>());
        if (!arrayBuffer)
            return false;
        switch (subTag) {
        case ByteArrayTag:
            *value = toV8(Int8Array::create(arrayBuffer.release(), byteOffset, byteLength));
            break;
        case UnsignedByteArrayTag:
            *value = toV8(Uint8Array::create(arrayBuffer.release(), byteOffset, byteLength));
            break;
        case ShortArrayTag: {
            uint32_t shortLength = byteLength / sizeof(int16_t);
            if (shortLength * sizeof(int16_t) != byteLength)
                return false;
            *value = toV8(Int16Array::create(arrayBuffer.release(), byteOffset, shortLength));
            break;
        }
        case UnsignedShortArrayTag: {
            uint32_t shortLength = byteLength / sizeof(uint16_t);
            if (shortLength * sizeof(uint16_t) != byteLength)
                return false;
            *value = toV8(Uint16Array::create(arrayBuffer.release(), byteOffset, shortLength));
            break;
        }
        case IntArrayTag: {
            uint32_t intLength = byteLength / sizeof(int32_t);
            if (intLength * sizeof(int32_t) != byteLength)
                return false;
            *value = toV8(Int32Array::create(arrayBuffer.release(), byteOffset, intLength));
            break;
        }
        case UnsignedIntArrayTag: {
            uint32_t intLength = byteLength / sizeof(uint32_t);
            if (intLength * sizeof(uint32_t) != byteLength)
                return false;
            *value = toV8(Uint32Array::create(arrayBuffer.release(), byteOffset, intLength));
            break;
        }
        case FloatArrayTag: {
            uint32_t floatLength = byteLength / sizeof(float);
            if (floatLength * sizeof(float) != byteLength)
                return false;
            *value = toV8(Float32Array::create(arrayBuffer.release(), byteOffset, floatLength));
            break;
        }
        case DoubleArrayTag: {
            uint32_t floatLength = byteLength / sizeof(double);
            if (floatLength * sizeof(double) != byteLength)
                return false;
            *value = toV8(Float64Array::create(arrayBuffer.release(), byteOffset, floatLength));
            break;
        }
        case DataViewTag:
            *value = toV8(DataView::create(arrayBuffer.release(), byteOffset, byteLength));
            break;
        default:
            return false;
        }
        // The various *Array::create() methods will return null if the range the view expects is
        // mismatched with the range the buffer can provide or if the byte offset is not aligned
        // to the size of the element type.
        return !value->IsEmpty();
    }

    bool readRegExp(v8::Handle<v8::Value>* value)
    {
        v8::Handle<v8::Value> pattern;
        if (!readString(&pattern))
            return false;
        uint32_t flags;
        if (!doReadUint32(&flags))
            return false;
        *value = v8::RegExp::New(pattern.As<v8::String>(), static_cast<v8::RegExp::Flags>(flags));
        return true;
    }

    bool readBlob(v8::Handle<v8::Value>* value)
    {
        String url;
        String type;
        uint64_t size;
        if (!readWebCoreString(&url))
            return false;
        if (!readWebCoreString(&type))
            return false;
        if (!doReadUint64(&size))
            return false;
        PassRefPtr<Blob> blob = Blob::create(KURL(ParsedURLString, url), type, size);
        *value = toV8(blob);
        return true;
    }

    bool readFile(v8::Handle<v8::Value>* value)
    {
        String path;
        String url;
        String type;
        if (!readWebCoreString(&path))
            return false;
        if (!readWebCoreString(&url))
            return false;
        if (!readWebCoreString(&type))
            return false;
        PassRefPtr<File> file = File::create(path, KURL(ParsedURLString, url), type);
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
            String urlString;
            String type;
            if (!readWebCoreString(&path))
                return false;
            if (!readWebCoreString(&urlString))
                return false;
            if (!readWebCoreString(&type))
                return false;
            fileList->append(File::create(path, KURL(ParsedURLString, urlString), type));
        }
        *value = toV8(fileList);
        return true;
    }

    template<class T>
    bool doReadUintHelper(T* value)
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

    bool doReadUint32(uint32_t* value)
    {
        return doReadUintHelper(value);
    }

    bool doReadUint64(uint64_t* value)
    {
        return doReadUintHelper(value);
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


typedef Vector<WTF::ArrayBufferContents, 1> ArrayBufferContentsArray;

class Deserializer : public CompositeCreator {
public:
    explicit Deserializer(Reader& reader, 
                          MessagePortArray* messagePorts, ArrayBufferContentsArray* arrayBufferContents)
        : m_reader(reader)
        , m_transferredMessagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_version(0)
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

    virtual bool completeDenseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>* value)
    {
        v8::Local<v8::Array> array;
        if (m_version > 0) {
            v8::Local<v8::Value> composite;
            if (!closeComposite(&composite))
                return false;
            array = composite.As<v8::Array>();
        }
        if (array.IsEmpty())
            return false;
        if (!initializeObject(array, numProperties, value))
            return false;
        if (length > stackDepth())
            return false;
        for (unsigned i = 0, stackPos = stackDepth() - length; i < length; i++, stackPos++) {
            v8::Local<v8::Value> elem = element(stackPos);
            if (!elem->IsUndefined())
                array->Set(i, elem);
        }
        pop(length);
        return true;
    }

    virtual void pushObjectReference(const v8::Handle<v8::Value>& object)
    {
        m_objectPool.append(object);
    }

    virtual bool tryGetTransferredMessagePort(uint32_t index, v8::Handle<v8::Value>* object)
    {
        if (!m_transferredMessagePorts)
            return false;
        if (index >= m_transferredMessagePorts->size())
            return false;
        *object = V8MessagePort::wrap(m_transferredMessagePorts->at(index).get());
        return true;
    }

    virtual bool tryGetTransferredArrayBuffer(uint32_t index, v8::Handle<v8::Value>* object)
    {
        if (!m_arrayBufferContents)
            return false;
        if (index >= m_arrayBuffers.size())
            return false;
        v8::Handle<v8::Object> result = m_arrayBuffers.at(index);
        if (result.IsEmpty()) {
            result = V8ArrayBuffer::wrap(ArrayBuffer::create(m_arrayBufferContents->at(index)).get());
            m_arrayBuffers[index] = result;
        }
        *object = result;
        return true;
    }

    virtual bool tryGetObjectFromObjectReference(uint32_t reference, v8::Handle<v8::Value>* object)
    {
        if (reference >= m_objectPool.size())
            return false;
        *object = m_objectPool[reference];
        return object;
    }

    virtual uint32_t objectReferenceCount()
    {
        return m_objectPool.size();
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
    Vector<v8::Handle<v8::Value> > m_objectPool;
    Vector<uint32_t> m_openCompositeReferenceStack;
    MessagePortArray* m_transferredMessagePorts;
    ArrayBufferContentsArray* m_arrayBufferContents;
    Vector<v8::Handle<v8::Object> > m_arrayBuffers;
    uint32_t m_version;
};

} // namespace

void SerializedScriptValue::deserializeAndSetProperty(v8::Handle<v8::Object> object, const char* propertyName,
                                                      v8::PropertyAttribute attribute, SerializedScriptValue* value)
{
    if (!value)
        return;
    v8::Handle<v8::Value> deserialized = value->deserialize();
    object->ForceSet(v8::String::NewSymbol(propertyName), deserialized, attribute);
}

void SerializedScriptValue::deserializeAndSetProperty(v8::Handle<v8::Object> object, const char* propertyName,
                                                      v8::PropertyAttribute attribute, PassRefPtr<SerializedScriptValue> value)
{
    deserializeAndSetProperty(object, propertyName, attribute, value.get());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(v8::Handle<v8::Value> value,
                                                                MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers,
                                                                bool& didThrow)
{
    return adoptRef(new SerializedScriptValue(value, messagePorts, arrayBuffers, didThrow));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(v8::Handle<v8::Value> value)
{
    bool didThrow;
    return adoptRef(new SerializedScriptValue(value, 0, 0, didThrow));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::createFromWire(String data)
{
    return adoptRef(new SerializedScriptValue(data));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(String data)
{
    Writer writer;
    writer.writeWebCoreString(data);
    String wireData = StringImpl::adopt(writer.data());
    return adoptRef(new SerializedScriptValue(wireData));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create()
{
    return adoptRef(new SerializedScriptValue());
}

SerializedScriptValue* SerializedScriptValue::nullValue()
{
    DEFINE_STATIC_LOCAL(RefPtr<SerializedScriptValue>, nullValue, (0));
    if (!nullValue) {
        Writer writer;
        writer.writeNull();
        String wireData = StringImpl::adopt(writer.data());
        nullValue = adoptRef(new SerializedScriptValue(wireData));
    }
    return nullValue.get();
}

SerializedScriptValue* SerializedScriptValue::undefinedValue()
{
    DEFINE_STATIC_LOCAL(RefPtr<SerializedScriptValue>, undefinedValue, (0));
    if (!undefinedValue) {
        Writer writer;
        writer.writeUndefined();
        String wireData = StringImpl::adopt(writer.data());
        undefinedValue = adoptRef(new SerializedScriptValue(wireData));
    }
    return undefinedValue.get();
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::release()
{
    RefPtr<SerializedScriptValue> result = adoptRef(new SerializedScriptValue(m_data));
    m_data = String();
    return result.release();
}

SerializedScriptValue::SerializedScriptValue()
{
}

static void neuterBinding(void* domObject) 
{
    DOMDataList& allStores = DOMDataStore::allStores();
    for (size_t i = 0; i < allStores.size(); i++) {
        v8::Handle<v8::Object> obj = allStores[i]->domObjectMap().get(domObject);
        if (!obj.IsEmpty())
            obj->SetIndexedPropertiesToExternalArrayData(0, v8::kExternalByteArray, 0);
    }
}

PassOwnPtr<SerializedScriptValue::ArrayBufferContentsArray> SerializedScriptValue::transferArrayBuffers(ArrayBufferArray& arrayBuffers, bool& didThrow) 
{
    for (size_t i = 0; i < arrayBuffers.size(); i++) {
        if (arrayBuffers[i]->isNeutered()) {
            throwError(INVALID_STATE_ERR);
            didThrow = true;
            return nullptr;
        }
    }

    OwnPtr<ArrayBufferContentsArray> contents = adoptPtr(new ArrayBufferContentsArray(arrayBuffers.size()));

    HashSet<ArrayBuffer*> visited;
    for (size_t i = 0; i < arrayBuffers.size(); i++) {
        Vector<RefPtr<ArrayBufferView> > neuteredViews;

        if (visited.contains(arrayBuffers[i].get()))
            continue;
        visited.add(arrayBuffers[i].get());

        bool result = arrayBuffers[i]->transfer(contents->at(i), neuteredViews);
        if (!result) {
            throwError(INVALID_STATE_ERR);
            didThrow = true;
            return nullptr;
        }

        neuterBinding(arrayBuffers[i].get());
        for (size_t j = 0; j < neuteredViews.size(); j++)
            neuterBinding(neuteredViews[j].get());
    }
    return contents.release();
}

SerializedScriptValue::SerializedScriptValue(v8::Handle<v8::Value> value, 
                                             MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers,
                                             bool& didThrow)
{
    didThrow = false;
    Writer writer;
    Serializer::Status status;
    {
        v8::TryCatch tryCatch;
        Serializer serializer(writer, messagePorts, arrayBuffers, tryCatch);
        status = serializer.serialize(value);
        if (status == Serializer::JSException) {
            // If there was a JS exception thrown, re-throw it.
            didThrow = true;
            tryCatch.ReThrow();
            return;
        }
    }
    if (status == Serializer::InputError) {
        // If there was an input error, throw a new exception outside
        // of the TryCatch scope.
        didThrow = true;
        throwError(NOT_SUPPORTED_ERR);
        return;
    case Serializer::InvalidStateError:
        didThrow = true;
        throwError(INVALID_STATE_ERR);
        return;
    case Serializer::JSFailure:
        // If there was a JS failure (but no exception), there's not
        // much we can do except for unwinding the C++ stack by
        // pretending there was a JS exception.
        didThrow = true;
        return;
    case Serializer::Success:
        m_data = String(StringImpl::adopt(writer.data())).isolatedCopy();
        if (arrayBuffers)
            m_arrayBufferContentsArray = transferArrayBuffers(*arrayBuffers, didThrow);
        return;
    case Serializer::JSException:
        // We should never get here because this case was handled above.
        break;
    }
    ASSERT(status == Serializer::Success);
    m_data = String(StringImpl::adopt(writer.data())).crossThreadString();
}

SerializedScriptValue::SerializedScriptValue(String wireData)
{
    m_data = wireData.isolatedCopy();
}

v8::Handle<v8::Value> SerializedScriptValue::deserialize()
{
    if (!m_data.impl())
        return v8::Null();
    COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
    Reader reader(reinterpret_cast<const uint8_t*>(m_data.impl()->characters()), 2 * m_data.length());
    Deserializer deserializer(reader, messagePorts, m_arrayBufferContentsArray.get());
    return deserializer.deserialize();
}

} // namespace WebCore
