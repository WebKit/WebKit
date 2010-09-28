/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PluginTest_h
#define PluginTest_h

#include <WebKit/npfunctions.h>
#include <assert.h>
#include <map>
#include <string>

// Helper classes for implementing has_member
typedef char (&no_tag)[1];
typedef char (&yes_tag)[2];

#define DEFINE_HAS_MEMBER_CHECK(member, returnType, argumentTypes) \
template<typename T, returnType (T::*member) argumentTypes> struct pmf_##member##_helper {}; \
template<typename T> no_tag has_member_##member##_helper(...); \
template<typename T> yes_tag has_member_##member##_helper(pmf_##member##_helper<T, &T::member >*); \
template<typename T> struct has_member_##member { \
static const bool value = sizeof(has_member_##member##_helper<T>(0)) == sizeof(yes_tag); \
};

DEFINE_HAS_MEMBER_CHECK(hasMethod, bool, (NPIdentifier methodName));
DEFINE_HAS_MEMBER_CHECK(invoke, bool, (NPIdentifier methodName, const NPVariant*, uint32_t, NPVariant* result));
DEFINE_HAS_MEMBER_CHECK(invokeDefault, bool, (const NPVariant*, uint32_t, NPVariant* result));
DEFINE_HAS_MEMBER_CHECK(hasProperty, bool, (NPIdentifier propertyName));
DEFINE_HAS_MEMBER_CHECK(getProperty, bool, (NPIdentifier propertyName, NPVariant* result));

class PluginTest {
public:
    static PluginTest* create(NPP, const std::string& identifier);
    virtual ~PluginTest();

    // NPP functions.
    virtual NPError NPP_DestroyStream(NPStream* stream, NPReason reason);
    virtual NPError NPP_GetValue(NPPVariable, void* value);
    virtual NPError NPP_SetWindow(NPP, NPWindow*);

    // NPN functions.
    NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name);
    NPIdentifier NPN_GetIntIdentifier(int32_t intid);
    NPObject* NPN_CreateObject(NPClass*);
    bool NPN_RemoveProperty(NPObject*, NPIdentifier propertyName);
    
    template<typename TestClassTy> class Register {
    public:
        Register(const std::string& identifier)
        {
            registerCreateTestFunction(identifier, Register::create);
        }
    
    private:
        static PluginTest* create(NPP npp, const std::string& identifier) 
        {
            return new TestClassTy(npp, identifier);
        }
    };

protected:
    PluginTest(NPP npp, const std::string& identifier);

    // FIXME: A plug-in test shouldn't need to know about it's NPP. Make this private.
    NPP m_npp;

    const std::string& identifier() const { return m_identifier; }

    // NPObject helper template.
    template<typename T> struct Object : NPObject {
    public:
        static NPObject* create(PluginTest* pluginTest)
        {
            Object* object = static_cast<Object*>(pluginTest->NPN_CreateObject(npClass()));

            object->m_pluginTest = pluginTest;
            return object;
        }
    
        // These should never be called.
        bool hasMethod(NPIdentifier methodName)
        {
            assert(false);
            return false;
        }

        bool invoke(NPIdentifier methodName, const NPVariant*, uint32_t, NPVariant* result)
        {
            assert(false);
            return false;
        }
        
        bool invokeDefault(const NPVariant*, uint32_t, NPVariant* result)
        {
            assert(false);
            return false;
        }

        bool hasProperty(NPIdentifier propertyName)
        {
            assert(false);
            return false;
        }

        bool getProperty(NPIdentifier propertyName, NPVariant* result)
        {
            assert(false);
            return false;
        }

    protected:
        Object()
            : m_pluginTest(0)
        {
        }
        
        virtual ~Object() 
        { 
        }

        PluginTest* pluginTest() const { return m_pluginTest; }

    private:
        static NPObject* NP_Allocate(NPP npp, NPClass* aClass)
        {
            return new T;
        }

        static void NP_Deallocate(NPObject* npObject)
        {
            delete static_cast<T*>(npObject);
        }

        static bool NP_HasMethod(NPObject* npObject, NPIdentifier methodName)
        {
            return static_cast<T*>(npObject)->hasMethod(methodName);
        }

        static bool NP_Invoke(NPObject* npObject, NPIdentifier methodName, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
        {
            return static_cast<T*>(npObject)->invoke(methodName, arguments, argumentCount, result);
        }

        static bool NP_InvokeDefault(NPObject* npObject, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
        {
            return static_cast<T*>(npObject)->invokeDefault(arguments, argumentCount, result);
        }

        static bool NP_HasProperty(NPObject* npObject, NPIdentifier propertyName)
        {
            return static_cast<T*>(npObject)->hasProperty(propertyName);
        }

        static bool NP_GetProperty(NPObject* npObject, NPIdentifier propertyName, NPVariant* result)
        {
            return static_cast<T*>(npObject)->getProperty(propertyName, result);
        }

        static NPClass* npClass()
        {
            static NPClass npClass = {
                NP_CLASS_STRUCT_VERSION, 
                NP_Allocate,
                NP_Deallocate,
                0, // NPClass::invalidate
                has_member_hasMethod<T>::value ? NP_HasMethod : 0,
                has_member_invoke<T>::value ? NP_Invoke : 0,
                has_member_invokeDefault<T>::value ? NP_InvokeDefault : 0,
                has_member_hasProperty<T>::value ? NP_HasProperty : 0,
                has_member_getProperty<T>::value ? NP_GetProperty : 0,
                0, // NPClass::setProperty
                0, // NPClass::removeProperty
                0, // NPClass::enumerate
                0  // NPClass::construct
            };
            
            return &npClass;
        };

        PluginTest* m_pluginTest;
    };
    
private:
    typedef PluginTest* (*CreateTestFunction)(NPP, const std::string&);
    
    static void registerCreateTestFunction(const std::string&, CreateTestFunction);
    static std::map<std::string, CreateTestFunction>& createTestFunctions();
    
    std::string m_identifier;
};

#endif // PluginTest_h
