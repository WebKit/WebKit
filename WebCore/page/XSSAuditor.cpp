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
#include "ResourceResponseBase.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "TextResourceDecoder.h"

using namespace WTF;

namespace WebCore {

static bool isNonNullControlCharacter(UChar c)
{
    return (c > '\0' && c < ' ') || c == 127;
}

XSSAuditor::XSSAuditor(Frame* frame)
    : m_frame(frame)
{
}

XSSAuditor::~XSSAuditor()
{
}

bool XSSAuditor::isEnabled() const
{
    Settings* settings = m_frame->settings();
    return (settings && settings->xssAuditorEnabled());
}

bool XSSAuditor::canEvaluate(const String& sourceCode) const
{
    if (!isEnabled())
        return true;

    if (findInRequest(sourceCode, false)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canCreateInlineEventListener(const String&, const String& code) const
{
    if (!isEnabled())
        return true;

    if (findInRequest(code)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canLoadExternalScriptFromSrc(const String& url) const
{
    if (!isEnabled())
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
    if (!isEnabled())
        return true;

    if (findInRequest(url, false, false)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canSetBaseElementURL(const String& url) const
{
    if (!isEnabled())
        return true;
    
    KURL baseElementURL(m_frame->document()->url(), url);
    if (m_frame->document()->url().baseAsString() != baseElementURL.baseAsString() && findInRequest(url)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

String XSSAuditor::decodeURL(const String& str, const TextEncoding& encoding, bool allowNullCharacters,
                             bool allowNonNullControlCharacters)
{
    String result;
    String url = str;

    url.replace('+', ' ');
    result = decodeURLEscapeSequences(url);
    String decodedResult = encoding.decode(result.utf8().data(), result.length());
    if (!decodedResult.isEmpty())
        result = decodedResult;
    if (!allowNullCharacters)
        result = StringImpl::createStrippingNullCharacters(result.characters(), result.length());
    if (!allowNonNullControlCharacters) {
        decodedResult = result.removeCharacters(&isNonNullControlCharacter);
        if (!decodedResult.isEmpty())
            result = decodedResult;
    }
    return result;
}

bool XSSAuditor::findInRequest(const String& string, bool matchNullCharacters, bool matchNonNullControlCharacters) const
{
    bool result = false;
    Frame* parentFrame = m_frame->tree()->parent();
    if (parentFrame && m_frame->document()->url() == blankURL())
        result = findInRequest(parentFrame, string, matchNullCharacters, matchNonNullControlCharacters);
    if (!result)
        result = findInRequest(m_frame, string, matchNullCharacters, matchNonNullControlCharacters);
    return result;
}

bool XSSAuditor::findInRequest(Frame* frame, const String& string, bool matchNullCharacters, bool matchNonNullControlCharacters) const
{
    ASSERT(frame->document());
    String pageURL = frame->document()->url().string();

    if (!frame->document()->decoder()) {
        // Note, JavaScript URLs do not have a charset.
        return false;
    }

    if (protocolIs(pageURL, "data"))
        return false;

    if (string.isEmpty())
        return false;

    if (string.length() < pageURL.length()) {
        // The string can actually fit inside the pageURL.
        String decodedPageURL = decodeURL(pageURL, frame->document()->decoder()->encoding(), matchNullCharacters,
                                          matchNonNullControlCharacters);
        if (decodedPageURL.find(string, 0, false) != -1)
           return true;  // We've found the smoking gun.
    }

    FormData* formDataObj = frame->loader()->documentLoader()->originalRequest().httpBody();
    if (formDataObj && !formDataObj->isEmpty()) {
        String formData = formDataObj->flattenToString();
        if (string.length() < formData.length()) {
            // Notice it is sufficient to compare the length of the string to
            // the url-encoded POST data because the length of the url-decoded
            // code is less than or equal to the length of the url-encoded
            // string.
            String decodedFormData = decodeURL(formData, frame->document()->decoder()->encoding(), matchNullCharacters,
                                               matchNonNullControlCharacters);
            if (decodedFormData.find(string, 0, false) != -1)
                return true;  // We found the string in the POST data.
        }
    }

    return false;
}

} // namespace WebCore

