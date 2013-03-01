/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef CachedRawResource_h
#define CachedRawResource_h

#include "CachedResource.h"

namespace WebCore {

class CachedResourceClient;
class SubresourceLoader;

class CachedRawResource : public CachedResource {
public:
    CachedRawResource(ResourceRequest&, Type);

    // FIXME: AssociatedURLLoader shouldn't be a DocumentThreadableLoader and therefore shouldn't
    // use CachedRawResource. However, it is, and it needs to be able to defer loading.
    // This can be fixed by splitting CORS preflighting out of DocumentThreacableLoader.
    virtual void setDefersLoading(bool);

    virtual void setDataBufferingPolicy(DataBufferingPolicy);
    
    // FIXME: This is exposed for the InpsectorInstrumentation for preflights in DocumentThreadableLoader. It's also really lame.
    unsigned long identifier() const { return m_identifier; }

    SubresourceLoader* loader() const;
    void clear();

    virtual bool canReuse(const ResourceRequest&) const;

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

private:
    virtual void didAddClient(CachedResourceClient*);
    virtual void data(PassRefPtr<ResourceBuffer> data, bool allDataReceived);

    virtual bool shouldIgnoreHTTPStatusCodeErrors() const { return true; }
    virtual void allClientsRemoved();

    virtual void willSendRequest(ResourceRequest&, const ResourceResponse&);
    virtual void responseReceived(const ResourceResponse&);
    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
#if PLATFORM(CHROMIUM)
    virtual void didDownloadData(int);
#endif

    unsigned long m_identifier;

    struct RedirectPair {
    public:
        explicit RedirectPair(const ResourceRequest& request, const ResourceResponse& redirectResponse)
            : m_request(request)
            , m_redirectResponse(redirectResponse)
        {
        }

        const ResourceRequest m_request;
        const ResourceResponse m_redirectResponse;
    };

    Vector<RedirectPair> m_redirectChain;
};

}

#endif // CachedRawResource_h
