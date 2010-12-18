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

#include "PluginTest.h"

using namespace std;


class NPRuntimeRemoveProperty : public PluginTest {
public:
    NPRuntimeRemoveProperty(NPP npp, const string& identifier)
        : PluginTest(npp, identifier)
    {
    }
    
private:
    struct TestObject : Object<TestObject> { 
    public:
        bool hasMethod(NPIdentifier methodName)
        {
            return methodName == pluginTest()->NPN_GetStringIdentifier("testRemoveProperty");
        }

        bool invoke(NPIdentifier methodName, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
        {
            assert(methodName == pluginTest()->NPN_GetStringIdentifier("testRemoveProperty"));

            if (argumentCount != 2)
                return false;

            if (!NPVARIANT_IS_OBJECT(arguments[0]))
                return false;
            
            if (!NPVARIANT_IS_STRING(arguments[1]) && !NPVARIANT_IS_DOUBLE(arguments[1]))
                return false;
            
            NPIdentifier propertyName;
            if (NPVARIANT_IS_STRING(arguments[1])) {
                string propertyNameString(arguments[1].value.stringValue.UTF8Characters,
                                          arguments[1].value.stringValue.UTF8Length);
            
                propertyName = pluginTest()->NPN_GetStringIdentifier(propertyNameString.c_str());
            } else {
                int32_t number = arguments[1].value.doubleValue;
                propertyName = pluginTest()->NPN_GetIntIdentifier(number);
            }
            
            pluginTest()->NPN_RemoveProperty(NPVARIANT_TO_OBJECT(arguments[0]), propertyName);

            VOID_TO_NPVARIANT(*result);
            return true;
        }
    };
    
    virtual NPError NPP_GetValue(NPPVariable variable, void *value)
    {
        if (variable != NPPVpluginScriptableNPObject)
            return NPERR_GENERIC_ERROR;
        
        *(NPObject**)value = TestObject::create(this);
        
        return NPERR_NO_ERROR;
    }
    
};

static PluginTest::Register<NPRuntimeRemoveProperty> npRuntimeRemoveProperty("npruntime-remove-property");
