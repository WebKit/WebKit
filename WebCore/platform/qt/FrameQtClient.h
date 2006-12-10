/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
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

#ifndef FrameQtClient_H
#define FrameQtClient_H

#include <wtf/PassRefPtr.h>
#include "ResourceHandleClient.h"

namespace WebCore {

class String;
class FrameQt;
class FormData;

class FrameQtClient : public ResourceHandleClient
{
public:
    FrameQtClient();
    virtual ~FrameQtClient();

    //local
    void openURL(const KURL&);
    void submitForm(const String& method, const KURL&, PassRefPtr<FormData>);

    // FrameQtClient
    virtual void setFrame(const FrameQt*);

    // ResourceHandleClient
    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int);
    virtual void didFinishLoading(ResourceHandle*);
    virtual void didFail(ResourceHandle*, const ResourceError&);
    virtual void receivedAllData(ResourceHandle*, PlatformData);
private:
    // Internal helpers
    FrameQt* traverseNextFrameStayWithin(FrameQt*) const;
    int numPendingOrLoadingRequests(bool recurse) const;

    FrameQt* m_frame;
};

}

#endif

// vim: ts=4 sw=4 et
