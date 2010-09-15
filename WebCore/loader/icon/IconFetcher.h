/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef IconFetcher_h
#define IconFetcher_h

#include <wtf/RefCounted.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

#include "ResourceHandleClient.h"

namespace WebCore {

class Frame;
struct IconLinkEntry;
class ResourceHandle;
class SharedBuffer;

class IconFetcherClient {
public:
    virtual void finishedFetchingIcon(PassRefPtr<SharedBuffer> iconData) = 0;
    
    virtual ~IconFetcherClient() { }
};

class IconFetcher : public RefCounted<IconFetcher>, ResourceHandleClient {
public:
    static PassRefPtr<IconFetcher> create(Frame*, IconFetcherClient*);
    ~IconFetcher();
    
    void cancel();
    
private:
    IconFetcher(Frame*, IconFetcherClient*);
    void loadEntry();
    void loadFailed();
    
    PassRefPtr<SharedBuffer> createIcon();
    
    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
    virtual void didFinishLoading(ResourceHandle*, double /*finishTime*/);
    virtual void didFail(ResourceHandle*, const ResourceError&);

    Frame* m_frame;
    IconFetcherClient* m_client;

    unsigned m_currentEntry;
    RefPtr<ResourceHandle> m_handle;
    Vector<IconLinkEntry> m_entries;
};
    
} // namespace WebCore

#endif // IconFetcher_h
