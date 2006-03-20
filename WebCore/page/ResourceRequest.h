// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
 */

#ifndef ResourceRequest_H_
#define ResourceRequest_H_

#include "formdata.h"
#include "PlatformString.h"
#include "KURL.h"

namespace WebCore {

    struct ResourceRequest {

        ResourceRequest() : m_lockHistory(false), reload(false), m_doPost(false) { }
        explicit ResourceRequest(const KURL& url) : m_lockHistory(false), reload(false), m_url(url), m_doPost(false) { }

        const KURL& url() const { return m_url; }
        void setURL(const KURL& url) { m_url = url; }

        String contentType() const { return m_contentType; }
        void setContentType(const String& t) { m_contentType = t; }
        
        bool doPost() const { return m_doPost; }
        void setDoPost(bool post) { m_doPost = post; }
        
        bool lockHistory() const { return m_lockHistory; }
        void setLockHistory(bool lock) { m_lockHistory = lock; }
        
        const String& referrer() const { return m_referrer; }
        void setReferrer(const String& referrer) { m_referrer = referrer; }

        // FIXME: these two parameters are specific to frame opening,
        // should move to FrameRequest once we have that
        String frameName;
    private:
        bool m_lockHistory;

    public:
        // FIXME: the response MIME type shouldn't be in here, it
        // should be in some kind of response object
        String m_responseMIMEType;
        
        FormData postData;
        bool reload;
    private:
        KURL m_url;
        String m_referrer;
        String m_contentType;
        bool m_doPost;
    };

}

#endif
