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

#ifndef XSSAuditor_h
#define XSSAuditor_h

#include "PlatformString.h"
#include "TextEncoding.h"

namespace WebCore {

    class Frame;
    class ScriptSourceCode;

    // The XSSAuditor class is used to prevent type 1 cross-site scripting
    // vulnerabilities (also known as reflected vulnerabilities).
    //
    // More specifically, the XSSAuditor class decides whether the execution of
    // a script is to be allowed or denied based on the content of any
    // user-submitted data, including:
    //
    // * the URL.
    // * the HTTP-POST data.
    //
    // If the source code of a script resembles any user-submitted data then it
    // is denied execution.
    //
    // When you instantiate the XSSAuditor you must specify the Frame of the
    // page that you wish to audit.
    //
    // Bindings
    //
    // An XSSAuditor is instantiated within the constructor of a
    // ScriptController object and passed the Frame the script originated. The
    // ScriptController calls back to the XSSAuditor to determine whether a
    // JavaScript script is safe to execute before executing it. The following
    // methods call into XSSAuditor:
    //
    // * ScriptController::evaluateInWorld - used to evaluate JavaScript scripts.
    // * ScriptController::executeIfJavaScriptURL - used to evaluate JavaScript URLs.
    // * ScriptEventListener::createAttributeEventListener - used to create JavaScript event handlers.
    // * HTMLBaseElement::process - used to set the document base URL.
    // * HTMLTokenizer::parseTag - used to load external JavaScript scripts.
    // * FrameLoader::requestObject - used to load <object>/<embed> elements.
    //
    class XSSAuditor : public Noncopyable {
    public:
        XSSAuditor(Frame*);
        ~XSSAuditor();

        bool isEnabled() const;

        // Determines whether the script should be allowed or denied execution
        // based on the content of any user-submitted data.
        bool canEvaluate(const String& code) const;

        // Determines whether the JavaScript URL should be allowed or denied execution
        // based on the content of any user-submitted data.
        bool canEvaluateJavaScriptURL(const String& code) const;

        // Determines whether the event listener should be created based on the
        // content of any user-submitted data.
        bool canCreateInlineEventListener(const String& functionName, const String& code) const;

        // Determines whether the external script should be loaded based on the
        // content of any user-submitted data.
        bool canLoadExternalScriptFromSrc(const String& context, const String& url) const;

        // Determines whether object should be loaded based on the content of
        // any user-submitted data.
        //
        // This method is called by FrameLoader::requestObject.
        bool canLoadObject(const String& url) const;

        // Determines whether the base URL should be changed based on the content
        // of any user-submitted data.
        //
        // This method is called by HTMLBaseElement::process.
        bool canSetBaseElementURL(const String& url) const;

    private:
        class CachingURLCanonicalizer
        {
        public:
            CachingURLCanonicalizer() : m_decodeEntities(false), m_decodeURLEscapeSequencesTwice(false) { }
            String canonicalizeURL(const String& url, const TextEncoding& encoding, bool decodeEntities, 
                                   bool decodeURLEscapeSequencesTwice);

        private:
            // The parameters we were called with last.
            String m_inputURL;
            TextEncoding m_encoding;
            bool m_decodeEntities;
            bool m_decodeURLEscapeSequencesTwice;

            // The cached result.
            String m_cachedCanonicalizedURL;
        };

        struct FindTask {
            FindTask()
                : decodeEntities(true)
                , allowRequestIfNoIllegalURICharacters(false)
                , decodeURLEscapeSequencesTwice(false)
            {
            }

            String context;
            String string;
            bool decodeEntities;
            bool allowRequestIfNoIllegalURICharacters;
            bool decodeURLEscapeSequencesTwice;
        };

        static String canonicalize(const String&);
        static String decodeURL(const String& url, const TextEncoding& encoding, bool decodeEntities, 
                                bool decodeURLEscapeSequencesTwice = false);
        static String decodeHTMLEntities(const String&, bool leaveUndecodableEntitiesUntouched = true);

        bool isSameOriginResource(const String& url) const;
        bool findInRequest(const FindTask&) const;
        bool findInRequest(Frame*, const FindTask&) const;

        bool shouldFullPageBlockForXSSProtectionHeader() const;

        // The frame to audit.
        Frame* m_frame;

        // A state store to help us avoid canonicalizing the same URL repeated.
        mutable CachingURLCanonicalizer m_cache;
    };

} // namespace WebCore

#endif // XSSAuditor_h
