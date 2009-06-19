/*
 * Copyright (C) 2008, 2009 Daniel Bates (dbates@intudata.com)
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
 */

#include "config.h"
#include "XSSAuditor.h"

#include <wtf/StdLibExtras.h>

#include "Console.h"
#include "CString.h"
#include "DocumentLoader.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "KURL.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "TextResourceDecoder.h"

namespace WebCore {
  
// This method also appears in file ResourceResponseBase.cpp.
static bool isControlCharacter(UChar c)
{
   return c < ' ' || c == 127;
}
  
XSSAuditor::XSSAuditor(Frame* frame)
    : m_isEnabled(false)
    , m_frame(frame)
{
    if (Settings* settings = frame->settings())
        m_isEnabled = settings->xssAuditorEnabled();
}

XSSAuditor::~XSSAuditor()
{
}
  
bool XSSAuditor::canEvaluate(const String& sourceCode) const
{
    if (!m_isEnabled)
        return true;
    
    if (findInRequest(sourceCode)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canCreateInlineEventListener(const String&, const String& code) const
{
    if (!m_isEnabled)
        return true;
    
    return canEvaluate(code);
}

bool XSSAuditor::canLoadExternalScriptFromSrc(const String& url) const
{
    if (!m_isEnabled)
        return true;
  
    if (findInRequest(url)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canLoadObject(const String& url) const
{
    if (!m_isEnabled)
        return true;

    if (findInRequest(url)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request"));
        m_frame->domWindow()->console()->addMessage(OtherMessageSource, ErrorMessageLevel, consoleMessage, 1, String());    
        return false;
    }
    return true;
}

String XSSAuditor::decodeURL(const String& str, const TextEncoding& encoding, bool allowNullCharacters)
{
    String result;
    String url = str;
    
    url.replace('+', ' ');
    result = decodeURLEscapeSequences(url, encoding);
    return allowNullCharacters ? result : result.removeCharacters(&isControlCharacter);
}

bool XSSAuditor::findInRequest(const String& string) const
{
    ASSERT(m_frame->document());
    String pageURL = m_frame->document()->url().string();
    
    if (protocolIs(pageURL, "data"))
        return false;

    if (string.isEmpty())
        return false;

    if (string.length() < pageURL.length()) {
        // The string can actually fit inside the pageURL.
        String decodedPageURL = decodeURL(pageURL, m_frame->document()->decoder()->encoding());
        if (decodedPageURL.find(string, 0, false) != -1)
           return true;  // We've found the smoking gun.
    }
    
    FormData* formDataObj = m_frame->loader()->documentLoader()->originalRequest().httpBody();
    if (formDataObj && !formDataObj->isEmpty()) {
        String formData = formDataObj->flattenToString();
        if (string.length() < formData.length()) {
            // Notice it is sufficient to compare the length of the string to
            // the url-encoded POST data because the length of the url-decoded
            // code is less than or equal to the length of the url-encoded
            // string.
            String decodedFormData = decodeURL(formData, m_frame->document()->decoder()->encoding());
            if (decodedFormData.find(string, 0, false) != -1)
                return true;  // We found the string in the POST data.
        }
    }

    return false;
}

} // namespace WebCore

