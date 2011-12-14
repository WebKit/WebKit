/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#ifndef V8Utilities_h
#define V8Utilities_h

#include <wtf/Forward.h>
#include <v8.h>

namespace WebCore {

    class EventListener;
    class Frame;
    class KURL;
    class ScriptExecutionContext;
    class ScriptState;

    // Use an array to hold dependents. It works like a ref-counted scheme. A value can be added more than once to the DOM object.
    void createHiddenDependency(v8::Handle<v8::Object>, v8::Local<v8::Value>, int cacheIndex);
    void removeHiddenDependency(v8::Handle<v8::Object>, v8::Local<v8::Value>, int cacheIndex);
    
    // Combo create/remove, for generated event-handler-setter bindings:
    void transferHiddenDependency(v8::Handle<v8::Object>, EventListener* oldValue, v8::Local<v8::Value> newValue, int cacheIndex);
    
    bool processingUserGesture();
    bool shouldAllowNavigation(Frame*);
    KURL completeURL(const String& relativeURL);

    ScriptExecutionContext* getScriptExecutionContext();

    void throwTypeMismatchException();

    enum CallbackAllowedValueFlag {
        CallbackAllowUndefined = 1,
        CallbackAllowNull = 1 << 1
    };

    typedef unsigned CallbackAllowedValueFlags;

    // 'FunctionOnly' is assumed for the created callback.
    template <typename V8CallbackType>
    PassRefPtr<V8CallbackType> createFunctionOnlyCallback(v8::Local<v8::Value> value, bool& succeeded, CallbackAllowedValueFlags acceptedValues = 0)
    {
        succeeded = true;

        if (value->IsUndefined() && (acceptedValues & CallbackAllowUndefined))
            return 0;

        if (value->IsNull() && (acceptedValues & CallbackAllowNull))
            return 0;

        if (!value->IsFunction()) {
            succeeded = false;
            throwTypeMismatchException();
            return 0;
        }

        return V8CallbackType::create(value, getScriptExecutionContext());
    }

    class AllowAllocation {
    public:
        inline AllowAllocation()
        {
            m_previous = m_current;
            m_current = true;
        }

        inline ~AllowAllocation()
        {
            m_current = m_previous;
        }

        static bool m_current;

    private:
        bool m_previous;
    };

    class SafeAllocation {
     public:
      static inline v8::Local<v8::Object> newInstance(v8::Handle<v8::Function>);
      static inline v8::Local<v8::Object> newInstance(v8::Handle<v8::ObjectTemplate>);
      static inline v8::Local<v8::Object> newInstance(v8::Handle<v8::Function>, int argc, v8::Handle<v8::Value> argv[]);
    };

    v8::Local<v8::Object> SafeAllocation::newInstance(v8::Handle<v8::Function> function)
    {
        if (function.IsEmpty())
            return v8::Local<v8::Object>();
        AllowAllocation allow;
        return function->NewInstance();
    }

    v8::Local<v8::Object> SafeAllocation::newInstance(v8::Handle<v8::ObjectTemplate> objectTemplate)
    {
        if (objectTemplate.IsEmpty())
            return v8::Local<v8::Object>();
        AllowAllocation allow;
        return objectTemplate->NewInstance();
    }

    v8::Local<v8::Object> SafeAllocation::newInstance(v8::Handle<v8::Function> function, int argc, v8::Handle<v8::Value> argv[])
    {
        if (function.IsEmpty())
            return v8::Local<v8::Object>();
        AllowAllocation allow;
        return function->NewInstance(argc, argv);
    }

} // namespace WebCore

#endif // V8Utilities_h
