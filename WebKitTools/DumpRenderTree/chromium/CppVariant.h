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

/*
  This file contains the declaration for CppVariant, a type used by C++ classes
  that are to be bound to JavaScript objects.

  CppVariant exists primarily as an interface between C++ callers and the
  corresponding NPVariant type.  CppVariant also provides a number of
  convenience constructors and accessors, so that the NPVariantType values
  don't need to be exposed, and a destructor to free any memory allocated for
  string values.
*/

#ifndef CppVariant_h
#define CppVariant_h

#include "WebBindings.h"
#include "webkit/support/webkit_support.h"
#include <string>
#include <wtf/Vector.h>

class CppVariant : public NPVariant {
public:
    CppVariant();
    ~CppVariant();
    void setNull();
    void set(bool);
    void set(int32_t);
    void set(double);

    // Note that setting a CppVariant to a string value involves copying the
    // string data, which must be freed with a call to freeData() when the
    // CppVariant is set to a different value or is no longer needed.  Normally
    // this is handled by the other set() methods and by the destructor.
    void set(const char*); // Must be a null-terminated string.
    void set(const std::string&);
    void set(const NPString&);
    void set(const NPVariant&);

    // Note that setting a CppVariant to an NPObject involves ref-counting
    // the actual object. freeData() should only be called if the CppVariant
    // is no longer needed. The other set() methods handle this internally.
    // Also, the object's NPClass is expected to be a static object: neither
    // the NP runtime nor CppVariant will ever free it.
    void set(NPObject*_value);

    // These three methods all perform deep copies of any string data.  This
    // allows local CppVariants to be released by the destructor without
    // corrupting their sources. In performance-critical code, or when strings
    // are very long, avoid creating new CppVariants.
    // In case of NPObject as the data, the copying involves ref-counting
    // as opposed to deep-copying. The ref-counting ensures that sources don't
    // get corrupted when the copies get destroyed.
    void copyToNPVariant(NPVariant* result) const;
    CppVariant& operator=(const CppVariant& original);
    CppVariant(const CppVariant& original);

    // Calls NPN_ReleaseVariantValue, which frees any string data
    // held by the object and sets its type to null.
    // In case of NPObject, the NPN_ReleaseVariantValue decrements
    // the ref-count (releases when ref-count becomes 0)
    void freeData();

    // Compares this CppVariant's type and value to another's.  They must be
    // identical in both type and value to be considered equal.  For string and
    // object types, a deep comparison is performed; that is, the contents of the
    // strings, or the classes and refcounts of the objects, must be the same,
    // but they need not be the same pointers.
    bool isEqual(const CppVariant&) const;

    // The value of a CppVariant may be read directly from its NPVariant (but
    // should only be set using one of the set() methods above). Although the
    // type of a CppVariant is likewise public, it can be accessed through these
    // functions rather than directly if a caller wishes to avoid dependence on
    // the NPVariantType values.
    bool isBool() const { return (type == NPVariantType_Bool); }
    bool isInt32() const { return (type == NPVariantType_Int32); }
    bool isDouble() const { return (type == NPVariantType_Double); }
    bool isNumber() const { return (isInt32() || isDouble()); }
    bool isString() const { return (type == NPVariantType_String); }
    bool isVoid() const { return (type == NPVariantType_Void); }
    bool isNull() const { return (type == NPVariantType_Null); }
    bool isEmpty() const { return (isVoid() || isNull()); }
    bool isObject() const { return (type == NPVariantType_Object); }

    // Converters.  The CppVariant must be of a type convertible to these values.
    // For example, toInt32() works only if isNumber() is true.
    std::string toString() const;
    int32_t toInt32() const;
    double toDouble() const;
    bool toBoolean() const;
    // Returns a vector of strings for the specified argument. This is useful
    // for converting a JavaScript array of strings into a vector of strings.
    Vector<std::string> toStringVector() const;

    // Invoke method of the given name on an object with the supplied arguments.
    // The first argument should be the object on which the method is to be
    // invoked.  Returns whether the method was successfully invoked.  If the
    // method was invoked successfully, any return value is stored in the
    // CppVariant specified by result.
    bool invoke(const std::string&, const CppVariant* arguments,
                uint32_t argumentCount, CppVariant& result) const;
};

#endif // CppVariant_h
