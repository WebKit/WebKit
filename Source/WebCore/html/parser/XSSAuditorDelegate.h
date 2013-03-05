/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XSSAuditorDelegate_h
#define XSSAuditorDelegate_h

#include "KURL.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;

class XSSInfo {
public:
    static PassOwnPtr<XSSInfo> create(const KURL& reportURL, const String& originalURL, const String& originalHTTPBody, bool didBlockEntirePage)
    {
        return adoptPtr(new XSSInfo(reportURL, originalURL, originalHTTPBody, didBlockEntirePage));
    }

    bool isSafeToSendToAnotherThread() const;

    KURL m_reportURL;
    String m_originalURL;
    String m_originalHTTPBody;
    bool m_didBlockEntirePage;
    TextPosition m_textPosition;

private:
    XSSInfo(const KURL& reportURL, const String& originalURL, const String& originalHTTPBody, bool didBlockEntirePage)
        : m_reportURL(reportURL)
        , m_originalURL(originalURL)
        , m_originalHTTPBody(originalHTTPBody)
        , m_didBlockEntirePage(didBlockEntirePage)
    { }
};

class XSSAuditorDelegate {
    WTF_MAKE_NONCOPYABLE(XSSAuditorDelegate);
public:
    explicit XSSAuditorDelegate(Document*);

    void didBlockScript(const XSSInfo&);

private:
    Document* m_document;
    bool m_didNotifyClient;
};

typedef Vector<OwnPtr<XSSInfo> > XSSInfoStream;

}

#endif
