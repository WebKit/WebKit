/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Pawel Hajdan (phajdan.jr@chromium.org)
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
  CppBoundClass class:
  This base class serves as a parent for C++ classes designed to be bound to
  JavaScript objects.

  Subclasses should define the constructor to build the property and method
  lists needed to bind this class to a JS object.  They should also declare
  and define member variables and methods to be exposed to JS through
  that object.
*/

#ifndef CppBoundClass_h
#define CppBoundClass_h

#include "CppVariant.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace WebKit {
class WebFrame;
class WebString;
}

typedef Vector<CppVariant> CppArgumentList;

// CppBoundClass lets you map Javascript method calls and property accesses
// directly to C++ method calls and CppVariant* variable access.
class CppBoundClass {
    WTF_MAKE_NONCOPYABLE(CppBoundClass);
public:
    class PropertyCallback {
    public:
        virtual ~PropertyCallback() { }

        // Sets |value| to the value of the property. Returns false in case of
        // failure. |value| is always non-0.
        virtual bool getValue(CppVariant* result) = 0;

        // sets the property value to |value|. Returns false in case of failure.
        virtual bool setValue(const CppVariant&) = 0;
    };

    // Callback class for "void function(CppVariant*)"
    class GetterCallback {
    public:
        virtual ~GetterCallback() {}
        virtual void run(CppVariant*) = 0;
    };

    // The constructor should call BindMethod, BindProperty, and
    // SetFallbackMethod as needed to set up the methods, properties, and
    // fallback method.
    CppBoundClass() : m_boundToFrame(false) {}
    virtual ~CppBoundClass();

    // Return a CppVariant representing this class, for use with BindProperty().
    // The variant type is guaranteed to be NPVariantType_Object.
    CppVariant* getAsCppVariant();

    // Given a WebFrame, BindToJavascript builds the NPObject that will represent
    // the class and binds it to the frame's window under the given name.  This
    // should generally be called from the WebView delegate's
    // WindowObjectCleared(). A class so bound will be accessible to JavaScript
    // as window.<classname>. The owner of the CppBoundObject is responsible for
    // keeping the object around while the frame is alive, and for destroying it
    // afterwards.
    void bindToJavascript(WebKit::WebFrame*, const WebKit::WebString& classname);

    // Used by a test.  Returns true if a method with name |name| exists,
    // regardless of whether a fallback is registered.
    bool isMethodRegistered(const std::string&) const;

protected:
    // Callback for "void function(const CppArguemntList&, CppVariant*)"
    class Callback {
    public:
        virtual ~Callback() {}
        virtual void run(const CppArgumentList&, CppVariant*) = 0;
    };

    // Callback for "void T::method(const CppArguemntList&, CppVariant*)"
    template <class T> class MemberCallback : public Callback {
    public:
        typedef void (T::*MethodType)(const CppArgumentList&, CppVariant*);
        MemberCallback(T* object, MethodType method)
            : m_object(object)
            , m_method(method) {}
        virtual ~MemberCallback() {}

        virtual void run(const CppArgumentList& arguments, CppVariant* result)
        {
            (m_object->*m_method)(arguments, result);
        }
    private:
        T* m_object;
        MethodType m_method;
    };

    // Callback class for "void T::method(CppVariant*)"
    template <class T> class MemberGetterCallback : public GetterCallback {
    public:
        typedef void (T::*MethodType)(CppVariant*);
        MemberGetterCallback(T* object, MethodType method)
            : m_object(object)
            , m_method(method) {}
        virtual ~MemberGetterCallback() {}

        virtual void run(CppVariant* result) { (m_object->*m_method)(result); }
    private:
        T* m_object;
        MethodType m_method;
    };

    // Bind the Javascript method called the string parameter to the C++ method.
    void bindCallback(const std::string&, Callback*);

    // A wrapper for bindCallback, to simplify the common case of binding a
    // method on the current object.  Though not verified here, |method|
    // must be a method of this CppBoundClass subclass.
    template<class T>
    void bindMethod(const std::string& name, void (T::*method)(const CppArgumentList&, CppVariant*))
    {
        Callback* callback = new MemberCallback<T>(static_cast<T*>(this), method);
        bindCallback(name, callback);
    }

    // Bind Javascript property |name| to the C++ getter callback |callback|.
    // This can be used to create read-only properties.
    void bindGetterCallback(const std::string&, GetterCallback*);

    // A wrapper for BindGetterCallback, to simplify the common case of binding a
    // property on the current object.  Though not verified here, |method|
    // must be a method of this CppBoundClass subclass.
    template<class T>
    void bindProperty(const std::string& name, void (T::*method)(CppVariant*))
    {
        GetterCallback* callback = new MemberGetterCallback<T>(static_cast<T*>(this), method);
        bindGetterCallback(name, callback);
    }

    // Bind the Javascript property called |name| to a CppVariant.
    void bindProperty(const std::string&, CppVariant*);

    // Bind Javascript property called |name| to a PropertyCallback.
    // CppBoundClass assumes control over the life time of the callback.
    void bindProperty(const std::string&, PropertyCallback*);

    // Set the fallback callback, which is called when when a callback is
    // invoked that isn't bound.
    // If it is 0 (its default value), a JavaScript exception is thrown in
    // that case (as normally expected). If non 0, the fallback method is
    // invoked and the script continues its execution.
    // Passing 0 clears out any existing binding.
    // It is used for tests and should probably only be used in such cases
    // as it may cause unexpected behaviors (a JavaScript object with a
    // fallback always returns true when checked for a method's
    // existence).
    void bindFallbackCallback(Callback* fallbackCallback)
    {
        m_fallbackCallback.set(fallbackCallback);
    }

    // A wrapper for BindFallbackCallback, to simplify the common case of
    // binding a method on the current object.  Though not verified here,
    // |method| must be a method of this CppBoundClass subclass.
    // Passing 0 for |method| clears out any existing binding.
    template<class T>
    void bindFallbackMethod(void (T::*method)(const CppArgumentList&, CppVariant*))
    {
        if (method) {
            Callback* callback = new MemberCallback<T>(static_cast<T*>(this), method);
            bindFallbackCallback(callback);
        } else
            bindFallbackCallback(0);
    }

    // Some fields are protected because some tests depend on accessing them,
    // but otherwise they should be considered private.

    typedef HashMap<NPIdentifier, PropertyCallback*> PropertyList;
    typedef HashMap<NPIdentifier, Callback*> MethodList;
    // These maps associate names with property and method pointers to be
    // exposed to JavaScript.
    PropertyList m_properties;
    MethodList m_methods;

    // The callback gets invoked when a call is made to an nonexistent method.
    OwnPtr<Callback> m_fallbackCallback;

private:
    // NPObject callbacks.
    friend struct CppNPObject;
    bool hasMethod(NPIdentifier) const;
    bool invoke(NPIdentifier, const NPVariant* args, size_t argCount,
                NPVariant* result);
    bool hasProperty(NPIdentifier) const;
    bool getProperty(NPIdentifier, NPVariant* result) const;
    bool setProperty(NPIdentifier, const NPVariant*);

    // A lazily-initialized CppVariant representing this class.  We retain 1
    // reference to this object, and it is released on deletion.
    CppVariant m_selfVariant;

    // True if our np_object has been bound to a WebFrame, in which case it must
    // be unregistered with V8 when we delete it.
    bool m_boundToFrame;
};

#endif // CppBoundClass_h
