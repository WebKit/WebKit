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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "SerializedScriptValue.h"

#include "Blob.h"
#include "File.h"
#include "FileList.h"
#include "ImageData.h"
#include "JSBlob.h"
#include "JSDOMGlobalObject.h"
#include "JSFile.h"
#include "JSFileList.h"
#include "JSImageData.h"
#include <JavaScriptCore/APICast.h>
#include <runtime/DateInstance.h>
#include <runtime/Error.h>
#include <runtime/ExceptionHelpers.h>
#include <runtime/JSLock.h>
#include <runtime/PropertyNameArray.h>
#include <wtf/ByteArray.h>
#include <wtf/HashTraits.h>
#include <wtf/Vector.h>

using namespace JSC;

namespace WebCore {

class SerializedObject : public SharedSerializedData
{
public:
    typedef Vector<RefPtr<StringImpl> > PropertyNameList;
    typedef Vector<SerializedScriptValueData> ValueList;

    void set(const Identifier& propertyName, const SerializedScriptValueData& value)
    {
        ASSERT(m_names.size() == m_values.size());
        m_names.append(identifierToString(propertyName).crossThreadString().impl());
        m_values.append(value);
    }

    PropertyNameList& names() { return m_names; }

    ValueList& values() { return m_values; }

    static PassRefPtr<SerializedObject> create()
    {
        return adoptRef(new SerializedObject);
    }

    void clear()
    {
        m_names.clear();
        m_values.clear();
    }

private:
    SerializedObject() { }
    PropertyNameList m_names;
    ValueList m_values;
};

class SerializedArray : public SharedSerializedData
{
    typedef HashMap<unsigned, SerializedScriptValueData, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned> > SparseMap;
public:
    void setIndex(unsigned index, const SerializedScriptValueData& value)
    {
        ASSERT(index < m_length);
        if (index == m_compactStorage.size())
            m_compactStorage.append(value);
        else
            m_sparseStorage.set(index, value);
    }

    bool canDoFastRead(unsigned index) const
    {
        ASSERT(index < m_length);
        return index < m_compactStorage.size();
    }

    const SerializedScriptValueData& getIndex(unsigned index)
    {
        ASSERT(index < m_compactStorage.size());
        return m_compactStorage[index];
    }

    SerializedScriptValueData getSparseIndex(unsigned index, bool& hasIndex)
    {
        ASSERT(index >= m_compactStorage.size());
        ASSERT(index < m_length);
        SparseMap::iterator iter = m_sparseStorage.find(index);
        if (iter == m_sparseStorage.end()) {
            hasIndex = false;
            return SerializedScriptValueData();
        }
        hasIndex = true;
        return iter->second;
    }

    unsigned length() const
    {
        return m_length;
    }

    static PassRefPtr<SerializedArray> create(unsigned length)
    {
        return adoptRef(new SerializedArray(length));
    }

    void clear()
    {
        m_compactStorage.clear();
        m_sparseStorage.clear();
        m_length = 0;
    }
private:
    SerializedArray(unsigned length)
        : m_length(length)
    {
    }

    Vector<SerializedScriptValueData> m_compactStorage;
    SparseMap m_sparseStorage;
    unsigned m_length;
};

class SerializedBlob : public SharedSerializedData {
public:
    static PassRefPtr<SerializedBlob> create(const Blob* blob)
    {
        return adoptRef(new SerializedBlob(blob));
    }

    const KURL& url() const { return m_url; }
    const String& type() const { return m_type; }
    unsigned long long size() const { return m_size; }

private:
    SerializedBlob(const Blob* blob)
        : m_url(blob->url().copy())
        , m_type(blob->type().crossThreadString())
        , m_size(blob->size())
    {
    }

    KURL m_url;
    String m_type;
    unsigned long long m_size;
};

class SerializedFile : public SharedSerializedData {
public:
    static PassRefPtr<SerializedFile> create(const File* file)
    {
        return adoptRef(new SerializedFile(file));
    }

    const String& path() const { return m_path; }
    const KURL& url() const { return m_url; }
    const String& type() const { return m_type; }

private:
    SerializedFile(const File* file)
        : m_path(file->path().crossThreadString())
        , m_url(file->url().copy())
        , m_type(file->type().crossThreadString())
    {
    }

    String m_path;
    KURL m_url;
    String m_type;
};

class SerializedFileList : public SharedSerializedData {
public:
    struct FileData {
        String path;
        KURL url;
        String type;
    };

    static PassRefPtr<SerializedFileList> create(const FileList* list)
    {
        return adoptRef(new SerializedFileList(list));
    }

    unsigned length() const { return m_files.size(); }
    const FileData& item(unsigned idx) { return m_files[idx]; }

private:
    SerializedFileList(const FileList* list)
    {
        unsigned length = list->length();
        m_files.reserveCapacity(length);
        for (unsigned i = 0; i < length; i++) {
            File* file = list->item(i);
            FileData fileData;
            fileData.path = file->path().crossThreadString();
            fileData.url = file->url().copy();
            fileData.type = file->type().crossThreadString();
            m_files.append(fileData);
        }
    }

    Vector<FileData> m_files;
};

class SerializedImageData : public SharedSerializedData {
public:
    static PassRefPtr<SerializedImageData> create(const ImageData* imageData)
    {
        return adoptRef(new SerializedImageData(imageData));
    }
    
    unsigned width() const { return m_width; }
    unsigned height() const { return m_height; }
    WTF::ByteArray* data() const { return m_storage.get(); }
private:
    SerializedImageData(const ImageData* imageData)
        : m_width(imageData->width())
        , m_height(imageData->height())
    {
        WTF::ByteArray* array = imageData->data()->data();
        m_storage = WTF::ByteArray::create(array->length());
        memcpy(m_storage->data(), array->data(), array->length());
    }
    unsigned m_width;
    unsigned m_height;
    RefPtr<WTF::ByteArray> m_storage;
};

SerializedScriptValueData::SerializedScriptValueData(RefPtr<SerializedObject> data)
    : m_type(ObjectType)
    , m_sharedData(data)
{
}

SerializedScriptValueData::SerializedScriptValueData(RefPtr<SerializedArray> data)
    : m_type(ArrayType)
    , m_sharedData(data)
{
}

SerializedScriptValueData::SerializedScriptValueData(const FileList* fileList)
    : m_type(FileListType)
    , m_sharedData(SerializedFileList::create(fileList))
{
}

SerializedScriptValueData::SerializedScriptValueData(const ImageData* imageData)
    : m_type(ImageDataType)
    , m_sharedData(SerializedImageData::create(imageData))
{
}

SerializedScriptValueData::SerializedScriptValueData(const Blob* blob)
    : m_type(BlobType)
    , m_sharedData(SerializedBlob::create(blob))
{
}

SerializedScriptValueData::SerializedScriptValueData(const File* file)
    : m_type(FileType)
    , m_sharedData(SerializedFile::create(file))
{
}

SerializedArray* SharedSerializedData::asArray()
{
    return static_cast<SerializedArray*>(this);
}

SerializedObject* SharedSerializedData::asObject()
{
    return static_cast<SerializedObject*>(this);
}

SerializedBlob* SharedSerializedData::asBlob()
{
    return static_cast<SerializedBlob*>(this);
}

SerializedFile* SharedSerializedData::asFile()
{
    return static_cast<SerializedFile*>(this);
}

SerializedFileList* SharedSerializedData::asFileList()
{
    return static_cast<SerializedFileList*>(this);
}

SerializedImageData* SharedSerializedData::asImageData()
{
    return static_cast<SerializedImageData*>(this);
}

static const unsigned maximumFilterRecursion = 40000;
enum WalkerState { StateUnknown, ArrayStartState, ArrayStartVisitMember, ArrayEndVisitMember,
    ObjectStartState, ObjectStartVisitMember, ObjectEndVisitMember };
template <typename TreeWalker> typename TreeWalker::OutputType walk(TreeWalker& context, typename TreeWalker::InputType in)
{
    typedef typename TreeWalker::InputObject InputObject;
    typedef typename TreeWalker::InputArray InputArray;
    typedef typename TreeWalker::OutputObject OutputObject;
    typedef typename TreeWalker::OutputArray OutputArray;
    typedef typename TreeWalker::InputType InputType;
    typedef typename TreeWalker::OutputType OutputType;
    typedef typename TreeWalker::PropertyList PropertyList;

    Vector<uint32_t, 16> indexStack;
    Vector<uint32_t, 16> lengthStack;
    Vector<PropertyList, 16> propertyStack;
    Vector<InputObject, 16> inputObjectStack;
    Vector<InputArray, 16> inputArrayStack;
    Vector<OutputObject, 16> outputObjectStack;
    Vector<OutputArray, 16> outputArrayStack;
    Vector<WalkerState, 16> stateStack;
    WalkerState state = StateUnknown;
    InputType inValue = in;
    OutputType outValue = context.null();

    unsigned tickCount = context.ticksUntilNextCheck();
    while (1) {
        switch (state) {
            arrayStartState:
            case ArrayStartState: {
                ASSERT(context.isArray(inValue));
                if (inputObjectStack.size() + inputArrayStack.size() > maximumFilterRecursion) {
                    context.throwStackOverflow();
                    return context.null();
                }

                InputArray inArray = context.asInputArray(inValue);
                unsigned length = context.length(inArray);
                OutputArray outArray = context.createOutputArray(length);
                if (!context.startArray(inArray, outArray))
                    return context.null();
                inputArrayStack.append(inArray);
                outputArrayStack.append(outArray);
                indexStack.append(0);
                lengthStack.append(length);
                // fallthrough
            }
            arrayStartVisitMember:
            case ArrayStartVisitMember: {
                if (!--tickCount) {
                    if (context.didTimeOut()) {
                        context.throwInterruptedException();
                        return context.null();
                    }
                    tickCount = context.ticksUntilNextCheck();
                }

                InputArray array = inputArrayStack.last();
                uint32_t index = indexStack.last();
                if (index == lengthStack.last()) {
                    InputArray inArray = inputArrayStack.last();
                    OutputArray outArray = outputArrayStack.last();
                    context.endArray(inArray, outArray);
                    outValue = outArray;
                    inputArrayStack.removeLast();
                    outputArrayStack.removeLast();
                    indexStack.removeLast();
                    lengthStack.removeLast();
                    break;
                }
                if (context.canDoFastRead(array, index))
                    inValue = context.getIndex(array, index);
                else {
                    bool hasIndex = false;
                    inValue = context.getSparseIndex(array, index, hasIndex);
                    if (!hasIndex) {
                        indexStack.last()++;
                        goto arrayStartVisitMember;
                    }
                }

                if (OutputType transformed = context.convertIfTerminal(inValue))
                    outValue = transformed;
                else {
                    stateStack.append(ArrayEndVisitMember);
                    goto stateUnknown;
                }
                // fallthrough
            }
            case ArrayEndVisitMember: {
                OutputArray outArray = outputArrayStack.last();
                context.putProperty(outArray, indexStack.last(), outValue);
                indexStack.last()++;
                goto arrayStartVisitMember;
            }
            objectStartState:
            case ObjectStartState: {
                ASSERT(context.isObject(inValue));
                if (inputObjectStack.size() + inputArrayStack.size() > maximumFilterRecursion) {
                    context.throwStackOverflow();
                    return context.null();
                }
                InputObject inObject = context.asInputObject(inValue);
                OutputObject outObject = context.createOutputObject();
                if (!context.startObject(inObject, outObject))
                    return context.null();
                inputObjectStack.append(inObject);
                outputObjectStack.append(outObject);
                indexStack.append(0);
                context.getPropertyNames(inObject, propertyStack);
                // fallthrough
            }
            objectStartVisitMember:
            case ObjectStartVisitMember: {
                if (!--tickCount) {
                    if (context.didTimeOut()) {
                        context.throwInterruptedException();
                        return context.null();
                    }
                    tickCount = context.ticksUntilNextCheck();
                }

                InputObject object = inputObjectStack.last();
                uint32_t index = indexStack.last();
                PropertyList& properties = propertyStack.last();
                if (index == properties.size()) {
                    InputObject inObject = inputObjectStack.last();
                    OutputObject outObject = outputObjectStack.last();
                    context.endObject(inObject, outObject);
                    outValue = outObject;
                    inputObjectStack.removeLast();
                    outputObjectStack.removeLast();
                    indexStack.removeLast();
                    propertyStack.removeLast();
                    break;
                }
                inValue = context.getProperty(object, properties[index], index);

                if (context.shouldTerminate())
                    return context.null();

                if (OutputType transformed = context.convertIfTerminal(inValue))
                    outValue = transformed;
                else {
                    stateStack.append(ObjectEndVisitMember);
                    goto stateUnknown;
                }
                // fallthrough
            }
            case ObjectEndVisitMember: {
                context.putProperty(outputObjectStack.last(), propertyStack.last()[indexStack.last()], outValue);
                if (context.shouldTerminate())
                    return context.null();

                indexStack.last()++;
                goto objectStartVisitMember;
            }
            stateUnknown:
            case StateUnknown:
                if (OutputType transformed = context.convertIfTerminal(inValue)) {
                    outValue = transformed;
                    break;
                }
                if (context.isArray(inValue))
                    goto arrayStartState;
                goto objectStartState;
        }
        if (stateStack.isEmpty())
            break;

        state = stateStack.last();
        stateStack.removeLast();

        if (!--tickCount) {
            if (context.didTimeOut()) {
                context.throwInterruptedException();
                return context.null();
            }
            tickCount = context.ticksUntilNextCheck();
        }
    }
    return outValue;
}

struct BaseWalker {
    BaseWalker(ExecState* exec)
        : m_exec(exec)
        , m_timeoutChecker(exec->globalData().timeoutChecker)
    {
        m_timeoutChecker.reset();
    }
    ExecState* m_exec;
    TimeoutChecker m_timeoutChecker;
    MarkedArgumentBuffer m_gcBuffer;

    bool shouldTerminate()
    {
        return m_exec->hadException();
    }

    unsigned ticksUntilNextCheck()
    {
        return m_timeoutChecker.ticksUntilNextCheck();
    }

    bool didTimeOut()
    {
        return m_timeoutChecker.didTimeOut(m_exec);
    }

    void throwStackOverflow()
    {
        throwError(m_exec, createStackOverflowError(m_exec));
    }

    void throwInterruptedException()
    {
        throwError(m_exec, createInterruptedExecutionException(&m_exec->globalData()));
    }
};

struct SerializingTreeWalker : public BaseWalker {
    typedef JSValue InputType;
    typedef JSArray* InputArray;
    typedef JSObject* InputObject;
    typedef SerializedScriptValueData OutputType;
    typedef RefPtr<SerializedArray> OutputArray;
    typedef RefPtr<SerializedObject> OutputObject;
    typedef PropertyNameArray PropertyList;

    SerializingTreeWalker(ExecState* exec)
        : BaseWalker(exec)
    {
    }

    OutputType null() { return SerializedScriptValueData(); }

    bool isArray(JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return isJSArray(&m_exec->globalData(), object) || object->inherits(&JSArray::info);
    }

    bool isObject(JSValue value)
    {
        return value.isObject();
    }

    JSArray* asInputArray(JSValue value)
    {
        return asArray(value);
    }

    JSObject* asInputObject(JSValue value)
    {
        return asObject(value);
    }

    PassRefPtr<SerializedArray> createOutputArray(unsigned length)
    {
        return SerializedArray::create(length);
    }

    PassRefPtr<SerializedObject> createOutputObject()
    {
        return SerializedObject::create();
    }

    uint32_t length(JSValue array)
    {
        ASSERT(array.isObject());
        JSObject* object = asObject(array);
        return object->get(m_exec, m_exec->propertyNames().length).toUInt32(m_exec);
    }

    bool canDoFastRead(JSArray* array, unsigned index)
    {
        return isJSArray(&m_exec->globalData(), array) && array->canGetIndex(index);
    }

    JSValue getIndex(JSArray* array, unsigned index)
    {
        return array->getIndex(index);
    }

    JSValue getSparseIndex(JSObject* object, unsigned propertyName, bool& hasIndex)
    {
        PropertySlot slot(object);
        if (object->getOwnPropertySlot(m_exec, propertyName, slot)) {
            hasIndex = true;
            return slot.getValue(m_exec, propertyName);
        }
        hasIndex = false;
        return jsNull();
    }

    JSValue getProperty(JSObject* object, const Identifier& propertyName, unsigned)
    {
        PropertySlot slot(object);
        if (object->getOwnPropertySlot(m_exec, propertyName, slot))
            return slot.getValue(m_exec, propertyName);
        return jsNull();
    }

    SerializedScriptValueData convertIfTerminal(JSValue value)
    {
        if (!value.isCell())
            return SerializedScriptValueData(value);

        if (value.isString())
            return SerializedScriptValueData(ustringToString(asString(value)->value(m_exec)));

        if (value.isNumber())
            return SerializedScriptValueData(SerializedScriptValueData::NumberType, value.uncheckedGetNumber());

        if (value.isObject() && asObject(value)->inherits(&DateInstance::info))
            return SerializedScriptValueData(SerializedScriptValueData::DateType, asDateInstance(value)->internalNumber());

        if (isArray(value))
            return SerializedScriptValueData();

        if (value.isObject()) {
            JSObject* obj = asObject(value);
            if (obj->inherits(&JSFile::s_info))
                return SerializedScriptValueData(toFile(obj));
            if (obj->inherits(&JSBlob::s_info))
                return SerializedScriptValueData(toBlob(obj));
            if (obj->inherits(&JSFileList::s_info))
                return SerializedScriptValueData(toFileList(obj));
            if (obj->inherits(&JSImageData::s_info))
                return SerializedScriptValueData(toImageData(obj));
                
            CallData unusedData;
            if (getCallData(value, unusedData) == CallTypeNone)
                return SerializedScriptValueData();
        }
        // Any other types are expected to serialize as null.
        return SerializedScriptValueData(jsNull());
    }

    void getPropertyNames(JSObject* object, Vector<PropertyNameArray, 16>& propertyStack)
    {
        propertyStack.append(PropertyNameArray(m_exec));
        object->getOwnPropertyNames(m_exec, propertyStack.last());
    }

    void putProperty(RefPtr<SerializedArray> array, unsigned propertyName, const SerializedScriptValueData& value)
    {
        array->setIndex(propertyName, value);
    }

    void putProperty(RefPtr<SerializedObject> object, const Identifier& propertyName, const SerializedScriptValueData& value)
    {
        object->set(propertyName, value);
    }

    bool startArray(JSArray* inArray, RefPtr<SerializedArray>)
    {
        // Cycle detection
        if (!m_cycleDetector.add(inArray).second) {
            throwError(m_exec, createTypeError(m_exec, "Cannot post cyclic structures."));
            return false;
        }
        m_gcBuffer.append(inArray);
        return true;
    }

    void endArray(JSArray* inArray, RefPtr<SerializedArray>)
    {
        m_cycleDetector.remove(inArray);
        m_gcBuffer.removeLast();
    }

    bool startObject(JSObject* inObject, RefPtr<SerializedObject>)
    {
        // Cycle detection
        if (!m_cycleDetector.add(inObject).second) {
            throwError(m_exec, createTypeError(m_exec, "Cannot post cyclic structures."));
            return false;
        }
        m_gcBuffer.append(inObject);
        return true;
    }

    void endObject(JSObject* inObject, RefPtr<SerializedObject>)
    {
        m_cycleDetector.remove(inObject);
        m_gcBuffer.removeLast();
    }

private:
    HashSet<JSObject*> m_cycleDetector;
};

SerializedScriptValueData SerializedScriptValueData::serialize(ExecState* exec, JSValue inValue)
{
    SerializingTreeWalker context(exec);
    return walk<SerializingTreeWalker>(context, inValue);
}


struct DeserializingTreeWalker : public BaseWalker {
    typedef SerializedScriptValueData InputType;
    typedef RefPtr<SerializedArray> InputArray;
    typedef RefPtr<SerializedObject> InputObject;
    typedef JSValue OutputType;
    typedef JSArray* OutputArray;
    typedef JSObject* OutputObject;
    typedef SerializedObject::PropertyNameList PropertyList;

    DeserializingTreeWalker(ExecState* exec, JSGlobalObject* globalObject, bool mustCopy)
        : BaseWalker(exec)
        , m_globalObject(globalObject)
        , m_isDOMGlobalObject(globalObject->inherits(&JSDOMGlobalObject::s_info))
        , m_mustCopy(mustCopy)
    {
    }

    OutputType null() { return jsNull(); }

    bool isArray(const SerializedScriptValueData& value)
    {
        return value.type() == SerializedScriptValueData::ArrayType;
    }

    bool isObject(const SerializedScriptValueData& value)
    {
        return value.type() == SerializedScriptValueData::ObjectType;
    }

    SerializedArray* asInputArray(const SerializedScriptValueData& value)
    {
        return value.asArray();
    }

    SerializedObject* asInputObject(const SerializedScriptValueData& value)
    {
        return value.asObject();
    }

    JSArray* createOutputArray(unsigned length)
    {
        JSArray* array = constructEmptyArray(m_exec, m_globalObject);
        array->setLength(length);
        return array;
    }

    JSObject* createOutputObject()
    {
        return constructEmptyObject(m_exec, m_globalObject);
    }

    uint32_t length(RefPtr<SerializedArray> array)
    {
        return array->length();
    }

    bool canDoFastRead(RefPtr<SerializedArray> array, unsigned index)
    {
        return array->canDoFastRead(index);
    }

    SerializedScriptValueData getIndex(RefPtr<SerializedArray> array, unsigned index)
    {
        return array->getIndex(index);
    }

    SerializedScriptValueData getSparseIndex(RefPtr<SerializedArray> array, unsigned propertyName, bool& hasIndex)
    {
        return array->getSparseIndex(propertyName, hasIndex);
    }

    SerializedScriptValueData getProperty(RefPtr<SerializedObject> object, const RefPtr<StringImpl>& propertyName, unsigned propertyIndex)
    {
        ASSERT(object->names()[propertyIndex] == propertyName);
        UNUSED_PARAM(propertyName);
        return object->values()[propertyIndex];
    }

    JSValue convertIfTerminal(SerializedScriptValueData& value)
    {
        switch (value.type()) {
            case SerializedScriptValueData::ArrayType:
            case SerializedScriptValueData::ObjectType:
                return JSValue();
            case SerializedScriptValueData::StringType:
                return jsString(m_exec, value.asString().crossThreadString());
            case SerializedScriptValueData::ImmediateType:
                return value.asImmediate();
            case SerializedScriptValueData::NumberType:
                return jsNumber(m_exec, value.asDouble());
            case SerializedScriptValueData::DateType:
                return new (m_exec) DateInstance(m_exec, m_globalObject->dateStructure(), value.asDouble());
            case SerializedScriptValueData::BlobType: {
                if (!m_isDOMGlobalObject)
                    return jsNull();
                SerializedBlob* serializedBlob = value.asBlob();
                ScriptExecutionContext* scriptExecutionContext = static_cast<JSDOMGlobalObject*>(m_exec->lexicalGlobalObject())->scriptExecutionContext();
                ASSERT(scriptExecutionContext);
                return toJS(m_exec, static_cast<JSDOMGlobalObject*>(m_globalObject), Blob::create(scriptExecutionContext, serializedBlob->url(), serializedBlob->type(), serializedBlob->size()));
            }
            case SerializedScriptValueData::FileType: {
                if (!m_isDOMGlobalObject)
                    return jsNull();
                SerializedFile* serializedFile = value.asFile();
                ScriptExecutionContext* scriptExecutionContext = static_cast<JSDOMGlobalObject*>(m_exec->lexicalGlobalObject())->scriptExecutionContext();
                ASSERT(scriptExecutionContext);
                return toJS(m_exec, static_cast<JSDOMGlobalObject*>(m_globalObject), File::create(scriptExecutionContext, serializedFile->path(), serializedFile->url(), serializedFile->type()));
            }
            case SerializedScriptValueData::FileListType: {
                if (!m_isDOMGlobalObject)
                    return jsNull();
                RefPtr<FileList> result = FileList::create();
                SerializedFileList* serializedFileList = value.asFileList();
                unsigned length = serializedFileList->length();
                ScriptExecutionContext* scriptExecutionContext = static_cast<JSDOMGlobalObject*>(m_exec->lexicalGlobalObject())->scriptExecutionContext();
                ASSERT(scriptExecutionContext);
                for (unsigned i = 0; i < length; i++) {
                    const SerializedFileList::FileData& fileData = serializedFileList->item(i);
                    result->append(File::create(scriptExecutionContext, fileData.path, fileData.url, fileData.type));
                }
                return toJS(m_exec, static_cast<JSDOMGlobalObject*>(m_globalObject), result.get());
            }
            case SerializedScriptValueData::ImageDataType: {
                if (!m_isDOMGlobalObject)
                    return jsNull();
                SerializedImageData* serializedImageData = value.asImageData();
                RefPtr<ImageData> result = ImageData::create(serializedImageData->width(), serializedImageData->height());
                memcpy(result->data()->data()->data(), serializedImageData->data()->data(), serializedImageData->data()->length());
                return toJS(m_exec, static_cast<JSDOMGlobalObject*>(m_globalObject), result.get());
            }
            case SerializedScriptValueData::EmptyType:
                ASSERT_NOT_REACHED();
                return jsNull();
        }
        ASSERT_NOT_REACHED();
        return jsNull();
    }

    void getPropertyNames(RefPtr<SerializedObject> object, Vector<SerializedObject::PropertyNameList, 16>& properties)
    {
        properties.append(object->names());
    }

    void putProperty(JSArray* array, unsigned propertyName, JSValue value)
    {
        array->put(m_exec, propertyName, value);
    }

    void putProperty(JSObject* object, const RefPtr<StringImpl> propertyName, JSValue value)
    {
        object->putDirect(Identifier(m_exec, stringToUString(String(propertyName))), value);
    }

    bool startArray(RefPtr<SerializedArray>, JSArray* outArray)
    {
        m_gcBuffer.append(outArray);
        return true;
    }
    void endArray(RefPtr<SerializedArray>, JSArray*)
    {
        m_gcBuffer.removeLast();
    }
    bool startObject(RefPtr<SerializedObject>, JSObject* outObject)
    {
        m_gcBuffer.append(outObject);
        return true;
    }
    void endObject(RefPtr<SerializedObject>, JSObject*)
    {
        m_gcBuffer.removeLast();
    }

private:
    void* operator new(size_t);
    JSGlobalObject* m_globalObject;
    bool m_isDOMGlobalObject;
    bool m_mustCopy;
};

JSValue SerializedScriptValueData::deserialize(ExecState* exec, JSGlobalObject* global, bool mustCopy) const
{
    DeserializingTreeWalker context(exec, global, mustCopy);
    return walk<DeserializingTreeWalker>(context, *this);
}

struct TeardownTreeWalker {
    typedef SerializedScriptValueData InputType;
    typedef RefPtr<SerializedArray> InputArray;
    typedef RefPtr<SerializedObject> InputObject;
    typedef bool OutputType;
    typedef bool OutputArray;
    typedef bool OutputObject;
    typedef SerializedObject::PropertyNameList PropertyList;

    bool shouldTerminate()
    {
        return false;
    }

    unsigned ticksUntilNextCheck()
    {
        return 0xFFFFFFFF;
    }

    bool didTimeOut()
    {
        return false;
    }

    void throwStackOverflow()
    {
    }

    void throwInterruptedException()
    {
    }

    bool null() { return false; }

    bool isArray(const SerializedScriptValueData& value)
    {
        return value.type() == SerializedScriptValueData::ArrayType;
    }

    bool isObject(const SerializedScriptValueData& value)
    {
        return value.type() == SerializedScriptValueData::ObjectType;
    }

    SerializedArray* asInputArray(const SerializedScriptValueData& value)
    {
        return value.asArray();
    }

    SerializedObject* asInputObject(const SerializedScriptValueData& value)
    {
        return value.asObject();
    }

    bool createOutputArray(unsigned)
    {
        return false;
    }

    bool createOutputObject()
    {
        return false;
    }

    uint32_t length(RefPtr<SerializedArray> array)
    {
        return array->length();
    }

    bool canDoFastRead(RefPtr<SerializedArray> array, unsigned index)
    {
        return array->canDoFastRead(index);
    }

    SerializedScriptValueData getIndex(RefPtr<SerializedArray> array, unsigned index)
    {
        return array->getIndex(index);
    }

    SerializedScriptValueData getSparseIndex(RefPtr<SerializedArray> array, unsigned propertyName, bool& hasIndex)
    {
        return array->getSparseIndex(propertyName, hasIndex);
    }

    SerializedScriptValueData getProperty(RefPtr<SerializedObject> object, const RefPtr<StringImpl>& propertyName, unsigned propertyIndex)
    {
        ASSERT(object->names()[propertyIndex] == propertyName);
        UNUSED_PARAM(propertyName);
        return object->values()[propertyIndex];
    }

    bool convertIfTerminal(SerializedScriptValueData& value)
    {
        switch (value.type()) {
            case SerializedScriptValueData::ArrayType:
            case SerializedScriptValueData::ObjectType:
                return false;
            case SerializedScriptValueData::StringType:
            case SerializedScriptValueData::ImmediateType:
            case SerializedScriptValueData::NumberType:
            case SerializedScriptValueData::DateType:
            case SerializedScriptValueData::EmptyType:
            case SerializedScriptValueData::BlobType:
            case SerializedScriptValueData::FileType:
            case SerializedScriptValueData::FileListType:
            case SerializedScriptValueData::ImageDataType:
                return true;
        }
        ASSERT_NOT_REACHED();
        return true;
    }

    void getPropertyNames(RefPtr<SerializedObject> object, Vector<SerializedObject::PropertyNameList, 16>& properties)
    {
        properties.append(object->names());
    }

    void putProperty(bool, unsigned, bool)
    {
    }

    void putProperty(bool, const RefPtr<StringImpl>&, bool)
    {
    }

    bool startArray(RefPtr<SerializedArray>, bool)
    {
        return true;
    }
    void endArray(RefPtr<SerializedArray> array, bool)
    {
        array->clear();
    }
    bool startObject(RefPtr<SerializedObject>, bool)
    {
        return true;
    }
    void endObject(RefPtr<SerializedObject> object, bool)
    {
        object->clear();
    }
};

void SerializedScriptValueData::tearDownSerializedData()
{
    if (m_sharedData && m_sharedData->refCount() > 1)
        return;
    TeardownTreeWalker context;
    walk<TeardownTreeWalker>(context, *this);
}

SerializedScriptValue::~SerializedScriptValue()
{
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(JSContextRef originContext, JSValueRef apiValue, JSValueRef* exception)
{
    JSLock lock(SilenceAssertionsOnly);
    ExecState* exec = toJS(originContext);
    JSValue value = toJS(exec, apiValue);
    PassRefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::create(exec, value);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        return 0;
    }
    
    return serializedValue;
}

SerializedScriptValue* SerializedScriptValue::nullValue() 
{
    DEFINE_STATIC_LOCAL(RefPtr<SerializedScriptValue>, nullValue, (SerializedScriptValue::create()));
    return nullValue.get();
}

JSValueRef SerializedScriptValue::deserialize(JSContextRef destinationContext, JSValueRef* exception)
{
    JSLock lock(SilenceAssertionsOnly);
    ExecState* exec = toJS(destinationContext);
    JSValue value = deserialize(exec, exec->lexicalGlobalObject());
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec, exec->exception());
        exec->clearException();
        return 0;
    }
    return toRef(exec, value);
}

}
