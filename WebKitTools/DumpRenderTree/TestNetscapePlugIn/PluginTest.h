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
#include <map>
#include <string>

class PluginTest {
public:
    static PluginTest* create(NPP, const std::string& identifier);
    virtual ~PluginTest();

    // Add more NPP functions here if needed.
    virtual NPError NPP_DestroyStream(NPStream *stream, NPReason reason);
    virtual NPError NPP_GetValue(NPPVariable, void *value);

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

    // NPN functions.
    NPObject* NPN_CreateObject(NPClass*);

    // FIXME: A plug-in test shouldn't need to know about it's NPP. Make this private.
    NPP m_npp;

    const std::string& identifier() const { return m_identifier; }

private:
    typedef PluginTest* (*CreateTestFunction)(NPP, const std::string&);
    
    static void registerCreateTestFunction(const std::string&, CreateTestFunction);
    static std::map<std::string, CreateTestFunction>& createTestFunctions();
    
    std::string m_identifier;
};

#endif // PluginTest_h
