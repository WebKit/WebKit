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

#include "AsyncFileSystem.h"
#include "Blob.h"
#include "DataView.h"
#include "ExceptionCode.h"
#include "File.h"
#include "FileList.h"
#include "ImageData.h"
#include "MessagePort.h"
#include "SharedBuffer.h"
#include "V8ArrayBuffer.h"
#include "V8ArrayBufferCustom.h"
#include "V8ArrayBufferView.h"
#include "V8Binding.h"
#include "V8Blob.h"
#include "V8DataView.h"
#include "V8File.h"
#include "V8FileList.h"
#include "V8Float32Array.h"
#include "V8Float64Array.h"
#include "V8ImageData.h"
#include "V8Int16Array.h"
#include "V8Int32Array.h"
#include "V8Int8Array.h"
#include "V8MessagePort.h"
#include "V8Uint16Array.h"
#include "V8Uint32Array.h"
#include "V8Uint8Array.h"
#include "V8Uint8ClampedArray.h"
#include "V8Utilities.h"

#include <wtf/ArrayBuffer.h>
#include <wtf/ArrayBufferView.h>
#include <wtf/Assertions.h>
#include <wtf/ByteOrder.h>
#include <wtf/Float32Array.h>
#include <wtf/Float64Array.h>
#include <wtf/Int16Array.h>
#include <wtf/Int32Array.h>
#include <wtf/Int8Array.h>
#include <wtf/RefCounted.h>
#include <wtf/Uint16Array.h>
#include <wtf/Uint32Array.h>
#include <wtf/Uint8Array.h>
#include <wtf/Uint8ClampedArray.h>
#include <wtf/Vector.h>

#if ENABLE(FILE_SYSTEM)
#include "V8DOMFileSystem.h"
#endif

// FIXME: consider crashing in debug mode on deserialization errors
// NOTE: be sure to change wireFormatVersion as necessary!

namespace WebCore {

namespace {

// This code implements the HTML5 Structured Clone algorithm:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/urls.html#safe-passing-of-structured-data

// V8ObjectMap is a map from V8 objects to arbitrary values of type T.
// V8 objects (or handles to V8 objects) cannot be used as keys in ordinary wtf::HashMaps;
// this class should be used instead. GCObject must be a subtype of v8::Object.
// Suggested usage:
//     V8ObjectMap<v8::Object, int> map;
//     v8::Handle<v8::Object> obj = ...;
//     map.set(obj, 42);
template<typename GCObject, typename T>
class V8ObjectMap {
public:
    bool contains(const v8::Handle<GCObject>& handle)
    {
        return m_map.contains(*handle);
    }

    bool tryGet(const v8::Handle<GCObject>& handle, T* valueOut)
    {
        typename HandleToT::iterator result = m_map.find(*handle);
        if (result != m_map.end()) {
            *valueOut = result->value;
            return true;
        }
        return false;
    }

    void set(const v8::Handle<GCObject>& handle, const T& value)
    {
        m_map.set(*handle, value);
    }

    uint32_t size()
    {
        return m_map.size();
    }

private:
    // This implementation uses GetIdentityHash(), which sets a hidden property on the object containing
    // a random integer (or returns the one that had been previously set). This ensures that the table
    // never needs to be rebuilt across garbage collections at the expense of doing additional allocation
    // and making more round trips into V8. Note that since GetIdentityHash() is defined only on
    // v8::Objects, this V8ObjectMap cannot be used to map v8::Strings to T (because the public V8 API
    // considers a v8::String to be a v8::Primitive).

    // If V8 exposes a way to get at the address of the object held by a handle, then we can produce
    // an alternate implementation that does not need to do any V8-side allocation; however, it will
    // need to rehash after every garbage collection because a key object may have been moved.
    template<typename G>
    struct V8HandlePtrHash {
        static unsigned hash(const G* key)
        {
            v8::Handle<G> objHandle(const_cast<G*>(key));
            return static_cast<unsigned>(objHandle->GetIdentityHash());
        }
        static bool equal(const G* a, const G* b)
        {
            return v8::Handle<G>(const_cast<G*>(a)) == v8::Handle<G>(const_cast<G*>(b));
        }
        // For HashArg.
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    typedef WTF::HashMap<GCObject*, T, V8HandlePtrHash<GCObject> > HandleToT;
    HandleToT m_map;
};

typedef UChar BufferValueType;

// Serialization format is a sequence of tags followed by zero or more data arguments.
// Tags always take exactly one byte. A serialized stream first begins with
// a complete VersionTag. If the stream does not begin with a VersionTag, we assume that
// the stream is in format 0.

// This format is private to the implementation of SerializedScriptValue. Do not rely on it
// externally. It is safe to persist a SerializedScriptValue as a binary blob, but this
// code should always be used to interpret it.

// WebCoreStrings are read as (length:uint32_t, string:UTF8[length]).
// RawStrings are read as (length:uint32_t, string:UTF8[length]).
// RawUCharStrings are read as (length:uint32_t, string:UChar[length/sizeof(UChar)]).
// RawFiles are read as (path:WebCoreString, url:WebCoreStrng, type:WebCoreString).
// There is a reference table that maps object references (uint32_t) to v8::Values.
// Tokens marked with (ref) are inserted into the reference table and given the next object reference ID after decoding.
// All tags except InvalidTag, PaddingTag, ReferenceCountTag, VersionTag, GenerateFreshObjectTag
//     and GenerateFreshArrayTag push their results to the deserialization stack.
// There is also an 'open' stack that is used to resolve circular references. Objects or arrays may
//     contain self-references. Before we begin to deserialize the contents of these values, they
//     are first given object reference IDs (by GenerateFreshObjectTag/GenerateFreshArrayTag);
//     these reference IDs are then used with ObjectReferenceTag to tie the recursive knot.
enum SerializationTag {
    InvalidTag = '!', // Causes deserialization to fail.
    PaddingTag = '\0', // Is ignored (but consumed).
    UndefinedTag = '_', // -> <undefined>
    NullTag = '0', // -> <null>
    TrueTag = 'T', // -> <true>
    FalseTag = 'F', // -> <false>
    StringTag = 'S', // string:RawString -> string
    StringUCharTag = 'c', // string:RawUCharString -> string
    Int32Tag = 'I', // value:ZigZag-encoded int32 -> Integer
    Uint32Tag = 'U', // value:uint32_t -> Integer
    DateTag = 'D', // value:double -> Date (ref)
    MessagePortTag = 'M', // index:int -> MessagePort. Fills the result with transferred MessagePort.
    NumberTag = 'N', // value:double -> Number
    BlobTag = 'b', // url:WebCoreString, type:WebCoreString, size:uint64_t -> Blob (ref)
    FileTag = 'f', // file:RawFile -> File (ref)
#if ENABLE(FILE_SYSTEM)
    DOMFileSystemTag = 'd', // type:int32_t, name:WebCoreString, url:WebCoreString -> FileSystem (ref)
#endif
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

enum ArrayBufferViewSubTag {
    ByteArrayTag = 'b',
    UnsignedByteArrayTag = 'B',
    UnsignedByteClampedArrayTag = 'C',
    ShortArrayTag = 'w',
    UnsignedShortArrayTag = 'W',
    IntArrayTag = 'd',
    UnsignedIntArrayTag = 'D',
    FloatArrayTag = 'f',
    DoubleArrayTag = 'F',
    DataViewTag = '?'
};

static bool shouldCheckForCycles(int depth)
{
    ASSERT(depth >= 0);
    // Since we are not required to spot the cycle as soon as it
    // happens we can check for cycles only when the current depth
    // is a power of two.
    return !(depth & (depth - 1));
}

// Increment this for each incompatible change to the wire format.
// Version 2: Added StringUCharTag for UChar v8 strings.
static const uint32_t wireFormatVersion = 2;

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
    Writer(v8::Isolate* isolate)
        : m_position(0)
        , m_isolate(isolate)
    {
    }

    // Write functions for primitive types.

    void writeUndefined() { append(UndefinedTag); }

    void writeNull() { append(NullTag); }

    void writeTrue() { append(TrueTag); }

    void writeFalse() { append(FalseTag); }

    void writeBooleanObject(bool value)
    {
        append(value ? TrueObjectTag : FalseObjectTag);
    }

    void writeString(const char* data, int length)
    {
        ASSERT(length >= 0);
        append(StringTag);
        doWriteString(data, length);
    }

    void writeAsciiString(v8::Handle<v8::String>& string)
    {
        int length = string->Length();
        ASSERT(length >= 0);

        append(StringTag);
        doWriteUint32(static_cast<uint32_t>(length));
        ensureSpace(length);

        char* buffer = reinterpret_cast<char*>(byteAt(m_position));
        string->WriteAscii(buffer, 0, length, v8StringWriteOptions());
        m_position += length;
    }

    void writeUCharString(v8::Handle<v8::String>& string)
    {
        int length = string->Length();
        ASSERT(length >= 0);

        int size = length * sizeof(UChar);
        int bytes = bytesNeededToWireEncode(static_cast<uint32_t>(size));
        if ((m_position + 1 + bytes) & 1)
            append(PaddingTag);

        append(StringUCharTag);
        doWriteUint32(static_cast<uint32_t>(size));
        ensureSpace(size);

        ASSERT(!(m_position & 1));
        uint16_t* buffer = reinterpret_cast<uint16_t*>(byteAt(m_position));
        string->Write(buffer, 0, length, v8StringWriteOptions());
        m_position += size;
    }

    void writeStringObject(const char* data, int length)
    {
        ASSERT(length >= 0);
        append(StringObjectTag);
        doWriteString(data, length);
    }

    void writeWebCoreString(const String& string)
    {
        // Uses UTF8 encoding so we can read it back as either V8 or
        // WebCore string.
        append(StringTag);
        doWriteWebCoreString(string);
    }

    void writeVersion()
    {
        append(VersionTag);
        doWriteUint32(wireFormatVersion);
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

    void writeNumberObject(double number)
    {
        append(NumberObjectTag);
        doWriteNumber(number);
    }

    void writeBlob(const String& url, const String& type, unsigned long long size)
    {
        append(BlobTag);
        doWriteWebCoreString(url);
        doWriteWebCoreString(type);
        doWriteUint64(size);
    }

#if ENABLE(FILE_SYSTEM)
    void writeDOMFileSystem(int type, const String& name, const String& url)
    {
        append(DOMFileSystemTag);
        doWriteUint32(type);
        doWriteWebCoreString(name);
        doWriteWebCoreString(url);
    }
#endif

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

    void writeArrayBuffer(const ArrayBuffer& arrayBuffer)
    {
        append(ArrayBufferTag);
        doWriteArrayBuffer(arrayBuffer);
    }

    void writeArrayBufferView(const ArrayBufferView& arrayBufferView)
    {
        append(ArrayBufferViewTag);
#ifndef NDEBUG
        const ArrayBuffer& arrayBuffer = *arrayBufferView.buffer();
        ASSERT(static_cast<const uint8_t*>(arrayBuffer.data()) + arrayBufferView.byteOffset() ==
               static_cast<const uint8_t*>(arrayBufferView.baseAddress()));
#endif
        ArrayBufferView::ViewType type = arrayBufferView.getType();

        if (type == ArrayBufferView::TypeInt8)
            append(ByteArrayTag);
        else if (type == ArrayBufferView::TypeUint8Clamped)
            append(UnsignedByteClampedArrayTag);
        else if (type == ArrayBufferView::TypeUint8)
            append(UnsignedByteArrayTag);
        else if (type == ArrayBufferView::TypeInt16)
            append(ShortArrayTag);
        else if (type == ArrayBufferView::TypeUint16)
            append(UnsignedShortArrayTag);
        else if (type == ArrayBufferView::TypeInt32)
            append(IntArrayTag);
        else if (type == ArrayBufferView::TypeUint32)
            append(UnsignedIntArrayTag);
        else if (type == ArrayBufferView::TypeFloat32)
            append(FloatArrayTag);
        else if (type == ArrayBufferView::TypeFloat64)
            append(DoubleArrayTag);
        else if (type == ArrayBufferView::TypeDataView)
            append(DataViewTag);
        else
            ASSERT_NOT_REACHED();
        doWriteUint32(arrayBufferView.byteOffset());
        doWriteUint32(arrayBufferView.byteLength());
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

    void writeTransferredMessagePort(uint32_t index)
    {
        append(MessagePortTag);
        doWriteUint32(index);
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

    void writeDenseArray(uint32_t numProperties, uint32_t length)
    {
        append(DenseArrayTag);
        doWriteUint32(numProperties);
        doWriteUint32(length);
    }

    Vector<BufferValueType>& data()
    {
        fillHole();
        return m_buffer;
    }

    void writeReferenceCount(uint32_t numberOfReferences)
    {
        append(ReferenceCountTag);
        doWriteUint32(numberOfReferences);
    }

    void writeGenerateFreshObject()
    {
        append(GenerateFreshObjectTag);
    }

    void writeGenerateFreshSparseArray(uint32_t length)
    {
        append(GenerateFreshSparseArrayTag);
        doWriteUint32(length);
    }

    void writeGenerateFreshDenseArray(uint32_t length)
    {
        append(GenerateFreshDenseArrayTag);
        doWriteUint32(length);
    }

    v8::Isolate* getIsolate() { return m_isolate; }

private:
    void doWriteArrayBuffer(const ArrayBuffer& arrayBuffer)
    {
        uint32_t byteLength = arrayBuffer.byteLength();
        doWriteUint32(byteLength);
        append(static_cast<const uint8_t*>(arrayBuffer.data()), byteLength);
    }

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

    int bytesNeededToWireEncode(uint32_t value)
    {
        int bytes = 1;
        while (true) {
            value >>= varIntShift;
            if (!value)
                break;
            ++bytes;
        }

        return bytes;
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

    uint8_t* byteAt(int position)
    {
        return reinterpret_cast<uint8_t*>(m_buffer.data()) + position;
    }

    int v8StringWriteOptions()
    {
        return v8::String::NO_NULL_TERMINATION | v8::String::PRESERVE_ASCII_NULL;
    }

    Vector<BufferValueType> m_buffer;
    unsigned m_position;
    v8::Isolate* m_isolate;
};

static v8::Handle<v8::Object> toV8Object(MessagePort* impl, v8::Isolate* isolate)
{
    if (!impl)
        return v8::Handle<v8::Object>();
    v8::Handle<v8::Value> wrapper = toV8(impl, v8::Handle<v8::Object>(), isolate);
    ASSERT(wrapper->IsObject());
    return wrapper.As<v8::Object>();
}

static v8::Handle<v8::Object> toV8Object(ArrayBuffer* impl, v8::Isolate* isolate)
{
    if (!impl)
        return v8::Handle<v8::Object>();
    v8::Handle<v8::Value> wrapper = toV8(impl, v8::Handle<v8::Object>(), isolate);
    ASSERT(wrapper->IsObject());
    return wrapper.As<v8::Object>();
}

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

    Serializer(Writer& writer, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, Vector<String>& blobURLs, v8::TryCatch& tryCatch)
        : m_writer(writer)
        , m_tryCatch(tryCatch)
        , m_depth(0)
        , m_execDepth(0)
        , m_status(Success)
        , m_nextObjectReference(0)
        , m_blobURLs(blobURLs)
    {
        ASSERT(!tryCatch.HasCaught());
        if (messagePorts) {
            for (size_t i = 0; i < messagePorts->size(); i++)
                m_transferredMessagePorts.set(toV8Object(messagePorts->at(i).get(), m_writer.getIsolate()), i);
        }
        if (arrayBuffers) {
            for (size_t i = 0; i < arrayBuffers->size(); i++)  {
                v8::Handle<v8::Object> v8ArrayBuffer = toV8Object(arrayBuffers->at(i).get(), m_writer.getIsolate());
                // Coalesce multiple occurences of the same buffer to the first index.
                if (!m_transferredArrayBuffers.contains(v8ArrayBuffer))
                    m_transferredArrayBuffers.set(v8ArrayBuffer, i);
            }
        }
    }

    Status serialize(v8::Handle<v8::Value> value)
    {
        v8::HandleScope scope;
        m_writer.writeVersion();
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

    StateBase* writeDenseArray(uint32_t numProperties, uint32_t length, StateBase* state)
    {
        m_writer.writeDenseArray(numProperties, length);
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

        // Returns 1 if this state is currently serializing a property
        // via an accessor and 0 otherwise.
        virtual uint32_t execDepth() const { return 0; }

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
            : StateBase(v8Undefined(), 0)
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

    class AbstractObjectState : public State<v8::Object> {
    public:
        AbstractObjectState(v8::Handle<v8::Object> object, StateBase* next)
            : State<v8::Object>(object, next)
            , m_index(0)
            , m_numSerializedProperties(0)
            , m_nameDone(false)
            , m_isSerializingAccessor(false)
        {
        }

        virtual uint32_t execDepth() const { return m_isSerializingAccessor ? 1 : 0; }

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer&) = 0;

        StateBase* serializeProperties(bool ignoreIndexed, Serializer& serializer) 
        {
            m_isSerializingAccessor = false;
            while (m_index < m_propertyNames->Length()) {
                bool isAccessor = false;
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
                    isAccessor = hasStringProperty && composite()->HasRealNamedCallbackProperty(propertyName.As<v8::String>());
                    if (StateBase* newState = serializer.checkException(this))
                        return newState;
                    if (hasStringProperty || (hasIndexedProperty && !ignoreIndexed))
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
                m_isSerializingAccessor = isAccessor;
                // If we return early here, it's either because we have pushed a new state onto the
                // serialization state stack or because we have encountered an error (and in both cases
                // we are unwinding the native stack). We reset m_isSerializingAccessor at the beginning
                // of advance() for this case (because advance() will be called on us again once we
                // are the top of the stack).
                if (StateBase* newState = serializer.doSerialize(value, this))
                    return newState;
                m_isSerializingAccessor = false;
            }
            return objectDone(m_numSerializedProperties, serializer);
        }

        v8::Local<v8::Array> m_propertyNames;

    private:
        v8::Local<v8::Value> m_propertyName;
        unsigned m_index;
        unsigned m_numSerializedProperties;
        bool m_nameDone;
        // Used along with execDepth() to determine the number of
        // accessors under which the serializer is currently serializing.
        bool m_isSerializingAccessor;
    };

    class ObjectState : public AbstractObjectState {
    public:
        ObjectState(v8::Handle<v8::Object> object, StateBase* next)
            : AbstractObjectState(object, next)
        {
        }

        virtual StateBase* advance(Serializer& serializer)
        {
            if (m_propertyNames.IsEmpty()) {
                m_propertyNames = composite()->GetPropertyNames();
                if (StateBase* newState = serializer.checkException(this))
                    return newState;
                if (m_propertyNames.IsEmpty())
                    return serializer.reportFailure(this);
            }
            return serializeProperties(false, serializer);
        }

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer& serializer)
        {
            return serializer.writeObject(numProperties, this);
        }
    };

    class DenseArrayState : public AbstractObjectState {
    public:
        DenseArrayState(v8::Handle<v8::Array> array, v8::Handle<v8::Array> propertyNames, StateBase* next)
            : AbstractObjectState(array, next)
            , m_arrayIndex(0)
            , m_arrayLength(array->Length())
        {
            m_propertyNames = v8::Local<v8::Array>::New(propertyNames);
        }

        virtual StateBase* advance(Serializer& serializer)
        {
            while (m_arrayIndex < m_arrayLength) {
                v8::Handle<v8::Value> value = composite().As<v8::Array>()->Get(m_arrayIndex);
                m_arrayIndex++;
                if (StateBase* newState = serializer.checkException(this))
                    return newState;
                if (StateBase* newState = serializer.doSerialize(value, this))
                    return newState;
            }
            return serializeProperties(true, serializer);
        }

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer& serializer)
        {
            return serializer.writeDenseArray(numProperties, m_arrayLength, this);
        }

    private:
        uint32_t m_arrayIndex;
        uint32_t m_arrayLength;
    };

    class SparseArrayState : public AbstractObjectState {
    public:
        SparseArrayState(v8::Handle<v8::Array> array, v8::Handle<v8::Array> propertyNames, StateBase* next)
            : AbstractObjectState(array, next)
        {
            m_propertyNames = v8::Local<v8::Array>::New(propertyNames);
        }

        virtual StateBase* advance(Serializer& serializer) 
        {
            return serializeProperties(false, serializer);
        }

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer& serializer)
        {
            return serializer.writeSparseArray(numProperties, composite().As<v8::Array>()->Length(), this);
        }
    };

    uint32_t execDepth() const
    {
        return m_execDepth;
    }

    StateBase* push(StateBase* state)
    {
        ASSERT(state);
        if (state->nextState())
            m_execDepth += state->nextState()->execDepth();
        ++m_depth;
        return checkComposite(state) ? state : handleError(InputError, state);
    }

    StateBase* pop(StateBase* state)
    {
        ASSERT(state);
        --m_depth;
        StateBase* next = state->nextState();
        if (next)
            m_execDepth -= next->execDepth();
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
            if (state)
                m_execDepth -= state->execDepth();
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
        v8::Handle<v8::String> string = value.As<v8::String>();
        if (!string->Length() || !string->MayContainNonAscii())
            m_writer.writeAsciiString(string);
        else
            m_writer.writeUCharString(string);
    }

    void writeStringObject(v8::Handle<v8::Value> value)
    {
        v8::Handle<v8::StringObject> stringObject = value.As<v8::StringObject>();
        v8::String::Utf8Value stringValue(stringObject->StringValue());
        m_writer.writeStringObject(*stringValue, stringValue.length());
    }

    void writeNumberObject(v8::Handle<v8::Value> value)
    {
        v8::Handle<v8::NumberObject> numberObject = value.As<v8::NumberObject>();
        m_writer.writeNumberObject(numberObject->NumberValue());
    }

    void writeBooleanObject(v8::Handle<v8::Value> value)
    {
        v8::Handle<v8::BooleanObject> booleanObject = value.As<v8::BooleanObject>();
        m_writer.writeBooleanObject(booleanObject->BooleanValue());
    }

    void writeBlob(v8::Handle<v8::Value> value)
    {
        Blob* blob = V8Blob::toNative(value.As<v8::Object>());
        if (!blob)
            return;
        m_writer.writeBlob(blob->url().string(), blob->type(), blob->size());
        m_blobURLs.append(blob->url().string());
    }

#if ENABLE(FILE_SYSTEM)
    StateBase* writeDOMFileSystem(v8::Handle<v8::Value> value, StateBase* next)
    {
        DOMFileSystem* fs = V8DOMFileSystem::toNative(value.As<v8::Object>());
        if (!fs)
            return 0;
        if (!fs->clonable())
            return handleError(DataCloneError, next);
        m_writer.writeDOMFileSystem(fs->type(), fs->name(), fs->rootURL().string());
        return 0;
    }
#endif

    void writeFile(v8::Handle<v8::Value> value)
    {
        File* file = V8File::toNative(value.As<v8::Object>());
        if (!file)
            return;
        m_writer.writeFile(file->path(), file->url().string(), file->type());
        m_blobURLs.append(file->url().string());
    }

    void writeFileList(v8::Handle<v8::Value> value)
    {
        FileList* fileList = V8FileList::toNative(value.As<v8::Object>());
        if (!fileList)
            return;
        m_writer.writeFileList(*fileList);
        unsigned length = fileList->length();
        for (unsigned i = 0; i < length; ++i)
            m_blobURLs.append(fileList->item(i)->url().string());
    }

    void writeImageData(v8::Handle<v8::Value> value)
    {
        ImageData* imageData = V8ImageData::toNative(value.As<v8::Object>());
        if (!imageData)
            return;
        Uint8ClampedArray* pixelArray = imageData->data();
        m_writer.writeImageData(imageData->width(), imageData->height(), pixelArray->data(), pixelArray->length());
    }

    void writeRegExp(v8::Handle<v8::Value> value)
    {
        v8::Handle<v8::RegExp> regExp = value.As<v8::RegExp>();
        m_writer.writeRegExp(regExp->GetSource(), regExp->GetFlags());
    }

    StateBase* writeAndGreyArrayBufferView(v8::Handle<v8::Object> object, StateBase* next)
    {
        ASSERT(!object.IsEmpty());
        ArrayBufferView* arrayBufferView = V8ArrayBufferView::toNative(object);
        if (!arrayBufferView)
            return 0;
        if (!arrayBufferView->buffer())
            return handleError(DataCloneError, next);
        v8::Handle<v8::Value> underlyingBuffer = toV8(arrayBufferView->buffer(), v8::Handle<v8::Object>(), m_writer.getIsolate());
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
        return push(new ObjectState(object, next));
    }

    // Marks object as having been visited by the serializer and assigns it a unique object reference ID.
    // An object may only be greyed once.
    void greyObject(const v8::Handle<v8::Object>& object)
    {
        ASSERT(!m_objectPool.contains(object));
        uint32_t objectReference = m_nextObjectReference++;
        m_objectPool.set(object, objectReference);
    }

    Writer& m_writer;
    v8::TryCatch& m_tryCatch;
    int m_depth;
    int m_execDepth;
    Status m_status;
    typedef V8ObjectMap<v8::Object, uint32_t> ObjectPool;
    ObjectPool m_objectPool;
    ObjectPool m_transferredMessagePorts;
    ObjectPool m_transferredArrayBuffers;
    uint32_t m_nextObjectReference;
    Vector<String>& m_blobURLs;
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
    else if (value->IsUndefined())
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
    else if (value->IsNumber())
        m_writer.writeNumber(value.As<v8::Number>()->Value());
    else if (V8ArrayBufferView::HasInstance(value))
        return writeAndGreyArrayBufferView(value.As<v8::Object>(), next);
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
#if ENABLE(FILE_SYSTEM)
        else if (V8DOMFileSystem::HasInstance(value))
            return writeDOMFileSystem(value, next);
#endif
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
    Reader(const uint8_t* buffer, int length, v8::Isolate* isolate)
        : m_buffer(buffer)
        , m_length(length)
        , m_position(0)
        , m_version(0)
        , m_isolate(isolate)
    {
        ASSERT(!(reinterpret_cast<size_t>(buffer) & 1));
        ASSERT(length >= 0);
    }

    bool isEof() const { return m_position >= m_length; }

    bool read(v8::Handle<v8::Value>* value, CompositeCreator& creator)
    {
        SerializationTag tag;
        if (!readTag(&tag))
            return false;
        switch (tag) {
        case ReferenceCountTag: {
            if (m_version <= 0)
                return false;
            uint32_t referenceTableSize;
            if (!doReadUint32(&referenceTableSize))
                return false;
            // If this test fails, then the serializer and deserializer disagree about the assignment
            // of object reference IDs. On the deserialization side, this means there are too many or too few
            // calls to pushObjectReference.
            if (referenceTableSize != creator.objectReferenceCount())
                return false;
            return true;
        }
        case InvalidTag:
            return false;
        case PaddingTag:
            return true;
        case UndefinedTag:
            *value = v8::Undefined();
            break;
        case NullTag:
            *value = v8NullWithCheck(m_isolate);
            break;
        case TrueTag:
            *value = v8BooleanWithCheck(true, m_isolate);
            break;
        case FalseTag:
            *value = v8BooleanWithCheck(false, m_isolate);
            break;
        case TrueObjectTag:
            *value = v8::BooleanObject::New(true);
            creator.pushObjectReference(*value);
            break;
        case FalseObjectTag:
            *value = v8::BooleanObject::New(false);
            creator.pushObjectReference(*value);
            break;
        case StringTag:
            if (!readString(value))
                return false;
            break;
        case StringUCharTag:
            if (!readUCharString(value))
                return false;
            break;
        case StringObjectTag:
            if (!readStringObject(value))
                return false;
            creator.pushObjectReference(*value);
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
            creator.pushObjectReference(*value);
            break;
        case NumberTag:
            if (!readNumber(value))
                return false;
            break;
        case NumberObjectTag:
            if (!readNumberObject(value))
                return false;
            creator.pushObjectReference(*value);
            break;
        case BlobTag:
            if (!readBlob(value))
                return false;
            creator.pushObjectReference(*value);
            break;
        case FileTag:
            if (!readFile(value))
                return false;
            creator.pushObjectReference(*value);
            break;
#if ENABLE(FILE_SYSTEM)
        case DOMFileSystemTag:
            if (!readDOMFileSystem(value))
                return false;
            creator.pushObjectReference(*value);
            break;
#endif
        case FileListTag:
            if (!readFileList(value))
                return false;
            creator.pushObjectReference(*value);
            break;
        case ImageDataTag:
            if (!readImageData(value))
                return false;
            creator.pushObjectReference(*value);
            break;

        case RegExpTag:
            if (!readRegExp(value))
                return false;
            creator.pushObjectReference(*value);
            break;
        case ObjectTag: {
            uint32_t numProperties;
            if (!doReadUint32(&numProperties))
                return false;
            if (!creator.completeObject(numProperties, value))
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
            if (!creator.completeSparseArray(numProperties, length, value))
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

    bool readVersion(uint32_t& version)
    {
        SerializationTag tag;
        if (!readTag(&tag)) {
            // This is a nullary buffer. We're still version 0.
            version = 0;
            return true;
        }
        if (tag != VersionTag) {
            // Versions of the format past 0 start with the version tag.
            version = 0;
            // Put back the tag.
            undoReadTag();
            return true;
        }
        // Version-bearing messages are obligated to finish the version tag.
        return doReadUint32(&version);
    }

    void setVersion(uint32_t version)
    {
        m_version = version;
    }

    v8::Isolate* getIsolate() { return m_isolate; }

private:
    bool readTag(SerializationTag* tag)
    {
        if (m_position >= m_length)
            return false;
        *tag = static_cast<SerializationTag>(m_buffer[m_position++]);
        return true;
    }

    void undoReadTag()
    {
        if (m_position > 0)
            --m_position;
    }

    bool readArrayBufferViewSubTag(ArrayBufferViewSubTag* tag)
    {
        if (m_position >= m_length)
            return false;
        *tag = static_cast<ArrayBufferViewSubTag>(m_buffer[m_position++]);
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

    bool readUCharString(v8::Handle<v8::Value>* value)
    {
        uint32_t length;
        if (!doReadUint32(&length) || (length & 1))
            return false;
        if (m_position + length > m_length)
            return false;
        ASSERT(!(m_position & 1));
        *value = v8::String::New(reinterpret_cast<const uint16_t*>(m_buffer + m_position), length / sizeof(UChar));
        m_position += length;
        return true;
    }

    bool readStringObject(v8::Handle<v8::Value>* value)
    {
        v8::Handle<v8::Value> stringValue;
        if (!readString(&stringValue) || !stringValue->IsString())
            return false;
        *value = v8::StringObject::New(stringValue.As<v8::String>());
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
        *value = v8Integer(static_cast<int32_t>(ZigZag::decode(rawValue)), m_isolate);
        return true;
    }

    bool readUint32(v8::Handle<v8::Value>* value)
    {
        uint32_t rawValue;
        if (!doReadUint32(&rawValue))
            return false;
        *value = v8UnsignedInteger(rawValue, m_isolate);
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
  
    bool readNumberObject(v8::Handle<v8::Value>* value)
    {
        double number;
        if (!doReadNumber(&number))
            return false;
        *value = v8::NumberObject::New(number);
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
        Uint8ClampedArray* pixelArray = imageData->data();
        ASSERT(pixelArray);
        ASSERT(pixelArray->length() >= pixelDataLength);
        memcpy(pixelArray->data(), m_buffer + m_position, pixelDataLength);
        m_position += pixelDataLength;
        *value = toV8(imageData.release(), v8::Handle<v8::Object>(), m_isolate);
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
        arrayBuffer->setDeallocationObserver(V8ArrayBufferDeallocationObserver::instance());
        v8::V8::AdjustAmountOfExternalAllocatedMemory(arrayBuffer->byteLength());
        m_position += byteLength;
        return arrayBuffer.release();
    }

    bool readArrayBuffer(v8::Handle<v8::Value>* value)
    {
        RefPtr<ArrayBuffer> arrayBuffer = doReadArrayBuffer();
        if (!arrayBuffer)
            return false;
        *value = toV8(arrayBuffer.release(), v8::Handle<v8::Object>(), m_isolate);
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
            *value = toV8(Int8Array::create(arrayBuffer.release(), byteOffset, byteLength), v8::Handle<v8::Object>(), m_isolate);
            break;
        case UnsignedByteArrayTag:
            *value = toV8(Uint8Array::create(arrayBuffer.release(), byteOffset, byteLength), v8::Handle<v8::Object>(),  m_isolate);
            break;
        case UnsignedByteClampedArrayTag:
            *value = toV8(Uint8ClampedArray::create(arrayBuffer.release(), byteOffset, byteLength), v8::Handle<v8::Object>(), m_isolate);
            break;
        case ShortArrayTag: {
            uint32_t shortLength = byteLength / sizeof(int16_t);
            if (shortLength * sizeof(int16_t) != byteLength)
                return false;
            *value = toV8(Int16Array::create(arrayBuffer.release(), byteOffset, shortLength), v8::Handle<v8::Object>(), m_isolate);
            break;
        }
        case UnsignedShortArrayTag: {
            uint32_t shortLength = byteLength / sizeof(uint16_t);
            if (shortLength * sizeof(uint16_t) != byteLength)
                return false;
            *value = toV8(Uint16Array::create(arrayBuffer.release(), byteOffset, shortLength), v8::Handle<v8::Object>(), m_isolate);
            break;
        }
        case IntArrayTag: {
            uint32_t intLength = byteLength / sizeof(int32_t);
            if (intLength * sizeof(int32_t) != byteLength)
                return false;
            *value = toV8(Int32Array::create(arrayBuffer.release(), byteOffset, intLength), v8::Handle<v8::Object>(), m_isolate);
            break;
        }
        case UnsignedIntArrayTag: {
            uint32_t intLength = byteLength / sizeof(uint32_t);
            if (intLength * sizeof(uint32_t) != byteLength)
                return false;
            *value = toV8(Uint32Array::create(arrayBuffer.release(), byteOffset, intLength), v8::Handle<v8::Object>(), m_isolate);
            break;
        }
        case FloatArrayTag: {
            uint32_t floatLength = byteLength / sizeof(float);
            if (floatLength * sizeof(float) != byteLength)
                return false;
            *value = toV8(Float32Array::create(arrayBuffer.release(), byteOffset, floatLength), v8::Handle<v8::Object>(), m_isolate);
            break;
        }
        case DoubleArrayTag: {
            uint32_t floatLength = byteLength / sizeof(double);
            if (floatLength * sizeof(double) != byteLength)
                return false;
            *value = toV8(Float64Array::create(arrayBuffer.release(), byteOffset, floatLength), v8::Handle<v8::Object>(), m_isolate);
            break;
        }
        case DataViewTag:
            *value = toV8(DataView::create(arrayBuffer.release(), byteOffset, byteLength), v8::Handle<v8::Object>(), m_isolate);
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
        *value = toV8(blob, v8::Handle<v8::Object>(), m_isolate);
        return true;
    }

#if ENABLE(FILE_SYSTEM)
    bool readDOMFileSystem(v8::Handle<v8::Value>* value)
    {
        uint32_t type;
        String name;
        String url;
        if (!doReadUint32(&type))
            return false;
        if (!readWebCoreString(&name))
            return false;
        if (!readWebCoreString(&url))
            return false;
        RefPtr<DOMFileSystem> fs = DOMFileSystem::create(getScriptExecutionContext(), name, static_cast<WebCore::FileSystemType>(type), KURL(ParsedURLString, url), AsyncFileSystem::create());
        *value = toV8(fs.release(), v8::Handle<v8::Object>(), m_isolate);
        return true;
    }
#endif

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
        *value = toV8(file, v8::Handle<v8::Object>(), m_isolate);
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
        *value = toV8(fileList, v8::Handle<v8::Object>(), m_isolate);
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
    uint32_t m_version;
    v8::Isolate* m_isolate;
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
        if (!m_reader.readVersion(m_version) || m_version > wireFormatVersion)
            return v8NullWithCheck(m_reader.getIsolate());
        m_reader.setVersion(m_version);
        v8::HandleScope scope;
        while (!m_reader.isEof()) {
            if (!doDeserialize())
                return v8NullWithCheck(m_reader.getIsolate());
        }
        if (stackDepth() != 1 || m_openCompositeReferenceStack.size())
            return v8NullWithCheck(m_reader.getIsolate());
        v8::Handle<v8::Value> result = scope.Close(element(0));
        return result;
    }

    virtual bool newSparseArray(uint32_t)
    {
        v8::Local<v8::Array> array = v8::Array::New(0);
        openComposite(array);
        return true;
    }

    virtual bool newDenseArray(uint32_t length)
    {
        v8::Local<v8::Array> array = v8::Array::New(length);
        openComposite(array);
        return true;
    }

    virtual bool consumeTopOfStack(v8::Handle<v8::Value>* object)
    {
        if (stackDepth() < 1)
            return false;
        *object = element(stackDepth() - 1);
        pop(1);
        return true;
    }

    virtual bool completeArray(uint32_t length, v8::Handle<v8::Value>* value)
    {
        if (length > stackDepth())
            return false;
        v8::Local<v8::Array> array;
        if (m_version > 0) {
            v8::Local<v8::Value> composite;
            if (!closeComposite(&composite))
                return false;
            array = composite.As<v8::Array>();
        } else
            array = v8::Array::New(length);
        if (array.IsEmpty())
            return false;
        const int depth = stackDepth() - length;
        // The V8 API ensures space exists for any index argument to Set; it will (eg) resize arrays as necessary.
        for (unsigned i = 0; i < length; ++i)
            array->Set(i, element(depth + i));
        pop(length);
        *value = array;
        return true;
    }

    virtual bool newObject()
    {
        v8::Local<v8::Object> object = v8::Object::New();
        if (object.IsEmpty())
            return false;
        openComposite(object);
        return true;
    }

    virtual bool completeObject(uint32_t numProperties, v8::Handle<v8::Value>* value)
    {
        v8::Local<v8::Object> object;
        if (m_version > 0) {
            v8::Local<v8::Value> composite;
            if (!closeComposite(&composite))
                return false;
            object = composite.As<v8::Object>();
        } else
            object = v8::Object::New();
        if (object.IsEmpty())
            return false;
        return initializeObject(object, numProperties, value);
    }

    virtual bool completeSparseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>* value)
    {
        v8::Local<v8::Array> array;
        if (m_version > 0) {
            v8::Local<v8::Value> composite;
            if (!closeComposite(&composite))
                return false;
            array = composite.As<v8::Array>();
        } else
            array = v8::Array::New();
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
        *object = toV8(m_transferredMessagePorts->at(index).get(), v8::Handle<v8::Object>(), m_reader.getIsolate());
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
            RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(m_arrayBufferContents->at(index));
            buffer->setDeallocationObserver(V8ArrayBufferDeallocationObserver::instance());
            v8::V8::AdjustAmountOfExternalAllocatedMemory(buffer->byteLength());
            result = toV8Object(buffer.get(), m_reader.getIsolate());
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

    void openComposite(const v8::Local<v8::Value>& object)
    {
        uint32_t newObjectReference = m_objectPool.size();
        m_openCompositeReferenceStack.append(newObjectReference);
        m_objectPool.append(object);
    }

    bool closeComposite(v8::Handle<v8::Value>* object)
    {
        if (!m_openCompositeReferenceStack.size())
            return false;
        uint32_t objectReference = m_openCompositeReferenceStack[m_openCompositeReferenceStack.size() - 1];
        m_openCompositeReferenceStack.shrink(m_openCompositeReferenceStack.size() - 1);
        if (objectReference >= m_objectPool.size())
            return false;
        *object = m_objectPool[objectReference];
        return true;
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

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(v8::Handle<v8::Value> value, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, bool& didThrow)
{
    return create(value, messagePorts, arrayBuffers, didThrow, v8::Isolate::GetCurrent());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(v8::Handle<v8::Value> value, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, bool& didThrow, v8::Isolate* isolate)
{
    return adoptRef(new SerializedScriptValue(value, messagePorts, arrayBuffers, didThrow, isolate));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(v8::Handle<v8::Value> value)
{
    return create(value, v8::Isolate::GetCurrent());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    bool didThrow;
    return adoptRef(new SerializedScriptValue(value, 0, 0, didThrow, isolate));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::createFromWire(const String& data)
{
    return adoptRef(new SerializedScriptValue(data));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::createFromWireBytes(const Vector<uint8_t>& data)
{
    // Decode wire data from big endian to host byte order.
    ASSERT(!(data.size() % sizeof(UChar)));
    size_t length = data.size() / sizeof(UChar);
    Vector<UChar> buffer(length);
    const UChar* src = reinterpret_cast<const UChar*>(data.data());
    UChar* dst = buffer.data();
    for (size_t i = 0; i < length; i++)
        dst[i] = ntohs(src[i]);

    return createFromWire(String::adopt(buffer));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(const String& data)
{
    return create(data, v8::Isolate::GetCurrent());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(const String& data, v8::Isolate* isolate)
{
    Writer writer(isolate);
    writer.writeWebCoreString(data);
    String wireData = StringImpl::adopt(writer.data());
    return adoptRef(new SerializedScriptValue(wireData));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create()
{
    return adoptRef(new SerializedScriptValue());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::nullValue()
{
    return nullValue(v8::Isolate::GetCurrent());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::nullValue(v8::Isolate* isolate)
{
    Writer writer(isolate);
    writer.writeNull();
    String wireData = StringImpl::adopt(writer.data());
    return adoptRef(new SerializedScriptValue(wireData));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::undefinedValue()
{
    return undefinedValue(v8::Isolate::GetCurrent());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::undefinedValue(v8::Isolate* isolate)
{
    Writer writer(isolate);
    writer.writeUndefined();
    String wireData = StringImpl::adopt(writer.data());
    return adoptRef(new SerializedScriptValue(wireData));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::booleanValue(bool value)
{
    return booleanValue(value, v8::Isolate::GetCurrent());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::booleanValue(bool value, v8::Isolate* isolate)
{
    Writer writer(isolate);
    if (value)
        writer.writeTrue();
    else
        writer.writeFalse();
    String wireData = StringImpl::adopt(writer.data());
    return adoptRef(new SerializedScriptValue(wireData));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::numberValue(double value)
{
    return numberValue(value, v8::Isolate::GetCurrent());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::numberValue(double value, v8::Isolate* isolate)
{
    Writer writer(isolate);
    writer.writeNumber(value);
    String wireData = StringImpl::adopt(writer.data());
    return adoptRef(new SerializedScriptValue(wireData));
}

Vector<uint8_t> SerializedScriptValue::toWireBytes() const
{
    // Convert serialized string to big endian wire data.
    size_t length = m_data.length();
    Vector<uint8_t> result(length * sizeof(UChar));

    const UChar* src = m_data.characters();
    UChar* dst = reinterpret_cast<UChar*>(result.data());
    for (size_t i = 0; i < length; i++)
        dst[i] = htons(src[i]);

    return result;
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::release()
{
    RefPtr<SerializedScriptValue> result = adoptRef(new SerializedScriptValue(m_data));
    m_data = String();
    return result.release();
}

SerializedScriptValue::SerializedScriptValue()
    : m_externallyAllocatedMemory(0)
{
}

template<typename T>
inline void neuterBinding(T* object) 
{
    Vector<DOMDataStore*>& allStores = V8PerIsolateData::current()->allStores();
    for (size_t i = 0; i < allStores.size(); i++) {
        v8::Handle<v8::Object> wrapper = allStores[i]->get(object);
        if (!wrapper.IsEmpty())
            wrapper->SetIndexedPropertiesToExternalArrayData(0, v8::kExternalByteArray, 0);
    }
}

PassOwnPtr<SerializedScriptValue::ArrayBufferContentsArray> SerializedScriptValue::transferArrayBuffers(ArrayBufferArray& arrayBuffers, bool& didThrow, v8::Isolate* isolate)
{
    for (size_t i = 0; i < arrayBuffers.size(); i++) {
        if (arrayBuffers[i]->isNeutered()) {
            setDOMException(INVALID_STATE_ERR, isolate);
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
            setDOMException(INVALID_STATE_ERR, isolate);
            didThrow = true;
            return nullptr;
        }

        neuterBinding(arrayBuffers[i].get());
        for (size_t j = 0; j < neuteredViews.size(); j++)
            neuterBinding(neuteredViews[j].get());
    }
    return contents.release();
}

SerializedScriptValue::SerializedScriptValue(v8::Handle<v8::Value> value, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, bool& didThrow, v8::Isolate* isolate)
    : m_externallyAllocatedMemory(0)
{
    didThrow = false;
    Writer writer(isolate);
    Serializer::Status status;
    {
        v8::TryCatch tryCatch;
        Serializer serializer(writer, messagePorts, arrayBuffers, m_blobURLs, tryCatch);
        status = serializer.serialize(value);
        if (status == Serializer::JSException) {
            // If there was a JS exception thrown, re-throw it.
            didThrow = true;
            tryCatch.ReThrow();
            return;
        }
    }
    switch (status) {
    case Serializer::InputError:
    case Serializer::DataCloneError:
        // If there was an input error, throw a new exception outside
        // of the TryCatch scope.
        didThrow = true;
        setDOMException(DATA_CLONE_ERR, isolate);
        return;
    case Serializer::InvalidStateError:
        didThrow = true;
        setDOMException(INVALID_STATE_ERR, isolate);
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
            m_arrayBufferContentsArray = transferArrayBuffers(*arrayBuffers, didThrow, isolate);
        return;
    case Serializer::JSException:
        // We should never get here because this case was handled above.
        break;
    }
    ASSERT_NOT_REACHED();
}

SerializedScriptValue::SerializedScriptValue(const String& wireData)
    : m_externallyAllocatedMemory(0)
{
    m_data = wireData.isolatedCopy();
}

v8::Handle<v8::Value> SerializedScriptValue::deserialize(MessagePortArray* messagePorts)
{
    return deserialize(v8::Isolate::GetCurrent(), messagePorts);
}

v8::Handle<v8::Value> SerializedScriptValue::deserialize(v8::Isolate* isolate, MessagePortArray* messagePorts)
{
    if (!m_data.impl())
        return v8NullWithCheck(isolate);
    COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
    Reader reader(reinterpret_cast<const uint8_t*>(m_data.impl()->characters()), 2 * m_data.length(), isolate);
    Deserializer deserializer(reader, messagePorts, m_arrayBufferContentsArray.get());
    return deserializer.deserialize();
}

#if ENABLE(INSPECTOR)
ScriptValue SerializedScriptValue::deserializeForInspector(ScriptState* scriptState)
{
    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(scriptState->context());

    return ScriptValue(deserialize(scriptState->isolate()));
}
#endif

void SerializedScriptValue::registerMemoryAllocatedWithCurrentScriptContext()
{
    if (m_externallyAllocatedMemory)
        return;
    m_externallyAllocatedMemory = static_cast<intptr_t>(m_data.length());
    v8::V8::AdjustAmountOfExternalAllocatedMemory(m_externallyAllocatedMemory);
}

SerializedScriptValue::~SerializedScriptValue()
{
    // If the allocated memory was not registered before, then this class is likely
    // used in a context other then Worker's onmessage environment and the presence of
    // current v8 context is not guaranteed. Avoid calling v8 then.
    if (m_externallyAllocatedMemory) {
        ASSERT(v8::Isolate::GetCurrent());
        v8::V8::AdjustAmountOfExternalAllocatedMemory(-m_externallyAllocatedMemory);
    }
}

uint32_t SerializedScriptValue::wireFormatVersion()
{
    return WebCore::wireFormatVersion;
}

} // namespace WebCore
