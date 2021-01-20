/*
 * Copyright (C) 2009-2010 Google Inc. All rights reserved.
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

#include "config.h"

#include "JSExportMacros.h"

namespace WTF {

class StringBuilder;

class String { };
template<typename T> class Ref { };
template<typename T> class RefPtr { };

} // namespace WTF

using WTF::Ref;
using WTF::RefPtr;
using WTF::String;
using WTF::StringBuilder;

namespace Inspector {
class InspectorArray;
class InspectorArrayBase;
class InspectorObject;
class InspectorObjectBase;
class InspectorValue;
}

namespace Inspector {

class JS_EXPORT_PRIVATE InspectorValue {
public:
    virtual ~InspectorValue() { }

    static Ref<InspectorValue> null();
    static Ref<InspectorValue> create(bool);
    static Ref<InspectorValue> create(int);
    static Ref<InspectorValue> create(double);
    static Ref<InspectorValue> create(const String&);
    static Ref<InspectorValue> create(const char*);

    enum class Type {
        Null = 0,
        Boolean,
        Double,
        Integer,
        String,
        Object,
        Array,
    };

    Type type() const { return Type::Null; }
    bool isNull() const { return true; }

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
    bool asValue(RefPtr<InspectorValue>&);

    virtual bool asObject(RefPtr<InspectorObject>&);
    virtual bool asArray(RefPtr<InspectorArray>&);

    static bool parseJSON(const String&, RefPtr<InspectorValue>&);

    String toJSONString() const;
    virtual void writeJSON(StringBuilder&) const;

    virtual size_t memoryCost() const;
};

Ref<InspectorValue> InspectorValue::null() { return { }; }
Ref<InspectorValue> InspectorValue::create(bool) { return { }; }
Ref<InspectorValue> InspectorValue::create(int) { return { }; }
Ref<InspectorValue> InspectorValue::create(double) { return { }; }
Ref<InspectorValue> InspectorValue::create(const String&) { return { }; }
Ref<InspectorValue> InspectorValue::create(const char*) { return { }; }

bool InspectorValue::asValue(RefPtr<Inspector::InspectorValue> &) { return true; }
bool InspectorValue::asObject(RefPtr<InspectorObject>&) { return false; }
bool InspectorValue::asArray(RefPtr<InspectorArray>&) { return false; }

bool InspectorValue::parseJSON(const String&, RefPtr<InspectorValue>&) { return false; }
String InspectorValue::toJSONString() const { return { }; }

bool InspectorValue::asBoolean(bool&) const { return false; }
bool InspectorValue::asDouble(double&) const { return false; }
bool InspectorValue::asDouble(float&) const { return false; }

bool InspectorValue::asInteger(int&) const { return false; }
bool InspectorValue::asInteger(unsigned&) const { return false; }
bool InspectorValue::asInteger(long&) const { return false; }
bool InspectorValue::asInteger(long long&) const { return false; }
bool InspectorValue::asInteger(unsigned long&) const { return false; }
bool InspectorValue::asInteger(unsigned long long&) const { return false; }

bool InspectorValue::asString(String&) const { return false; }
void InspectorValue::writeJSON(StringBuilder&) const { }
size_t InspectorValue::memoryCost() const { return 0; }


class JS_EXPORT_PRIVATE InspectorObjectBase : public InspectorValue {
public:
    InspectorObject* openAccessors();

    size_t memoryCost() const final;

    bool getBoolean(const String& name, bool& output) const;
    bool getString(const String& name, String& output) const;
    bool getObject(const String& name, RefPtr<InspectorObject>&) const;
    bool getArray(const String& name, RefPtr<InspectorArray>&) const;
    bool getValue(const String& name, RefPtr<InspectorValue>&) const;

    void remove(const String&);
};

InspectorObject* InspectorObjectBase::openAccessors() { return nullptr; }
size_t InspectorObjectBase::memoryCost() const { return 0; }

bool InspectorObjectBase::getBoolean(const String&, bool&) const { return false; }
bool InspectorObjectBase::getString(const String&, String&) const { return false; }
bool InspectorObjectBase::getObject(const String&, RefPtr<InspectorObject>&) const { return false; }
bool InspectorObjectBase::getArray(const String&, RefPtr<InspectorArray>&) const { return false; }
bool InspectorObjectBase::getValue(const String&, RefPtr<InspectorValue>&) const { return false; }
void InspectorObjectBase::remove(const String&) { }


class InspectorObject : public InspectorObjectBase {
public:
    static JS_EXPORT_PRIVATE Ref<InspectorObject> create();
};

Ref<InspectorObject> InspectorObject::create() { return { }; }


class JS_EXPORT_PRIVATE InspectorArrayBase : public InspectorValue {
public:
    RefPtr<InspectorValue> get(size_t index) const;

    size_t memoryCost() const final;
};

RefPtr<InspectorValue> InspectorArrayBase::get(size_t) const { return { }; }
size_t InspectorArrayBase::memoryCost() const { return 0; }


class InspectorArray : public InspectorArrayBase {
public:
    static JS_EXPORT_PRIVATE Ref<InspectorArray> create();
};

Ref<InspectorArray> InspectorArray::create() { return { }; }


class JS_EXPORT_PRIVATE BackendDispatcher {
public:
    // COMPATIBILITY: remove this when no longer needed by system WebInspector.framework.
    void sendResponse(long requestId, RefPtr<InspectorObject>&& result);
};

// COMPATIBILITY: remove this when no longer needed by system WebInspector.framework <http://webkit.org/b/179847>.
void BackendDispatcher::sendResponse(long, RefPtr<InspectorObject>&&) { }

} // namespace Inspector
