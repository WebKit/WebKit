/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ResourceLoader.h"
#include <wtf/Forward.h>

#ifdef __OBJC__
#import "WebPlugInStreamLoaderDelegate.h"
#endif

namespace WebCore {
#if USE(CFNETWORK)
    class NetscapePlugInStreamLoader;

    class NetscapePlugInStreamLoaderClient {
    public:
        virtual void didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse&) = 0;
        virtual void didReceiveData(NetscapePlugInStreamLoader*, const char*, int) = 0;
        virtual void didFail(NetscapePlugInStreamLoader*, const ResourceError&) = 0;
        virtual void didFinishLoading(NetscapePlugInStreamLoader*) { }
    };
#endif

#ifdef __OBJC__
        typedef id <WebPlugInStreamLoaderDelegate> PlugInStreamLoaderDelegate;
#else
        class NetscapePlugInStreamLoaderClient;
        typedef NetscapePlugInStreamLoaderClient* PlugInStreamLoaderDelegate;
#endif

    class NetscapePlugInStreamLoader : public ResourceLoader {
    public:
        static PassRefPtr<NetscapePlugInStreamLoader> create(Frame*, PlugInStreamLoaderDelegate);
        virtual ~NetscapePlugInStreamLoader();

        bool isDone() const;

#if PLATFORM(MAC) || USE(CFNETWORK)
        virtual void didReceiveResponse(const ResourceResponse&);
        virtual void didReceiveData(const char *, int, long long lengthReceived, bool allAtOnce);
        virtual void didFinishLoading();
        virtual void didFail(const ResourceError&);

        virtual void releaseResources();
#endif

    private:
        NetscapePlugInStreamLoader(Frame*, PlugInStreamLoaderDelegate);

#if PLATFORM(MAC) || USE(CFNETWORK)
        virtual void didCancel(const ResourceError& error);
#endif

#if PLATFORM(MAC)
        RetainPtr<PlugInStreamLoaderDelegate > m_stream;
#elif USE(CFNETWORK)
        NetscapePlugInStreamLoaderClient* m_client;
#endif
    };

}
