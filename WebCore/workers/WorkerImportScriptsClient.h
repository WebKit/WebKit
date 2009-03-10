/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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

#ifndef WorkerImportScriptsClient_h
#define WorkerImportScriptsClient_h

#if ENABLE(WORKERS)

#include "ResourceResponse.h"
#include "ScriptString.h"
#include "TextResourceDecoder.h"
#include "ThreadableLoaderClient.h"

namespace WebCore {

    class ScriptExecutionContext;
    
    class WorkerImportScriptsClient : public ThreadableLoaderClient {
    public:
        WorkerImportScriptsClient(ScriptExecutionContext* scriptExecutionContext, const String& url, const String& callerURL, int callerLineNumber)
            : m_scriptExecutionContext(scriptExecutionContext)
            , m_url(url)
            , m_callerURL(callerURL)
            , m_callerLineNumber(callerLineNumber)
            , m_failed(false)
        {
        }

        const String& script() const { return m_script; }
        bool failed() const { return m_failed; }

        virtual void didReceiveResponse(const ResourceResponse& response);
        virtual void didReceiveData(const char* data, int lengthReceived);
        virtual void didFinishLoading(unsigned long identifier);
        virtual void didFail(const ResourceError&);
        virtual void didFailRedirectCheck();
        virtual void didReceiveAuthenticationCancellation(const ResourceResponse&);

    private:
        ScriptExecutionContext* m_scriptExecutionContext;
        String m_url;
        String m_callerURL;
        int m_callerLineNumber;
        String m_responseEncoding;        
        RefPtr<TextResourceDecoder> m_decoder;
        String m_script;
        bool m_failed;
    };

}

#endif // ENABLE(WORKERS)
#endif

