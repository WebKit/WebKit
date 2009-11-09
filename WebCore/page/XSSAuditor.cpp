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
#include <wtf/Vector.h>

#include "Console.h"
#include "CString.h"
#include "DocumentLoader.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "KURL.h"
#include "PreloadScanner.h"
#include "ResourceResponseBase.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "TextResourceDecoder.h"

using namespace WTF;

namespace WebCore {

static bool isNonCanonicalCharacter(UChar c)
{
    // We remove all non-ASCII characters, including non-printable ASCII characters.
    //
    // Note, we don't remove backslashes like PHP stripslashes(), which among other things converts "\\0" to the \0 character.
    // Instead, we remove backslashes and zeros (since the string "\\0" =(remove backslashes)=> "0"). However, this has the 
    // adverse effect that we remove any legitimate zeros from a string.
    //
    // For instance: new String("http://localhost:8000") => new String("http://localhost:8").
    return (c == '\\' || c == '0' || c < ' ' || c >= 127);
}

static bool isIllegalURICharacter(UChar c)
{
    // The characters described in section 2.4.3 of RFC 2396 <http://www.faqs.org/rfcs/rfc2396.html> in addition to the 
    // single quote character "'" are considered illegal URI characters. That is, the following characters cannot appear
    // in a valid URI: ', ", <, >
    //
    // If the request does not contain these characters then we can assume that no inline scripts have been injected 
    // into the response page, because it is impossible to write an inline script of the form <script>...</script>
    // without "<", ">".
    return (c == '\'' || c == '"' || c == '<' || c == '>');
}

String XSSAuditor::CachingURLCanonicalizer::canonicalizeURL(const String& url, const TextEncoding& encoding, bool decodeEntities, 
                                                            bool decodeURLEscapeSequencesTwice)
{
    if (decodeEntities == m_decodeEntities && decodeURLEscapeSequencesTwice == m_decodeURLEscapeSequencesTwice 
        && encoding == m_encoding && url == m_inputURL)
        return m_cachedCanonicalizedURL;

    m_cachedCanonicalizedURL = canonicalize(decodeURL(url, encoding, decodeEntities, decodeURLEscapeSequencesTwice));
    m_inputURL = url;
    m_encoding = encoding;
    m_decodeEntities = decodeEntities;
    m_decodeURLEscapeSequencesTwice = decodeURLEscapeSequencesTwice;
    return m_cachedCanonicalizedURL;
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

bool XSSAuditor::canEvaluate(const String& code) const
{
    if (!isEnabled())
        return true;

    if (findInRequest(code, false, true)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canEvaluateJavaScriptURL(const String& code) const
{
    if (!isEnabled())
        return true;

    if (findInRequest(code, true, false, true)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canCreateInlineEventListener(const String&, const String& code) const
{
    if (!isEnabled())
        return true;

    if (findInRequest(code, true, true)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canLoadExternalScriptFromSrc(const String& context, const String& url) const
{
    if (!isEnabled())
        return true;

    if (isSameOriginResource(url))
        return true;

    if (findInRequest(context + url)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canLoadObject(const String& url) const
{
    if (!isEnabled())
        return true;

    if (isSameOriginResource(url))
        return true;

    if (findInRequest(url)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

bool XSSAuditor::canSetBaseElementURL(const String& url) const
{
    if (!isEnabled())
        return true;

    if (isSameOriginResource(url))
        return true;

    if (findInRequest(url)) {
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request"));
        m_frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage, 1, String());
        return false;
    }
    return true;
}

String XSSAuditor::canonicalize(const String& string)
{
    String result = decodeHTMLEntities(string);
    return result.removeCharacters(&isNonCanonicalCharacter);
}

String XSSAuditor::decodeURL(const String& string, const TextEncoding& encoding, bool decodeEntities, bool decodeURLEscapeSequencesTwice)
{
    String result;
    String url = string;

    url.replace('+', ' ');
    result = decodeURLEscapeSequences(url);
    CString utf8Url = result.utf8();
    String decodedResult = encoding.decode(utf8Url.data(), utf8Url.length());
    if (!decodedResult.isEmpty())
        result = decodedResult;
    if (decodeURLEscapeSequencesTwice) {
        result = decodeURLEscapeSequences(result);
        utf8Url = result.utf8();
        decodedResult = encoding.decode(utf8Url.data(), utf8Url.length());
        if (!decodedResult.isEmpty())
            result = decodedResult;
    }
    if (decodeEntities)
        result = decodeHTMLEntities(result);
    return result;
}

String XSSAuditor::decodeHTMLEntities(const String& string, bool leaveUndecodableEntitiesUntouched)
{
    SegmentedString source(string);
    SegmentedString sourceShadow;
    Vector<UChar> result;
    
    while (!source.isEmpty()) {
        UChar cc = *source;
        source.advance();
        
        if (cc != '&') {
            result.append(cc);
            continue;
        }
        
        if (leaveUndecodableEntitiesUntouched)
            sourceShadow = source;
        bool notEnoughCharacters = false;
        unsigned entity = PreloadScanner::consumeEntity(source, notEnoughCharacters);
        // We ignore notEnoughCharacters because we might as well use this loop
        // to copy the remaining characters into |result|.

        if (entity > 0xFFFF) {
            result.append(U16_LEAD(entity));
            result.append(U16_TRAIL(entity));
        } else if (entity && (!leaveUndecodableEntitiesUntouched || entity != 0xFFFD)){
            result.append(entity);
        } else {
            result.append('&');
            if (leaveUndecodableEntitiesUntouched)
                source = sourceShadow;
        }
    }
    
    return String::adopt(result);
}

bool XSSAuditor::isSameOriginResource(const String& url) const
{
    // If the resource is loaded from the same URL as the enclosing page, it's
    // probably not an XSS attack, so we reduce false positives by allowing the
    // request. If the resource has a query string, we're more suspicious,
    // however, because that's pretty rare and the attacker might be able to
    // trick a server-side script into doing something dangerous with the query
    // string.
    KURL resourceURL(m_frame->document()->url(), url);
    return (m_frame->document()->url().host() == resourceURL.host() && resourceURL.query().isEmpty());
}

bool XSSAuditor::findInRequest(const String& string, bool decodeEntities, bool allowRequestIfNoIllegalURICharacters, 
                               bool decodeURLEscapeSequencesTwice) const
{
    bool result = false;
    Frame* parentFrame = m_frame->tree()->parent();
    if (parentFrame && m_frame->document()->url() == blankURL())
        result = findInRequest(parentFrame, string, decodeEntities, allowRequestIfNoIllegalURICharacters, decodeURLEscapeSequencesTwice);
    if (!result)
        result = findInRequest(m_frame, string, decodeEntities, allowRequestIfNoIllegalURICharacters, decodeURLEscapeSequencesTwice);
    return result;
}

bool XSSAuditor::findInRequest(Frame* frame, const String& string, bool decodeEntities, bool allowRequestIfNoIllegalURICharacters, 
                               bool decodeURLEscapeSequencesTwice) const
{
    ASSERT(frame->document());

    if (!frame->document()->decoder()) {
        // Note, JavaScript URLs do not have a charset.
        return false;
    }

    if (string.isEmpty())
        return false;

    FormData* formDataObj = frame->loader()->documentLoader()->originalRequest().httpBody();
    String pageURL = frame->document()->url().string();

    if (!formDataObj && string.length() >= 2 * pageURL.length()) {
        // Q: Why do we bother to do this check at all?
        // A: Canonicalizing large inline scripts can be expensive.  We want to
        //    bail out before the call to canonicalize below, which could
        //    result in an unneeded allocation and memcpy.
        //
        // Q: Why do we multiply by two here?
        // A: We attempt to detect reflected XSS even when the server
        //    transforms the attacker's input with addSlashes.  The best the
        //    attacker can do get the server to inflate his/her input by a
        //    factor of two by sending " characters, which the server
        //    transforms to \".
        return false;
    }

    if (frame->document()->url().protocolIs("data"))
        return false;

    String canonicalizedString = canonicalize(string);
    if (canonicalizedString.isEmpty())
        return false;

    if (string.length() < pageURL.length()) {
        // The string can actually fit inside the pageURL.
        String decodedPageURL = m_cache.canonicalizeURL(pageURL, frame->document()->decoder()->encoding(), decodeEntities, decodeURLEscapeSequencesTwice);

        if (allowRequestIfNoIllegalURICharacters && (!formDataObj || formDataObj->isEmpty()) 
            && decodedPageURL.find(&isIllegalURICharacter, 0) == -1)
            return false; // Injection is impossible because the request does not contain any illegal URI characters. 

        if (decodedPageURL.find(canonicalizedString, 0, false) != -1)
            return true;  // We've found the smoking gun.
    }

    if (formDataObj && !formDataObj->isEmpty()) {
        String formData = formDataObj->flattenToString();
        if (string.length() < formData.length()) {
            // Notice it is sufficient to compare the length of the string to
            // the url-encoded POST data because the length of the url-decoded
            // code is less than or equal to the length of the url-encoded
            // string.
            String decodedFormData = m_cache.canonicalizeURL(formData, frame->document()->decoder()->encoding(), decodeEntities, decodeURLEscapeSequencesTwice);
            if (decodedFormData.find(canonicalizedString, 0, false) != -1)
                return true;  // We found the string in the POST data.
        }
    }

    return false;
}

} // namespace WebCore

