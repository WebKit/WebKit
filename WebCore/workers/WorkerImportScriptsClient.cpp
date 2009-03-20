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

#include "config.h"

#if ENABLE(WORKERS)

#include "WorkerImportScriptsClient.h"

#include "ScriptExecutionContext.h"

namespace WebCore {

void WorkerImportScriptsClient::didReceiveResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() / 100 != 2 && response.httpStatusCode() != 0) {
        m_failed = true;
        return;
    }
    m_responseEncoding = response.textEncodingName();
}

void WorkerImportScriptsClient::didReceiveData(const char* data, int len)
{
    if (m_failed)
        return;

    if (!m_decoder) {
        if (!m_responseEncoding.isEmpty())
            m_decoder = TextResourceDecoder::create("text/javascript", m_responseEncoding);
        else
            m_decoder = TextResourceDecoder::create("text/javascript", "UTF-8");
    }

    if (!len)
        return;
    
    if (len == -1)
        len = strlen(data);
    
    m_script += m_decoder->decode(data, len);
}

void WorkerImportScriptsClient::didFinishLoading(unsigned long identifier)
{
    if (m_failed)
        return;

    if (m_decoder)
        m_script += m_decoder->flush();
    
    m_scriptExecutionContext->scriptImported(identifier, m_script);
    m_scriptExecutionContext->addMessage(InspectorControllerDestination, JSMessageSource, LogMessageLevel, "Worker script imported: \"" + m_url + "\".", m_callerLineNumber, m_callerURL);
}

void WorkerImportScriptsClient::didFail(const ResourceError&)
{
    m_failed = true;
}

void WorkerImportScriptsClient::didFailRedirectCheck()
{
    m_failed = true;
}

void WorkerImportScriptsClient::didReceiveAuthenticationCancellation(const ResourceResponse&)
{
    m_failed = true;
}

}

#endif // ENABLE(WORKERS)
